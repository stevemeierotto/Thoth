# Docker Containerization Roadmap

**Last Updated:** 2026-07-13  
**Purpose:** Track containerization work after portable-runtime prerequisites  
**Status:** Plan F ✅ · Plan G ✅ · Plan H ✅ · Plan I ✅ (I1–I6)

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

---

## Plan letter index

| Plan | Topic | Roadmap step |
|------|-------|--------------|
| A–E | Portable runtime prerequisites | 0 ✅ |
| **F** | [Engine Runtime & HTTP](plan_f_engine_runtime_http.md) 🔒 | 3 |
| **G** | [Streaming & Observability](plan_g_streaming_observability.md) 🔒 ✅ | 4 |
| **H** | [Inference adapter](plan_h_inference_adapter.md) 🔒 | 2 |
| **I** | [Docker Compose v1](plan_i_docker_compose_v1.md) ✅ | 5 |
| **J** | CI compose job (optional) | 7 |
| **K** | GUI API client (optional) | 9 |

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
| **6** | Hybrid dev docs | Not started | — |
| **7** | CI compose job (optional) | Partial | **J** |
| **8** | Nightly full suite vs containerized inference | Not started | — |
| **9** | GUI API client (optional) | Not started | **K** |

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

### Step 6 — Hybrid dev docs

Host GUI → container engine (after Plans F–I).

### Step 7 — CI compose (Plan J)

Optional parallel job: build image + `curl /health`. Native `engine-only` CI already exists.

### Step 8 — Nightly containerized inference

`test-suite-full` via compose.

### Step 9 — GUI API client (Plan K)

`AgentInterface` → HTTP client when engine is remote.

---

## Gate rule

```
0 ✅ → 1 → 2 → 3 ✅ → 4 ✅ → 5 → 6 → 7 🔶 → 8 → 9 (optional)
```

Steps 2–4 (H, F, G) and Step 5 (I) are complete. Optional: Plan J (CI compose), Step 6 hybrid dev docs.

---

## Explicitly out of scope

- MariaDB / `thoth-db` container
- Containerized wxWidgets GUI (v1)
- API authentication (v1)
- Cognitive architecture changes

---

## Related docs

- [plan_i_docker_compose_v1.md](plan_i_docker_compose_v1.md) ✅
- [plan_h_inference_adapter.md](plan_h_inference_adapter.md) 🔒
- [plan_f_engine_runtime_http.md](plan_f_engine_runtime_http.md)
- [plan_g_streaming_observability.md](plan_g_streaming_observability.md) 🔒
- [GETTING_STARTED.md](GETTING_STARTED.md)
- [AGENTS.md](../AGENTS.md)
