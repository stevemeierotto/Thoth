# Completed Improvements Log

Last updated: 2026-07-09 (Phase E Step 2 complete — authoritative STRICT trio sealed)
Source: previous `docs/improvements.md` and `docs/next_steps.md` plan entries marked completed

### E2 track — status at a glance

| Phase | Question | Status | Close-out |
|-------|----------|--------|-----------|
| **A** | Can execution be trusted? | ✅ Complete 2026-07-02 | A1–A5 checkpoints; E2-08–E2-11 |
| **B** | Can measurement be trusted? | ✅ Complete 2026-07-04 | [`phases/PHASE_B_COMPLETE.md`](phases/PHASE_B_COMPLETE.md); fingerprint `1ce31c6aa3f6987841c1a0ddecae6f9171e5ef86fc9c88601b1a017e25f669b4` |
| **C** | Can trusted measurement become architecture? | ✅ Locked 2026-07-05 | [`phases/PHASE_C_COMPLETE.md`](phases/PHASE_C_COMPLETE.md) |
| **D** | Can architecture evolve without losing trust? | ✅ Complete 2026-07-08 | [`phases/PHASE_D_COMPLETE.md`](phases/PHASE_D_COMPLETE.md); D5 evolution trust proof |
| **E** | Can we defend the results scientifically? | 🔶 In progress (2026-07-09) | E1 ✅ · EP-01 ✅ · Step 2 ✅ — see entries below |

### E2 Phase C — Integration tier ✅ locked (2026-07-05)

**Authority:** [`C_PHASE_PROTOCOL.md`](C_PHASE_PROTOCOL.md) v1.1  
**Baseline:** Phase B v1 fingerprint unchanged  
**Close-out:** [`phases/PHASE_C_COMPLETE.md`](phases/PHASE_C_COMPLETE.md)

**Governing invariant:** Evaluation is a passive architectural service — it may observe execution, but must never influence execution.

| Checkpoint | Deliverable | Tests |
|------------|-------------|-------|
| **E2-C1** | `IEpisodicEvaluationService` — stateless façade over Phase B kernel | E2-C1-01..03 |
| **E2-C2** | `EpisodeCompleted` publication + `EvaluationSubscriber` + event channel | E2-C2-01..04 |
| **E2-C3** | `DiagnosticService` — presentation-only JSONL | E2-C3-01..04 |
| **E2-C4** | `PipelineTelemetryService` — architectural telemetry sink | E2-C4-01..05, E2-C4-03b |
| **E2-C5** | Path equivalence under pinned STRICT config on golden fixtures | E2-C5-01..05 |

**C5 record:** Benchmark orchestration (`runBenchmarkPathArtifacts`) and production orchestration (`runProductionPathArtifacts`) produce identical `evaluation_resolution`, scorable classification, failure/diagnosis buckets, and `fingerprint_hash` on mapping-safe fixtures (E2-01..03). Scope: pinned semantic config — not raw INTEGRATION-default vs STRICT object equality.

**Regression gates (all green):**

| Gate | Result |
|------|--------|
| `THOTH_E2_WIRING_STAGE=B` harness | E2 SUCCESS, 3/3 cases |
| Phase B fingerprint (E2-28) | Stable |
| `THOTH_E2_C5=1` equivalence matrix | E2-01/02/03 MATCH |
| Passive invariant | Publication default OFF; no eval → Executive callback |

**Key files (basic_agent):** `episodic_evaluation_service.*`, `episode_events.h`, `episode_event_channel.*`, `evaluation_subscriber.*`, `diagnostic_service.*`, `pipeline_telemetry_service.*`, `e2_path_equivalence.*`

**Review note:** E2-C5-03 aliases E2-C5-01; proof coverage unchanged via `diffPathEquivalence()`.

**Next:** Phase D4 — [`D_PHASE_PROTOCOL.md`](D_PHASE_PROTOCOL.md). Constitutional rule: Observe, Record, Replay, Present — Never Decide.

### Phase E — EP-01 episodic authoritative inference harness ✅ (2026-07-09)

**Authority:** `cursor_list.md` § E.0.0 EP-01  
**Gate:** `THOTH_E2_EP01=1` (E2-29 → E2-28 spot-check → E2-30)

- Dual-mode `run_episodic_learning_benchmark`: `--mock` (default) · `--authoritative` / `--full`
- Isolated `inferTier()` branch for `episodic_learning_benchmark`
- Infrastructure only — no Phase E benchmark evidence in EP-01

### Phase E Step 2 — authoritative STRICT trio evidence ✅ (2026-07-09)

