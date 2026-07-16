# Plan I — Docker Compose v1

**Status:** 🔒 Locked — 2026-07-13 · **Implemented** 2026-07-13 (I1–I6)  
**Prerequisites:** Plans A–E ✅ · [Plan F](plan_f_engine_runtime_http.md) ✅ · [Plan G](plan_g_streaming_observability.md) ✅ · [Plan H](plan_h_inference_adapter.md) ✅  
**Next:** [Plan J — CI Compose Packaging Job](plan_j_ci_compose.md) ✅ · Step 8 nightly · Plan K (optional)  
**Roadmap:** [docker_roadmap.md](docker_roadmap.md) Step 5 — Docker packaging phase

---

## Purpose

Implements the containerization roadmap's **Docker packaging phase**. Plans F–H remain **prerequisites** and are **packaged unchanged** into the container stack:

- **Plans F–H** created the runtime, HTTP API, SSE observability, and inference adapter.
- **Plan I** containers those components (`thoth-engine` + `llama-server` + volumes + networking).
- **Nothing from F–H is superseded or replaced.**

Plan I delivers the initial Docker Compose stack, consolidating roadmap prep work (standalone `llama-server` profile) and compose delivery into one gated implementation plan.

---

## Goal

Deliver a **minimal, reproducible container stack** for `thoth-engine` with a co-located inference service, persistent workspace/logs volumes, and HTTP/SSE endpoints suitable for hybrid deployment (host GUI → container engine).

This is **packaging only** — no cognitive, memory, GRAG, or evaluation changes.

### Target architecture (v1)

```
┌─────────────────────┐     HTTP/SSE      ┌──────────────────────────┐
│  thoth-control-panel │ ────────────────► │  thoth-engine (container) │
│  (host-native, opt)  │    :8090          │  --serve · EngineRuntime  │
└─────────────────────┘                   └────────────┬─────────────┘
                                                         │
                    docker network `thoth-net`           │
                          ┌──────────────────────────────┼──────────────────┐
                          ▼                              ▼                  ▼
                 ┌─────────────────┐          ┌─────────────────┐  ┌──────────────┐
                 │ thoth-workspace │          │  llama-server   │  │  thoth-logs  │
                 │ volume          │          │  (default)      │  │  volume      │
                 │ SQLite + RAG    │          │  :8080 internal │  │  JSONL       │
                 └─────────────────┘          └─────────────────┘  └──────────────┘
                                                        ▲
                                               ┌────────┴────────┐
                                               │  llama-models   │
                                               │  volume (ro)    │
                                               └─────────────────┘
```

**Critical design rule:**

- **Compose owns process topology, volumes, and env wiring.**
- **`thoth-engine` owns runtime semantics** (unchanged from Plans F–H).

---

## Current state (baseline)

| Component | Today |
|-----------|-------|
| Engine binary | `thoth-engine` built via `engine-only` preset (`THOTH_BUILD_GUI=OFF`) |
| HTTP API | Plan F — `/health`, `/ready`, `/version`, `/v1/chat`, `/v1/goals`, `/v1/control/*` |
| SSE | Plan G — `GET /v1/events` |
| Inference | Plan H — `THOTH_INFERENCE_BACKEND=ollama\|llama_cpp` |
| Portable paths | Plan A/B — `THOTH_WORKSPACE_PATH`, `THOTH_LOGS_PATH`, `THOTH_INFERENCE_BASE_URL` |
| Shutdown | `SIGINT`/`SIGTERM` → bounded drain, SSE close (F6/G6) |
| Docker artifacts | **None** — no `Dockerfile`, no `docker-compose.yml` |

---

## Non-negotiable constraints

1. **Packaging only** — no planner, memory, GRAG, executive, or benchmark logic changes.
2. **Engine-only image** — no wxWidgets / GUI in the container (Plan C boundary).
3. **SQLite on local volume** — `memory.db` must not use NFS/network filesystems (WAL).
4. **Graceful stop** — container `stop` must use `SIGTERM`; engine must exit cleanly (existing F6/G6 behavior).
5. **Host-native dev unchanged** — `cmake --preset engine-only` remains the primary developer workflow; Docker is additive.
6. **Locked persistence model** — no MariaDB / `thoth-db` container in v1.
7. **No API authentication in v1** — compose exposes engine port; operator secures at network boundary.

