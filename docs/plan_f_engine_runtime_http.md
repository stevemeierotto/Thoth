# Plan F — Engine Runtime & HTTP Server

**Status:** 🔒 Locked — 2026-07-13 (goals semantics + F+1 backlog)  
**Prerequisites:** Plans A ✅ · B ✅ · C ✅ · D ✅ · E ✅  
**Next:** [Plan G — Streaming & Observability](plan_g_streaming_observability.md) ✅ · [Plan I — Docker Compose v1](plan_i_docker_compose_v1.md) ✅  
**Roadmap:** [docker_roadmap.md](docker_roadmap.md) Step 3

---

## Architectural goal

Introduce a **transport layer** for `thoth-engine`. **HTTP is the first transport implementation.** The engine remains **transport-agnostic**.

```
BasicAgentPlugin
        ^
        |
 EngineRuntime  (command queue, worker thread, sessions, plugin lifetime)
        ^
        |
+----------------------------+
| CLI Transport (refactor)   |
| HTTP Transport (Plan F)    |
| SSE Transport (Plan G)     |
| Future GUI Transport (K)   |
+----------------------------+
```

**Plan F scope:** `EngineRuntime` + request/response HTTP routes. **No SSE** — that is Plan G.

### Ownership model

`EngineRuntime` is the core object — not the HTTP server. `thoth-engine` owns bootstrap and runtime; transports are adapters on top.

```
thoth-engine
    |
    +-- bootstrapRuntimeEnvironment()
    +-- EngineRuntime
            |
            +-- BasicAgentPlugin
            +-- worker thread
            +-- command queue
            +-- session manager
```

HTTP and CLI are peers that call into `EngineRuntime`. They do not own the plugin or worker thread.

---

## Boundary

### In scope
- `EngineRuntime` — plugin lifetime, command queue, worker thread, sessions
- HTTP transport adapter — routing, JSON request/response
- `thoth-engine --serve` mode
- Ops endpoints: `/health`, `/ready`, `/version`
- API endpoints: `/v1/chat`, `/v1/goals`, `/v1/control/*`
- Error schema, session rules, config precedence
- Basic graceful shutdown (no SSE clients yet)
- Vendored **cpp-httplib** at pinned release
- CLI refactor to route through `EngineRuntime`
- Unit/smoke tests

### Explicitly excluded (Plan G or later)
- `GET /v1/events` SSE
- Event fan-out to multiple HTTP clients
- `EventBus` multi-subscriber wiring (stub/hook only in F)
- Event envelope (`event_id`, `sequence`) documentation for wire format
- Docker / compose (**Plan I**)
- Inference adapter (**Plan H**) — see [plan_h_inference_adapter.md](plan_h_inference_adapter.md) 🔒
- GUI HTTP client (**Plan K**)
- Auth

**Future-proofing:** Future transports (SSE, GUI, IPC, WebSocket, etc.) integrate through `EngineRuntime` rather than directly with `BasicAgentPlugin`.

---

## `EngineRuntime`

Owns everything transports share. HTTP does **not** call `BasicAgentPlugin` directly.

| Responsibility | Owner |
|----------------|-------|
| `BasicAgentPlugin` lifetime | `EngineRuntime` |
| Command queue + worker thread | `EngineRuntime` |
| Session management | `EngineRuntime` |
| Plugin thread safety | `EngineRuntime` (single worker) |
| Event capture hook | `EngineRuntime` (internal; SSE export in Plan G) |

**Public API (Plan F):**

```cpp
namespace Thoth {

class EngineRuntime {
public:
    static std::unique_ptr<EngineRuntime> create();
    void shutdown();

    std::future<std::string> submitChat(const std::string& session_id, const std::string& text);
    std::future<std::string> submitGoal(const std::string& session_id, const std::string& goal);
    void pause();
    void resume();
    void abort();

    std::string workspacePath() const;
    bool isReady() const;
    std::vector<std::string> capabilities() const;  // F: chat, goals, control
};

} // namespace Thoth
```

- `isReady()` — single source of truth for readiness; `GET /ready` is a thin wrapper (maps `isReady()` → `status`, plus `workspacePath()` and `capabilities()`).
- `submitGoal()` — blocks until `execute_goal()` returns on the worker thread, including **synchronous initial planning** (`create_plan`). Step execution then continues asynchronously on the executive `loop_thread`. This preserves existing engine semantics (Plans A–E); no fire-and-forget goal API in Plan F.
- `subscribeEvents()` / `EventBus` fan-out added in **Plan G**.

