# Plan K — GUI API Client (optional)

**Status:** 🔒 Locked — 2026-07-14 (K0) · **K1 ✅** · **K2 ✅** · **K3 ✅** · **K4 ✅** · **K5 ✅ Complete** — 2026-07-15  
**Prerequisites:** [Plan F](plan_f_engine_runtime_http.md) ✅ · [Plan G](plan_g_streaming_observability.md) ✅ · [Plan I](plan_i_docker_compose_v1.md) ✅ · Step 6 hybrid docs ✅ · [Plan J](plan_j_ci_compose.md) ✅  
**Next:** Plan K closed — optional Step 8 nightly; workspace corpus ownership → [Plan L](plan_l_workspace_corpus.md) ✅ Complete (L3 deferred)  
**Roadmap:** [docker_roadmap.md](docker_roadmap.md) Step 9 ✅

---

## Purpose

Make the host-native GUI an **optional HTTP/SSE client** of remote `thoth-engine` (e.g. Compose `:8090`) while keeping in-process `BasicAgentPlugin` as the **default**.

Transport separation only. No cognitive, GRAG, planner, tool, EngineRuntime, or F/G API contract changes. No cognate HTTP APIs. No containerized wxWidgets GUI.

---

## Architectural goal

```
MainFrame
    │
    ▼
AgentInterface  ──┬── LocalBackend  → BasicAgentPlugin (unchanged)
                  │
                  └── RemoteBackend → libcurl HTTP + SSE
                                        │
                                        ▼
                              thoth-engine (Plans F + G)
```

`AgentInterface` remains the **only** GUI↔backend boundary. Public method signatures stay stable.

---

## Locked decisions (K0)

### Backend selection

| Condition | Backend |
|-----------|---------|
| `THOTH_ENGINE_URL` unset or empty/whitespace-only | **LocalBackend** (default) |
| `THOTH_ENGINE_URL` non-empty | **RemoteBackend** with that string as base URL |

- Selection once at `AgentInterface` construction.
- One backend per process — never both.
- Switch modes by changing env and **restarting** the GUI (no rebuild; no Preferences UI in Plan K).
- Normalize base URL by stripping one trailing `/`.
- Invalid URL: construct RemoteBackend; fail clearly on connect/`/ready` — **no silent fallback to local**.

### Configuration

- Env-only: `THOTH_ENGINE_URL`.
- No CLI flags, config-file keys, or wx Settings in Plan K.

### HTTP dependency

- **libcurl** for RemoteBackend (request/response + SSE).
- Already linked and globally initialized in the GUI process (`src/main.cpp`).
- Do not use cpp-httplib as the GUI client stack.
- Do not introduce a new HTTP library.

### Cognate / local-only APIs in remote mode

**Do not add cognate (or any new) engine HTTP APIs.**

| API | Remote behavior |
|-----|-----------------|
| `getStrategies` / `getTrajectories` / `getEpisodeSteps` / `getExperiments` / `getGraphStats` | Empty JSON matching **existing** MainFrame shapes |
| `saveExperiment` | `false` + stderr warning |
| `setRagFiles` | No-op + warning |
| `loadConversationMemory*` | `false` / no-op + warning |
| `checkResumablePlan` | No-op + warning |
| `getLatestDecisionTraceSummary` | Empty / unavailable string (no inventing host paths into container traces) |
| `GetBenchmarkBinaryPath` | Unchanged host helper |

Prefer **no MainFrame rewrite**; empty cognate panels are acceptable. Document in K4.

### F/G consumption (no server changes)

| Client action | Endpoint |
|---------------|----------|
| Connect gate | `GET /health`, `GET /ready` |
| Chat | `POST /v1/chat` — `{ "text", "session_id" }` → `{ "response", "session_id" }` |
| Goal | `POST /v1/goals` — `{ "goal", "session_id" }` → F acceptance JSON |
| Control | `POST /v1/control/pause\|resume\|abort` |
| Events (K3) | `GET /v1/events` — SSE → map to `ControllerEvent` |

Client-local string→`EventType` mapper from [`ENGINE_EVENTS.md`](ENGINE_EVENTS.md) (core has enum→string only).

