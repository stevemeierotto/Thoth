# E1 — Benchmark Environment Pinning

**Status:** 🔶 Checkpoint D3 complete — next: **Checkpoint D4** (`run_chat_rag_benchmark`)
**Spec version:** v3.1 (2026-06-26)  
**Effort:** ~10–12 hours total, **split across checkpoints A–E** (see below)  
**Roadmap:** Must complete before **E2**, **B1**, and **V3** Zenodo re-upload.

**Related:** `docs/cursor_list.md` § Reflection (E1 rationale), `check_baseline.py`, `logs/cognitive_metrics.jsonl`.

---

## Invariant

No benchmark event is valid unless it is causally attributable to a captured environment snapshot.

---

## Goal

Log model, embed version, git SHA, mock vs `--full` tier, corpus fingerprint, and index binding alongside every benchmark run so results stay reproducible and comparable across sessions.

**Outputs:**

| Sink | Content |
|------|---------|
| Sidecar `logs/benchmark_env.latest.json` | Full `BenchmarkEnvironment` + `run_id`, hashes, index binding |
| JSONL event stream (harness) | Envelope `{ event, ts, run_id, env_hash, env?, payload }` |
| `GOAL_COGNITIVE_METRICS` (thin hook) | Optional `run_id` + `env_hash` only — full env stays in sidecar |

---

## Core types

Nested structs assembled by pure `assembleEnvironment()` (no network in capture):

- `EnvironmentProvenance`, `ModelEnvironment`, `RuntimeEnvironment`, `CorpusEnvironment`, `IndexEnvironment`, `OllamaEnvironment` → `BenchmarkEnvironment`
- `BenchmarkEnvironmentInputs` + harness-fetched `OllamaSnapshot` (HTTP outside `assembleEnvironment`)
- `BenchmarkAttribution` — `{ run_id, env_hash }` passed explicitly (see Propagation)
- `BenchmarkRun` — return value of `BenchmarkContext::create()`

### Hashing (two-phase)

1. **`environment_hash`** at `BenchmarkContext::create()` — excludes index state.
2. **`index_hash`** + `BENCHMARK_INDEX_BOUND` event after index load via `BenchmarkRun::bindIndex()`.

### Tier

Always use `inferTier()`. On declared vs inferred mismatch, emit **`TIER_MISMATCH`** JSONL event (not stderr-only).

### Git SHA

Human checklist in this doc (B1/V3): authoritative runs require SHA ≠ `unknown`.

### Sidecar timing

Write `logs/benchmark_env.latest.json` at **`BenchmarkContext::create()`**.

---

## Propagation — explicit `BenchmarkAttribution` (v3.1 #1)

**Do not use** `setActive()`, thread-local, or any ambient “active context” lookup.

`ExecutiveController::execute_goal()` spawns `loop_thread_`; `GOAL_COGNITIVE_METRICS` is emitted from that worker. Thread-local on the harness thread is invisible there → silent no-op. A single global active slot mis-attributes under concurrent goals (C5-09).

Follow the C7 **`StepExecutionContext`** pattern: lightweight struct passed at the goal boundary.

```cpp
struct BenchmarkAttribution {
    std::string run_id;
    std::string env_hash;
    bool empty() const { return run_id.empty() && env_hash.empty(); }
};
```

| Stage | Action |
|-------|--------|
| Harness | `auto run = BenchmarkContext::create(...)` |
| Harness | `controller.execute_goal(goal, run.attribution())` |
| `ExecutiveController::execute_goal` | Copy into `benchmark_attribution_` **before** spawning `loop_thread_` |
| Terminal emit | `emit_goal_cognitive_metrics_unlocked()` reads `benchmark_attribution_` |
| `GoalCognitiveMetricsRecord` | Optional `run_id`, `env_hash`; omit from JSON when empty |
| GUI / `CommandProcessor` | `execute_goal(goal)` — no second arg → no benchmark fields |

Concurrent goals: each call carries its own attribution; no shared ambient slot.

### `BenchmarkContext` public surface

```cpp
struct BenchmarkRun {
    std::string run_id;
    std::string environment_hash;
    BenchmarkEnvironment environment;

    BenchmarkAttribution attribution() const;
    void emit(const std::string& event, const nlohmann::json& payload);
    void bindIndex(const IndexEnvironment& index);  // BENCHMARK_INDEX_BOUND
};

static BenchmarkRun create(const BenchmarkEnvironmentInputs& inputs);
// NO setActive(), NO thread_local
```

### Sidecar synchronization (v3.1 #2)

All writes to `logs/benchmark_env.latest.json` (create, `bindIndex()` merge, any `emit()` sidecar touch) use a **single static mutex** inside `BenchmarkContext` (same discipline as JSONL append). Merge = read-modify-write under lock. `IndexManager::init()` may call `bindIndex()` off the harness thread.

### Harness terminal events (RAII recorders)