### Explicitly excluded (Plan I)

- Containerized wxWidgets GUI (**Plan K** / deferred)
- `AgentInterface` HTTP client (**Plan K**)
- CI compose job (**Plan J** — optional follow-on)
- API auth / TLS termination in compose (operator concern)
- Automatic model download in image build (models mounted via volume)
- Multi-replica / horizontal scaling of `thoth-engine`
- Kubernetes manifests

### Optional compose profile (locked as optional, not default)

| Profile | Inference | When |
|---------|-----------|------|
| *(default)* | `llama-server` + `THOTH_INFERENCE_BACKEND=llama_cpp` | Recommended v1 |
| `ollama` | Ollama sidecar + `THOTH_INFERENCE_BACKEND=ollama` | Transitional / dev parity |

---

## Boundary

### In scope

| Checkpoint | Work |
|------------|------|
| **I1** | `thoth-engine` Dockerfile (multi-stage, `engine-only` build) |
| **I2** | `llama-server` service definition (version-pinned image; roadmap Step 1 prep absorbed here) |
| **I3** | `docker-compose.yml` — services, network, `depends_on`, ports |
| **I4** | Env vars + named volumes + first-run workspace bootstrap |
| **I5** | Healthchecks, `stop_grace_period`, readiness ordering |
| **I6** | Docs, smoke script, verification gates |

### Locked file layout

| Path | Purpose |
|------|---------|
| `docker/Dockerfile.engine` | Multi-stage build → runtime image |
| `docker/entrypoint-engine.sh` | Minimal dir ensure + exec `thoth-engine` (optional if CMD sufficient) |
| `docker-compose.yml` | v1 stack at repo root |
| `.dockerignore` | Exclude `build/`, `agent_workspace/`, `.git`, GUI artifacts |
| `docker/smoke.sh` | Post-`up` verification (curl health/chat/events) |
| `docker/README.md` | Operator quickstart (or section in `GETTING_STARTED.md`) |

---

## `thoth-engine` image (I1)

### Headless build target (locked)

The Dockerfile **must build the headless runtime only** — no Qt/wxWidgets artifacts in any stage.

| Requirement | Locked value |
|-------------|--------------|
| GUI | `THOTH_BUILD_GUI=OFF` (via `engine-only` preset or explicit `-DTHOTH_BUILD_GUI=OFF`) |
| Build type | **Release** (`CMAKE_BUILD_TYPE=Release`) |
| Install scope | **Runtime artifacts only** — `thoth-engine`, `libbasic_agent.so`, and required shared libs |
| Forbidden in image | `thoth`, wxWidgets, Qt, GUI test binaries, `build/debug` GUI targets |

**Verify at I1:** `docker run --rm thoth-engine:local sh -c 'command -v thoth && ! command -v thoth-gui 2>/dev/null'` (or equivalent — no GUI binary present).

### Build strategy (locked)

Multi-stage Dockerfile:

1. **builder** — `cmake --preset engine-only` + `cmake --build --preset build-engine-only --target thoth-engine` with **Release** configuration
2. **runtime** — slim base (`debian:bookworm-slim` or `ubuntu:22.04`), copy **only** `thoth-engine` + `libbasic_agent.so` + runtime deps

**Build args (optional):** `THOTH_ENGINE_VERSION` — default from CMake.

### Runtime dependencies (locked minimum)

- `libcurl4`, `libsqlite3-0`, `ca-certificates`
- No Ollama / llama.cpp inside engine image — inference is a **separate service**

### Container defaults (locked)

| Setting | Value |
|---------|-------|
| `USER` | non-root (`thoth`, uid 1000) when feasible |
| `WORKDIR` | `/workspace` |
| `EXPOSE` | `8090` |
| `CMD` | `thoth-engine --serve --bind 0.0.0.0 --port 8090` |

`THOTH_ENGINE_BIND` must be `0.0.0.0` in compose (not `127.0.0.1`).

---

## `llama-server` service (I2)

