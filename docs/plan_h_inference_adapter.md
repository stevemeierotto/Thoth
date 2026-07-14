# Plan H — Inference Adapter

**Status:** ✅ Implemented 2026-07-13 (H1–H6) · 🔒 Locked spec — 2026-07-13  
**Prerequisites:** [Plan A](completed_improvements_log.md) (inference URLs) ✅ · [Plan F](plan_f_engine_runtime_http.md) ✅ · [Plan G](plan_g_streaming_observability.md) ✅  
**Recommended:** [docker_roadmap.md](docker_roadmap.md) Step 1 (`llama-server` compose profile) for H3/H6 integration tests  
**Next:** [Plan I — Docker Compose v1](plan_i_docker_compose_v1.md) ✅  
**Roadmap:** [docker_roadmap.md](docker_roadmap.md) Step 2

---

## Goal

Introduce a **provider-independent inference transport layer** so `thoth-engine` can use **Ollama** or **llama.cpp (`llama-server`)** without changes to cognitive, memory, GRAG, planner, or evaluation logic.

### Target architecture

```
LLMInterface                    EmbeddingEngine
     |                                |
     +------------+-------------------+
                  |
                  v
            InferenceClient  (interface)
                  |
     +------------+------------+---------------------------+
     |            |            |                           |
     v            v            v                           v
 Implement now:              Reserved interface
 OllamaClient                compatibility (not in Plan H):
 LlamaServerClient             OpenAICompatibleClient
     |            |
     v            v
 Ollama REST     llama-server
 /api/generate   /v1/completions
 /api/embed      /v1/embeddings
 /api/tags       GET /health
```

**Implement now (H2–H3):** `OllamaClient`, `LlamaServerClient`

**Reserved interface compatibility (future):** `OpenAICompatibleClient` — interface and request/response types must accommodate this adapter without breaking changes to `InferenceClient`.

> **Interface design note:** The inference abstraction must not assume Ollama semantics. The `InferenceClient` contract should be capable of supporting OpenAI-compatible APIs in the future without modification to the interface itself — only new adapter implementations and factory wiring.

**Critical design rule:**

- **`InferenceClient` owns provider HTTP details.**
- **`LLMInterface` and `EmbeddingEngine` own Thoth semantics** (token accounting, TfIdf fallback, model selection policy, normalization).

Cognitive code must never branch on `ollama` vs `llama_cpp`. Only the factory and client implementations may.

---

## Current state (baseline)

| Component | Today |
|-----------|-------|
| URL resolution | `Thoth::resolveInferenceEndpoints()` — `THOTH_INFERENCE_BASE_URL`, `THOTH_EMBED_BASE_URL`, `OLLAMA_HOST`, `config.json` |
| Text generation | `LLMInterface::askOllama()` — direct CURL to `{base}/api/generate` |
| Embeddings | `EmbeddingEngine::embedExternal()` / `embedBatch()` — direct CURL to `{embed_base}/api/embed` |
| Health (benchmarks) | `ollama_snapshot.cpp` — `GET {base}/api/tags` |
| Abstraction | **None** — hard Ollama JSON paths and response shapes |
| OpenAI path | `LLMInterface::askOpenAI()` — separate; **not in Plan H scope** |

**Default base URL:** `http://127.0.0.1:11434` (Ollama). Unchanged for backward compatibility.

---

## Non-negotiable constraints

These are **locked requirements**. Any implementation that violates them is out of spec.

1. **Transport-only refactor** — no changes to planner logic, prompts, memory algorithms, GRAG scoring, executive state machine, or evaluation metrics.
2. **Public API stability** — `LLMInterface` and `EmbeddingEngine` public method signatures unchanged.
3. **Default behavior preserved** — with no `THOTH_INFERENCE_BACKEND` set, behavior must match current Ollama path (regression-safe).
4. **No provider leakage** — cognitive/runtime code must not include Ollama- or llama.cpp-specific JSON keys or URL paths.
5. **Embedding dimension contract** — external embeddings must remain **768** for `nomic-embed-text:v1.5` (or configured `embedding_model`) on both backends; dimension mismatch must surface clearly, not silently corrupt retrieval.
6. **Thread-safety preserved** — existing `LLMInterface` recursive mutex and `EmbeddingEngine` CURL pool semantics retained at the facade layer.