### Gaps (do not invent)

| Gap | Action |
|-----|--------|
| No cognate HTTP | Empty shaped returns |
| No conversation-sync HTTP | Local-only; remote warns |
| No reverse `EventType` in core | Client mapper from docs |
| Host vs container workspace | Document only |

If a **required** live F/G field is missing, **STOP** and report — do not extend EngineRuntime.

---

## Mandatory locks (additional)

### 1. Backend lifecycle ownership

- **`AgentInterface` owns backend lifetime** (create in ctor; destroy in dtor after workers shut down).
- **No detached threads.** All backend threads are joinable and owned by the backend / AgentInterface shutdown path.
- **`RemoteBackend` must stop SSE (and any other) threads before destruction completes** — cancel → join → then release curl handles / complete destructor.
- **`curl_global_init` / `curl_global_cleanup` remain application-owned** (`src/main.cpp`). RemoteBackend must not call global init/cleanup.

### 2. Error propagation contract

- Transport failures, HTTP non-success, JSON parse failures, and SSE disconnects **flow through existing `AgentInterface` callback patterns** (`onResponse`, `onEvent` where applicable, and/or stderr consistent with today’s bridge logging) — not new GUI channels.
- **No exceptions cross the backend boundary** (`IAgentBackend` methods must not throw into `AgentInterface` / wx). Catch at the backend edge; convert to callback/log outcomes.
- **No retry/reconnect logic in Plan K** (no auto-reconnect SSE, no HTTP retry loops). One attempt per call; SSE ends on disconnect until process restart / future plan.

### 3. Session ownership

- **`AgentInterface` remains the session owner** (`setSessionId` / `activeSessionId` as today).
- **RemoteBackend receives `session_id` on requests** but does **not** create or manage an independent session store.
- **Preserve existing local session semantics** for LocalBackend (plugin paths unchanged).

### 4. Blocking / threading rules

- **HTTP requests must never run on the wx UI thread.**
- **Use the existing AgentInterface worker queue** for chat, goals, control, and readiness checks invoked from UI actions.
- **SSE runs on its own joinable thread**, independent of the worker queue; shutdown via **cancellation + join**. Event delivery into `onEvent` must remain wx-safe (marshal detail in K3).

### 5. `/ready` compatibility

- If `/ready` JSON includes **capability metadata**, validate that required capabilities for the intended use are present (chat / goals / control; for K3 also `events` when connecting SSE).
- If capability metadata is **absent**, treat a **successful `/ready`** (2xx + parseable or empty-tolerant body) as acceptable.
- **Do not require server changes** to shape or always emit capabilities.

### 6. JSON / API compatibility

- Client **tolerates missing optional fields and additional unknown fields**.
- **Unknown SSE fields or event type strings must not crash** the client (log + ignore / skip).
- **Do not require exact JSON schema equality** with fixtures; forward-compatible parsing only.

### 7. Remote empty / default data

- Remote fallback empty values **must match shapes MainFrame already expects** from today’s getters (same top-level JSON types / array-vs-object conventions).
- **Do not invent new remote-mode JSON schemas** or alternate panel payload formats.

---

## Checkpoint sequence

Each checkpoint is a **stop gate**: plan → approve → implement → verify → stop before the next.

| ID | Work | Verify |
|----|------|--------|
| **K0** | Specification lock (this document) | ✅ Locked 2026-07-14 |
| **K1** | `IAgentBackend` + LocalBackend; public API stable; local default | ✅ Implemented 2026-07-14 |
| **K2** | RemoteBackend: `/health`, `/ready`, chat, goals, control | ✅ Implemented 2026-07-15 |
| **K3** | SSE `/v1/events` → `ControllerEvent`; clean shutdown | ✅ Implemented 2026-07-15 |
| **K4** | `THOTH_ENGINE_URL` wiring + hybrid docs | ✅ Implemented 2026-07-15 |
| **K5** | Mapping tests + smoke docs + closeout + audit + final report | ✅ Complete 2026-07-15 |

---

## K1 — Backend abstraction (✅ implemented 2026-07-14)