Roadmap **Step 1** (`llama-server` compose profile) is **delivered here** as part of Plan I — not as a separate implementation track. This does not alter Plans F–H engine code.

### Image pinning (locked)

- **Registry/format:** `ghcr.io/ggerganov/llama.cpp:<version>` (or successor org tag verified at lock — document exact choice in `docker-compose.yml`)
- **Forbidden:** `:latest`, floating tags, or unpinned `server` aliases
- **Recommended:** pin **image digest** in compose comments or `docker-compose.lock.yml` for reproducibility
- **Example (illustrative — replace at lock):** `ghcr.io/ggerganov/llama.cpp:b3961`

Tag selection is locked at Plan I implementation approval and recorded in `docker/README.md`.

### Locked default

- **Internal port:** `8080`
- **Models:** read-only mount `llama-models:/models:ro`
- **Command:** load chat + embed models from `/models/` (exact flags documented in `docker/README.md`)
- **`restart`:** `unless-stopped` (see I5)

### Health (locked)

- `GET http://llama-server:8080/health` → 200

### Not published to host by default

Inference is reached only via Docker network DNS name `llama-server` from `thoth-engine`.

---

## Compose wiring (I3)

### Startup dependency (locked — anti-race)

`thoth-engine` **must not** start until `llama-server` is healthy. Floating `depends_on` without a condition is **out of spec**.

```yaml
thoth-engine:
  depends_on:
    llama-server:
      condition: service_healthy   # REQUIRED — not optional
```

### Services (default profile)

```yaml
services:
  llama-server:
    image: ghcr.io/ggerganov/llama.cpp:<version>   # I2 — never :latest
    restart: unless-stopped
    healthcheck: ...                              # I5
  thoth-engine:
    build:
      context: .
      dockerfile: docker/Dockerfile.engine
    restart: unless-stopped
    depends_on:
      llama-server:
        condition: service_healthy
    ports:
      - "${THOTH_ENGINE_PUBLISH_PORT:-8090}:8090"
    environment: # I4
    volumes:     # I4
    stop_grace_period: 30s                        # I5

networks:
  thoth-net:
    driver: bridge

volumes:
  thoth-workspace:
  thoth-logs:
  llama-models:
```

### Port policy (locked)

| Service | Host publish | Reason |
|---------|--------------|--------|
| `thoth-engine` | `8090` (configurable) | Host GUI / curl / hybrid dev |
| `llama-server` | none | Internal only |

---

## Environment variables (I4 — locked)

Compose sets runtime env; **no new Thoth code env vars** beyond Plans A/B/F/H.

### Configuration precedence (locked)

When the same setting appears in multiple places, resolution order is:

```
compose `.env` / `env_file`
        ↓
container `environment:` block in docker-compose.yml
        ↓
`EnvLoader` / workspace `config.json` (lowest — only for keys not set above)
```

**Rules:**

- Compose-owned keys (`THOTH_WORKSPACE_PATH`, `THOTH_LOGS_PATH`, `THOTH_INFERENCE_*`, `THOTH_ENGINE_*`) are set in `docker-compose.yml` and **must not** rely on `config.json` overrides inside the container.
- `EnvLoader` applies only to unset variables (existing Plan A/B semantics).
- Document this table in `docker/README.md` to avoid future ambiguity.

| Variable | Compose value | Notes |
|----------|---------------|-------|
| `THOTH_WORKSPACE_PATH` | `/workspace` | Mount `thoth-workspace` |
| `THOTH_LOGS_PATH` | `/logs` | Mount `thoth-logs` |
| `THOTH_INFERENCE_BASE_URL` | `http://llama-server:8080` | Docker DNS |
| `THOTH_INFERENCE_BACKEND` | `llama_cpp` | Plan H default for compose |
| `THOTH_ENGINE_BIND` | `0.0.0.0` | Required in container |
| `THOTH_ENGINE_PORT` | `8090` | Match `EXPOSE` |
| `THOTH_LOG_CONFIG` | `1` | Optional; helpful first-run diagnostics |

**Optional overrides (unchanged semantics):**

- `THOTH_EMBED_BASE_URL` — separate embed host if needed
- `THOTH_LLM_TIMEOUT_SECONDS`, model names via mounted `config.json`