### Explicitly excluded

- Docker / compose images (**Plan I**)
- OpenAI backend refactor via `OpenAICompatibleClient` (**deferred** — `LLMInterface::askOpenAI()` stays as-is in Plan H; H1 types must not block a future adapter)
- Model fine-tuning, prompt changes, GRAG weight changes
- `ollama_snapshot` / benchmark harness rename (may remain Ollama-named; optional thin wrapper post-H)
- Automatic migration of `rag_index.bin` across backends (operator re-index if semantic vectors differ)
- New config surface beyond `THOTH_INFERENCE_BACKEND` (no new URL env vars in H5)

---

## Boundary

### In scope

| Checkpoint | Work |
|------------|------|
| **H1** | `InferenceClient` interface + provider-neutral request/response types (no Ollama-specific fields; OpenAI-compatible-ready) |
| **H2** | `OllamaClient` — extract existing Ollama behavior |
| **H3** | `LlamaServerClient` — llama-server HTTP (verified API, not assumed Ollama-compat) |
| **H4** | `createInferenceClient()` factory + `THOTH_INFERENCE_BACKEND` |
| **H5** | Config precedence cleanup; bootstrap logging |
| **H6** | Embedding parity validation + full PR regression |

### Provider API mapping (locked)

Do **not** assume llama-server speaks Ollama paths. Locked routes:

| Operation | `ollama` backend | `llama_cpp` backend |
|-----------|------------------|---------------------|
| Generate | `POST /api/generate` | `POST /v1/completions` (OpenAI-compatible) |
| Embed (single + batch) | `POST /api/embed` | `POST /v1/embeddings` (OpenAI-compatible) |
| Health | `GET /api/tags` (200 + parseable JSON) | `GET /health` (200) |
| Model discovery | `/api/tags` auto-detect (preserve today) | **Config only** (`config.llm_model`, `config.embedding_model`) — no `/api/tags` equivalent |

**Base URL:** `THOTH_INFERENCE_BASE_URL` is the server origin for both backends. For `llama_cpp`, operators set e.g. `http://llama-server:8080` (not the Ollama default port).

**Embed URL:** `THOTH_EMBED_BASE_URL` override preserved; falls back to inference base (Plan A).

---

## `InferenceClient` contract (H1)

Namespace: `Thoth`

### Types

```cpp
struct InferenceGenerateRequest {
    std::string model;
    std::string prompt;
    double temperature = 0.7;
    double top_p = 1.0;
    int max_tokens = 2048;
};

struct InferenceGenerateResult {
    std::string text;
    LlmTokenUsage token_usage;  // reuse existing struct where possible
    bool ok = false;
    std::string error;
};

struct InferenceEmbedRequest {
    std::string model;
    std::vector<std::string> inputs;  // single or batch
};

struct InferenceEmbedResult {
    std::vector<std::vector<float>> embeddings;
    bool ok = false;
    std::string error;
};

struct InferenceHealthResult {
    bool reachable = false;
    std::vector<std::string> available_models;  // Ollama populates; llama_cpp may be empty
    std::string error;
};

class InferenceClient {
public:
    virtual ~InferenceClient() = default;
    virtual InferenceGenerateResult generate(const InferenceGenerateRequest& req) = 0;
    virtual InferenceEmbedResult embed(const InferenceEmbedRequest& req) = 0;
    virtual InferenceHealthResult health() = 0;
    virtual std::string backendName() const = 0;  // "ollama" | "llama_cpp"
};
```

### Factory (H4)

```cpp
enum class InferenceBackend { Ollama, LlamaCpp };

InferenceBackend resolveInferenceBackend();  // env + default
std::unique_ptr<InferenceClient> createInferenceClient(
    const InferenceEndpointConfig& endpoints,
    const Config* config = nullptr);
```