---

## Session behavior

| Case | Behavior |
|------|----------|
| Missing `session_id` | `"default"` |
| Unknown `session_id` | Create lazily on first use |
| Command dispatch | `setSessionId()` on worker thread before handler |

---

## Error schema (all endpoints)

```json
{
  "error": {
    "code": "INVALID_REQUEST",
    "message": "Goal cannot be empty."
  }
}
```

| Code | HTTP |
|------|------|
| `INVALID_REQUEST` | 400 |
| `NOT_FOUND` | 404 |
| `ENGINE_BUSY` | 503 |
| `INTERNAL_ERROR` | 500 |

---

## HTTP API (Plan F)

| Method | Path | Purpose |
|--------|------|---------|
| `GET` | `/health` | Liveness — `{ "status": "ok" }` |
| `GET` | `/ready` | Thin wrapper over `isReady()`, `workspacePath()`, `capabilities()` |
| `GET` | `/version` | `{ "engine", "git", "protocol" }` |
| `POST` | `/v1/chat` | Chat / slash commands |
| `POST` | `/v1/goals` | Initiate goal; returns after acceptance + initial planning |
| `POST` | `/v1/control/pause` | Pause |
| `POST` | `/v1/control/resume` | Resume |
| `POST` | `/v1/control/abort` | Abort |

### `GET /ready` (Plan F capabilities)

HTTP handler delegates to `EngineRuntime::isReady()` — no duplicated readiness logic in the transport layer.

```json
{
  "status": "ready",
  "workspace": "/data/agent_workspace",
  "capabilities": ["chat", "goals", "control"]
}
```

When `isReady()` is false (e.g. during shutdown), return `503` with `"status": "not_ready"`. Plan G adds `"events"` to capabilities when SSE is live.

### `GET /version`

```json
{
  "engine": "0.2",
  "git": "94012a4",
  "protocol": "v1"
}
```

### `POST /v1/chat`

```json
{ "text": "What is GRAG?", "session_id": "default" }
→ { "response": "...", "session_id": "default" }
```

### `POST /v1/goals`

Initiates goal execution. The request returns after the engine accepts the goal and **completes initial planning** (`create_plan` on the worker thread). Subsequent step execution proceeds asynchronously on the executive loop.

Maps to `EngineRuntime::submitGoal()` → `execute_goal()`. Without a reachable LLM, the request may block until planning fails or times out — same as the in-process path.

```json
{ "goal": "...", "session_id": "default" }
→ { "status": "accepted", "message": "GOAL ACCEPTED: ..." }
```

**Future enhancement (out of Plan F scope):** `EngineRuntime::submitGoalAsync()` — return immediately after enqueue; acceptance and progress via Plan G SSE. See [`improvements.md`](improvements.md) § Containerization — Future Enhancements.

---

## Configuration precedence

```
CLI flag  >  environment variable  >  built-in default
```

| Setting | CLI | Env | Default |
|---------|-----|-----|---------|
| Bind | `--bind` | `THOTH_ENGINE_BIND` | `127.0.0.1` |
| Port | `--port` | `THOTH_ENGINE_PORT` | `8090` |
| Serve | `--serve` | — | off |

---

## Dependency

- **cpp-httplib** — vendored pinned release (e.g. `v0.15.3`), documented in `DEPENDENCIES.md`

---

## Graceful shutdown (Plan F)

On `SIGINT` / `SIGTERM`:

1. Stop accepting new HTTP connections
2. Return `503 ENGINE_BUSY` to new requests
3. Drain command queue (bounded wait)
4. Destroy `BasicAgentPlugin`
5. Close HTTP listener

SSE connection teardown deferred to **Plan G**.

---

## Checkpoints

Each checkpoint is a **stop gate** — complete verification, then pause for approval before starting the next checkpoint. F6 is the **final stop** before Plan F is considered complete.

