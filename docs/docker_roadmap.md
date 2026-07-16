# Docker Containerization Roadmap

**Last Updated:** 2026-07-16  
**Purpose:** Track containerization work after portable-runtime prerequisites  
**Status:** Plan F ✅ · Plan G ✅ · Plan H ✅ · Plan I ✅ · Step 6 ✅ · Plan J ✅ · Plan K ✅ · Plan L ✅ Complete (L3 deferred)

---

## Decisions (locked)

| Decision | Choice |
|----------|--------|
| Persistence | **SQLite on volume** — no MariaDB container |
| Prerequisites | Plans A–E complete before Dockerfiles |
| v1 compose | `thoth-engine` + `llama-server` (or Ollama sidecar) + named volumes |
| GUI in v1 | Host-native `thoth-control-panel` preferred; containerized wxWidgets deferred |
| Default dev workflow | Host-native build remains primary; Docker is additive |
| Transport split | Plan F = request/response HTTP; Plan G = SSE & observability |
| Compose RAG ownership | **Plan L** — engine-owned corpus on `thoth-workspace`; seed from `docker/seed_rag/` (not host mirror) |

---

## Plan letter index

| Plan | Topic | Roadmap step |
|------|-------|--------------|
| A–E | Portable runtime prerequisites | 0 ✅ |
| **F** | [Engine Runtime & HTTP](plan_f_engine_runtime_http.md) 🔒 | 3 |
| **G** | [Streaming & Observability](plan_g_streaming_observability.md) 🔒 ✅ | 4 |
| **H** | [Inference adapter](plan_h_inference_adapter.md) 🔒 | 2 |
| **I** | [Docker Compose v1](plan_i_docker_compose_v1.md) ✅ | 5 |
| **J** | [CI compose job](plan_j_ci_compose.md) ✅ | 7 |
| **K** | [GUI API client (optional)](plan_k_gui_api_client.md) 🔒 | 9 |
| **L** | [Engine workspace corpus](plan_l_workspace_corpus.md) ✅ Complete (L3 deferred) | 10 |
| **M** | [Grounded retrieval gate](plan_m_grounded_retrieval_gate.md) ✅ Complete | (chat RAG) |

*(Former single “Plan F” HTTP work is split into F + G. Former G/H/I shifted down one letter.)*

---

## Architecture target (v1)

```
┌─────────────────────┐     HTTP/SSE      ┌──────────────────┐
│  thoth-control-panel │ ────────────────► │  thoth-engine    │
│  (host-native, opt)  │                   │  EngineRuntime   │
│                      │                   │  HTTP + SSE (G)  │
└─────────────────────┘                   └────────┬─────────┘
                                                     │
                          ┌──────────────────────────┼──────────────────────────┐
                          ▼                          ▼                          ▼
                 ┌────────────────┐         ┌─────────────────┐        ┌─────────────────┐
                 │ thoth-workspace│         │  llama-server   │        │  thoth-logs     │
                 │ volume         │         │  (or Ollama)    │        │  volume         │
                 │ SQLite + RAG   │         │  inference      │        │  benchmarks     │
                 └────────────────┘         └─────────────────┘        └─────────────────┘
```

**Volumes (SQLite-only):**

| Volume | Mount | Contents |
|--------|-------|----------|
| `thoth-workspace` | `THOTH_WORKSPACE_PATH` | `memory.db`, `rag/`, `rag_index.bin`, config, traces |
| `thoth-logs` | `THOTH_LOGS_PATH` | Benchmark / metrics JSONL |
| `llama-models` | `/models` (read-only) | GGUF model files |

Keep `memory.db` on a **local Docker volume** (not NFS) because of SQLite WAL.

---

## Roadmap