**Status:** ✅ Implemented — plan locked 2026-07-14; code landed 2026-07-14.

### Goal

Insert an **internal** backend abstraction behind `AgentInterface` without changing the GUI contract or local behavior. **K1 = LocalBackend only** — no RemoteBackend, no `THOTH_ENGINE_URL`, no HTTP, no selection logic.

### Clarifications (mandatory)

1. **AgentInterface remains the only GUI contract.** `IAgentBackend` is internal; MainFrame must not know it exists (no includes, types, or calls). Public `AgentInterface` signatures and callbacks stay stable.

2. **AgentInterface owns the event bridge.** Backends invoke callbacks only via `setEventHandler` / stored `EventCallback`. Backends must **never** reference wx objects, `MainFrame`, `CallAfter`, or UI types.

3. **LocalAgentBackend owns `BasicAgentPlugin` exclusively after K1.** Only Local constructs/holds the plugin. `AgentInterface` must not retain a plugin pointer. No other class gains direct plugin access.

4. **LocalBackend exception handling.** Catch at the backend boundary on every `IAgentBackend` method. Log **operation name + error** (stderr). Return a **failure sentinel** — do **not** silently swallow:
   - `processInput` → `std::nullopt` (AgentInterface skips `onResponse`)
   - control / setters → no-op after log
   - cognate getters → empty shaped JSON after log
   - `saveExperiment` → `false` after log  
   Worker-queue try/catch remains a safety net only.

5. **`isRemote()` exists only for identification/logging.** Local returns `false`. No backend selection logic in K1 (no env reads, no remote branch).

6. **Move cognate JSON serialization only.** Move existing AgentInterface cognate→JSON blocks into LocalBackend. Do **not** move cognition, memory ownership, or business logic. Plugin remains owner of memory/cognition; Local only forwards + serializes.

7. **Before K1 closeout verify `git diff` scope.** Only backend abstraction files, `AgentInterface`, CMake, and docs should change. No MainFrame, no engine, no unrelated refactors.

### Delivered files

| Path | Action |
|------|--------|
| `includes/i_agent_backend.h` | Added (internal) |
| `includes/local_agent_backend.h` | Added |
| `src/local_agent_backend.cpp` | Added — exclusive plugin owner; cognate serialize only |
| `includes/AgentInterface.h` | `plugin` → `backend`; public API unchanged |
| `src/AgentInterface.cpp` | Always `LocalAgentBackend`; owns event bridge |
| `CMakeLists.txt` | `local_agent_backend.cpp` on GUI target |
| `tests/CMakeLists.txt` | `local_agent_backend.cpp` + `third_party` include for `thoth-gui-tests` |

### Verification (K1)

- `thoth-control-panel` builds
- `thoth-gui-tests` builds (links LocalBackend)
- MainFrame unchanged / no `IAgentBackend` references
- Always Local; no `THOTH_ENGINE_URL` selection

### Protocol

K1 complete. K2 plan is locked below; code still requires **Implement K2**.

---

## K2 — Remote HTTP backend (✅ implemented 2026-07-15)

**Status:** ✅ Implemented — plan locked 2026-07-14; code landed 2026-07-15.

### Goal

Add **`RemoteAgentBackend`**: libcurl client for Plan F request/response against `thoth-engine`. Clear failures when the engine is unavailable. **No SSE (K3). No production `THOTH_ENGINE_URL` / AgentInterface selection (K4).**

### Clarifications (mandatory) — followed

1. **Construction boundary.** Linked into GUI / gui-tests. **Not** constructed by production `AgentInterface` (still always Local). Tests / opt-in harness only until K4.
2. **libcurl ownership.** Request-level easy handles only; no `curl_global_*`.
3. **Ready gate.** First connectivity use: `/health` + `/ready`; cached; no retries.
4. **URL handling.** Trailing slash only.
5. **HTTP handling.** 2xx parse; non-2xx `EngineError`/`message`; parse failures logged; no throw across boundary.
6. **Response handling.** Optional fields tolerated; missing semantic fields warn; no crash.
7. **Tests.** Offline utils + empty-URL in PR/gui; live via `THOTH_REMOTE_LIVE_URL` opt-in; `ctest -L pr` not container-dependent.
8. **Timeouts.** Named constants in `remote_agent_http_utils.h`.