| Checkpoint | Work | Stop | Verify |
|------------|------|------|--------|
| **F1** | Implement `EngineRuntime` + CLI refactor | ✅ Yes | Build; run `--execute`; REPL |
| **F2** | Implement sessions + error handling | ✅ Yes | Run unit tests |
| **F3** | Implement `/health`, `/ready`, `/version` | ✅ Yes | `curl` endpoints |
| **F4** | Implement `/v1/chat` | ✅ Yes | Compare with `--execute` |
| **F5** | Implement goals + control endpoints | ✅ Yes | Goal execution; pause / resume / abort |
| **F6** | Shutdown, tests, docs | ✅ Final stop | Full PR suite |

### Verification commands

Assume `engine-only` preset; adjust paths if using `debug`.

**F1 — EngineRuntime + CLI refactor**

```bash
cmake --preset engine-only
cmake --build --preset build-engine-only
./build/engine-only/external/basic_agent/thoth-engine --execute "/prune status"
./build/engine-only/external/basic_agent/thoth-engine   # REPL smoke
```

**F2 — Sessions + error handling**

```bash
ctest --test-dir build/engine-only -L pr -R "EngineRuntime|engine_runtime" --output-on-failure
```

*(Add `-R` filter once test names exist; otherwise run targeted tests from `thoth-core-tests`.)*

**F3 — Ops endpoints**

```bash
./build/engine-only/external/basic_agent/thoth-engine --serve --port 8090 &
curl -s http://127.0.0.1:8090/health
curl -s http://127.0.0.1:8090/ready
curl -s http://127.0.0.1:8090/version
```

**F4 — `/v1/chat`**

```bash
./build/engine-only/external/basic_agent/thoth-engine --execute "What is GRAG?"
curl -s -X POST http://127.0.0.1:8090/v1/chat \
  -H 'Content-Type: application/json' \
  -d '{"text":"What is GRAG?","session_id":"default"}'
# Responses should be equivalent for the same prompt
```

**F5 — Goals + control**

```bash
curl -s -X POST http://127.0.0.1:8090/v1/goals \
  -H 'Content-Type: application/json' \
  -d '{"goal":"List files in agent_workspace","session_id":"default"}'
curl -s -X POST http://127.0.0.1:8090/v1/control/pause
curl -s -X POST http://127.0.0.1:8090/v1/control/resume
curl -s -X POST http://127.0.0.1:8090/v1/control/abort
```

**F6 — Shutdown, tests, docs (final stop)**

```bash
# SIGINT (local dev)
kill -INT <pid>    # clean exit, no hang

# SIGTERM (container-style)
kill -TERM <pid>   # clean exit, no hang

# Full PR regression suite
ctest --test-dir build/engine-only -L pr -j1 --output-on-failure
```

Update `GETTING_STARTED.md` (and related docs) with `--serve`, bind/port env vars, and HTTP endpoint summary before marking F6 complete.

---

## Success criteria

- [ ] `thoth-engine` owns `EngineRuntime`; runtime owns plugin, worker, queue, sessions
- [ ] CLI and HTTP use `EngineRuntime`; no direct `BasicAgentPlugin` from transports
- [ ] `GET /ready` delegates to `isReady()` — no readiness logic in HTTP layer
- [ ] `GET /health` suitable for Docker healthcheck
- [ ] `POST /v1/chat` equivalent to `--execute` for same prompt
- [ ] Config precedence: CLI > env > default
- [ ] cpp-httplib pinned vendored version
- [ ] `ctest -L pr` green
- [ ] No cognitive behavior changes

---

## Rollback

Remove `EngineRuntime`, HTTP transport, httplib. Revert CLI to direct plugin usage.

---

## Plan lock

| Field | Value |
|-------|-------|
| **Locked** | 2026-07-13 |
| **Re-locked** | 2026-07-13 — `/v1/goals` semantics + backlog **F+1** (`submitGoalAsync`) |
| **Goals semantics (locked)** | `submitGoal()` / `POST /v1/goals` return after acceptance **and** synchronous initial planning (`create_plan`); step execution async on executive loop. No fire-and-forget goal API in Plan F. |
| **Deferred (backlog)** | **F+1** — `submitGoalAsync()` + Plan G SSE — see [`improvements.md`](improvements.md) § Containerization — Future Enhancements |
| **Scope** | `EngineRuntime` ownership, HTTP v1 (no SSE), checkpoints F1–F6 with stop gates |
| **Implementation** | Requires explicit approval per [AGENTS.md](../AGENTS.md) |
| **Post-lock changes** | Protocol lock rule — stop and request approval before editing this spec |

---

STATUS: PLAN LOCKED