### `ollama` profile overrides

| Variable | Value |
|----------|-------|
| `THOTH_INFERENCE_BASE_URL` | `http://ollama:11434` |
| `THOTH_INFERENCE_BACKEND` | `ollama` |

---

## Volume bootstrap (I4)

### Named volumes (locked)

| Volume | Container path | Contents |
|--------|----------------|----------|
| `thoth-workspace` | `/workspace` | `memory.db`, `rag/`, `rag_index.bin`, `config.json`, traces |
| `thoth-logs` | `/logs` | `cognitive_metrics.jsonl`, benchmark JSONL |
| `llama-models` | `/models` (ro on server) | Operator-supplied GGUF files |

### First-run behavior (document, do not auto-seed in image)

1. Empty `thoth-workspace` → engine creates SQLite DB on first use (existing behavior).
2. Empty `rag/` → RAG bootstrap may index sandbox on first start (**slow**; document in operator guide).
3. **Do not** bake `agent_workspace/` from git into the image — runtime state belongs on the volume.
4. Optional `docker/seed-workspace.sh` for **dev only** (copy minimal `rag/` test corpus) — **not** run automatically in v1. **Ownership/seed source locked in [Plan L](plan_l_workspace_corpus.md)** (`docker/seed_rag/`; no host `agent_workspace` mirror).

### Permissions

Entrypoint ensures `/workspace` and `/logs` exist and are writable by runtime user.

---

## Healthchecks, restart & shutdown (I5)

### Restart policy (locked)

Both `llama-server` and `thoth-engine`:

```yaml
restart: unless-stopped
```

**Forbidden:** omitting `restart` (undefined policy). **`restart: no`** is allowed only in a documented `dev` compose override file, not the locked v1 `docker-compose.yml`.

### `thoth-engine` health probes (locked)

| Probe | Endpoint | Use |
|-------|----------|-----|
| **Liveness** | `GET /health` | Docker `HEALTHCHECK` |
| **Readiness** | `GET /ready` | `depends_on` ordering for downstream smoke (optional second check) |

`GET /ready` must report `capabilities` including `chat`, `goals`, `control`, `events` when Plans F+G are in the image.

**Readiness vs inference:** `/ready` reflects `EngineRuntime::isReady()`, not remote inference health. Compose `depends_on: llama-server: service_healthy` provides inference ordering.

### `llama-server` (locked)

| Probe | Endpoint |
|-------|----------|
| **Health** | `GET /health` |

### Graceful shutdown (locked)

```yaml
thoth-engine:
  stop_grace_period: 30s
  stop_signal: SIGTERM
```

**Verify:** `docker compose stop thoth-engine` exits 0; no hung SSE clients; no corrupt SQLite WAL.

---

## Checkpoints

Plan I uses the same **stop-gate discipline** as Plans F/G/H: implement one checkpoint, verify, **STOP**, wait for explicit approval.

### Checkpoint summary

| Checkpoint | Purpose | Lock point |
|------------|---------|------------|
| **I1** | Engine image builds | Dockerfile + `.dockerignore` approved |
| **I2** | Inference service runs | llama-server health green |
| **I3** | Compose topology | Services communicate on `thoth-net` |
| **I4** | Volumes + env | Persistence paths correct |
| **I5** | Health + shutdown | Container stop is clean |
| **I6** | Docs + smoke | Operator can run stack from README |

### Checkpoint detail

| CP | Scope | Intentionally NOT included | Verify |
|----|-------|---------------------------|--------|
| **I1** | `docker/Dockerfile.engine`; **headless Release build** (`THOTH_BUILD_GUI=OFF`); runtime artifacts only; `.dockerignore` | Compose, llama image, CI | `docker build` succeeds; `--version`; **no GUI binaries** in image |
| **I2** | `llama-server`; **version-pinned** image (never `:latest`); `llama-models` volume | Engine Dockerfile, ollama profile | `docker compose up llama-server` → health 200; tag recorded in README |
| **I3** | `docker-compose.yml`; `thoth-net`; **`depends_on` + `service_healthy`**; `restart: unless-stopped` | Seed scripts, CI, auth | Engine starts only after llama-server healthy |
| **I4** | Env table wired; volume mounts; entrypoint dir ensure; bootstrap docs | Auto model download, git workspace in image | Chat persists `memory.db` across `compose down`/`up`; logs written to `thoth-logs` |
| **I5** | `HEALTHCHECK` both services; `stop_grace_period: 30s`; SIGTERM smoke | K8s probes, TLS | `docker compose stop` clean; `curl /ready` 200 when up |
| **I6** | `docker/smoke.sh`; `docker/README.md` or `GETTING_STARTED` section; roadmap log | Plan J CI job, Plan K GUI | Smoke script green; `ctest -L pr -j1` still green (host-native, no regression) |