### Delivered files

| Path | Role |
|------|------|
| `includes/remote_agent_http_utils.h` | URL / capabilities / error format + timeout constants |
| `includes/remote_agent_backend.h` | Remote backend |
| `src/remote_agent_backend.cpp` | libcurl F client |
| `CMakeLists.txt` / `tests/CMakeLists.txt` | Link Remote; core includes for offline utils |
| `tests/unit_tests.cpp` | `testRemoteHttpUtilsOffline`; GUI empty-URL + live opt-in |

### Verification (K2)

- `thoth-control-panel` builds with Remote linked but unused by AgentInterface
- `thoth-core-tests` / `thoth-gui-tests` PASS (offline)
- AgentInterface still `LocalAgentBackend` only
- Live Compose: `THOTH_REMOTE_LIVE_URL=http://127.0.0.1:8090 ctest -R thoth-gui-tests` (opt-in)

### Protocol

K2 complete. K3 plan is locked below; code still requires **Implement K3**.

---

## K3 — SSE event streaming (✅ implemented 2026-07-15)

**Status:** ✅ Implemented — plan locked 2026-07-15; code landed 2026-07-15.

### Goal

Extend **`RemoteAgentBackend`** with a **single joinable** `GET /v1/events` SSE client that maps wire JSON to `ControllerEvent` and invokes the existing `setEventHandler` callback. No wx in the backend. No AgentInterface env selection (K4). No reconnect.

### Clarifications — followed

1. One SSE thread; `startSseIfNeeded()` idempotent  
2. Serial callbacks in receive order  
3. Shutdown: cancel → abort curl → exit loop → join → destroy  
4. Parse failures continue; transport failures end stream  
5. Blank-line frame buffering; no partial JSON parse  
6. `setEventHandler` replaces atomically without SSE restart  
7. Pure helpers in `remote_agent_sse_utils.h`

### Delivered files

| Path | Role |
|------|------|
| `includes/remote_agent_sse_utils.h` | Pure framing + EventType/JSON mapping + `kSseConnectTimeoutSec` |
| `includes/remote_agent_backend.h` / `src/remote_agent_backend.cpp` | SSE thread + shutdown |
| `tests/unit_tests.cpp` | `testRemoteSseUtilsOffline`; live opt-in listens for events |

### Verification (K3)

- `thoth-gui-tests` / `thoth-core-tests` PASS (offline)  
- Empty-URL + handler: dtor `stopSse` without hang  
- AgentInterface still Local-only  
- Live: `THOTH_REMOTE_LIVE_URL=http://127.0.0.1:8090 ctest -R thoth-gui-tests`

### Protocol

K3 complete. K4 selection is implemented below.

---

## K4 — Backend selection & docs (✅ implemented 2026-07-15)

**Status:** ✅ Implemented 2026-07-15.

### Goal

Wire **`THOTH_ENGINE_URL`** so `AgentInterface` selects Local or Remote at startup. Hybrid docs updated for Compose GUI targeting.

### Clarifications — followed

1. Only AgentInterface reads `THOTH_ENGINE_URL`; backends and MainFrame do not.
2. One log line: `backend=local` or `backend=remote url=<normalized>`.
3. Trim → empty Local → non-empty Remote → trailing-slash normalize only.
4. Remote ctor does not require engine; GUI starts offline; failures on use.
5. Selection unit tests are pure (no wx/Docker/engine/plugin construction).
6. Docs: workflow + limitations; cognate APIs not claimed implemented.

### Delivered

| Path | Role |
|------|------|
| `remote_agent_http_utils.h` | `trimWhitespace`, `resolveEngineBaseUrlFromRawEnvValue`, `resolveThothEngineUrlFromEnv` |
| `AgentInterface.cpp` | ctor selection + one log line |
| `tests/unit_tests.cpp` | `testThothEngineUrlSelectionOffline` |
| `docker/README.md`, `GETTING_STARTED.md` | Hybrid GUI remote workflow |