**`THOTH_INFERENCE_BACKEND`:**

| Value | Client |
|-------|--------|
| unset / `ollama` | `OllamaClient` |
| `llama_cpp` | `LlamaServerClient` |
| anything else | **Fail at client creation** with clear stderr message; do not fall back silently |

### Mock client (tests)

`MockInferenceClient` implementing `InferenceClient` for unit tests — no network.

---

## Integration points

### `LLMInterface` (H2)

- Construct or hold `std::unique_ptr<Thoth::InferenceClient>` when `LLMBackend::Ollama`.
- `askOllama()` maps to `client->generate()`; preserve:
  - `resolveOllamaModel()` / `/api/tags` fallback logic **inside `OllamaClient` or LLMInterface adapter** (not in cognitive code)
  - `num_predict_override`, temperature, top_p
  - `parseOllamaTokenUsage()` behavior for Ollama responses
  - `THOTH_MOCK_LLM_*`, `RobustnessMockResponses`, `testSuiteDevTierEnabled()` short-circuits **before** client call (unchanged)
- OpenAI path untouched.

### `EmbeddingEngine` (H2 + H3)

- `embedExternal()` / batch path call `InferenceClient::embed()`.
- Preserve:
  - 8000-char truncation
  - `MAX_BATCH_SIZE` micro-batching
  - TfIdf fallback on failure (unchanged policy)
  - `normalizeVector()` after successful external embed
- `getDimension()` remains **768** for `Method::External` until a probe-based dimension is added (out of H scope — locked constant matches `nomic-embed-text`).

### `runtime_bootstrap.cpp` (H5)

Log resolved backend alongside existing `inference_base` / `embed_base` lines:

```
[Thoth] inference_backend=ollama
[Thoth] inference_base=http://127.0.0.1:11434
```

---

## Checkpoint summary

Plan H uses the same **stop-gate discipline** as Plans F and G: implement one checkpoint, verify, **STOP**, wait for explicit approval.

| Checkpoint | Purpose | Lock point |
|------------|---------|------------|
| **H1** | Interface + types + mock | Provider-neutral contract approved |
| **H2** | Ollama extraction | Zero regression on default path |
| **H3** | LlamaServerClient | llama-server API verified (not assumed) |
| **H4** | Factory + env backend | Switchable via env only |
| **H5** | Config cleanup + bootstrap logs | No config sprawl |
| **H6** | Embedding parity + PR suite | Safe for Plan I compose |

### Checkpoint detail

| CP | Scope | Intentionally NOT included | Verify |
|----|-------|---------------------------|--------|
| **H1** | `inference_client.h`, `inference_types.h`, `MockInferenceClient`, factory stubs | Real HTTP, `LLMInterface` wiring | Unit tests: mock generate/embed/health; invalid backend string rejected |
| **H2** | `OllamaClient`; wire `LLMInterface` + `EmbeddingEngine` | LlamaServer, Docker | `ctest -L pr` green; Ollama integration smoke (if reachable); token usage + model fallback unchanged |
| **H3** | `LlamaServerClient` with `/v1/completions`, `/v1/embeddings`, `/health` | Ollama path changes, compose | Manual or CI smoke against running `llama-server`; parse OpenAI response shapes |
| **H4** | `createInferenceClient()`, `resolveInferenceBackend()`, startup failure on bad backend | New URL env vars | Factory unit tests; `THOTH_INFERENCE_BACKEND=llama_cpp` selects client |
| **H5** | Bootstrap logging; document backend + URL in `GETTING_STARTED.md` | `config.json` backend field | Logs show backend; existing `testInferenceEndpointResolution` still passes |
| **H6** | Embedding dimension probe test; optional cosine smoke; docs; full PR | Baseline re-certification, index auto-migration | Dimension == 768 both backends; `ctest -L pr -j1` green; note in `ENGINE_EVENTS` N/A — update `GETTING_STARTED` + `docker_roadmap` |

### Gate rule (mandatory)

**After each checkpoint: STOP.** Confirm:

1. Non-negotiable constraints 1–6 still hold.
2. No cognitive files modified outside inference transport path.
3. Verification criteria for the checkpoint are met.
4. No scope from “Explicitly excluded” crept in.

**H6 is the final implementation stop** before Plan H is considered complete.

---

## H6 — Embedding parity validation (critical)

Thoth depends on consistent embedding geometry for:

- GRAG retrieval (`rag_index.bin`, `IndexManager` metadata)
- Episodic memory and plan similarity
- Strategy / trajectory scoring
- E1/E2 benchmark environment pinning (`embedding_dimension`, `embedding_model`)

### Locked validation (H6)

| Check | Method | Pass criteria |
|-------|--------|---------------|
| **Dimension** | `InferenceClient::embed()` on fixed probe string `"thoth-embedding-probe"` | `embeddings[0].size() == 768` for configured `nomic-embed-text` model on **both** backends when reachable |
| **Normalization** | `EmbeddingEngine::normalizeVector` applied post-embed | Unit test unchanged; vectors unit-length after engine path |
| **Index metadata** | `IndexManager` header `embedding_dimension` | Still 768 for external method; mismatch still triggers re-index (existing behavior) |
| **Regression** | `ctest -L pr -j1` | All existing tests green (Ollama default path) |
| **Semantic parity** | Optional smoke only | Same probe text: cosine similarity ≥ 0.99 between Ollama and llama.cpp **if both reachable** — **informational**, not a merge gate (implementations may differ slightly) |

### Operator note (document in H6)

Switching `THOTH_INFERENCE_BACKEND` or embedding provider may change vector semantics even when dimension matches. **Re-index RAG** (`agent_workspace/rag/`) if retrieval quality degrades. Plan H does not auto-migrate indexes.

---

## Testing strategy

### Unit tests (`tests/unit_tests.cpp` or dedicated `inference_client_tests`)

| Test | CP |
|------|-----|
| `testInferenceBackendResolution` — default, `ollama`, `llama_cpp`, invalid | H1/H4 |
| `testMockInferenceClient` — generate, embed, health | H1 |
| `testOllamaClientParse` — response JSON fixtures (no network) | H2 |
| `testLlamaServerClientParse` — OpenAI completion + embedding fixtures | H3 |
| `testInferenceFactorySelection` | H4 |

### Integration tests (gated)

```bash
export THOTH_INFERENCE_INTEGRATION_TESTS=1
```

| Test | Backend | Skip when |
|------|---------|-----------|
| Ollama generate smoke | `ollama` | `!isOllamaReachable()` |
| Ollama embed + dimension | `ollama` | unreachable |
| llama-server generate | `llama_cpp` | `GET /health` fails |
| llama-server embed + dimension | `llama_cpp` | unreachable |
| H6 embedding parity (optional cosine) | both | either unreachable |

Integration tests must **not** run in default `ctest -L pr` (keep PR fast). Add label `inference-integration` or env gate only.

### Existing tests

- `testInferenceEndpointResolution` — must remain green (H5).
- Full `ctest -L pr -j1` — required at H2 and H6.

---

## Verification commands

Assume `engine-only` preset.

**H1 — Interface + mock**

```bash
cmake --build --preset build-engine-only --target thoth-core-tests
./build/engine-only/tests/thoth-core-tests  # new unit tests only
```

**H2 — Ollama regression**

```bash
ctest --test-dir build/engine-only -L pr -j1 --output-on-failure

# Optional smoke (Ollama running)
THOTH_INFERENCE_INTEGRATION_TESTS=1 \
  THOTH_INFERENCE_BACKEND=ollama \
  ./build/engine-only/tests/thoth-core-tests
```

**H3 — llama-server smoke**

```bash
# Terminal A — example; model paths vary by deployment
llama-server -m /models/nomic-embed-text.gguf --port 8080

# Terminal B
export THOTH_INFERENCE_BASE_URL=http://127.0.0.1:8080
export THOTH_INFERENCE_BACKEND=llama_cpp
THOTH_INFERENCE_INTEGRATION_TESTS=1 ./build/engine-only/tests/thoth-core-tests
```