| Step | Work | Status | Plan |
|------|------|--------|------|
| **0** | Prerequisites A–E | ✅ Done | A–E |
| **1** | `llama-server` compose profile | Superseded by **I** (I2) | (prep) |
| **2** | Inference adapter (Ollama or llama.cpp) | ✅ Done (H1–H6) | **H** |
| **3** | Engine Runtime + HTTP (`/health`, `/v1/chat`, goals, control) | ✅ Done (F1–F6) | **F** 🔒 |
| **4** | SSE streaming + event observability | ✅ Done (G1–G6) | **G** 🔒 |
| **5** | `docker-compose.yml` v1 (Docker packaging) | ✅ Done (I1–I6) | **I** |
| **6** | Hybrid dev docs | ✅ Done | — |
| **7** | CI compose job (optional) | ✅ Done (J1–J4) | **J** |
| **8** | Nightly full suite vs containerized inference | Not started | — |
| **9** | GUI API client (optional) | ✅ Plan K complete (K0–K5) | **K** |
| **10** | Engine workspace corpus (Compose RAG) | ✅ Complete (L3 deferred) | **L** |

---

## Step details

### Step 0 — Prerequisites ✅

Completed 2026-07-12. Plans A–E: inference URLs, portable paths, `thoth-engine`, test split, runtime bootstrap.

### Step 1 — llama-server compose profile

Delivered by **Plan I** checkpoint I2 (packaging). No separate Step 1 implementation after Plan I. Spec: [plan_i_docker_compose_v1.md](plan_i_docker_compose_v1.md) 🔒

### Step 2 — Inference adapter (Plan H)

`InferenceClient` with `OllamaClient` + `LlamaServerClient`; `THOTH_INFERENCE_BACKEND=ollama|llama_cpp`.

Spec: [plan_h_inference_adapter.md](plan_h_inference_adapter.md) 🔒

**Transitional:** Ollama sidecar — zero adapter work for early compose experiments.

### Step 3 — Engine Runtime & HTTP (Plan F) ✅

`EngineRuntime` + request/response HTTP. **No SSE** (Plan G).

**Checkpoints:**

| Checkpoint | Status |
|------------|--------|
| F1 `EngineRuntime` + CLI refactor | ✅ |
| F2 Sessions + error schema | ✅ |
| F3 `/health`, `/ready`, `/version`, `--serve` | ✅ |
| F4 `POST /v1/chat` | ✅ |
| F5 Goals + control endpoints | ✅ |
| F6 Graceful shutdown, docs, `ctest -L pr` | ✅ |

| Endpoint | Purpose |
|----------|---------|
| `GET /health` | Liveness |
| `GET /ready` | Readiness + capabilities (`chat`, `goals`, `control`) |
| `GET /version` | Engine / git / protocol |
| `POST /v1/chat` | Chat / commands |
| `POST /v1/goals` | Goal start — returns after acceptance + initial planning |
| `POST /v1/control/*` | pause / resume / abort |

Spec: [plan_f_engine_runtime_http.md](plan_f_engine_runtime_http.md) 🔒

### Step 4 — Streaming & Observability (Plan G) ✅

| Endpoint | Purpose |
|----------|---------|
| `GET /v1/events` | SSE with `event_id`, `sequence`, `timestamp` |

Multi-client fan-out, disconnect handling, extended shutdown, event docs.

**Checkpoints:** G1–G6 ✅

Spec: [plan_g_streaming_observability.md](plan_g_streaming_observability.md) 🔒 · Events: [ENGINE_EVENTS.md](ENGINE_EVENTS.md)

### Step 5 — docker-compose v1 (Plan I)

Spec: [plan_i_docker_compose_v1.md](plan_i_docker_compose_v1.md) 🔒

**Purpose:** Docker packaging phase — packages Plans F–H unchanged into containers. Does not replace runtime, HTTP, SSE, or inference adapter work.

```yaml
services:
  llama-server:
  thoth-engine:
volumes:
  thoth-workspace:
  thoth-logs:
  llama-models:
```

**Checkpoints:** I1–I6 (headless Dockerfile, version-pinned llama-server, compose wiring, volumes/env precedence, healthchecks + `unless-stopped`, docs/smoke)