### Gate rule (mandatory)

**After each checkpoint: STOP.** Confirm:

1. Non-negotiable constraints 1–7 hold.
2. No cognitive / protocol code changed (Dockerfiles + compose + docs only until I6).
3. Verification criteria met.
4. Scope from “Explicitly excluded” did not creep in.

**I6 is the final implementation stop** before Plan I is considered complete.

---

## Verification commands

Assume repo root.

**I1 — Build engine image**

```bash
docker build -f docker/Dockerfile.engine -t thoth-engine:local .
docker run --rm thoth-engine:local --version
```

**I2 — Inference service only**

```bash
# Operator must place GGUF models in named volume or bind mount first
docker compose up -d llama-server
curl -sf http://127.0.0.1:8080/health   # if temporarily published for dev; else docker compose exec
docker compose down
```

**I3 — Full stack**

```bash
docker compose up -d --build
docker compose ps
```

**I4 — Persistence smoke**

```bash
curl -s -X POST http://127.0.0.1:8090/v1/chat \
  -H 'Content-Type: application/json' \
  -d '{"text":"/help","session_id":"default"}'
docker compose down
docker compose up -d
# memory.db and session state should survive on thoth-workspace volume
```

**I5 — Health + shutdown**

```bash
curl -sf http://127.0.0.1:8090/health
curl -sf http://127.0.0.1:8090/ready | jq .
docker compose stop thoth-engine
echo exit=$?
```

**I6 — SSE + smoke script**

```bash
./docker/smoke.sh
# includes: /health, /ready, /v1/chat, brief /v1/events connection

# Host-native regression (must stay green)
ctest --test-dir build/engine-only -L pr -j1 --output-on-failure
```

---

## `docker/smoke.sh` contract (I6)

Deterministic sequence (script **must** follow this order):

| Step | Action | Pass criteria |
|------|--------|---------------|
| 1 | `docker compose up -d --build` | Exit 0 |
| 2 | Wait for healthy | `docker compose ps` shows `llama-server` and `thoth-engine` healthy (bounded timeout, e.g. 120s) |
| 3 | `GET /health` | HTTP 200 |
| 4 | `GET /ready` | HTTP 200; `capabilities` includes `chat`, `goals`, `control`, `events` |
| 5 | `POST /v1/chat` | `{"text":"/help","session_id":"default"}` → HTTP 200 |
| 6 | `POST /v1/goals` | Minimal goal JSON → HTTP 200 `status: accepted` (or documented skip when inference offline) |
| 7 | Verify SSE stream | `curl -N /v1/events` receives keepalive or event within 5s, then disconnect |
| 8 | `docker compose down` | Exit 0 |

Any step failure → script exit non-zero.

Use `curl` (not cpp-httplib client) for SSE per [Plan G testing note](plan_g_streaming_observability.md).

**I6 verify:**

```bash
./docker/smoke.sh
ctest --test-dir build/engine-only -L pr -j1 --output-on-failure
```

---

## Success criteria

- [x] `docker compose` topology: `llama-server` + `thoth-engine` (`depends_on: service_healthy`, volumes, env)
- [x] Engine image builds; `docker run --rm thoth-engine:local --version` → `thoth-engine 0.2`
- [x] No GUI binary in image; headless Release build (`THOTH_BUILD_GUI=OFF`)
- [x] Isolated engine HTTP smoke: `/health`, `/ready` (chat/goals/control/events), SSE keepalive, clean `docker stop`
- [x] Operator docs (`docker/README.md`) + `docker/smoke.sh` (8-step contract)
- [ ] Full `./docker/smoke.sh` green (requires GGUF on `llama-models` volume)
- [ ] Host-native `ctest -L pr -j1` fully green — **`test-suite-dev` SEGFAULT** (see below)