**H4 — Backend switch (runtime, no rebuild)**

Use the **same** `thoth-engine` binary twice — no recompile, no config edits:

```bash
ENGINE=./build/engine-only/external/basic_agent/thoth-engine
export THOTH_INFERENCE_BASE_URL=http://127.0.0.1:11434
export THOTH_LOG_CONFIG=1

# Run 1 — Ollama (requires reachable inference)
THOTH_INFERENCE_BACKEND=ollama "$ENGINE" --execute "ping"

# Run 2 — llama.cpp (requires llama-server at THOTH_INFERENCE_BASE_URL)
export THOTH_INFERENCE_BASE_URL=http://127.0.0.1:8080
THOTH_INFERENCE_BACKEND=llama_cpp "$ENGINE" --execute "ping"

# Invalid backend — must fail at client creation with clear error
export THOTH_INFERENCE_BASE_URL=http://127.0.0.1:11434
THOTH_INFERENCE_BACKEND=invalid "$ENGINE" --execute "ping"
```

Pass criteria: `[Thoth] inference_backend=ollama` / `llama_cpp` in startup logs; Ollama run succeeds when inference is up; `llama_cpp` selects the llama-server client (connection errors are OK if no server); invalid backend returns `Invalid THOTH_INFERENCE_BACKEND`.

**Verified 2026-07-13:** same binary, `ollama` chat succeeded; `llama_cpp` selected correct client (connection refused without llama-server); `invalid` rejected at runtime.

**H6 — Final regression**

```bash
ctest --test-dir build/engine-only -L pr -j1 --output-on-failure
```

---

## Files / components affected

### New files

| File | CP |
|------|-----|
| `external/basic_agent/include/inference_types.h` | H1 |
| `external/basic_agent/include/inference_client.h` | H1 |
| `external/basic_agent/include/ollama_client.h` | H2 |
| `external/basic_agent/include/llama_server_client.h` | H3 |
| `external/basic_agent/include/mock_inference_client.h` | H1 |
| `external/basic_agent/src/ollama_client.cpp` | H2 |
| `external/basic_agent/src/llama_server_client.cpp` | H3 |
| `external/basic_agent/src/inference_client.cpp` | H1/H4 (factory) |
| `external/basic_agent/src/mock_inference_client.cpp` | H1 |

### Modified files

| File | CP | Change |
|------|-----|--------|
| `external/basic_agent/src/llm_interface.cpp` | H2 | Delegate Ollama path to `InferenceClient` |
| `external/basic_agent/include/llm_interface.h` | H2 | Hold client pointer (minimal) |
| `external/basic_agent/src/embedding_engine.cpp` | H2/H3 | Delegate external embed to client |
| `external/basic_agent/src/runtime_bootstrap.cpp` | H5 | Log backend |
| `external/basic_agent/CMakeLists.txt` | H1+ | Add new sources |
| `tests/unit_tests.cpp` | H1–H6 | New tests |
| `docs/GETTING_STARTED.md` | H5/H6 | `THOTH_INFERENCE_BACKEND`, llama-server notes |
| `docs/docker_roadmap.md` | H6 | Link spec; mark Step 2 complete |
| `docs/completed_improvements_log.md` | H6 | Append on completion |
| `external/basic_agent/src/thoth_engine_main.cpp` | H5 | Help text for `THOTH_INFERENCE_BACKEND` |

### Untouched (unless bugfix required)

- Planner, executive, GRAG scorer, memory repositories, benchmarks (except test additions)
- `ollama_snapshot.cpp` (benchmark-specific; optional follow-up)
- OpenAI code path in `LLMInterface`

---

## Risks and mitigations

