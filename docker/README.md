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
| `llama-server` | `ghcr.io/ggml-org/llama.cpp:server@sha256:823b6f01…` | *(internal)* | Chat completions (`/v1/completions`) |
| `llama-embed-server` | same digest-pinned image | *(internal)* | Embeddings (`/v1/embeddings`, `--embeddings`) |

**Pinned llama image:** `ghcr.io/ggml-org/llama.cpp:server@sha256:823b6f019cafbee8878dfdd0d4750eae4f81dfafb60dc1fbefb66794a59903c8` (pulled 2026-07-14). Never `:latest`. To upgrade:

```bash
docker pull ghcr.io/ggml-org/llama.cpp:server
docker image inspect ghcr.io/ggml-org/llama.cpp:server --format '{{index .RepoDigests 0}}'
# paste the digest into docker-compose.yml
```

Package index: [ggml-org/llama.cpp packages](https://github.com/ggml-org/llama.cpp/pkgs/container/llama.cpp).

## Model setup

Chat and embeddings use **separate GGUF files** on the shared `llama-models` volume. The chat server cannot serve embeddings from the chat model alone — `llama-embed-server` runs with `--embeddings` on a dedicated embedding GGUF.

| Variable | Default | Role |
|----------|---------|------|
| `LLAMA_CHAT_MODEL` | `/models/chat.gguf` | Chat / completion model path inside volume |
| `LLAMA_EMBED_MODEL` | `/models/nomic-embed-text.gguf` | Embedding model path inside volume |
| `OLLAMA_MODEL` | `chat` | API model name passed to llama-server for chat |
| `OLLAMA_EMBED_MODEL` | `nomic-embed-text` | API model name passed for embeddings (also `THOTH_EMBEDDING_MODEL`) |

1. Copy a chat GGUF to the `llama-models` volume as `chat.gguf`, **or** set `LLAMA_CHAT_MODEL` in `.env`.
2. Copy an embedding GGUF (e.g. `nomic-embed-text`) as `nomic-embed-text.gguf`, **or** set `LLAMA_EMBED_MODEL`.
3. After adding the embedding model or changing embed endpoints, **rebuild the RAG index** — indexes built during embed failures may contain TF-IDF fallback vectors (see below).
4. First start with an empty `thoth-workspace` volume creates SQLite on demand; initial RAG indexing can be **slow** — allow extra time before `/ready` reports full capability.

### Embedding model (one-time)

If you already have `nomic-embed-text` in Ollama, export a GGUF and copy it into the volume:

```bash
# Example: download a GGUF (adjust URL/model to your environment)
docker run --rm -v thoth_llama-models:/models curlimages/curl:8.5.0 -fL \
  -o /models/nomic-embed-text.gguf \
  "https://huggingface.co/nomic-ai/nomic-embed-text-v1.5-GGUF/resolve/main/nomic-embed-text-v1.5.f16.gguf"
```

Verify the embedding server before indexing:

```bash
docker compose up -d llama-embed-server
docker exec thoth-llama-embed-server-1 curl -sf -X POST http://127.0.0.1:8081/v1/embeddings \
  -H 'Content-Type: application/json' \
  -d '{"model":"nomic-embed-text","input":"thoth-embedding-probe"}' | head -c 200
```

### Re-index after embed migration

Indexes built while embeddings were failing (HTTP 501 / unreachable Ollama) may store TF-IDF fallback vectors under an External/768 header. After the embed server is healthy:

```bash
./docker/seed-workspace.sh   # seeds rag/, deletes rag_index.bin, restarts engine
# or manually: docker exec thoth-thoth-engine-1 rm -f /workspace/rag_index.bin && docker compose restart thoth-engine
```

Startup logs should show `[Thoth] embed_probe=ok … dimension=768`. No `Falling back … TfIdf` lines during indexing.

Example `.env` (optional, do not commit secrets):

```bash
THOTH_ENGINE_PUBLISH_PORT=8090
LLAMA_CHAT_MODEL=/models/qwen2.5-3b-instruct-q4_k_m.gguf
LLAMA_EMBED_MODEL=/models/nomic-embed-text.gguf
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
| `THOTH_EMBED_BASE_URL` | `http://llama-embed-server:8081` |
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
| HTTP timeouts | Chat default 600s; goals 1260s; override `THOTH_REMOTE_HTTP_TIMEOUT_SECONDS` (R4: failures surface in status bar; Send disabled while turn in flight). **R4 Engine verify script:** `scripts/r4_engine_verify.sh` — sanity **240s**, full multi-turn **600s** (override `R4_VERIFY_SANITY_TIMEOUT` / `R4_VERIFY_TURN_TIMEOUT`). |
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
7. **RAG honesty (GUI Phase 1 / Restoration R1):** drop or Import Corpus a host file — status must say host-only / not sent to Engine; must **not** say “indexing…”. Slots show `(host-only)`. Engine `/workspace/rag` unchanged until **[Send to Engine]** (item 15). **`ingest` on `/ready` does not change drop behavior.**
8. **Mode / capabilities (GUI Phase 2):** status bar field 0 shows `Backend: Engine` (never “Remote”). Strategy / Trajectories / Experiments / Graph-stats show **Unavailable with the current backend.** — not empty tables. Benchmarks menu items disabled; status line `Status: Available in Local backend`. Logs tab shows Unavailable (not host `decision_trace.jsonl`).
9. **Cognitive diagnostics authority (GUI Phase 3 / D11):** after a remote goal, **Explain Plan** shows Engine decision summary (Phase 4) — structured fields from `GET /v1/diagnostics/latest-decision`, never host `decision_trace.jsonl`. If Engine is old and lacks the resource, capability/HTTP failure must not invent host traces. Logs remain Unavailable + why-copy until a later phase.
10. `unset THOTH_ENGINE_URL` and restart — returns to Local without rebuild; status shows `Backend: Local`; cognate panels Empty/Populated from real data; Benchmarks enabled (`Status: Available`); Explain Plan uses Local `getLatestDecisionSummary()`; local drop may show real indexing again.
11. **Progress discipline (GUI Phase 5 / D3a):** Engine mode drop → host-only status (same as item 7), **no** “indexing…” / strip counter from the drop itself regardless of `ingest` on `/ready`. Local drop → chrome “Added N file(s)” first; strip/counter only after INDEXING_* events. Goal send → “Goal submitted” until STATE_CHANGED/PLAN_*; never “Syncing RAG…” / invent Planning… before events. Remote goal must **not** call host `setRagFiles`.
12. **SSE resilience (GUI Phase 6):** with Engine running, status field 1 stays quiet when `Events: Connected`. `docker compose stop thoth-engine` → field 1 shows `Events: Reconnecting` (and `Engine: …` if `/ready` fails); progress strip freezes without inventing work; agent controls disabled when Engine unhealthy. `docker compose start thoth-engine` → reconnects to `Connected` without GUI restart. GRAG panel footer shows `Last event: N s ago` when events flow.
13. **Operation result honesty (GUI Phase 7):** with Engine stopped, Pause/Abort/goal/chat must show **one** correlated failure (e.g. `Engine unavailable — …`), never premature “paused/aborted/response received”. With Engine up, Pause succeeds → status shows success **after** backend confirms; chat failure surfaces in panel (not fake assistant success); no duplicate connection-lost spam alongside field 1 indicators.
14. **Engine corpus listing (GUI Phase 8):** after `./docker/seed-workspace.sh`, RAG tab **Engine Corpus** lists seeded document **names** (not host paths). **Local Notes** collapsed in Engine mode. Empty volume → `Corpus is empty.` Engine down → `Corpus listing unavailable.` — never merged.
15. **Corpus document creation (GUI Phase 9):** with Engine running and `THOTH_ENGINE_URL` set, add a host file to **Local Notes** (drop/import — no auto-ingest). **[Send to Engine]** enabled when `/ready` includes `ingest`. Click → **OperationResult** acceptance (`Document accepted: …`); indexing progress only via **INDEXING_*** SSE afterward. Created document appears in **Engine Corpus** after refresh/indexing. Engine stopped → honest failure, no fabricated indexing.
16. **Conversation authority (GUI Phase 10):** with Engine running and `THOTH_ENGINE_URL` set, send a chat message — user bubble appears **only after** Engine accepts the turn. Restart GUI, reopen same session — history loads from Engine (**Get conversation**), not from `chat_sessions.json` messages. Failed send → **Failed to send** with no committed user bubble. `/ready` includes `conversation`.
17. **Research resources (GUI Phase 11):** with Engine running and `THOTH_ENGINE_URL` set, open Strategies / Trajectories panels — data loads from Engine (`GET /v1/research/*`) or shows **Error** on fetch failure (never silent empty when the API failed). `/ready` includes `strategies`, `trajectories`, `episodes`. Restart GUI and refresh — panels re-fetch from Engine (no authoritative local cache).
18. **Graph statistics (GUI Phase 12A):** with Engine running and `THOTH_ENGINE_URL` set, open Graph panel — stats load from Engine (`GET /v1/graph/stats`) or show **Error** on fetch failure (never treat failed fetch or `{}` as Empty). Valid zero-node/zero-edge snapshot → **Empty**. `/ready` includes `graph_stats`. Refresh re-fetches from Engine.

19. **ALP-G certification (Attachment Lifecycle — manual G2b/G3):** requires ALP flags on **both** Engine and GUI. Use a **clean workspace** only: greenfield empty volume (`THOTH_ALP_GREENFIELD=1`) or documented post-D1 brownfield — **not** an unknown host `agent_workspace`.

    **Engine (Compose example):** copy `docker/alp.env.example` into `.env` or export before `docker compose up -d`:

    ```bash
    THOTH_ALP_ENABLED=1 THOTH_ALP_TX_INDEX=1 THOTH_ALP_GREENFIELD=1
    ```

    **GUI (restart required):**

    ```bash
    export THOTH_ENGINE_URL=http://127.0.0.1:8090
    export THOTH_ALP_ENABLED=1 THOTH_ALP_TX_INDEX=1 THOTH_ALP_GUI=1
    ```

    **Automated minimum (run first):** `./scripts/alp_g_verify.sh gate` — writes `agent_workspace/alp_certification/alp_g_report.json`.

    **G2b — EGAR operator lifecycle (manual):**

    1. Open GUI; create/select session.
    2. Import `docs/EGAR.md` to Local Notes.
    3. **Send to Engine** — wait for indexing; note UUID in Engine Corpus.
    4. Close GUI.
    5. Reopen; delete Local Note (**X**) — slot empty; Engine row remains.
    6. Wait for reconcile (Send disabled until complete when Engine down; enabled after verify).
    7. Import a **newer** `EGAR.md` (edit file or replace content).
    8. Picker shows **Update available** / `new_revision`; Send again.
    9. Verify: **one UUID**; new revision; no `EGAR_1.md` in corpus; `rev1` superseded in registry (if inspectable).
    10. **Retrieval:** Session A chat query finds EGAR content; **new session B** must **not** retrieve EGAR until B sends/links.

    **G3 — ALP-E reconcile smoke:**

    | Step | Expected |
    |------|----------|
    | Engine down at startup | Send disabled; stale cache → “cache not verified” |
    | Engine up + corpus refresh | Reconcile completes; Send enabled for eligible slots |
    | Picker labels | Send / Update / Retry / Replace from Engine intent |
    | Decline force-replace | Local file + cache unchanged |
    | Restart GUI | No phantom Send from stale cache alone |
    | One bad Local Note file | Partial reconcile; GUI not stuck forever |

    Sign off in `alp_g_report.json` (`ALP_G_OPERATOR=your_initials ./scripts/alp_g_verify.sh gate`) and append `completed_improvements_log.md`.

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