---

## Risks and mitigations

| Risk | Impact | Mitigation |
|------|--------|------------|
| llama.cpp image tag drift | Broken server | Pin image digest in compose; document in `docker/README.md` |
| Large GGUF models | Slow / failed start | Models on `llama-models` volume; document operator prep |
| First-start RAG index | Timeout on readiness | Document; optional `THOTH_SKIP_RAG_BOOTSTRAP` env **only if already exists** — do not add without approval |
| SQLite on bind mount | WAL corruption | Prefer named local volume; warn against NFS |
| SSE + proxy timeouts | GUI disconnect | Document for hybrid dev; out of compose scope |
| Engine image size | Slow CI | Multi-stage build; strip debug symbols in release tag |
| Ollama profile maintenance | Two paths | Optional profile only; default stays `llama_cpp` |

---

## Rollback

Remove `docker/`, `docker-compose.yml`, `.dockerignore`. Host-native workflow unaffected.

---

## Plan history

| Date | Change |
|------|--------|
| 2026-07-13 | Initial draft: I1–I6 checkpoints, compose topology, env/volume/healthcheck locks, verification commands |
| 2026-07-13 | Review: headless Release I1 build; pinned llama image (no `:latest`); `service_healthy` depends_on; env precedence; `restart: unless-stopped`; deterministic smoke sequence |
| 2026-07-13 | Purpose clarified: Plan I packages F–H unchanged; does not supersede runtime/API work |
| **Locked** | 2026-07-13 — packaging scope, I1–I6 checkpoints, env/restart/health/smoke locks |
| **Implemented** | 2026-07-13 — Dockerfile, compose, smoke, docs; engine image verified |

---

## Plan lock record

| Field | Value |
|-------|-------|
| **Locked** | 2026-07-13 |
| **Purpose (locked)** | Docker packaging phase — F–H prerequisites packaged unchanged; no runtime/API replacement |
| **Prerequisites (locked)** | Plans A–E, F, G, H complete |
| **Topology (locked)** | `thoth-engine` + `llama-server` + 3 named volumes; SQLite local volume only |
| **Default inference (locked)** | `llama_cpp` @ `http://llama-server:8080` |
| **Engine CMD (locked)** | `thoth-engine --serve --bind 0.0.0.0 --port 8090` |
| **Headless build (locked)** | `THOTH_BUILD_GUI=OFF`, Release, runtime artifacts only — no Qt/wx in image |
| **llama image (locked)** | Version-pinned tag; **never** `:latest` |
| **Startup order (locked)** | `depends_on.llama-server.condition: service_healthy` |
| **Env precedence (locked)** | compose `.env` → container `environment` → `EnvLoader` / `config.json` |
| **Restart (locked)** | `unless-stopped` on both services |
| **Smoke (locked)** | 8-step deterministic sequence in `docker/smoke.sh` |
| **Publish (locked)** | Host port `8090` → engine only |
| **Scope (locked)** | Dockerfile, compose, volumes, healthchecks, smoke script, operator docs — packaging only |
| **Deferred** | Plan J CI compose; Plan K GUI client; auth/TLS; K8s; model auto-download |
| **Implementation** | Delivered 2026-07-13 (I1–I6) |
| **Post-lock changes** | Protocol lock rule — stop and request approval before editing this spec |

---

## Implementation notes (2026-07-13)

- Builder packages: `uuid-dev` / `libuuid1` (required by `default_planner.cpp`).
- Image/compose set **`THOTH_PROJECT_ROOT=/workspace`** as a deterministic deploy override. Root-cause fix (2026-07-14): `getProjectRoot()` terminates at filesystem root (`parent == current`).
- Full `./docker/smoke.sh` needs a GGUF at `${LLAMA_CHAT_MODEL:-/models/chat.gguf}` on volume `llama-models`.

---

STATUS: IMPLEMENTED