### Protocol

K4 complete. K5 is implemented below.

---

## K5 — Testing & closeout (✅ complete 2026-07-15)

**Status:** ✅ Implemented 2026-07-15.

### Delivered

| ID | Work | Evidence |
|----|------|----------|
| **K5.1** | Chat/goal pure mapping helpers + `testRemoteChatGoalMappingOffline` | `remote_agent_http_utils.h`, `tests/unit_tests.cpp` |
| **K5.2** | Smoke checklist + local regression note | `docker/README.md`, `GETTING_STARTED.md` |
| **K5.3** | `ctest -L pr` verified; Plan K marked complete | this doc, `docker_roadmap.md`, `completed_improvements_log.md` |
| **K5.4** | Architecture audit | final report below |
| **K5.5** | Final report | § K5 final report |

### K5 final report

**New files (Plan K total):** `i_agent_backend.h`, `local_agent_backend.h/cpp`, `remote_agent_backend.h/cpp`, `remote_agent_http_utils.h`, `remote_agent_sse_utils.h`

**Modified (Plan K total):** `AgentInterface.h/cpp`, `CMakeLists.txt`, `tests/CMakeLists.txt`, `tests/unit_tests.cpp`, `docker/README.md`, `docs/GETTING_STARTED.md`, `docs/docker_roadmap.md`, `docs/completed_improvements_log.md`, `docs/plan_k_gui_api_client.md`

**Explicitly unchanged:** EngineRuntime, GRAG, planner, tools, F/G HTTP contracts, `MainFrame.cpp`, cognate HTTP APIs

**Regression evidence:**

- **Local:** `THOTH_ENGINE_URL` unset → `backend=local`; in-process `BasicAgentPlugin` path unchanged
- **Remote:** additive via env + GUI restart; no silent fallback to Local

**Remote validation:** manual smoke checklist in `docker/README.md`; opt-in `THOTH_REMOTE_LIVE_URL` harness (not required for PR tests)

### K5.4 architecture audit

| Check | Result |
|-------|--------|
| MainFrame | No `THOTH_ENGINE_URL` / backend selection |
| AgentInterface | Owns selection + backend lifetime at ctor |
| Backends | No wx includes in `local_agent_backend.*` / `remote_agent_backend.*` |
| `THOTH_ENGINE_URL` | Read only in `AgentInterface.cpp` ctor (via `resolveThothEngineUrlFromEnv`) |
| Local path | `LocalAgentBackend` — no libcurl; Remote not constructed when URL unset |

### Protocol

Plan K closed. No further K checkpoints unless a new plan is opened.

---

## Testing strategy (direction locked; execute in K5)

| Layer | Scope |
|-------|--------|
| **Unit (PR suite)** | URL select/normalize; chat/goal JSON map; event-type map; tolerant parsers (§6–7) |
| **Integration** | Documented Compose smoke — live Docker **not** required in `ctest -L pr` |
| **Regression** | Unset URL → local unchanged; `ctest -L pr` green |
| **Lifecycle** | No throw across boundary; SSE cancel+join where feasible |

---

## Explicitly out of scope

- Cognate (or any new) engine HTTP APIs
- Containerized wxWidgets GUI
- Auth / TLS / API keys
- GRAG, planner, cognition, tools, EngineRuntime, or F/G contract changes
- Retry / reconnect
- Changing curl global ownership
- Syncing host `agent_workspace/` with container volumes
- Step 8 nightly compose+inference
- Making remote the default

---

## Related docs

- [ENGINE_EVENTS.md](ENGINE_EVENTS.md) — SSE wire format
- [plan_f_engine_runtime_http.md](plan_f_engine_runtime_http.md) — HTTP contracts
- [plan_g_streaming_observability.md](plan_g_streaming_observability.md) — SSE server
- [docker_roadmap.md](docker_roadmap.md) — Step 9
- [docker/README.md](../docker/README.md) — hybrid workflow (updated in K4)

---

## Protocol

Human approval gates every checkpoint. **Plan K ✅ complete** (K0–K5).