| Risk | Impact | Mitigation |
|------|--------|------------|
| llama-server API drift | H3 breakage | Lock OpenAI `/v1/*` routes; document server version in plan; integration test |
| Embedding semantic mismatch (same dim, different vectors) | Retrieval quality regression | H6 dimension gate + operator re-index note; optional cosine smoke |
| `getDimension()` hardcoded 768 | Wrong dim if model changes | H6 probe test fails loudly; future: probe-based dim (out of H) |
| CURL handle duplication | Resource leaks | Centralize HTTP in clients; facades hold shared client per instance |
| llama-server single-model vs separate embed server | Compose complexity | `THOTH_EMBED_BASE_URL` allows split endpoints (Plan A) |
| Integration test flakiness | CI noise | Env-gated; skip when unreachable; never in `pr` label |
| Model auto-detect only on Ollama | llama_cpp misconfiguration | Fail generate with clear error if model empty; document in GETTING_STARTED |
| Thread safety regression | Worker hangs | Preserve mutex at facade; clients stateless per call or internally synchronized |

---

## Configuration (H5 — locked)

| Setting | Env | Default | Notes |
|---------|-----|---------|-------|
| Inference base | `THOTH_INFERENCE_BASE_URL` | `http://127.0.0.1:11434` | Origin only; paths chosen by backend |
| Embed base | `THOTH_EMBED_BASE_URL` | same as inference | Plan A — unchanged |
| Backend | `THOTH_INFERENCE_BACKEND` | `ollama` | `ollama` \| `llama_cpp` |
| LLM model | `config.llm_model` / `OLLAMA_MODEL` | `qwen2.5:3b` | Unchanged |
| Embed model | `config.embedding_model` | `nomic-embed-text:v1.5` | Unchanged |

**Precedence:** Backend env is read at client creation; invalid values fail startup of inference client (first use), not silently at HTTP time.

---

## Success criteria

- [ ] `InferenceClient` interface with generate, embed, health
- [ ] `OllamaClient` preserves current Ollama behavior (default path)
- [ ] `LlamaServerClient` uses verified `/v1/completions` + `/v1/embeddings` + `/health`
- [ ] `THOTH_INFERENCE_BACKEND` switches backend without code changes
- [ ] No planner / memory / GRAG / eval logic changes
- [ ] Embedding dimension 768 validated on both backends (when reachable)
- [ ] `ctest -L pr -j1` green
- [ ] Docs updated (`GETTING_STARTED.md`, roadmap)

---

## Rollback

Remove `InferenceClient`, clients, and factory. Restore direct CURL in `LLMInterface` and `EmbeddingEngine`. Delete new headers/sources.

---

## Plan history

| Date | Change |
|------|--------|
| 2026-07-13 | Initial draft: H1–H6 checkpoints, provider API mapping, embedding parity gate, stop gates |
| 2026-07-13 | Architecture: future `OpenAICompatibleClient` reserved; interface must not assume Ollama semantics |
| **Implemented** | 2026-07-13 — H1–H6 complete; `ctest -L pr -j1` green |

---

## Plan lock record

| Field | Value |
|-------|-------|
| **Locked** | 2026-07-13 |
| **Re-locked** | 2026-07-13 — `OpenAICompatibleClient` reserved; interface must not assume Ollama semantics |
| **Constraints (locked)** | Transport-only; public facade APIs unchanged; default `ollama`; no provider leakage; dim 768 contract |
| **Interface (locked)** | `InferenceClient` provider-neutral; OpenAI-compatible-ready without interface changes; implement now: `OllamaClient`, `LlamaServerClient` |
| **API mapping (locked)** | Ollama: `/api/*`; llama_cpp: `/v1/completions`, `/v1/embeddings`, `/health` |
| **Config (locked)** | `THOTH_INFERENCE_BACKEND=ollama\|llama_cpp`; URLs via Plan A only |
| **Deferred** | `OpenAICompatibleClient` adapter + `LLMInterface` migration; probe-based `getDimension()`; `ollama_snapshot` rename; index auto-migration |
| **Scope** | `InferenceClient`, `OllamaClient`, `LlamaServerClient`, factory, tests, docs |
| **Implementation** | ✅ H1–H6 complete (2026-07-13); see [`completed_improvements_log.md`](completed_improvements_log.md) |
| **Post-lock changes** | Protocol lock rule — stop and request approval before editing this spec |

---

STATUS: IMPLEMENTED (H1–H6, 2026-07-13)