Harnesses emit a **terminal summary** via `BenchmarkRun::emit()` when the run finishes normally, e.g.:

| Harness | Complete event | Aborted event |
|---------|----------------|---------------|
| `run_test_suite` | `TEST_SUITE_COMPLETE` | `TEST_SUITE_ABORTED` |
| `run_reflection_ab_benchmark` | `REFLECTION_AB_COMPLETE` | `REFLECTION_AB_ABORTED` |
| `run_robustness_suite` | `ROBUSTNESS_COMPLETE` | `ROBUSTNESS_ABORTED` |
| *(D4–D5)* | *(per harness)* | *(same pattern)* |

Each harness wraps its case loop in an RAII recorder: `complete()` sets a flag and emits the **COMPLETE** event; if scope exits without `complete()` (early `return`, exception propagated to `main()`), the destructor emits **ABORTED** with whatever counts are available.

**Worker-thread crash — known gap (all harnesses):** **ABORTED** events are only guaranteed on **main-thread exit paths** (early return, exception that unwinds through the recorder on the harness thread). A **worker-thread** failure that calls `std::terminate` (uncaught exception on `loop_thread_`, segfault, etc.) kills the process before the main thread's destructor stack runs — the RAII recorder cannot emit **ABORTED** in that case. This is a structural boundary, not a recorder bug.

**Analysis rule:** A `BENCHMARK_ENV` row with **no** terminal **COMPLETE** / **ABORTED** event for the same `run_id` in `benchmark_env.jsonl` indicates a crash or hard kill, not a silent successful run. Treat orphaned `BENCHMARK_ENV` rows accordingly when comparing sessions.

Dev-only smoke hooks (e.g. `THOTH_TEST_SUITE_BENCHMARK_ABORT_SMOKE`, `THOTH_REFLECTION_AB_BENCHMARK_ABORT_SMOKE`, `THOTH_ROBUSTNESS_BENCHMARK_ABORT_SMOKE`) force the main-thread **ABORTED** path for verification; they do not exercise worker-thread crashes.

### Python (v3.1 #3)

`check_baseline.py --require-env` is **opt-in post-E1 hardening only** — not default-on. Document in script help and close-out checklist.

---

## Implementation checkpoints

**Rule:** A checkpoint is **done** only when it is **compile-clean**, **test-green** for its listed tests, and **committable**. Do not stop mid-checkpoint except at sub-session boundaries inside **D** (one harness per sub-session).

**Rule (checkpoint C — no exceptions):** Do not start checkpoint **D** until checkpoint **C** is fully green. The `execute_goal()` signature change and all call sites (GUI, harness, `CommandProcessor`) must land in **one** session — never split across a multi-day gap.

| Checkpoint | Covers | Stop criteria | Why safe to stop |
|------------|--------|---------------|------------------|
| **A** | Steps 1–2: nested structs, `assembleEnvironment()`, `inferTier()`, `environment_hash`, `index_hash` | ✅ E1-01–E1-06 green | Pure logic, no I/O, nothing wired into existing call sites. Zero blast radius if untouched for days. |
| **B** | Step 2b (design lock headers) + Step 3 (`BenchmarkContext`) + Step 5 (`GitMetadata`, corpus fingerprint, `OllamaSnapshot`) | ✅ E1-07–E1-08 green | New files only — no existing production call sites changed. |
| **C** | Step 4 only: `BenchmarkAttribution` through `execute_goal()` → `CognitiveMetricsLogger` | ✅ E1-09–E1-11 green | Invasive hot-path step — isolated so signature + all callers land together. |
| **D1** | `run_test_suite` harness wiring + E1-12 smoke | ✅ E1-12 + `--dev` suite green | Template for D2–D5; RAII terminal event guard. |
| **D2** | `run_reflection_ab_benchmark` harness wiring + E1-13 smoke | ✅ E1-13 + harness green | Direct-controller path; probe-stack index bind. |
| **D3** | `run_robustness_suite` harness wiring + E1-14 smoke | ✅ E1-14 + harness green | 7 `execute_goal` calls; **6** metrics rows — C5-09 slow goal preempted (verified pre-D3 baseline at `fb4fd9f`: same 6 rows; one controller; join-on-replace, no terminal emit). |
| **D4–D5** | Step 6: remaining harnesses | Harness-specific smoke + existing suite green before next sub-session | One harness per sub-session. |
| **E** | Step 7 (index mismatch / `BENCHMARK_INDEX_BOUND`) + Step 8 (Python scripts, docs, close-out) | Script smoke; close-out checklist | Final pass; low risk individually. |

### Checkpoint D — harness sub-sessions (Step 6)

Wire **one harness per sub-session**; test before starting the next:

| Sub | Harness | Notes |
|-----|---------|-------|
| **D1** | `run_test_suite` | ✅ `--dev` green |
| **D2** | `run_reflection_ab_benchmark` | ✅ mock path green |
| **D3** | `run_robustness_suite` | ✅ mock path green |
| **D4** | `run_chat_rag_benchmark` | |
| **D5** | `run_grag_benchmark` / `BenchmarkReporter` | |

