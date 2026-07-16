# Thoth Docker Compose v1

Plan I packages the headless `thoth-engine` runtime (Plans F–H) with a co-located `llama-server` inference container. This is **packaging only** — no changes to engine semantics.

## Prerequisites

- Docker Engine 24+ with Compose v2
- GGUF models on the `llama-models` named volume (see below)
- ~4 GB free disk for images + models

## Quick start

From the repository root:

```bash
# 1. Place models (example paths inside the volume)
docker volume create llama-models 2>/dev/null || true
docker run --rm -v llama-models:/models -v "$PWD/models:/seed:ro" alpine \
  sh -c 'cp -n /seed/*.gguf /models/ 2>/dev/null || true'

# 2. Start stack
docker compose up -d --build

# 3. Verify
curl -s http://127.0.0.1:8090/health
curl -s http://127.0.0.1:8090/ready | jq .
```

Full smoke (requires chat model at `/models/chat.gguf` by default):

```bash
chmod +x docker/smoke.sh
./docker/smoke.sh
```

Without models, use `SMOKE_SKIP_INFERENCE=1` to skip the `/v1/goals` step when planning cannot reach inference.

## Services

| Service | Image | Host port | Role |
|---------|-------|-----------|------|
| `thoth-engine` | `thoth-engine:local` (built) | `8090` | HTTP API + SSE (`--serve`) |
| `llama-server` | `ghcr.io/ggml-org/llama.cpp:server@sha256:823b6f01…` | *(internal)* | `llama_cpp` inference |

**Pinned llama image:** `ghcr.io/ggml-org/llama.cpp:server@sha256:823b6f019cafbee8878dfdd0d4750eae4f81dfafb60dc1fbefb66794a59903c8` (pulled 2026-07-14). Never `:latest`. To upgrade:

```bash
docker pull ghcr.io/ggml-org/llama.cpp:server
docker image inspect ghcr.io/ggml-org/llama.cpp:server --format '{{index .RepoDigests 0}}'
# paste the digest into docker-compose.yml
```