### Step 6 — Hybrid dev docs ✅

Completed 2026-07-14. Docs only — host clients (`curl` / scripts) against compose `thoth-engine` on `:8090`.

**Delivered in:** [`docker/README.md`](../docker/README.md) § Hybrid development · [`GETTING_STARTED.md`](GETTING_STARTED.md) § Hybrid development

**Notes:** Host GUI remains in-process until **Plan K**. Container volumes ≠ host `agent_workspace/`.

### Step 7 — CI compose (Plan J) ✅

Completed 2026-07-14. Parallel packaging job: build engine image + CI compose (engine-only, no GGUF) + `/health` / `/ready`. Native `engine-only` + `ctest -L pr` unchanged.

Spec: [plan_j_ci_compose.md](plan_j_ci_compose.md) ✅  
Workflow: `.github/workflows/ci-compose.yml` · Override: `docker/compose.ci.yml` · Smoke: `SMOKE_MODE=ci ./docker/smoke.sh`

### Step 8 — Nightly containerized inference

`test-suite-full` via compose.

### Step 9 — GUI API client (Plan K)

`AgentInterface` → HTTP client when engine is remote. Local default; remote via `THOTH_ENGINE_URL`.

**Status:** ✅ Plan K complete (K0–K5, 2026-07-15).

Spec: [plan_k_gui_api_client.md](plan_k_gui_api_client.md) 🔒

**GUI remote:** `THOTH_ENGINE_URL=http://127.0.0.1:8090` then restart `thoth-control-panel` (unset → Local). Smoke checklist: [docker/README.md](../docker/README.md#gui-remote-smoke-checklist-plan-k5).

### Step 10 — Engine workspace corpus (Plan L)

Completes the **engine-owned** Compose workspace corpus model and supported seeding workflow (not host `agent_workspace` mirroring; not GUI sync).

**Status:** ✅ **Complete (L3 deferred)** — 2026-07-16 (L4 closeout).

Spec: [plan_l_workspace_corpus.md](plan_l_workspace_corpus.md) ✅

**Final architecture (remote):** GUI ↔ engine over HTTP/SSE (Plan K); engine owns `/workspace` (RAG corpus, index, `memory.db`, traces) and `/logs`. Supported corpus path: `docker/seed_rag/` → `./docker/seed-workspace.sh` → engine workspace → L2 verification. Empty-workspace retrieval issues after remote migration were addressed jointly by Phase 1/Plan K (timeouts/routing) and Plan L (ownership/seed/verify).

---

## Gate rule

```
0 ✅ → 1 → 2 ✅ → 3 ✅ → 4 ✅ → 5 ✅ → 6 ✅ → 7 ✅ → 8 → 9 ✅ → 10 ✅
```

Steps 2–7, 9, and 10 complete. **Plan L ✅ Complete (L3 deferred).** Optional: Step 8 nightly.

---

## Explicitly out of scope

- MariaDB / `thoth-db` container
- Containerized wxWidgets GUI (v1)
- API authentication (v1)
- Cognitive architecture changes

---

## Related docs

- [plan_i_docker_compose_v1.md](plan_i_docker_compose_v1.md) ✅
- [plan_j_ci_compose.md](plan_j_ci_compose.md) ✅
- [plan_k_gui_api_client.md](plan_k_gui_api_client.md) 🔒
- [plan_l_workspace_corpus.md](plan_l_workspace_corpus.md) ✅ Complete (L3 deferred)
- [plan_m_grounded_retrieval_gate.md](plan_m_grounded_retrieval_gate.md) ✅ Complete
- [plan_h_inference_adapter.md](plan_h_inference_adapter.md) 🔒
- [plan_f_engine_runtime_http.md](plan_f_engine_runtime_http.md)
- [plan_g_streaming_observability.md](plan_g_streaming_observability.md) 🔒
- [GETTING_STARTED.md](GETTING_STARTED.md)
- [AGENTS.md](../AGENTS.md)