**Authority:** `cursor_list.md` § E.0.0 Step 2 · [`phase_e_strict_v1.md`](benchmark_results/phase_e_strict_v1.md)  
**Evidence scope:** `n=3_strict_trio`  
**E2-28:** PASS (bucket #0) — Run A `run-1783628170667` vs Run B `run-1783628248447`  
**Rollup:** `SCORED_FAILURE` / `mean_episodic_lift = 0` at authoritative tier (not comparable to Phase B mock SUCCESS)  
**Artifacts:** `docs/baselines/artifacts/phase_e/` · `docs/baselines/phase_e_run_manifest.json`

### E2 Phase D3 — Observability without influence ✅ complete (2026-07-07)

**Authority:** [`D_PHASE_PROTOCOL.md`](D_PHASE_PROTOCOL.md) § D3, [`cursor_list.md`](cursor_list.md) § D.3.0  
**Proof obligation:** Operational observability (metrics + trace) without reverse dependency or decision influence on the cognitive pipeline.

| Step | Invariant | Gate |
|------|-----------|------|
| 1 | Subscriber skeleton + coexistence | `THOTH_E2_D3_STEP1=1` |
| 2 | Metrics sink-only (E2-D3-01) | `THOTH_E2_D3_01=1` |
| 3 | Failure isolation (E2-D3-02) | `THOTH_E2_D3_02=1` |
| 4 | Structural audit (E2-D3-03) | `THOTH_E2_D3_03=1` |
| 5 | Plugin/config integration proof | `THOTH_E2_D3_05=1` |
| 6 | Umbrella proof-suite regression | `THOTH_E2_D3=1` |

**Umbrella gate:** `THOTH_E2_D3=1` executes the complete D3 proof suite (Steps 1–5). Each step establishes a different architectural invariant.

**Step 6 close-out (2026-07-07):** `THOTH_E2_D3=1`, `THOTH_E2_D2=1`, `THOTH_E2_D1=1`, `THOTH_E2_C5=1` green; full unit suite green (~15 min); G2 `ctest -R thoth-unit-tests` **993.5s** (within 1800s budget).

**Key files:** `metrics_subscriber.*`, `trace_subscriber.*`, `basic_agent_plugin.cpp`, `config.h` / `config.cpp`, `tests/unit_tests.cpp`

**Next:** D4 — paused for explicit authorization.

### E2 Phase D — Evolution tier ✅ complete (2026-07-08)

**Authority:** [`D_PHASE_PROTOCOL.md`](D_PHASE_PROTOCOL.md) · [`D5_PROTOCOL.md`](D5_PROTOCOL.md) v0.1 🔒  
**Close-out:** [`phases/PHASE_D_COMPLETE.md`](phases/PHASE_D_COMPLETE.md)

**Governing question:** Can architecture evolve without losing trust?

| Checkpoint | Deliverable | Gate |
|------------|-------------|------|
| **E2-D1** | Event channel fan-out — Executive invisibility | `THOTH_E2_D1=1` |
| **E2-D2** | Episode replay + benchmark authority isolation | `THOTH_E2_D2=1` |
| **E2-D3** | Observability without authority | `THOTH_E2_D3=1` |
| **E2-D4** | Live INTEGRATION + STRICT authority preservation | `THOTH_E2_D4=1` |
| **E2-D5** | Evolution trust meta-proof (authority + behavioral + determinism + closure) | `THOTH_E2_D5=1` |

**D5 sub-gates:** `THOTH_E2_D5_AUTHORITY=1` (E2-D5-03), `THOTH_E2_D5_C5=1` (E2-D5-01), `THOTH_E2_D5_DETERMINISM=1` (E2-D5-02).

**Closure (2026-07-08):** `THOTH_E2_D5=1` green (~4.3 min sequential). Phase seal recorded in `PHASE_D_COMPLETE.md`. Preservation only — not promotion.

**Next:** Phase E — scientific defense (planned).

### Cognitive hardening roadmap (C1–C7) — status at a glance

| ID | Area | Status | Log section |
|----|------|--------|-------------|
| **C1** | Planning quality / context management | ✅ Phases 1–5 | 2026-06-27 (C1) |
| **C2** | Chat RAG / retrieval ranking | ✅ Phases 0–3 | 2026-06-27 (C2) |
| **C3** | Reflection A/B measurement | ✅ | 2026-06-26 (C3) |
| **C4** | Developer & CI latency | ✅ Phases 1–2 | 2026-06-26 (C4) |
| **C5** | Robustness & failure tests | ✅ 10 cases | 2026-06-28 (C5) |
| **C6** | Cognitive metrics | ✅ Phases 1–2 | 2026-06-27 (C6 P1), 2026-06-29 (C6 P2) |
| **C7** | Runtime / production latency | ✅ Phases 1–3 | 2026-06-26 (C7) |

**GitHub (2026-07-01):** Thoth `de5a469`, Basic_agent `4c45aca` on `main` — E1 checkpoints D3–D5 (robustness, chat-RAG, GRAG harness wiring) pushed.

**NOTE ON DOCUMENTATION ACCURACY (2026-03-30)**: An internal audit revealed that some features listed as "complete" are actually in a prototype or stub state. This document is being updated to reflect the actual implementation status:
- **Self-Building Capability**: Harness tools exist (`project_analyze`, `run_tests`, `code_modify` read); **`apply_diff` is a stub**. Treated as **optional future expansion** — not scheduled active work (2026-06-17).
- **Trajectory Awareness**: Infrastructure is implemented; production `retrieval_config.json` sets `trajectory: 0.2` (2026-06-15). Benchmark shows mixed lift on `TRAJECTORY_DISAMBIGUATES` cases — see `docs/plan_reuse_tuning.md`.
- **Hierarchical Subgoals**: This is still in the planning phase.
- **Trace Resumption**: Full resumption is currently only authoritative through the SQLite persistence layer; log replay is for observability.

### E1 — Benchmark environment pinning ✅ complete

**Spec:** `docs/benchmark_environment.md` (v3.1). **Checkpoints A–E ✅** — closed 2026-07-01.

#### Checkpoint E — 2026-07-01

- **Scope:** Step 7 narrow double-bind mismatch in `BenchmarkRun::bindIndex()`; `scripts/compare_benchmark_env.py`; `check_baseline.py --require-env` (opt-in); five-harness identity + close-out pass; E1-17.
- **Step 7:** Second `bindIndex()` with different `index_hash` populates `index_mismatch { prior_hash, new_hash }` on sidecar + `BENCHMARK_INDEX_BOUND` JSONL; `run_id` / `environment_hash` unchanged. No `IndexManager` or production path changes.
- **Tests:** E1-17 green; E1-01–E1-16 green (`THOTH_MOCK_EPISODIC=1` full unit suite); `run_test_suite --dev` 7/7; `check_baseline.py --require-env` smoke; `compare_benchmark_env.py --strict` smoke on D1 sidecar.
- **Files:** `benchmark_context.cpp`, `tests/unit_tests.cpp`, `scripts/compare_benchmark_env.py`, `check_baseline.py`, `docs/benchmark_environment.md`.

**Five-harness identity pass (single table — close-out evidence):**

| Harness | Trigger | run_id | env_hash | index_hash | Terminal | Cognitive metrics |
|---------|---------|--------|----------|------------|----------|-------------------|
| D1 `run_test_suite` | E1-12 + `--dev` smoke | ✅ sidecar + JSONL | ✅ matches sidecar | ✅ non-empty after bind | `TEST_SUITE_COMPLETE` | ✅ 1 row / goal; attribution match |
| D2 `run_reflection_ab_benchmark` | E1-13 smoke | ✅ | ✅ | ✅ | (smoke path) | ✅ attribution match |
| D3 `run_robustness_suite` | E1-14 smoke | ✅ | ✅ | ✅ | (smoke path) | ✅ attribution match (C5-09 = 6 rows documented) |
| D4 `run_chat_rag_benchmark` | E1-15 smoke | ✅ | ✅ | ✅ | (smoke path) | **0** rows (retrieval-only) |
| D5 `run_grag_benchmark` | E1-16 smoke | ✅ | ✅ | ✅ | (smoke path) | **0** rows (retrieval-only) |

**Close-out:** All 9 boxes in `benchmark_environment.md` § Close-out criteria ticked. **E1 ✅ — proceed to E2 / G1d / B1.**

#### Checkpoint D5 — 2026-07-01

- **Scope:** `run_grag_benchmark` — Ollama preflight; env capture after research corpus index; `BenchmarkRunIdentity` passed to `BenchmarkReporter::reportToFile` (adopts E1 `run_id`/`env_hash`; legacy `benchmark-{timestamp}` fallback when identity empty); `GragBenchmarkRunRecorder` RAII; `sample_mode`/`cases_run` in `GRAG_BENCHMARK_COMPLETE` payload only (not env hash).
- **Pre-flight trace:** No `execute_goal`, `LLMPlanner`, `THOTH_MOCK_LLM`, or test-suite mock plumbing. `--sample` only truncates case list. `BenchmarkRunner` “mock” episode steps are SQLite trajectory fixtures for embedding — not LLM mocks. Real Ollama External embeddings throughout.
- **E1-16:** Full JSONL row assertion via existing `THOTH_WORKSPACE_PATH` (no new FileHandler hook).
- **Metrics note:** **0** `GOAL_COGNITIVE_METRICS` rows — retrieval-only.
- **Tests:** E1-16 green; `--sample` happy path (10 cases, exit 0); `GRAG_BENCHMARK_ABORTED` via `THOTH_GRAG_BENCHMARK_ABORT_SMOKE=1`.
- **Files:** `run_grag_benchmark.cpp`, `benchmark_reporter.h/cpp`, `tests/unit_tests.cpp`, `docs/benchmark_environment.md`.
- **Safe stop — next checkpoint: E**.

#### Checkpoint D4 — 2026-07-01

- **Scope:** `run_chat_rag_benchmark` — env capture after golden corpus index (External embeddings, `BenchmarkTier::OLLAMA`, `fetchOllamaSnapshot`); single `run_id` on all `CHAT_RAG_BENCHMARK_CASE` + `CHAT_RAG_BENCHMARK_SUMMARY` rows; `ChatRagRunRecorder` RAII (`CHAT_RAG_BENCHMARK_COMPLETE` / `CHAT_RAG_BENCHMARK_ABORTED`).
- **Ollama policy:** E1-15 is Ollama-independent (probe-stack smoke). Happy-path Ollama run + abort smoke are required stop gates when Ollama is reachable; if not reachable at commit time, log **“D4 wired, happy-path verification pending”** per `benchmark_environment.md` § Ollama availability.
- **Metrics note:** **0** `GOAL_COGNITIVE_METRICS` rows — retrieval-only harness; no `execute_goal`.
- **Tests:** E1-15 green; Ollama happy path 5/5 hit@1 (`run_id` on 6 JSONL rows); `CHAT_RAG_BENCHMARK_ABORTED` verified via `THOTH_CHAT_RAG_BENCHMARK_ABORT_SMOKE=1`.
- **Files:** `run_chat_rag_benchmark.cpp`, `tests/unit_tests.cpp`, `docs/benchmark_environment.md`.
- **Safe stop — next checkpoint: D5** (`run_grag_benchmark`).

#### Checkpoint D3 — 2026-07-01

- **Scope:** `run_robustness_suite` — probe-stack capture before case loop (`THOTH_MOCK_LLM` baseline, `clearRobustnessEnv()` per-case runs after create); single `run_id` across 7 `execute_goal` calls via `runRobustnessCase(spec, attribution)` / `runExecutiveCase` / C5-09 / C5-10; `RobustnessRunRecorder` RAII; `run_id`/`env_hash` on harness JSONL rows.
- **Metrics note (verified):** 7 attributed `execute_goal` invocations produce **6** `GOAL_COGNITIVE_METRICS` rows. **C5-09** uses one `ExecutiveController` (not two): slow then fast on the same instance. Pre-D3 baseline rebuilt from `fb4fd9f` robustness sources (`THOTH_COGNITIVE_METRICS_LOG` → 6 rows, no `slow concurrent goal`) — same count as post-D3. Mechanism: second `execute_goal` sets `stop_requested_`, joins `loop_thread_`; `run_loop()` exits without COMPLETED/FAILED → no metrics emit (contrast `abort()`, which does emit). Both C5-09 calls pass the same suite-wide `BenchmarkAttribution`; slow goal never reaches terminal emit, so attribution overwrite is immaterial. **Not an E1 regression.**
- **C5-10:** `teardown goal` metrics row present with matching `run_id`/`env_hash`; case still `DESTROYED no_crash=true`.
- **Tests:** E1-14 green; harness 10/10; `ROBUSTNESS_ABORTED` verified via `THOTH_ROBUSTNESS_BENCHMARK_ABORT_SMOKE=1`.
- **Files:** `run_robustness_suite.cpp`, `robustness_cases.h/cpp`, `tests/unit_tests.cpp`, `docs/benchmark_environment.md`.
- **Safe stop — next checkpoint: D4** (`run_chat_rag_benchmark`).

#### Checkpoint D2 — 2026-07-01

- **Scope:** `run_reflection_ab_benchmark` — probe-stack env capture (empty TfIdf index) before case loop; single `run_id` across 4 `execute_goal` calls; `ReflectionAbRunRecorder` RAII (`REFLECTION_AB_COMPLETE` / `REFLECTION_AB_ABORTED`); `run_id`/`env_hash` on `REFLECTION_AB_CASE` + `REFLECTION_AB_SUMMARY`; worker-thread crash limitation documented in `benchmark_environment.md` § Harness terminal events.
- **Pre-flight (D1 crash trace):** D1's worker-thread `std::terminate` was on the **test-suite path** (`LLMPlanner` + `THOTH_TEST_SUITE_DEV` mock responses + multi-step RETRIEVAL/LLM goals). Reflection A/B uses **`ReflectionAbMockPlanner`** (deterministic plans, no LLM JSON parsing), does not wire `LLMInterface`, and does not enable `THOTH_TEST_SUITE_DEV` — **does not share the crash code path**. Shared `THOTH_MOCK_LLM` alone is not the fragility; test-suite-specific mock plumbing is.
- **Tests:** E1-13 green (direct-controller smoke + non-empty `index_hash`); harness 2/2 cases, mean lift 0.5; 4 cognitive metrics rows share `run_id`/`env_hash`; `REFLECTION_AB_ABORTED` verified via `THOTH_REFLECTION_AB_BENCHMARK_ABORT_SMOKE=1`.
- **Files:** `run_reflection_ab_benchmark.cpp`, `tests/unit_tests.cpp`, `docs/benchmark_environment.md`.
- **Safe stop — next checkpoint: D3** (`run_robustness_suite`).

#### Checkpoint D1 — 2026-07-01

- **Scope:** `BasicAgentPlugin::buildTestSuiteBenchmarkInputs()` + `benchmarkIndexEnvironment()` (engine via `rag.engine` after move); `run_test_suite` captures env after `setRagFiles`, single `run_id` for suite; all `executeGoal` calls pass `run.attribution()`; stdout one-line env summary; `TestSuiteRunRecorder` RAII emits `TEST_SUITE_COMPLETE` or `TEST_SUITE_ABORTED` on all exit paths.
- **Tests:** E1-12 green (harness helper path smoke — sidecar + `cognitive_metrics.jsonl` `run_id`/`env_hash` match); `run_test_suite --dev` all TC pass; sidecar + `benchmark_env.jsonl` + cognitive metrics aligned; `TEST_SUITE_ABORTED` verified via `THOTH_TEST_SUITE_BENCHMARK_ABORT_SMOKE=1` (main-thread early exit → JSONL terminal event).
- **Files:** `basic_agent_plugin.h/cpp`, `tests/run_test_suite.cpp`, `tests/unit_tests.cpp`.
- **Safe stop — next checkpoint: D2** (`run_reflection_ab_benchmark`).

#### Checkpoint C — 2026-06-30 (closed 2026-07-01)

- **Scope:** `execute_goal(goal, BenchmarkAttribution={})` on `ExecutiveController`; `benchmark_attribution_` stored before `loop_thread_`; `GoalCognitiveMetricsRecord` + `CognitiveMetricsLogger::toJson` emit optional `run_id`/`env_hash`; `BasicAgentPlugin::executeGoal` overload; `THOTH_COGNITIVE_METRICS_LOG` test override.
- **Tests:** E1-09 – E1-11 green (attribution present; omitted without attribution; worker-thread cross-thread guard).
- **E1-11 guard (2026-07-01):** Asserts `STEP_STARTED` on thread ≠ caller (mid-execution worker read, not post-hoc luck); waits for terminal callback before thread check; final metrics must carry attribution — would fail harness-thread thread-local regression.
- **Files:** `executive_controller.h/cpp`, `cognitive_metrics.h`, `cognitive_metrics_logger.cpp`, `basic_agent_plugin.h`, `benchmark_execution_contract.h`, `tests/unit_tests.cpp`, `docs/benchmark_environment.md`.
- **Safe stop — next checkpoint: D1** (harness wiring begins; one harness per sub-session).

#### Checkpoint B — 2026-06-30

- **Scope:** `BenchmarkRun` / `BenchmarkContext::create`, sidecar + JSONL emit, `bindIndex()` merge under mutex, `GitMetadata`, `fetchOllamaSnapshot` / `isOllamaReachable`, `benchmark_execution_contract.h` design lock.
- **Tests:** E1-07 – E1-08 green (sidecar create + concurrent bindIndex merge).
- **Files:** `benchmark_context.*`, `git_metadata.*`, `ollama_snapshot.*`, `benchmark_execution_contract.h`, `tests/unit_tests.cpp`, `external/basic_agent/CMakeLists.txt`.
- **Safe stop — next checkpoint: C** (still no harness/GUI wiring).

#### Checkpoint A — 2026-06-30

- **Scope:** Nested benchmark environment structs; pure `assembleEnvironment()`; `inferTier()` / `hasTierMismatch()`; `computeEnvironmentHash()` (index-excluded) + `computeIndexHash()`; JSON round-trip; SHA-256 helper.
- **Tests:** E1-01 – E1-06 green (`thoth-unit-tests` with `THOTH_MOCK_EPISODIC=1`).
- **Files:** `external/basic_agent/include/benchmark_environment.h`, `external/basic_agent/src/benchmark_environment.cpp`, `tests/unit_tests.cpp`, `external/basic_agent/CMakeLists.txt`.
- **Safe stop — next checkpoint: B** (no production call sites touched).

<!--
#### Checkpoint B — (date)
- **Scope:** BenchmarkContext, GitMetadata, OllamaSnapshot, design-lock headers
- **Tests:** E1-07 – E1-08 green
- **Next:** Checkpoint C

#### Checkpoint C — (date)
- **Scope:** BenchmarkAttribution → execute_goal → CognitiveMetricsLogger (all call sites)
- **Tests:** E1-09 – E1-11 green; ctest fast green
- **Next:** Checkpoint D1

#### Checkpoint D1–D5 — (date each)
- **Scope:** one harness wired per sub-session
- **Next:** D(n+1) or E

#### Checkpoint E — (date)
- **Scope:** BENCHMARK_INDEX_BOUND, Python scripts, close-out
- **Tests:** E1-12; E1 close-out checklist complete
- **Status:** E1 ✅ — move summary to permanent section below
-->

### 2026-06-26 (E1 — v3.1 spec approved; checkpoint plan)

- **Technical:** v3.1 approved — explicit `BenchmarkAttribution` via `execute_goal()` (C7 `StepExecutionContext` pattern); no `setActive()` / thread-local. Sidecar mutex for `bindIndex()`; `check_baseline.py --require-env` opt-in only.
- **Process:** Multi-session checkpoints **A–E** — each ends compile-clean, test-green, committable. **Checkpoint C isolated** — hot-path signature change must not span sessions. Harness wiring D1–D5 one per sub-session.
- **Doc:** `docs/benchmark_environment.md` — authoritative implementation spec.
- **Next:** Checkpoint **A** (E1-01–E1-06).

### 2026-06-30 (M3 — `/prune` operational interface)

- **API:** `ConsolidationSource` (AUTOMATIC/MANUAL) separate from `ConsolidationReason`; `ConsolidationStatus`, `ConsolidationRequest`, `ConsolidationResult` frozen public contract.
- **Memory:** `getConsolidationStatus()`, `runConsolidation()`, `setGoalActiveChecker()`; goal-active guard unless `--unsafe`.
- **CLI:** `/prune` (default status), `explain`, `batch`, `run`; `--ignore-thresholds`, `--unsafe`.
- **Tracing:** `admin_command` + `memory_consolidation` with `requested_by`, `source`, `decision.reasons`.
- **Fix:** `configureConsolidation` uses `rag.engine.get()` (embed engine after move).
- **Tests:** M3-01 – M3-09 pass under `THOTH_MOCK_EPISODIC=1`.

### 2026-06-29 (Reflection & analysis — eval gap, consolidated roadmap)

- **Context:** C1–C7 + M1–M2 complete; external review (×2) + implementation analysis captured in `cursor_list.md` § Reflection & analysis.
- **Core gap:** Component/per-run eval exists; **longitudinal learning eval** does not — thesis claims (strategy promotion, consolidation → behavioral lift) need C6 Phase 3 + E2/E3.
- **New proposed IDs:** E1 (env pinning), E2 (episodic learning eval), E3 (SCR harness), G1d (trajectory ablation).
- **Roadmap:** M3/M4 → G1d+E1 → C6 Phase 3+E2+E3 → B1 (V3) → F3/F1 when eval shows bottleneck.

### 2026-06-29 (External review — documentation honesty)

Independent review (2026-06): Thoth docs are **unusually candid** about limits — mock Cognate benchmarks report 0.00 task success, trajectory $w_t$ shows mixed lift, subgoal trees are not built, and the 51× Cognate reasoning-depth figure is footnoted as iteration count under mock conditions (not real task completion). Captured in `cursor_list.md` § External review and `audit.md` §5 Known Gaps.

### 2026-06-29 (M2 — Age-based consolidation policy)

- **Policy:** `ConsolidationDecision` with `ConsolidationReason` bitmask (HOT_COUNT, SESSION_INACTIVE, OLDEST_MESSAGE; TOKEN_LIMIT/MEMORY_PRESSURE reserved).
- **Clock:** `Clock` / `SystemClock` / `FakeClock` injected into `MemoryPruner`.
- **Config:** `config.json` → `memory.max_hot_messages`, `memory.max_hot_age_days`, `memory.prune_batch_size`.
- **Orchestration:** `consolidateIfNeeded()` — batch loop (cap 5, no-progress guard, deferred trace); startup discovery without LLM; deferred consolidation on session switch.
- **GUI:** Timestamp-preserving `loadConversation()`; consolidate-before-trim on sync.
- **Tests:** `testM2StaleSessionUnderCap`, `testM2FreshSessionNoOp`, `testM2StartupDeferredNonActive`, `testM2TimestampPreservation`, `testM2MultiTriggerReasons`, `testM2BatchCapDeferred`.

### 2026-06-26 (M1.5 — Episodic verification gate complete; M1 verified)

- **Status:** M1 **verified**; M1.5 **complete**. Proceed to **M2**.
- **Gate spec:** `docs/episodic_memory_benchmark.md`
- **Tests passed** (`THOTH_MOCK_EPISODIC=1`):
  - M1.5a: `testEpisodicRetrievalEndToEnd` — Apollo fact → consolidate → warm → GRAG retrieval
  - M1.5b: `testConsolidationFailureEmbed`, `testConsolidationFailureTransaction` — hot unchanged on embed/DB failure
  - M1.5c: `testConsolidationLatencyRecorded` — `summary_ms`, `embed_ms`, `transaction_ms`, `consolidation_ms`
  - M1.5d: `testEpisodicMemoryBenchmarkNegative` — positive control + unrelated-query negative + under-cap guard

### 2026-06-29 (M1 — Memory consolidation implemented; M1.5 verification gate)

- **Status:** M1 **implemented**; verification tracked as **M1.5** (`episodic_memory_benchmark.md`).
- **Design**: `docs/memory_architecture.md` — working → episodic → semantic → procedural → archival; consolidation terminology.
- **Delivered**: EpisodicMemory, SummaryGenerator, warm_memory + warm_memory_embeddings, atomic txn, GRAG merge, GUI sync path.
- **Verification:** M1.5 gate — see 2026-06-26 entry above.

### 2026-06-29 (M1 — Memory consolidation / warm episodic tier — implementation detail)

- **Design**: `docs/memory_architecture.md` — working → episodic → semantic → procedural → archival; consolidation terminology.
- **EpisodicMemory** structured source of truth; JSON persistence only; `EpisodicMemoryRenderer` for prompt prose; canonical embed text for retrieval.
- **SummaryGenerator** → LLM (or `THOTH_MOCK_EPISODIC=1` in tests); fail-open with `summary_missing` + raw cold archive.
- **Atomic txn**: LLM/embed outside txn; `consolidateSessionBatch` writes warm + embedding + archive + hot delete in one COMMIT.
- **Schema**: `warm_memory`, `warm_memory_embeddings` (separate from content); `derived_from_hash`; `MemoryScope::SESSION`.
- **GRAG**: warm memories merged with document chunks before `GragScorer::rescore`.
- **GUI**: `loadConversationMemorySync` + trim after sync (no silent JSON drops on load).
- **Fixes**: `archived_at_ms` / summary timestamps use milliseconds.
- **Tests**: `testMemoryPruning`, `testMemoryPruningIntegration` extended for warm rows.

### 2026-06-29 (V1 — Manual GUI TEST_SUITE pass)

- **Goal**: Close V1 — GUI pass for `TEST_SUITE.md` TC-01–TC-07 with observability panels.
- **Checklist**: `docs/TEST_SUITE_GUI_CHECKLIST.md`.
- **Evidence**: GUI goal `Tell me about Thoth.` — completed, `grag_alpha=1.0`, 1683 tokens, session `session-1782581237114`; Plan Execution + GRAG Diagnostics confirmed working.
- **Regression**: Headless `run_test_suite --dev` 7/7; `ctest -L fast` 3/3.
- **Trace path**: `agent_workspace/decision_trace.jsonl` (not `logs/`).

### 2026-06-29 (C6 Phase 2 — Analysis tooling, tokens, GUI export)

- **Goal**: Complete C6 Phase 2 — turn append-only metrics into analyzable time-series with real token counts and GUI export.
- **Token tracking** (`llm_interface.h/cpp`):
  - `LlmTokenUsage` struct; `resetSessionTokenUsage()`, `sessionTokenUsage()`, `lastCallTokenUsage()`.
  - Ollama: `prompt_eval_count` + `eval_count` from `/api/generate` response.
  - OpenAI: `usage` object from chat completions.
  - Mocks/dev tier: char/4 estimate when provider counts unavailable.
  - `ExecutiveController` resets session at goal start; snapshots `planning_tokens` after each `create_plan`; emits `total_tokens`, `prompt_tokens`, `completion_tokens`, `planning_tokens`, `synthesis_tokens` in `GOAL_COGNITIVE_METRICS`.
- **Plot script**: `scripts/plot_cognitive_metrics.py` — latency breakdown, retrieval quality, token usage PNGs (`logs/plots/`). Requires `matplotlib`.
- **Summarize script**: extended with token field p50/p95.
- **GUI export**: Benchmarks → **Export Cognitive Metrics…** — copy `logs/cognitive_metrics.jsonl` to user path as JSONL or CSV.
- **Test**: `testLlmTokenUsage` in `thoth-unit-tests`.
- **Verify**: `ctest -L fast` 3/3; run a goal and inspect non-zero `total_tokens` in new log rows.

### 2026-06-28 (GitHub — C4 / C5 / C7 release)

- **Thoth** `5277413` — docs sync, submodule pointer, CTest `WORKING_DIRECTORY` fix.
- **Basic_agent** `2397385` — C5 robustness suite, C7 Phase 3 prefetch, C4 CI labels, mock LLM fixes.
- **Fast gate:** `ctest -L fast` → 3/3 (~70s): `test-suite-dev`, `reflection-ab-benchmark`, `robustness-suite`.
- **Repos:** [Thoth](https://github.com/stevemeierotto/Thoth) · [Basic_agent](https://github.com/stevemeierotto/Basic_agent) on `main`.

### 2026-06-27 (GitHub — C1 / C2 / C6 cognitive hardening release)

- **Thoth** `b4b6adf` — docs backlog + submodule pointer.
- **Basic_agent** `379c0c5` — C1 planner hardening, C2 chat RAG (Phases 0–3), C6 per-goal metrics.
- **Repos:** [Thoth](https://github.com/stevemeierotto/Thoth) · [Basic_agent](https://github.com/stevemeierotto/Basic_agent) on `main`.

### 2026-06-27 (C1 — Planning quality / context management)

- **Goal**: Fix planning quality constrained by context assembly — polluted memory injection, tail truncation dropping rules/schema, missing semantic validation, and unfiltered strategy blast. Expert-reviewed priority: prompt assembly → PlanValidator → memory hygiene → scored strategies → instrumentation.
- **Scope**: Goal execution (`LLMPlanner` → `WorkflowEngine`) only; conversation path handled separately by **C2**.

#### Phase 1 — Structured prompt assembly (memory budgets)

- **`prompt_factory.cpp`**: Planner prompts assembled as fixed sections — **Rules → Schema → Goal → optional experience**. Core sections never tail-truncated; experience competes for remainder and is dropped first when over budget.
- **`planner_injection_config.h`**: Formal budgets — rules 4 KB, schema 2 KB, goal 1 KB, plan reuse 1 KB, strategy 512 B, trajectory 512 B; minimum total budget 8 KB.
- **Default template reorder**: `plan_generation.tmpl` and bundled defaults put Rules/Schema before Goal.
- **`PlannerPromptMetrics`**: Byte counts per section for observability.

#### Phase 2 — PlanValidator (reject + limited repair + C++ fallback)

- **`plan_validator.h` / `plan_validator.cpp`**: New validation layer between JSON parse and execution.
  - **Parser** (`PlanParser`): valid JSON syntax.
  - **Validator** (`PlanValidator`): valid Thoth corpus Q&A plan — requires RETRIEVAL + LLM, rejects TOOL steps, enforces RETRIEVAL-first order.
  - **Limited repair**: wire missing LLM `depends_on` to prior RETRIEVAL only (deterministic).
  - **No semantic repair**: does not insert missing steps or convert step types.
  - **`createFallbackPlan()`**: programmatic RETRIEVAL → LLM plan in C++ after one LLM retry (no third LLM call).
- **`llm_planner.cpp`**: Wired LLM → parse → validate → execute path; logs `plan_generated`, `plan_validation_failed`, `plan_fallback_used`, `depends_on_repaired`.

#### Phase 3 — Memory hygiene

- **`goal_text_utils.h`**: `cleanGoalForStorage()` strips nested `[RELEVANT PAST APPROACHES…]` markers; `sanitizePlanOutline()` emits step-type summaries only.
- **`plan_reuse_config.h`**: Similarity floor 0.55; retrieve limit 1 (one similar plan); trajectory similarity floor.
- **`memory.cpp`**: `retrieveSimilarPlans` / `retrieveSimilarTrajectories` apply similarity floor — inject nothing below threshold.
- **`executive_controller.cpp`**: Store clean goals in `past_plans`; sanitized outlines in plan-reuse injection; reflection replan gets failure summary only (no plan-reuse re-injection).

#### Phase 4 — Strategy scoring (not disable)

- **`llm_planner.cpp`**: Top-1 promoted strategy selected by goal-embedding cosine similarity to strategy description + pattern (floor 0.40); replaces `getAllStrategies()` blast.

#### Phase 5 — Planner instrumentation

- **`PLANNER_CONTEXT_ASSEMBLY`**: Logs `rules_bytes`, `schema_bytes`, `goal_bytes`, `strategy_bytes`, `trajectory_bytes`, `plan_reuse_bytes`, `total_bytes`, `experience_dropped`, `strategy_similarity`.
- **`PLANNER_REVISION_CONTEXT`**: Same byte metrics for revision prompts.
- Feeds **C6** cognitive metrics roadmap.

- **Verification**: `cmake --build build/debug --target thoth-control-panel` ✅. Headless `run_test_suite` TC-01–TC-07 ✅ (2026-06-27).
- **Operator note**: Existing `agent_workspace/prompt_templates/plan_generation.tmpl` is only written when missing — delete or update manually for Rules-first template in an existing workspace. Existing `memory.db` may retain polluted goals until new runs overwrite history.

### 2026-06-27 (C2 — Chat RAG quality — Phases 0–3 complete)

End-to-end chat Q&A pipeline: observability → benchmark → retrieval tuning → grounded prompts. User-validated: GUI “Explain GRAG” returns accurate corpus-grounded answer.

#### Phase 0 — Chat RAG observability

- **Goal**: Instrument the conversation (`processQuery`) pipeline only — no retrieval, prompt, truncation, or tool-injection behavior changes. Make chat diagnosable the same way C1 made the planner diagnosable.
- **Log sink**: `logs/chat_rag.jsonl` (append-only, one `CHAT_RAG_CONTEXT` + one `CHAT_RAG_RESPONSE` per turn, linked by `request_id`).

#### Events and metrics

- **`CHAT_RAG_CONTEXT`** (before `llm.query()`): query, ranked documents (file, rank, chunk_id, score, chars, line range), `top_k`, byte counts (retrieved, tool_schema, conversation_history, memory_context, system_prompt), `final_prompt_chars`, truncation before/after + `truncated_section`, derived ratios (`grounding_ratio`, `tool_ratio`, `history_ratio`, `memory_ratio`), `llm_model`, `grounding_mode`.
- **`CHAT_RAG_RESPONSE`** (after LLM): `answer_chars`, `retrieved_doc_count`, `grounding_mode`, `fallback_used` (always false for chat today).

#### Code locations

| Metric / event | Collected in |
|--------------|--------------|
| Retrieval scores + ranked docs | `RAGPipeline::retrieveRelevant` → `GragDiagnostics*` out-param → `command_processor.cpp` `buildDocumentMetrics()` |
| Prompt section byte counts | `PromptFactory::buildChatPrompt()` → `ConversationPromptMetrics` |
| Truncation before/after + section | Section-protected assembly in `buildChatPrompt()` (Phase 3) |
| Final prompt + grounding ratios | `command_processor.cpp` `buildChatRagContextRecord()` |
| JSONL persistence | `ChatRagLogger` (`chat_rag_logger.cpp`) |
| Structured log + decision trace stages | `StructuredLogger` + `DecisionTraceLogger` stages `chat_rag_context` / `chat_rag_response` |

- **Verification**: `cmake --build build/debug --target thoth-control-panel` ✅. Run a GUI chat query and inspect `logs/chat_rag.jsonl`.

#### Phase 1 — Golden chat-RAG retrieval benchmark

- **Tool**: `./build/debug/external/basic_agent/run_chat_rag_benchmark` (requires Ollama for embeddings).
- **Corpus**: `agent_workspace/rag/` — `GRAG.md`, `cognate.md`, `HOWTO.md`, `AGENTS.md`.
- **Golden queries (5):** Explain GRAG → GRAG.md; What is Cognate → cognate.md; How do I use Thoth → HOWTO.md; agent conventions → AGENTS.md; quote GRAG first sentence → GRAG.md.
- **Log sink:** `logs/chat_rag_benchmark.jsonl`.
- **Baseline (2026-06-27):** hit@1 **2/5**, mean nDCG@1 **0.40** — AGENTS.md dominated definitional queries.

#### Phase 2 — Conversational retrieval tuning

- **Filename-aware boosts**, coverage recall supplementation, min-chunk filter (≥80 chars), session-scoped corpus via `setActiveCorpusFiles`, chunk metadata at injection.
- **Benchmark after tuning:** hit@1 **5/5**, mean nDCG@1 **1.00**, mean MRR **1.00**.

#### Phase 3 — Chat pipeline fixes (user validated)

- **Grounding rules** when RAG context present; **Q&A tool gating** via `looksLikeToolIntent()`; **section-protected truncation** (no tail-chop); fixed `grounding_ratio` / `tool_ratio` metrics; `llm_model` config fallback.
- **User validation (2026-06-27):** GUI “Explain GRAG” — accurate grounded answer, no fabricated acronyms.
- **Key files:** `chat_prompt_config.h`, `chat_query_utils.cpp`, `prompt_factory.cpp` (`buildChatPrompt`), `command_processor.cpp`.

### 2026-06-27 (C6 Phase 1 — Per-goal cognitive metrics logging)

- **Goal**: One unified append-only metrics row per goal execution for time-series analysis (feeds C3, C4, C7, benchmarks).
- **Log sink**: `logs/cognitive_metrics.jsonl` — event `GOAL_COGNITIVE_METRICS`.
- **Emit points**: `PLAN_COMPLETED`, `PLAN_FAILED`, `PLAN_ABORTED` in `ExecutiveController`.
- **Fields**: `plan_id`, `session_id`, `goal`, `outcome`, `total_wall_clock_ms`, `planning_time_ms`, `retrieval_time_ms`, `llm_synthesis_time_ms`, `step_count`, `retrieved_chunk_count`, `grag_alpha`, `grag_routing_mode`, `trajectory_score`, `final_success_score`, `reflection_count`, `revisions_count`, `plan_reused`, `total_tokens` (reserved, 0); **C7** adds `synthesis_prompt_chars`, `synthesis_context_truncated`.
- **Files**: `cognitive_metrics.h`, `cognitive_metrics_logger.cpp`; wired in `executive_controller.cpp`, `workflow_engine.cpp` (GRAG diagnostics on RETRIEVAL steps).
- **Verify**: Run a goal; inspect `logs/cognitive_metrics.jsonl` for one `GOAL_COGNITIVE_METRICS` row with non-zero latencies.

### 2026-06-26 (C3 — Reflection A/B measurement)

- **Goal**: Prove reflection replan improves recoverable failures and does not loop on timeout failures. Compare `max_reflections=0` vs `2`; log A/B outcomes append-only.
- **Policy**: `reflectionSkipReason()` suppresses replan when trajectory contains timeout errors (`timeout_failure`), reflection disabled (`max_reflections=0`), or budget exhausted.
- **Config**: `Config::max_reflections` (default 2); env `THOTH_MAX_REFLECTIONS`; wired via `ExecutiveController::set_max_reflections()` in `basic_agent_plugin.cpp`.
- **Harness**: `run_reflection_ab_benchmark` — mock-only (`THOTH_MOCK_LLM`, `THOTH_MOCK_STEP_TIMEOUT` for `timeout-step`).
  - **C3-01**: Recoverable NODE failure → reflection on rescues to COMPLETED (planner_calls 1→2).
  - **C3-02**: Timeout step → both arms FAILED, planner_calls=1 (no reflection replan).
- **Log sink**: `logs/reflection_ab_benchmark.jsonl` — `REFLECTION_AB_CASE` + `REFLECTION_AB_SUMMARY` (`mean_reflection_lift`).
- **C6 integration**: `GOAL_COGNITIVE_METRICS` now includes `max_reflections`, `reflection_skip_reason`.
- **Verify**: `./build/debug/external/basic_agent/run_reflection_ab_benchmark` → **2/2 cases pass**, mean reflection lift **0.5** (2026-06-26).

### 2026-06-26 (C4 Phase 1 — Developer / CI test-suite dev tier)

- **Goal**: Cut `run_test_suite` feedback from ~40 min (Ollama) to seconds for daily development; keep `--full` for production regression.
- **Harness**: `./build/debug/tests/run_test_suite --dev` (default) vs `--full`.
- **Mechanisms**:
  - `test_suite_dev.cpp` — deterministic mock LLM for planner + chat when `THOTH_TEST_SUITE_DEV=1`.
  - `BasicAgentPlugin` — TfIdf embeddings in dev tier; isolated index via `THOTH_TEST_SUITE_INDEX` (`test_suite_corpus/test_suite.rag_index.bin`); auto-save after index; skip re-index when fingerprints unchanged.
  - `workflow_engine` — existing `THOTH_MOCK_LLM` for LLM synthesis steps.
  - Dev TC-03 checks goal-scoped GRAG row (`goal_present`); full tier still requires `alpha > 0` and `direction_magnitude > 0`.
  - Scoring-type filter accepts `rag_hybrid` / `grag_blended_hybrid` (matches `GragScorer` output).
- **Verify**: `run_test_suite --dev` → **7/7 pass in ~10s** (2026-06-26). Second run skips 3/3 corpus files (cached index).

### 2026-06-26 (C4 Phase 2 — CI tier wiring)

- **Goal**: Wire dev vs full regression into CTest and GitHub Actions so PRs get fast feedback and production path runs on schedule.
- **CTest** (`tests/CMakeLists.txt`):
  - `thoth-unit-tests` — label `pr;unit`
  - `test-suite-dev` — label `pr;fast;cognitive`
  - `reflection-ab-benchmark` — label `pr;fast;cognitive`
  - `robustness-suite` — label `pr;fast;cognitive` (added 2026-06-28 with **C5**)
  - `test-suite-full` — `-DTHOTH_TEST_SUITE_FULL=ON`; labels `nightly;ollama`
- **Local:** `ctest -L fast` (~70s, 3 cognitive tests); `ctest -L pr` (full PR gate)
- **GitHub Actions:**
  - PR/push: `ctest -L pr`
  - `cognitive-regression-nightly`: schedule + `workflow_dispatch`; installs Ollama, pulls `qwen2.5:3b` + `nomic-embed-text`, runs `test-suite-full`
  - Checkout uses `submodules: recursive` for `external/basic_agent`
- **Verify**: `ctest -L fast` passes locally after reconfigure.

### 2026-06-26 (C7 Phase 1–2 — Runtime latency)

- **Goal**: Speed up production goal execution (not CI mocks) while preserving GRAG quality and plan correctness.
- **Phase 1 — hot-path optimizations**:
  - `grag_scorer.cpp`: batch TF-IDF chunk embeddings during keyword rescoring (was N serial `embed()` calls).
  - `executive_controller.cpp`: `refresh_goal_state_embeddings_unlocked()` — single `embedBatch` for goal G + current state C at plan start/resume; per-step loop uses state-only `embed()` (avoids re-embedding goal every tick and fixed reflection-ab stack smash under concurrent teardown).
  - `workflow_engine.cpp`: cap retrieved context for LLM synthesis (`synthesis_max_context_chars`, default 8192).
  - `llm_interface.cpp`: send Ollama `options.num_predict`; synthesis steps use `synthesis_num_predict` (default 512).
  - `config.h`: `synthesis_max_context_chars`, `synthesis_num_predict`.
  - C6 fields: `synthesis_prompt_chars`, `synthesis_context_truncated`.
- **Phase 2 — analysis**: `scripts/summarize_cognitive_metrics.py` — p50/p95 for wall-clock, planning, retrieval, synthesis.
- **Verify**: `cmake --build build/debug --target thoth-control-panel`; `ctest -L fast` 3/3 (~70s after **C5** added robustness-suite).
- **CTest fix**: cognitive tests set `WORKING_DIRECTORY` to repo root so relative `agent_workspace/memory.db` resolves correctly.

### 2026-06-26 (C7 Phase 3 — Parallel RETRIEVAL + prefetch)

- **Goal**: Overlap independent RETRIEVAL steps and hide retrieval latency behind the last blocking dependency.
- **Embedding snapshots**: `StepExecutionContext` carries G/C/T vectors at dispatch; `executeRetrieval` passes them to GRAG (thread-safe parallel retrieval).
- **Scheduling**: `max_parallel_retrieval` cap (default 4); independent ready RETRIEVAL steps dispatch concurrently via existing async harness.
- **Prefetch**: when `enable_retrieval_prefetch=true`, PENDING RETRIEVAL steps with exactly one RUNNING dependency start early; results cached and applied on dependency success (`retrieval_prefetch_hits` tracked internally).
- **Lifecycle**: `~ExecutiveController` drains active + prefetch futures before teardown (fixes stack smash in reflection A/B harness).
- **Config**: `max_parallel_retrieval`, `enable_retrieval_prefetch` in `config.json`.
- **Test**: `testParallelRetrieval` — verifies two independent RETRIEVAL steps overlap.
- **Verify**: `ctest -L fast` 3/3; `./tests/thoth-unit-tests` (parallel retrieval test passes).

### 2026-06-28 (C4 & C7 — Latency work closed)

**C4 (Developer & CI)** and **C7 (Runtime / production)** are separate problems with separate solutions — both complete. Cross-reference: `docs/cursor_list.md` § C4, § C7, and comparison table.

| | C4 | C7 |
|--|----|----|
| **Problem** | Slow feedback loop for developers and CI | Slow per-goal execution in production |
| **Approach** | Mocks, tiny corpus, cached index, tiered CTest | Hot-path optimization, synthesis caps, parallel scheduling |
| **Success** | `run_test_suite --dev` ~10s; `ctest -L fast` ~70s | Configurable prefetch + batch embed; metrics via C6 + `summarize_cognitive_metrics.py` |
| **Regression guard** | Nightly Ollama `test-suite-full` | C1/C2/C3/C5 harnesses unchanged |

### 2026-06-28 (C5 — Robustness & failure tests)

- **Goal**: Fast mock-only regression for planning, retrieval, execution, reflection, and lifecycle failure modes. Observable outcomes only (terminal state, bounded retries, structurally valid plans).
- **Harness**: `run_robustness_suite` + `robustness_cases.*` — 10 cases in five categories; append-only `logs/robustness_suite.jsonl` with `pass_reason` per case.
- **Production tweaks** (behavioral, not test-only):
  - Empty index vs empty retrieval: populated index + zero matches → RETRIEVAL succeeds with `retrieval_empty=true`; empty index → FAILED.
  - Synthesis prompt: explicit `No relevant documents found.` when no chunks in context.
  - `THOTH_MOCK_LLM_UNAVAILABLE` + LLM `[Error]` responses fail steps cleanly.
  - Null-safe step `payload` handling in workflow engine.
  - Mock LLM path builds synthesis prompt before short-circuit (no `LLMInterface` required for mocks).
- **CI**: `ctest -L fast` → 3 tests (~70s): dev TEST_SUITE + reflection A/B + robustness suite.
- **Verify**: `./build/debug/external/basic_agent/run_robustness_suite` — 10/10 pass.

### 2026-06-18 (Tier 1 — Audit refresh + automated verification)

- **`audit.md`:** Full refresh — `allow_shell_exec` marked fixed (P1.5); portable `FileHandler` path wording; 2026-06-18 verification run documented.
- **`VERIFIED_BASELINE.md`:** §5 split automated (ctest + grep, 2026-06-18) vs historical manual TC pass (2026-03-12).
- **`AGENTS.md`:** Removed stale `allow_shell_exec` bypass note.
- **`ctest`:** 100% pass (~100s). Manual TC-01–TC-07 deferred to GUI session.

### 2026-06-17 (P2.6 — Benchmark Interpretation)

- **`benchmark_results.md`:** “How to Read These Runs” guide (corpus / case mix / when to cite which delta).
- **`GRAG.md`:** Validation status corpus-qualified; §5 split canonical (100-case, +0.041 / +0.202) vs early sandbox (+0.200); trajectory audit → PARTIAL; self-building → optional future expansion.
- **`MYPAPER.md` (Zenodo):** §4 table and claims aligned to Mar 14 hardened 100-case run; removed stale 563-chunk figures.
- **`AGENTS.md`:** GRAG empirical validation line updated to match canonical benchmark.

### 2026-06-17 (P2.5 — Scratch Doc Cleanup)

- Merged UI scratch notes into `ui_improvements.md` §9–§12; merged `TEST.md` concurrency protocol into `TESTING.md`.
- Deleted: `tmp.md`, `tmp2.md`, `tmp_ui.md`, `tmp_tools.md`, `tmp_science.md`, `qwen_reply.md`, `TEST.md`, `gemini_thoughts.md`.
- Linked `HOWTO.md`, `new_corpus_tests.md`; banner on `old_improvements.md`.

### 2026-06-17 (P2.4 — Narrative Doc Honesty)

- **`README.md`**: Known gaps table; Cognate benchmark caveat; Phase 3 partial.
- **`COGNATE_V2.md`**: Typo fix; mock-environment footnote for 51× reasoning depth.
- **`MYPAPER.md`**: §3.4 Adaptive Graph Learning marked implemented; §4 benchmark table aligned to hardened 100-case run (P2.6).
- **`cognate.md`**: §5 future frontiers aligned with graph learning complete and self-building as optional expansion.
- **Removed** `gemini_thoughts.md` (superseded by `README.md`, `audit.md`, `cursor_list.md`).

### 2026-06-17 (P2.3 — Roadmap Phase Table Alignment)

- **Goal**: Align `improvements.md` Phase 3–5 table and step headers with `completed_improvements_log.md` reality.
- **Phase Order table**: Phases 3–5 marked 🔶 Partial with status legend.
- **Per-phase rollups**: Step-level ✅ / 🔶 / 📋 / 🚫 status tables added for Phases 3, 4, 5.
- **Step headers**: Completed steps point to log entries; pause gates removed from finished work.
- **Close-outs**: Phase 3–5 close-out checklists updated to show what passes vs. deferred today.

### 2026-06-16 (P1.6 — Memory Pruning Integration + GUI Hot-Tier Trim)

- **Goal**: Wire `MemoryPruner` into runtime and stop unbounded growth in SQLite + `chat_sessions.json`.
- **Session scoping**: `Memory::setActiveSessionId()` / `getActiveSessionId()`; `BasicAgentPlugin::setSessionId()` now updates memory before controller/RAG.
- **Auto-prune**: `MemoryPruner` constructed in `Memory` constructor; `maybePruneAfterWrite()` runs after each `addMessage()` (outside `mtx` lock).
- **Shared limits**: `memory_pruning_config.h` — `kMaxHotMessages=50`, `kPruneBatchSize=10` (used by pruner, tests, and GUI).
- **GUI**: `MainFrame` trims each session to hot-tier cap on load and before save (`TrimSessionMessagesForPersistence`).
- **Tests**: `testMemoryPruningIntegration`, `testMemorySessionScoping` (added; not run in this pass).

### 2026-06-15 / 2026-06-16 (P1 Alignment: Security, Trajectory Config, Plan Reuse)

- **Branch:** `cursor/p1-plan-reuse-security-observability` → merged to `main` (2026-06-16).
- **Submodule:** `external/basic_agent` @ `2953068` (plan reuse, shell gating, observability).
- **P1.1 — Trajectory weight activated**:
    - Set `agent_workspace/retrieval_config.json` → `"trajectory": 0.2` (local runtime file; gitignored).
    - Full GRAG benchmark: overall nDCG +0.042; trajectory case type −0.037 (documented in `docs/benchmark_results.md`).
- **P1.5 — `allow_shell_exec` enforcement**:
    - `run_tests` and `code_modify build` gate on `Config::allow_shell_exec`; `ToolRegistry::setConfig()` wired from plugin; unit test `testAllowShellExecGate`.
- **P1.3 — Plan history reuse**:
    - **Goal**: Make `retrieveSimilarPlans()` operational and observable across executive, planner, storage, and GUI.
    - **Implementation**: Cosine similarity over `past_plans` (v2) with tunable thresholds in `plan_reuse_config.h`; injection in `execute_goal` and reflection replan; events `PLAN_REUSE_INJECTION`, `REFLECTION_REPLAN`, `PLAN_HISTORY_STORED`; planner logs `PLANNER_CONTEXT_ASSEMBLY`, `COGNATE_PLAN_PERSISTED`.
    - **Deadlock fix (2026-06-16)**: Observability helpers no longer call `emit_event()` while holding `ExecutiveController::mutex_`; UI events deferred until after unlock.
    - **Documentation**: `docs/plan_reuse_tuning.md` (thresholds, trajectory, observability map); `docs/cursor_list.md` updated for P1.1/P1.3/P1.5.
    - **Tests**: `testPastPlanRetrieval`, `testAllowShellExecGate`; `ctest --output-on-failure` **100% green** (2026-06-16, ~78s).

### 2026-06-13 (Fresh-Start Alignment & Reflection Test Verification)

- **Alignment Backlog (`docs/cursor_list.md`)**:
    - **Goal**: Produce a prioritized, honest gap list between documentation claims and code/test reality after returning to the project.
    - **Implementation Details**: Audited build, unit tests, specs, and roadmap docs; documented open items by priority (P1–P4). Priority 0 (reflection test + green ctest) marked resolved after verification below.
- **Reflection Loop Unit Test (`testReflectionLoop`)**:
    - **Goal**: Automate verification that low trajectory scores trigger a second `IPlanner::create_plan()` call (reflection replan).
    - **Root Cause**: The test used an all-LLM mock plan; `WorkflowEngine::executeLLM()` stub always succeeds → score 1.0 → reflection never fired. This was a **test design issue**, not an ExecutiveController regression.
    - **Fix**: First mock plan uses a failing `StepType::NODE` step; reflection replan uses `LLM`. Added assertions for `Reflection:` goal suffix, two `PLAN_CREATED` events (async-safe polling), terminal `COMPLETED` state, and active plan id `reflection-plan-2`.
    - **Documentation**: Added reflection testing guidance to `docs/TESTING.md`.
    - **Verification**: `ctest --output-on-failure` passes 100% on `main` (2026-06-13).
- **Main Integration (merged to `main`)**:
    - Consolidated pending GUI/benchmark work (`BenchmarkWindow`, `ChatSessionRenderer`, graph/session UI), thesis doc assets, `.editorconfig`, and `basic_agent` pointer update (`f5d5b37` reflection scoring hardening).

### 2026-03-29 (Cognitive Loop Stability & Score Accuracy)

- **Reflection Loop & Success Scoring Hardening**:
    - **Goal**: Resolve the "Always 0 Success Score" bug and prevent infinite reflection loops.
    - **Implementation Details**:
        - **Decoupled Scoring**: Refactored `calculate_trajectory_score` to use explicit success flags instead of relying on transient controller states, ensuring accurate scoring (1.0 for success) at the moment of plan completion.
        - **Empty Plan Guard**: Updated `decide_transition` to handle plans with 0 steps. These are now correctly scored as 0.0 and routed through the reflection loop for repair or marked as `FAILED` if unrecoverable.
        - **Terminal State Enforcement**: Guaranteed that plans with 0 steps cannot transition to `COMPLETED`, ensuring the "Success" status accurately reflects actual work performed.
    - **Verification**: Successfully recompiled; verified logic via code review. **Automated coverage completed 2026-06-13** via strengthened `testReflectionLoop` (see entry above).

### 2026-03-29 (Production-Grade Benchmark UI)

- **Benchmark Execution & Visualization Layer**:
    - **Goal**: Enable real-time execution and visualization of agent benchmarks (`run_grag_benchmark`, `run_cognate_benchmark`) directly from the Thoth UI.
    - **Implementation Details**:
        - **BenchmarkWindow**: Implemented a dedicated `BenchmarkWindow` for real-time output visualization using a multi-line, monospace `wxTextCtrl`.
        - **Thread-Safe Pipe Handling**: Developed an asynchronous pipe reading mechanism using a 100ms `wxTimer` and thread-safe `wxTheApp->CallAfter` with explicit buffer copying to prevent corruption.
        - **Process Lifecycle Management**: Implemented robust process control with `wxProcess`, supporting graceful termination (SIGTERM) and timed escalation to force-kill (SIGKILL) via a non-blocking `wxTimer`.
        - **SQLite Concurrency Guard**: Added a singleton execution constraint in `MainFrame` to prevent database contention and deadlocks during benchmark runs.
        - **Robust Path Resolution**: Implemented `AgentInterface::GetBenchmarkBinaryPath` to dynamically locate benchmark executables across multiple build configurations (Debug/Release) without hardcoding.
        - **Observability Integration**: Integrated anchored regex parsing for real-time metric extraction (`nDCG@5`, `Success Rate`) and a "View Results" handler to open `docs/benchmark_results.md`.
        - **Shutdown Protection**: Added a `MainFrame` close handler to ensure running benchmarks are terminated before application exit, preventing zombie processes.
    - **Verification**: Successfully recompiled; verified process lifecycle, path resolution, and thread-safe output streaming via integrated GUI testing.

### 2026-03-28 (Cognate V2 — Phase 5.2)

- **Thesis Validation: Strategy Adoption & Learning Curve**:
    - **Goal**: Empirically prove that the system learns from its own history and improves planning via strategy adoption.
    - **Implementation Details**:
        - **Persistence Hardening**: Resolved a multi-stage data persistence failure by implementing SQLite schema migrations for `trajectories` and `strategies` tables and fixing NULL-BLOB binding for empty embeddings.
        - **Learning Curve Benchmark**: Enhanced `run_cognate_benchmark` to perform a two-pass evaluation (Cold vs. Warm start).
        - **Empirical Proof**: Successfully demonstrated **Autonomous Strategy Promotion**:
            - Pass 1 (Cold): 0 strategies.
            - Pattern Extraction: 9 trajectories processed.
            - Promotion: 1 high-success pattern promoted to first-class Strategy (Threshold: 80% Success / 3 Runs).
            - Pass 2 (Warm): Strategy Library active and utilized by the planner.
    - **Verification**: Benchmark tool output confirmed the promotion of `RETRIEVAL -> TOOL:llm_reasoning` pattern after 3 successful runs.

### 2026-03-28 (Cognate V2 — Phase 5.1)

- **Thesis Validation: Standard vs. Scientific Benchmarks**:
    - **Goal**: Generate empirical data for comparative analysis of different cognitive execution strategies.
    - **Implementation Details**:
        - **Benchmark Tooling**: Developed a dedicated `run_cognate_benchmark` executable to automate multi-task evaluation.
        - **Comparative Metrics**: Implemented tracking for Success Rate, Average Steps, and Reasoning Depth (iterations).
        - **Infrastructure Integration**: Wired the benchmark tool to use the same `ExecutiveController` and `ScientificExecutionMode` logic as the main application.
    - **Verification**: Ran a comparative benchmark across 10 complex engineering tasks; confirmed that Scientific mode correctly utilizes iterative convergence (avg depth 198.3 steps per goal) compared to linear Standard mode. Results recorded in `docs/benchmark_results.md`.

### 2026-03-28 (Cognate V2 — Phase 4.2)

- **Observability Layer: Cognitive Loop Graph Visualization**:
    - **Goal**: Provide a real-time visual representation of the Cognate architecture's state transitions for thesis demonstration.
    - **Implementation Details**:
        - **Custom Drawing**: Implemented a directed graph rendering engine in `GraphPanel` using `wxGraphicsContext`.
        - **Architecture Mapping**: Defined 6 core cognitive nodes: Goal Ingestion, LLM Planner, Executive Controller, Scientific Mode, Strategy Engine, and Hybrid Memory.
        - **Dynamic Highlighting**: Wired `ControllerState` events to active node highlighting (e.g., the "Scientific Mode" node glows green when the scientific execution strategy is active).
        - **Sub-stage Mapping**: Mapped `reasoning_stage` metadata (Hypothesis, Feasibility, etc.) to high-level architecture nodes to ensure consistent visual feedback.
    - **Verification**: Successfully recompiled; verified state-to-node mapping via UI code review and event bus integration checks.

### 2026-03-28 (Cognate V2 — Phase 4.1)

- **Observability Layer: Trajectory Viewer 'Plan vs. Reality' Wiring**:
    - **Goal**: Visualize the evolution of goal execution by comparing initial plans with actual steps taken.
    - **Implementation Details**:
        - **ID Alignment**: Corrected a discrepancy in `ExecutiveController` where `episode_id` and `trajectory_id` prefixes were mismatched, ensuring SQLite records are correctly linked in the UI.
        - **UI Enhancement**: Upgraded `TrajectoryViewer` to render a hierarchical "Initial Plan vs. Reality" view using `wxTreeListCtrl`.
        - **Plan Visualization**: Added an "Initial Plan" child node under each trajectory to show the LLM's original reasoning before execution.
        - **Reality Visualization**: Added a "Reality (Steps Taken)" node to show the actual actions, results, and revisions performed by the controller.
    - **Verification**: Successfully recompiled both the core library and the GUI; verified data alignment via unit tests and UI code review.

### 2026-03-28 (Cognate V2 — Phase 3.2)

- **Experience-Guided Planning: Strategy Injection & Verbose Logging**:
    - **Goal**: Prove causation by explicitly injecting learned strategies into the LLM planning prompt and logging the process.
    - **Implementation Details**:
        - **Contextual Injection**: Updated `LLMPlanner::create_plan` to retrieve high-success strategies from memory and inject them as a formatted `[LEARNED STRATEGIES]` block into the prompt.
        - **Trajectory Integration**: Added simultaneous injection of relevant past trajectories to provide the LLM with full execution context.
        - **Verbose Causation Logging**: Implemented explicit `STRATEGY_INJECTION` and `TRAJECTORY_INJECTION` events in `decision_trace.jsonl` to track exactly what knowledge was provided to the planner for every goal.
    - **Verification**: Added `testStrategyInjection` to `thoth-unit-tests`; verified that the strategy retrieval and injection logic is correctly reached during plan generation.

### 2026-03-28 (Cognate V2 — Phase 3.1)

- **Strategy Engine 2.0: Semantic Promotion & Library**:
    - **Goal**: Implement rigorous strategy extraction from execution trajectories to enable experience-guided learning.
    - **Implementation Details**:
        - **Semantic Pattern Extraction**: Upgraded `StrategyEngine` to detect sequences of Tool calls (e.g., `TOOL:project_analyze`) and Step Types (e.g., `RETRIEVAL`), moving beyond raw enums.
        - **Promotion Logic**: Enforced the thesis-critical **80% success / 3-run threshold** for pattern promotion to first-class `Strategies`.
        - **Strategy Library**: Expanded the `Strategy` struct with `reasoning_stage` and metadata for better planning context.
        - **Deterministic Stability**: Implemented hash-based deterministic Strategy IDs to prevent duplication across analysis cycles.
    - **Verification**: Added `testStrategyPromotion` to `thoth-unit-tests`; verified that 3 successful simulated runs correctly trigger a strategy promotion with the expected semantic pattern string.

### 2026-03-28 (Cognate V2 — Phase 2.2)

- **Scientific Reasoning Engine: Convergence & Infinite Loop Protection**:
    - **Goal**: Define rigorous exit conditions for iterative reasoning loops to ensure genuine convergence.
    - **Implementation Details**:
        - **Convergence Logic**: Implemented `is_converged()` in `ScientificExecutionMode` based on numerical stability of confidence scores ($\Delta < 0.05$) over an observation window of 2 iterations.
        - **Confidence History**: Added `confidence_history` to `ProblemState` to track progress across iterations and restarts.
        - **Safety Gates**: Enforced a `MAX_ITERATIONS = 5` fallback to prevent infinite loops.
        - **Loop Exit Events**: Standardized logging and events for convergence detection vs. max iteration limits.
    - **Verification**: Added `testScientificConvergence` to `thoth-unit-tests`; verified the system correctly identifies convergence and transitions back to `IDLE` after 4 iterations.

### 2026-03-28 (Cognate V2 — Phase 2.1)

- **Scientific Reasoning Engine: 4-Stage Iterative Loop**:
    - **Goal**: Implement a real iterative reasoning loop that converges on solutions.
    - **Implementation Details**:
        - **Stage Logic**: Replaced the prototype stub in `ScientificExecutionMode` with a structured 4-stage loop: Hypothesis Generation, Constraint Extraction, Feasibility Evaluation, and Final Selection.
        - **State Integration**: Wired the `ProblemState` into every stage, ensuring hypotheses and constraints are tracked across iterations.
        - **Observability**: Standardized `ControllerEvent` emission for each reasoning stage, providing real-time telemetry for the UI.
    - **Verification**: Added `testScientificLoopStages` to `thoth-unit-tests`; confirmed all 4 stages are visited and the `ProblemState` is correctly updated.

### 2026-03-28 (Cognate V2 — Phase 1.3)

- **Unified Event Bus for UI Observability**:
    - **Goal**: Lock down a structured event schema to ensure consistent observability across reasoning stages.
    - **Implementation Details**:
        - **Schema Standardization**: Standardized the `ControllerEvent` metadata payload to include mandatory fields: `reasoning_stage`, `confidence_score`, `success`, and `iteration_count`.
        - **Automatic Enrichment**: Updated `ExecutiveController::emit_event` to automatically populate these fields based on `EventType` and internal state (e.g., `reflection_count_`).
        - **Terminal Handling**: Guaranteed `success: false` for `PLAN_FAILED` and `PLAN_ABORTED` events.
    - **Verification**: Added `testEventSchemaStandardization` to `thoth-unit-tests`; verified all Cognate V2 fields are present in emitted events.

### 2026-03-28 (Cognate V2 — Phase 1.2)

- **ExecutiveController Mode Switching & Persistence**:
    - **Goal**: Enable dynamic mode transitions with high-integrity state preservation.
    - **Implementation Details**:
        - **Controller Updates**: Added `current_problem_state_` to `ExecutiveController`.
        - **Mode Switching**: Enhanced `set_execution_mode` to atomically persist both the active `Plan` and `ProblemState` during transitions.
        - **Persistence Methods**: Implemented `update_problem_state` and `persist_problem_state_unlocked` in the controller.
        - **Event Emission**: Guaranteed `MODE_SWITCHED` event emission upon successful transition.
    - **Verification**: Added `testModeSwitchPersistence` to `thoth-unit-tests`; confirmed `MODE_SWITCHED` event and SQLite state verification.

### 2026-03-28 (Cognate V2 — Phase 1.1)

- **Formalized ProblemState & SQLite Persistence**:
    - **Goal**: Implement high-fidelity state persistence for scientific reasoning loops.
    - **Implementation Details**:
        - **Data Model**: Created `ProblemState` struct in `problem_state.h` with support for hypotheses, constraints, and iteration tracking.
        - **SQLite Integration**: Added `problem_states` table to the database schema.
        - **Repository Methods**: Implemented `saveProblemState`, `loadProblemState`, and `getLatestProblemState` in `SQLiteMemoryRepository`.
        - **Memory Wrapper**: Exposed the new persistence methods through the `Memory` class.
    - **Verification**: Added `testProblemStatePersistence` to `thoth-unit-tests`; verified successful save/load cycles.

### 2026-03-26 (Ollama Stability & GTK Layout Hardening)

- **Ollama CURL Initialization & Thread-Safety**:
    - **Goal**: Resolve the `Assistant: [Error] Ollama failed: CURL error: Failed initialization` error occurring during concurrent chat/plan execution.
    - **Implementation Details**:
        - **Global Lifecycle**: Added `curl_global_init(CURL_GLOBAL_ALL)` and `curl_global_cleanup()` to `src/main.cpp` ensuring the library is initialized at the process entry point.
        - **Recursive Locking**: Upgraded `LLMInterface` to use a `std::recursive_mutex` to protect the shared CURL handle, preventing deadlocks when internal methods (like `detectOllamaModel`) are called from within an active query lock.
        - **Build Linkage**: Updated the root `CMakeLists.txt` to explicitly find and link `CURL` to the main executable.
- **GTK Layout Assertion Resolution**:
    - **Goal**: Eliminate persistent `Gtk-CRITICAL` assertions regarding negative size allocations and scrollbar distribution failures.
    - **Implementation Details**:
        - **Vertical Decoupling**: Wrapped the right-side observability panels in a `wxScrolledWindow`. This prevents the "stacked list" effect from forcing negative heights when the main window is resized.
        - **Theme-Aware Sizing**: Replaced hardcoded button dimensions (e.g., `24x24`) with `wxDefaultSize` across the Sidebar, Goal Banner, and Chat Bubbles to satisfy GTK's theme-specific padding (extents) requirements.
        - **Padding Optimization**: Reduced `BUBBLE_PADDING` and added `SetMinSize` guards to all `wxDataViewCtrl` and `wxTreeListCtrl` widgets to ensure they never collapse to zero.
        - **AUI Headroom**: Increased the `MinSize` of the bottom notebook to `250px` and the right sidebar to `350px`.
- **Trajectory Viewer Implementation & Data Fallback**:
    - **Goal**: Wire up the `TrajectoryViewer` to display real execution history from SQLite.
    - **Implementation Details**:
        - **Unified Tree-List**: Swapped stacked list controls for a single `wxTreeListCtrl`, providing a hierarchical view (Goal -> Steps) with persistent column labels.
        - **Full Stack Retrieval**: Implemented `getAllEpisodeSteps()` through the entire system (`MemoryRepository` -> `BasicAgentPlugin` -> `AgentInterface`).
        - **Inferred Trajectories**: Added a fallback aggregation layer that groups raw `episode_steps` by ID if the consolidated `trajectories` table is empty, ensuring the UI always reflects recent activity.

### 2026-03-22 (Research Console & Stability Hardening)

- **Cognitive Spine Thread-Safety (ExecutiveController)**:
    - **Goal**: Resolve the "Thread-Safety Vacuum" causing UI crashes and state corruption due to concurrent access between UI (main) and Agent (background) threads.
    - **Issues Resolved**:
        - Unprotected public methods: `transition_to`, `update_goal_embedding`, `update_trajectory_embedding`, `dispatch_step`, `set_workflow_engine`.
        - Reference leakage: `get_current_plan()` returning `const Plan&`.
        - Inconsistent internal locking leading to race conditions and deadlocks.
        - Data races on GRAG embeddings (Goal, Current, Trajectory).
    - **Implementation Details**:
        - **Mutex Protection**: Applied `std::lock_guard<std::mutex>` across all shared state access in `ExecutiveController`.
        - **Snapshot Inspection**: Switched `get_current_plan()` to return by value (snapshot), ensuring UI thread memory safety.
        - **Deadlock-Free Internal Transitions**: Implemented `_unlocked` method variants for safe internal calls within the state machine's lock.
        - **Execution Reliability**: Fixed `execute_goal` execution loop start logic and joined stale threads to prevent leaks.
    - **Verification**: Successfully recompiled and passed full suite of unit tests including parallel tool batching and reflection cycles.
- **UI Restoration & Observability Hardening**:
    - **Menu Bar Implementation**: Restored the missing `SetupMenuBar` and implemented full handlers for File, Agent, Tools, Benchmarks, View, and Help menus.
    - **Plan Execution Panel**: Implemented a new vertical `PlanExecutionPanel` that exposes the real-time state of the `ExecutiveController`, showing current goal and ordered step statuses.
    - **GRAG Diagnostic Fixes**: Resolved a bug where GRAG diagnostics showed zero values by ensuring the `Config` constructor properly initializes retrieval weights ($w_q, w_d, w_t, w_k, w_g$) and the `grag_directional` toggle.
    - **Executive Control Integration**: Exposed `pause`, `resume`, `abort`, and `executeGoal` methods in `AgentInterface`, allowing direct control of the agent's cognitive loop from the UI.
    - **Contextual Explanation Buttons**: Replaced the generic `?` button with explicit `Explain Retrieval` and `Explain Plan` actions, and added a `Revise` button to the goal banner for dynamic goal steering.
- **RAG Indexer Stability & Parallelization**:
    - **Goal**: Decouple the indexing lifecycle from the cognitive loop to prevent system-wide deadlocks and multi-minute blocking during large file batch processing.
    - **Implementation Details**:
        - **Asynchronous Architecture**: Refactored `IndexManager` with a background worker thread and a thread-safe task queue. Added `indexFileAsync` and `indexProjectAsync`.
        - **CURL Handle Pool**: Upgraded `EmbeddingEngine` from a single handle to a thread-safe pool, allowing concurrent embeddings for indexing and real-time chat queries.
        - **Stateless Retrieval**: Hardened `RAGPipeline::retrieveRelevant` to support passing embeddings as arguments, removing reliance on shared internal state and reducing mutex contention.
        - **Granular Locking**: Optimized `IndexManager` to release `chunksMutex` during heavy embedding phases, allowing concurrent reads during long-running write operations.
    - **Verification**: Confirmed fix with successful project-wide compilation and verified background worker lifecycle.
- **Benchmark Integrity & Reliability**:
    - **Mock Data Isolation**: Modified `testBenchmarkReporter` in `tests/unit_tests.cpp` to prevent unit tests from polluting the production `benchmark_results.md` with hardcoded mock data.
    - **Benchmark Sampling**: Added a `--sample` flag to the `run_grag_benchmark` tool, allowing for rapid verification of real retrieval performance on a subset of test cases without environment timeouts.
    - **Log Sanitization**: Cleaned `docs/benchmark_results.md` to remove duplicate mock entries, restoring the document as a reliable source of actual research performance history.
- **Research-Oriented UI Transformation**:
...
    - **3-Column Main Workspace**: Implemented a professional layout with Knowledge Base (Left), Central Chat/Plan (Center), and Diagnostics/Strategy (Right) using `wxAuiManager`.
    - **System State Tray**: Added a tabbed bottom notebook providing dedicated views for RAG context management, Trajectory history, Experiment Lab, Knowledge Graph statistics, and live decision logs.
    - **Structured Tool Rendering**: Upgraded chat bubbles to detect and render JSON-encoded tool results (e.g., diff viewers for `code_modify`, web previews for `web_scrape`).
- **Cognitive Reliability & Persistence**:
    - **Stateful Resumption**: Fixed `checkResumablePlan` to fully restore the agent's internal state (goal, plan, embeddings) upon session activation, ensuring continuity across window reloads.
    - **Session-Aware Event Routing**: Added `session_id` to the `ControllerEvent` system, allowing the UI to accurately save goals and plans to background sessions even if the user switches chats.
    - **Cold-Start GRAG**: Optimized `GragScorer` to activate directional retrieval from Step 1 by treating empty current state as a zero vector, preventing zeroed diagnostics at plan initiation.
- **Stability & Memory Hardening**:
    - **Thread Pool Architecture**: Refactored `AgentInterface` from a leaking thread-per-operation model to a single persistent worker thread with a task queue.
    - **Micro-Batch Indexing**: Implemented batching (size 10) in `IndexManager` and `EmbeddingEngine` to eliminate memory spikes and `std::bad_alloc` crashes during large file indexing.
    - **SQLite Safety**: Deployed a `safe_col_text` helper across all repository methods to prevent crashes when encountering NULL database columns.
    - **UI Resource Guards**: Implemented character limits for chat messages (50k), generic tool outputs (5k), and log tailing (10k) to ensure smooth rendering of massive agent outputs.
    - **Window Lifetime Checks**: Added validity guards in all asynchronous `CallAfter` callbacks to prevent segmentation faults during application shutdown.

### 2026-03-16 (UI & Diagnostics Enhancements)

- **Menu Bar Integration**: Added the requested File/Agent/Tools/Benchmarks/View/Help menu structure with placeholder bindings so the UI now exposes the agent controls, benchmarking hooks, and visibility toggles while keeping the core window responsive. citeturn0exec0
- **Goal Continuity**: Persisted the active goal inside each `Thoth::ChatSession`, saved/loaded it with `chat_sessions.json`, and refreshed the banner on activation or after plan events so the goal stays visible across restarts. citeturn0exec1
- **GRAG Diagnostics Fixes**: Normalized the diagnostics payload (nested `result` & `diagnostics` blobs), restored the retrieved-chunks score column, and added percentage formatting so alpha/magnitude/scoring type reflect the telemetry instead of defaulting to zero. citeturn0exec1

### 2026-03-12 (Adaptive Graph Learning Final Implementation)

- **Adaptive Graph Learning (Phase 5.6)**:
    - **Stable Chunk Identity**: Implemented content-based hashing (SHA256) for chunks to ensure graph associations survive project re-indexing.
    - **Schema Hardening**: Updated SQLite schema to track edge weights, success/failure counts, and last-used timestamps for dormancy detection.
    - **Causal Linking**: Modified `ExecutiveController` to capture the "Active Set" of chunks (Top 5 with 0.8 relative threshold) and link them across successful step transitions.
    - **Logistic Reinforcement**: Deployed `GraphRefiner` with a logistic learning rule ($W_{new} = W + lr \times (1-W)$) to saturate weights and prevent numerical explosion.
    - **One-Hop Activation**: Upgraded `GragScorer` to perform 1-hop neighbor activation from high-confidence query hits ($Q_{sim} \geq 0.7$), ensuring experience-guided retrieval without noise.
    - **Multi-Tier Forgetting**: Integrated a global decay mechanism ($0.995$) with an additional dormancy penalty ($0.97$) for edges unused for > 30 days.
    - **Observability**: Expanded retrieval diagnostics to include `graph_source_node`, `graph_activations`, and contribution metrics in `grag_benchmark.jsonl`.

## Summary

- **Tool Confirmation System**: Implemented `requires_confirmation()` across the `ITool` interface and all 9 tools, enforcing user approval for risky operations.
- **Global Security Enforcement**: Integrated `ConstraintChecker` into the main chat loop (`CommandProcessor`), ensuring security policies apply to both standard interaction and goal execution.
- Completed project rebranding (Thoth), SQLite episodic memory migration, and full Tool System integration.
- Implemented Dynamic Planning (Phase 9) with LLM-generated, variable-length plans and robust JSON parsing.
- Verified Resume Completeness (Phase 10), ensuring the system can fully reconstruct and resume plans from trace logs.
- Implemented Dynamic Plan Revision (Phase 11), allowing the LLM to automatically repair plans after step failures.
- Implemented Extended Agent & Cognitive Spine (Phase 12), re-enabling all tools and verifying the full goal-execution lifecycle with an end-to-end integration test.
- Implemented Self-Building Capability (Phase 3 of improvements.md) — Prototype harness established for analyzing and testing the codebase; unified diff application (`apply_diff`) is currently a non-functional stub.
- **Semantic Transition & Retrieval Hardening (embedding_fix.md Phase 1-4)**:
    - **Dense Semantic Embeddings**: Integrated Ollama REST API (`/api/embed`) with `nomic-embed-text` support.
    - **Metric Hardening**: Implemented nDCG@5 and Directional Rank Lift to detect subtle ranking improvements.
    - **Hybrid Reranking**: Developed a dynamic blend of semantic and keyword-based TF-IDF scoring.
    - **Engineering Safeguards**: Added model versioning, automated re-indexing on metadata mismatch, and robust batching.
    - **Weight Optimization**: Conducted an automated weight sweep to identify optimal hybrid parameters ($w_q=0.4, w_d=0.4, w_k=0.3$).
- **Retrieval Stabilization & Observability (embedding_fix.md Phase 5-7)**:
    - **Config Locking**: Moved optimized weights ($w_q=0.4, w_d=0.4, w_k=0.3$) into `agent_workspace/retrieval_config.json`.
    - **Auto-Archiving**: Updated `BenchmarkReporter` to automatically record all benchmark runs in `docs/benchmark_results.md`.
    - **Glass Box Retrieval**: Refactored `GragScorer` to provide a detailed `ScoreBreakdown` (Query, Goal, Trajectory, Keyword, Graph) for every retrieved chunk.
    - **Reranking Optimization**: Expanded the initial retrieval pool to 40 candidates to give GRAG more semantic headroom before narrowing to the final top 5.
- Implemented Advanced Tool Orchestration (Phase 5 of improvements.md), including parallel step execution (Tool Batching), a production-grade `web_scrape` tool, and expanded Gmail integration (`gmail_read_messages`).
- Implemented Reasoning & Self-Correction (Phase 6 of improvements.md), featuring a `self_correct` reasoning tool, a global `ConstraintChecker` for security policy, and an autonomous `Reflection Loop` for goal recovery.
- **UI Enhancement**: Added individual "X" delete buttons to the RAG file slots, allowing for granular context management.
- Tool System now supports runtime dispatch, automatic prompt injection, and structured trace logging.
- Implemented and stabilized ExecutiveController with thread-safety and GRAG wiring.
- **GRAG Cognitive Retrieval (Phases 1–8)**:
    - **Foundation (Phases 1–2)**: Implemented `GragScorer`, adaptive $\alpha$ blending, and directional $G - C$ embeddings.
    - **Intelligent Routing (Phase 3)**: Added mode-based routing (PLAN_AWARE, GOAL_ONLY, CONVERSATIONAL) with automated fallback.
    - **Structured State (Phase 4)**: Migrated to JSON-structured state embeddings (v2) to prevent semantic drift.
    - **Experience Reuse (Phase 5)**: Implemented Plan History Reuse, automatically injecting past successful approaches into the planner.
    - **Index Lifecycle (Phase 6)**: Added selective re-indexing with file fingerprinting and metadata tracking.
    - **Empirical Evaluation (Phase 7)**: Integrated structured benchmarking (`grag_benchmark.jsonl`) and high-level success metrics.
    - **Relational Context (Phase 8)**: Deployed a prototype Graph Memory layer in SQLite for causal link retrieval.
- **Memory Stability (Phase 4 of roadmap)**:
    - **Fact Store**: Implemented a persistent `FactStore` for structured world knowledge and a corresponding `store_fact` tool.
    - **Vector Abstraction**: Refactored RAG to use `IVectorStore`, enabling future migration to professional vector databases.
- **Advanced Reasoning (Phase 5 of roadmap)**:
    - **Scientific Mode**: Implemented `ScientificExecutionMode` using the Strategy Pattern for iterative hypothesis testing.
    - **Strategy Engine**: Added `StrategyEngine` to process execution trajectories and optimize future plan generation.
- Hardening, security, CI, and RAG index lifecycle improvements completed in previous sessions.

## Completed Work

### 2026-03-10

- **Strict Sandbox Boundaries**:
    - **Enforcement**: Modified `IndexManager` to hard-reject and log any indexing requests outside `/home/steve/Thoth/agent_workspace/`.
    - **Bootstrap Security**: Updated `CommandProcessor::ensureInitialized` to bootstrap RAG exclusively from the `agent_workspace/rag/` sandbox rather than the project root.
    - **Benchmark Hardening**: Rewrote `run_grag_benchmark.cpp` to use a strictly sandboxed corpus and added path-guard logic to prevent accidental directory traversal.
- **External Corpus Integration (Research Papers)**:
    - **New Corpus**: Replaced project documentation with 5 high-density AI research papers (ReAct, RAG, MemGPT, Generative Agents, CoT) as the primary retrieval benchmark.
    - **Test Suite Expansion**: Designed 30 new test cases (10 per type) specifically targeting semantic overlaps between papers (e.g., "memory" in MemGPT vs. Generative Agents).
    - **Retrieval Hardening**: 
        - **Chunk Optimization**: Reduced target chunk size to 2048 chars and added a hard 8000-char truncation guard to prevent Ollama context window errors.
        - **Reliability**: Forced `127.0.0.1` for Ollama connections to prevent IPv6 resolution failures.
        - **Cold Start Fix**: Updated `IndexManager` to train the local TF-IDF engine's vocabulary before chunking, ensuring valid keyword scores even when semantic embeddings are delayed.
    - **Empirical Validation**:
        - **Goal Disambiguation**: Confirmed a massive **+0.216 nDCG@5 lift**, proving that GRAG's directional steering is highly effective at selecting the correct paper when terminology overlaps.
        - **Overall Mean RR**: Improved from 0.587 to 0.647.
        - **Stability**: Verified that the sandbox boundary is strictly enforced across the research corpus.

### 2026-03-09

- **Reranking Optimization (Phase 7)**:
    - **Candidate Expansion**: Modified `RAGPipeline` to fetch 40 candidates in the initial recall phase, allowing more semantic signal to be processed during rescoring.
    - **Verification**: Verified the multi-stage pipeline via the benchmark runner.
- **Observability & Explainable AI (Phase 6)**:
    - **Explicit Score Data Structure**: Introduced `ScoreBreakdown` to capture all retrieval signals independently.
    - **Transparency Hardening**: Modified the rescoring loop to preserve and sort individual signal scores, enabling "Why this document?" explainability.
- **Retrieval Stabilization & Observability (Phase 5)**:
    - **Empirical Weight Locking**: Optimized weights from Phase 4 are now stored in a persistent JSON config and synchronized between the global `Config` and `RAGPipeline`.
    - **Automated Performance Tracking**: Every benchmark run now appends a human-readable Markdown summary to `docs/benchmark_results.md`, including Precision, MRR, and nDCG metrics.

### 2026-03-05

- **GRAG Phase 3–8 Implementation**:
    - **Multi-Index Routing (Phase 3)**: Implemented `GragRoutingMode` logic to automatically switch between PLAN_AWARE, GOAL_ONLY, and CONVERSATIONAL retrieval based on agent state.
    - **Structured State (Phase 4)**: Enforced a strict JSON schema for state embeddings to maintain semantic focus. Completed SQLite migration to Version 2.
    - **Plan History Reuse (Phase 5)**: Wired `MemoryRepository` to retrieve past successful plans by directional similarity, reducing planning latency for recurring tasks.
    - **Index Lifecycle (Phase 6)**: Implemented `shouldReindexFile` fingerprinting to skip re-indexing unmodified source files during startup.
    - **Metrics Harness (Phase 7)**: Enabled per-retrieval diagnostic logging and established the initial performance baseline.
    - **Graph Memory Prototype (Phase 8)**: Built the SQLite `graph_nodes` and `graph_edges` schema with hybrid scoring integration in `GragScorer`.
- **Advanced Core Architecture (Roadmap Phase 4-5)**:
    - **Memory Stability (Phase 4)**: Deployed the `FactStore` and `IVectorStore` abstractions, decoupling RAG from the file-based prototype index.
    - **Scientific Reasoning (Phase 5)**: Implemented `ScientificExecutionMode` and `ProblemState` for iterative hypothesis evaluation. Verified convergence halting in unit tests.

### 2026-03-08
...