### Step mapping (reference)

| Step | Checkpoint |
|------|------------|
| 1 — structs + assemble + inferTier + environment_hash | A |
| 2 — index_hash + doc skeleton | A |
| 2b — `BenchmarkAttribution` + `execute_goal` overload stubs (headers) | B |
| 3 — `BenchmarkContext` (create, emit, sidecar mutex, bindIndex) | B |
| 4 — wire attribution → ExecutiveController → CognitiveMetricsLogger | **C** |
| 5 — GitMetadata, corpus fingerprint, OllamaSnapshot | B |
| 6 — harnesses | D1–D5 |
| 7 — index mismatch + BENCHMARK_INDEX_BOUND | E |
| 8 — `compare_benchmark_env.py`, `check_baseline.py --require-env`, close-out | E |

---

## Unit tests (planned IDs)

Add to `tests/unit_tests.cpp` as each checkpoint completes:

| ID | Checkpoint | Asserts |
|----|------------|---------|
| **E1-01** | A | `assembleEnvironment()` deterministic from fixed inputs |
| **E1-02** | A | `inferTier()` FAST/STRICT/MOCK from env flags |
| **E1-03** | A | `computeEnvironmentHash()` stable; excludes index fields |
| **E1-04** | A | `index_hash` distinct from `environment_hash` |
| **E1-05** | A | `TIER_MISMATCH` predicate when declared ≠ inferred |
| **E1-06** | A | Nested struct JSON round-trip |
| **E1-07** | B | `BenchmarkContext::create()` writes sidecar; `run_id` + `environment_hash` present |
| **E1-08** | B | Sidecar merge under mutex (bindIndex simulation) |
| **E1-09** | C | `execute_goal(goal, attribution)` → metrics JSON includes `run_id`/`env_hash` |
| **E1-10** | C | `execute_goal(goal)` without attribution → fields omitted |
| **E1-11** | C | Attribution set before worker thread; `STEP_STARTED` + terminal events on worker; metrics JSON includes `run_id`/`env_hash` |
| **E1-12** | D1 | End-to-end harness helper path: create → `executeGoal(attribution)` → sidecar + metrics hash match |
| **E1-13** | D2 | Reflection A/B direct-controller path: probe stack → `execute_goal(attribution)` → sidecar + metrics; `index_hash` non-empty |
| **E1-14** | D3 | Robustness harness path: probe stack → `execute_goal(attribution)` → sidecar + metrics; `index_hash` non-empty |

---

## Close-out criteria (E1 complete)

- [ ] Every listed harness emits events with valid `run_id` + `env_hash` tied to sidecar
- [ ] Harness-driven `GOAL_COGNITIVE_METRICS` rows carry matching `run_id` + `env_hash`
- [ ] GUI goals omit benchmark fields (not an error)
- [ ] `TIER_MISMATCH` emitted when tier drifts
- [ ] `BENCHMARK_INDEX_BOUND` updates sidecar `index_hash` without replacing run record
- [ ] `docs/benchmark_environment.md` human checklist: B1/V3 runs require git SHA ≠ unknown
- [ ] `compare_benchmark_env.py` compares two sidecars
- [ ] `check_baseline.py --require-env` documented; **off by default**

---

## Human checklist — authoritative benchmark runs (B1 / V3)

Before treating a run as thesis- or publication-facing:

1. Git SHA captured and ≠ `unknown` (Thoth + basic_agent submodule if applicable)
2. Tier matches intent (`--full` / nightly Ollama vs `--dev` mock)
3. Sidecar `environment_hash` matches event stream `env_hash`
4. After index load, `index_hash` present via `BENCHMARK_INDEX_BOUND`
5. No `TIER_MISMATCH` unless explained and re-run

---

## Resume discipline

At the **end of each checkpoint A–E**, append an interim entry to `docs/completed_improvements_log.md` under **E1 — Benchmark environment pinning** (same pattern as C1 phase-by-phase entries). Include:

- Checkpoint letter completed
- Tests green (IDs)
- Files touched
- Explicit “safe stop — next checkpoint: X”

Do not rely on chat memory after multi-day gaps; the log entry is the anchor.

---

## Files to create / modify (forecast)

**New:** `benchmark_environment.h/cpp`, `benchmark_context.h/cpp`, `git_metadata.h/cpp`, `ollama_snapshot.h/cpp`, `scripts/compare_benchmark_env.py`

**Modify:** `executive_controller.h/cpp`, `cognitive_metrics.h`, `cognitive_metrics_logger.cpp`, `command_processor.cpp`, `AgentInterface` (if needed), harnesses in `tests/`, `check_baseline.py`, `external/basic_agent/CMakeLists.txt`, `tests/unit_tests.cpp`