Package index: [ggml-org/llama.cpp packages](https://github.com/ggml-org/llama.cpp/pkgs/container/llama.cpp).

## Model setup

`llama-server` loads the chat model from:

```
${LLAMA_CHAT_MODEL:-/models/chat.gguf}
```

1. Copy a chat GGUF to the `llama-models` volume as `chat.gguf`, **or** set `LLAMA_CHAT_MODEL` in `.env`.
2. For embeddings / RAG, also place an embedding GGUF and set `OLLAMA_EMBED_MODEL` / `config.json` `embedding_model` to match the name you pass to the API.
3. First start with an empty `thoth-workspace` volume creates SQLite on demand; initial RAG indexing can be **slow** — allow extra time before `/ready` reports full capability.

Example `.env` (optional, do not commit secrets):

```bash
THOTH_ENGINE_PUBLISH_PORT=8090
LLAMA_CHAT_MODEL=/models/qwen2.5-3b-instruct-q4_k_m.gguf
OLLAMA_MODEL=chat
OLLAMA_EMBED_MODEL=nomic-embed-text
THOTH_LOG_CONFIG=1
```

## Configuration precedence (locked)

```
compose .env / env_file
        ↓
docker-compose.yml environment: block
        ↓
EnvLoader / workspace config.json (unset keys only)
```

Compose-owned keys (`THOTH_WORKSPACE_PATH`, `THOTH_LOGS_PATH`, `THOTH_INFERENCE_*`, `THOTH_ENGINE_*`) are set in `docker-compose.yml` and should not be overridden via `config.json` inside the container.

## Volumes

| Volume | Mount | Contents |
|--------|-------|----------|
| `thoth-workspace` | `/workspace` | `memory.db`, `rag/`, `config.json`, traces |
| `thoth-logs` | `/logs` | metrics / benchmark JSONL |
| `llama-models` | `/models` (ro on server) | Operator-supplied GGUF files |

Use **named local volumes** for SQLite (`memory.db`). Do not mount workspace over NFS — WAL corruption risk.

## Environment (compose defaults)

| Variable | Value |
|----------|-------|
| `THOTH_WORKSPACE_PATH` | `/workspace` |
| `THOTH_LOGS_PATH` | `/logs` |
| `THOTH_PROJECT_ROOT` | `/workspace` (deterministic deploy override; optional when walk terminates) |
| `THOTH_INFERENCE_BASE_URL` | `http://llama-server:8080` |
| `THOTH_INFERENCE_BACKEND` | `llama_cpp` |
| `THOTH_ENGINE_BIND` | `0.0.0.0` |
| `THOTH_ENGINE_PORT` | `8090` |

## Ollama profile (optional, not default)

For transitional parity with host-native Ollama:

1. Run Ollama separately (host or container on `thoth-net`).
2. Override inference env on `thoth-engine`:

```yaml
environment:
  THOTH_INFERENCE_BASE_URL: http://host.docker.internal:11434
  THOTH_INFERENCE_BACKEND: ollama
```

Remove or disable the `llama-server` service and its `depends_on` when using Ollama exclusively.

## Operations

```bash
# Logs
docker compose logs -f thoth-engine

# Graceful stop (30s grace, SIGTERM)
docker compose stop thoth-engine

# Rebuild engine after code changes
docker compose up -d --build thoth-engine

# Tear down (volumes persist)
docker compose down
```

## Build engine image only

```bash
docker build -f docker/Dockerfile.engine -t thoth-engine:local .
docker run --rm thoth-engine:local --version
```

Verify no GUI binary in the image:

```bash
docker run --rm --entrypoint sh thoth-engine:local -c \
  'command -v thoth-engine && ! command -v thoth-control-panel 2>/dev/null'
```

## Hybrid development

Roadmap **Step 6** — run cognitive work in Docker while developing tools and clients on the host.

### Topology

```
┌──────────────────────────┐     HTTP/SSE :8090      ┌─────────────────────────┐
│ Host                     │ ──────────────────────► │ Docker Compose          │
│  curl / scripts          │                         │  thoth-engine           │
│  thoth-control-panel     │  (THOTH_ENGINE_URL set)  │  llama-server (internal)│
│  cmake engine-only (alt) │                         │  volumes: workspace/logs│
└──────────────────────────┘                         └─────────────────────────┘
```

Host-native `cmake --preset engine-only` / in-process GUI remain the **default**. Docker is additive. Point the GUI at Compose with `THOTH_ENGINE_URL` (Plan K).

### What works today (v1)

| Client | Against compose engine? | Notes |
|--------|-------------------------|-------|
| `curl` / shell scripts | ✅ | Use published host port (default `8090`) |
| SSE (`GET /v1/events`) | ✅ | Prefer `curl -N`; see [`ENGINE_EVENTS.md`](../docs/ENGINE_EVENTS.md) |
| Host `thoth-control-panel` | ✅ optional | Unset URL → in-process Local; `THOTH_ENGINE_URL` → Remote HTTP/SSE (Plan K) |
| Host `thoth-engine --serve` | ❌ (separate process) | Either run compose **or** a host `--serve`, not both on the same port |

### Workflow

```bash
# Terminal A — container engine + inference
docker compose up -d --build
curl -sf http://127.0.0.1:8090/health
curl -sf http://127.0.0.1:8090/ready | jq .

# Ops / hybrid clients (host)
curl -s -X POST http://127.0.0.1:8090/v1/chat \
  -H 'Content-Type: application/json' \
  -d '{"text":"/help","session_id":"hybrid"}'

curl -s -X POST http://127.0.0.1:8090/v1/goals \
  -H 'Content-Type: application/json' \
  -d '{"goal":"Summarize GRAG","session_id":"hybrid"}'

curl -N -H 'Accept: text/event-stream' http://127.0.0.1:8090/v1/events

# Optional — host GUI against compose engine (restart required to switch)
export THOTH_ENGINE_URL=http://127.0.0.1:8090
./build/debug/thoth-control-panel
# unset THOTH_ENGINE_URL && relaunch → local in-process again
```

Change the published port with `THOTH_ENGINE_PUBLISH_PORT` in `.env` if `8090` is already taken by a host `thoth-engine`.

### GUI remote mode (Plan K)

| Item | Behavior |
|------|----------|
| Selection | Only at GUI startup via `THOTH_ENGINE_URL` (trim; empty → Local) |
| Engine offline at launch | GUI still starts; chat/goals fail clearly when used |
| Cognate side panels | Empty / unavailable in remote mode (no cognate HTTP APIs yet) |
| Host vs container memory | Unchanged — volumes ≠ host `agent_workspace/` |
| Explicit goals (`goal:` / `/goal`) | Routed to `/v1/goals` (not blocking `/v1/chat` on planning) |
| HTTP timeouts | Chat default 600s; goals 1260s; override `THOTH_REMOTE_HTTP_TIMEOUT_SECONDS` |
| Switch modes | Change env and **restart** the GUI (no rebuild) |

Spec: [`plan_k_gui_api_client.md`](../docs/plan_k_gui_api_client.md)

### GUI remote smoke checklist (Plan K5)

Use this for manual validation when Compose or a host `thoth-engine` is available. **Not required** for `ctest -L pr` (offline tests cover mapping/selection).

**Local regression (default path)**

1. Ensure `THOTH_ENGINE_URL` is unset (or empty).
2. Launch `thoth-control-panel`; stderr should show `[AgentInterface] backend=local`.
3. Send a chat message and run a short goal — same in-process behavior as before Plan K.

**Remote additive path**

1. Start engine: `docker compose up -d thoth-engine` (or host `thoth-engine` on `:8090`).
2. `curl -s http://127.0.0.1:8090/ready` — expect HTTP 200.
3. `export THOTH_ENGINE_URL=http://127.0.0.1:8090` and **restart** the GUI.
4. Stderr: `[AgentInterface] backend=remote url=http://127.0.0.1:8090`.
5. Chat in GUI — response from remote engine (or clear `[RemoteEngine]` error if offline).
6. Execute a goal — observe SSE-driven plan events in the UI when `/v1/events` is available.
7. `unset THOTH_ENGINE_URL` and restart — returns to Local without rebuild.

**Opt-in automated live harness** (skips when unset):

```bash
THOTH_REMOTE_LIVE_URL=http://127.0.0.1:8090 ctest -R thoth-gui-tests
```

### Persistence boundary

- Engine state lives on compose volumes (`thoth-workspace`, `thoth-logs`), **not** in the host `agent_workspace/` tree used by native GUI/engine-only builds.
- Do not expect host GUI sessions and container memory to share `memory.db` unless you deliberately bind-mount the same path (usually avoid — SQLite WAL risks and UID mismatches).
- **RAG ownership (Plan L ✅ Complete (L3 deferred)):** Compose corpus is **engine-owned** on the named volume attached to `thoth-engine:/workspace`. The remote GUI does **not** own or synchronize that workspace. Seed only from repo-controlled `docker/seed_rag/` via the explicit script below. No host `agent_workspace` mirroring; no default `memory.db` bind. Spec: [`docs/plan_l_workspace_corpus.md`](../docs/plan_l_workspace_corpus.md).

### Seed engine RAG corpus (Plan L L1)

Fresh Compose volumes often have an empty `/workspace/rag`. Seed the **locked whitelist** (`GRAG.md`, `HOWTO.md`, `AGENTS.md`, `cognate.md`) without touching `memory.db`:

```bash
# From repository root (honors COMPOSE_PROJECT_NAME)
./docker/seed-workspace.sh --dry-run    # validate only
./docker/seed-workspace.sh              # copy whitelist → engine /workspace/rag, clear rag_index.bin, restart thoth-engine
./docker/seed-workspace.sh --no-restart # seed only; restart yourself later
```

- Source is **only** `docker/seed_rag/` (extra files there are ignored).
- Target volume is whatever Compose attaches to **`thoth-engine:/workspace`** (not a hardcoded volume name).
- After restart, the first indexing pass may take time if embeddings run.
- Verify: `docker compose exec thoth-engine ls -la /workspace/rag`

Not required for Plan J CI packaging smoke. Never run automatically on `compose up`.

### Plan L L2 — seed → restart → retrieval evidence (docs checklist)

**Locks:** docs-only (no `SMOKE_SEED`); Evidence C uses structured telemetry / decision-trace — not qualitative answer grading. Sentinel lives in curated `docker/seed_rag/HOWTO.md`.

**Sentinel phrase (exact):**

```text
THOTH_PLAN_L_SEED_SENTINEL_7f3a9c2e
```

#### Happy path (seeded + inference available)

1. From repo root: `./docker/seed-workspace.sh --dry-run`
2. `./docker/seed-workspace.sh` (or `--no-restart` then start/restart the stack)
3. `docker compose up -d` if needed; wait until `thoth-engine` is healthy (`curl -sf http://127.0.0.1:8090/ready`)
4. **Evidence A — corpus on volume**

   ```bash
   docker compose exec thoth-engine ls -la /workspace/rag
   # expect GRAG.md HOWTO.md AGENTS.md cognate.md
   docker compose exec thoth-engine grep -F 'THOTH_PLAN_L_SEED_SENTINEL_7f3a9c2e' /workspace/rag/HOWTO.md
   ```

5. **Evidence B — index rebuilt** (after engine has indexed; often after first chat)

   ```bash
   docker compose exec thoth-engine ls -la /workspace/rag/rag_index.bin
   # expect a non-trivial file (not missing / not ~137-byte stub)
   ```

6. **Probe chat** (forces retrieval against the sentinel):

   ```bash
   curl -s -X POST "http://127.0.0.1:${THOTH_ENGINE_PUBLISH_PORT:-8090}/v1/chat" \
     -H 'Content-Type: application/json' \
     -d '{"text":"What is THOTH_PLAN_L_SEED_SENTINEL_7f3a9c2e?","session_id":"plan-l-l2"}'
   ```

7. **Evidence C — structured retrieval telemetry (preferred)**

   Compose mounts logs at `/logs` (`thoth-logs` volume). Prefer `chat_rag.jsonl`:

   ```bash
   docker compose exec thoth-engine sh -c \
     'tail -n 20 /logs/chat_rag.jsonl'
   ```

   Pass when the latest relevant `CHAT_RAG_CONTEXT` row for this probe shows **all** of:

   | Field | Expected |
   |-------|----------|
   | `event` | `CHAT_RAG_CONTEXT` |
   | `query` | contains `THOTH_PLAN_L_SEED_SENTINEL_7f3a9c2e` |
   | `documents` | non-empty array |
   | `documents[].file` | path/name referring to `HOWTO.md` (seeded file) |
   | `grounding_mode` | **`retrieved_context`** (not `no_retrieval_hits`) |
   | `grounded` | `true` (Plan M) |
   | `retrieval_ran` | `true` |
   | `candidates_passed_gate` | `≥ 1` |
   | `grounding_decision_reason` | `injected_meaningful_hits` |

   Matching `CHAT_RAG_RESPONSE` for the same `request_id`:

   | Field | Expected |
   |-------|----------|
   | `retrieved_doc_count` | `> 0` |
   | `grounding_mode` | `retrieved_context` |

   **Plan M — reading `CHAT_RAG_CONTEXT` (do not use `grounding_mode` alone):**

   | Situation | Typical fields |
   |-----------|----------------|
   | Success (this sentinel probe) | `retrieval_ran=true`, `retrieval_skip_reason=none`, `candidates_passed_gate≥1`, `grounded=true`, `grounding_mode=retrieved_context` |
   | Greeting only (`Hello`) | `retrieval_ran=false`, `retrieval_skip_reason=greeting`, `grounding_decision_reason=greeting_skip`, `grounded=false`, `grounding_mode=no_retrieval_hits` |
   | Ran but nothing injectable | `retrieval_ran=true`, `candidates_passed_gate=0`, `grounding_decision_reason=below_threshold` or `no_candidates`, `grounded=false`, `grounding_mode=no_retrieval_hits` |

   Spec: [`docs/plan_m_grounded_retrieval_gate.md`](../docs/plan_m_grounded_retrieval_gate.md) ✅ Complete. Plan M did **not** change Plan L ownership or seeding.

   **Secondary (same request):** `/workspace/decision_trace.jsonl` stages named `chat_rag_context` / `chat_rag_response` with the same metrics embedded in stage payloads.

   ```bash
   docker compose exec thoth-engine sh -c \
     'tail -n 5 /workspace/decision_trace.jsonl'
   ```

   Do **not** treat the natural-language chat answer as pass/fail for Evidence C.

8. Confirm `memory.db` still present under `/workspace` and host `agent_workspace/` was not modified by seeding.

#### Expected failure — workspace **not** seeded

If you skip `./docker/seed-workspace.sh` (empty `/workspace/rag`, missing whitelist files, and/or only a trivial/absent `rag_index.bin`):

| Check | Expected unseeded symptom |
|-------|---------------------------|
| Evidence A | Whitelist `.md` files missing; `grep` for the sentinel fails |
| Evidence B | `rag_index.bin` missing, empty, or tiny (~100s of bytes) |
| Evidence C | Prefer Plan M fields: often `retrieval_ran=true`, `candidates_passed_gate=0` or empty docs, `grounded=false`, `grounding_mode=no_retrieval_hits` (or `no_index` / `empty_index` if the index never built). Legacy symptom: `documents: []`, `retrieved_chars: 0`. |

That failure mode is **correct** for an empty engine-owned volume — it is not a Plan K transport bug. Fix: run the L1 seed script, clear/rebuild index (script removes `rag_index.bin`), restart, re-probe.

#### Skips

- **No GGUF / inference:** complete Evidence A; attempt B after start; mark Evidence C **SKIP (no inference)** — do not change Plan J CI.
- Re-seed after pulling sentinel updates: re-run `./docker/seed-workspace.sh` so the volume’s `HOWTO.md` matches `docker/seed_rag/`.

### Parallel native development

Keep coding and testing headless logic on the host without Docker:

```bash
cmake --preset engine-only
cmake --build --preset build-engine-only
ctest --test-dir build/engine-only -L pr -j1
```

Use compose when you need the packaged inference topology (`llama_cpp` via Docker DNS) or a disposable workspace volume.

### Caveats

- **SSE + reverse proxies:** long-lived `/v1/events` streams can time out behind proxies; for local hybrid, hit the publish port directly.
- **First-run RAG:** empty volume may delay readiness while the sandbox indexes — wait for `/ready` before load tests.

## CI packaging smoke (Plan J)

PR CI verifies packaging **without GGUF**. Override clears the llama health dependency; smoke starts **only** `thoth-engine`:

```bash
SMOKE_MODE=ci ./docker/smoke.sh
# equivalent manual form:
# docker compose -f docker-compose.yml -f docker/compose.ci.yml up -d --build thoth-engine
```

| Mode | Command | Requires GGUF |
|------|---------|---------------|
| CI (Plan J) | `SMOKE_MODE=ci ./docker/smoke.sh` | No |
| Operator full | `./docker/smoke.sh` | Yes (`/models/chat.gguf`) |

GitHub Actions: `.github/workflows/ci-compose.yml` (parallel to native `ctest -L pr` — does not replace it).

Full inference + goals in CI is **Step 8** (nightly), not Plan J.

## See also

- [plan_i_docker_compose_v1.md](../docs/plan_i_docker_compose_v1.md)
- [plan_j_ci_compose.md](../docs/plan_j_ci_compose.md)
- [plan_k_gui_api_client.md](../docs/plan_k_gui_api_client.md)
- [docker_roadmap.md](../docs/docker_roadmap.md) Step 6–7
- [GETTING_STARTED.md](../docs/GETTING_STARTED.md)
- [ENGINE_EVENTS.md](../docs/ENGINE_EVENTS.md)
