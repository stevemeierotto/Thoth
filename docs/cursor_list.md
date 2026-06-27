# Thoth Working Backlog

**Last updated:** 2026-06-27  
**Purpose:** Active todo list for the next development sessions. Specs live in `improvements.md`; finished work is logged in `completed_improvements_log.md`.

**Baseline locked:** Headless cognitive loop verified — `run_test_suite` **TC-01–TC-07 all pass** (2026-06-27) with real `executeLLM`, RETRIEVAL→LLM plans, and GRAG scoring. Prior P0–P2 alignment (2026-06-17) in `completed_improvements_log.md`.

---

## How to use

1. Work **top to bottom** — finish internals before UI polish or self-building.
2. After code changes: `cmake --build --preset build-debug` and `ctest --output-on-failure`.
3. Mark items done only when implemented **and** logged in `completed_improvements_log.md`.
4. When docs conflict: **code** → `audit.md` + `completed_improvements_log.md` → specs (`GRAG.md`, `PLAN.md`, …) → `improvements.md` → narrative (`README.md`, Zenodo papers).

---

## What's solid (do not re-implement)

| Area | Status |
|------|--------|
| GRAG directional core + graph learning | ✅ |
| Executive loop, reflection, scientific mode, strategy promotion | ✅ |
| `executeLLM` (real synthesis, not stub) | ✅ 2026-06-27 |
| Headless `run_test_suite` TC-01–TC-07 | ✅ 2026-06-27 (~40 min; Ollama required) |
| Production plan templates (RETRIEVAL → LLM) + plan-reuse strip | ✅ 2026-06-27 |
| Plan history reuse + observability events | ✅ |
| `allow_shell_exec` tool gating | ✅ |
| Memory hot-tier prune + session scoping + GUI trim | 🔶 core wired; warm/cold tier open |
| Trajectory $w_t$ in scoring path | 🔶 config 0.2; mixed lift on trajectory-disambiguation cases |
| Unit tests | ✅ full suite green (~78s, 2026-06-16) |
| Doc alignment P2.1–P2.6 | ✅ |

---

## Active backlog

### 0. Cognitive loop hardening — **work now**

End-to-end goal execution works; next focus is **quality, speed, and evidence** — not wiring.

| ID | Task | Status | Notes |
|----|------|--------|-------|
| **C1** | **Improve planning quality** | 📋 | Reduce bad plans (wrong step types, missing `depends_on`, Gmail/tool scaffolds). Tune `plan_generation.tmpl`, plan-reuse injection limits, strategy/trajectory noise; validate on GUI goals outside test corpus. |
| **C2** | **Improve retrieval ranking** | 📋 | GRAG lift on real goals: weight tuning (`retrieval_config.json`), chunk quality, multi-index routing. Extend hardened benchmark beyond 3-file test corpus; track nDCG@5 / goal-disambiguation bucket per `benchmark_results.md`. |
| **C3** | **Measure reflection outcomes** | 📋 | Reflection fires on low trajectory score — prove it helps. Compare success rate / answer quality with reflection on vs off (or max cycles 0 vs 2); log before/after metrics; avoid infinite replan on LLM timeout failures. |
| **C4** | **Developer & CI latency** | 📋 | Fast feedback for engineers — see **§ C4/C7**. ~40 min `run_test_suite` is the signal. Mock LLM, tiny corpus, cached embeddings, skip re-index, dev vs full regression tiers. |
| **C5** | **Robustness & failure tests** | 📋 | Unusual/failure scenarios: Ollama unreachable, empty RAG corpus, malformed plan JSON, step timeout, reflection exhaustion, plan parse retry, concurrent goals. Add to `unit_tests.cpp` or a `run_robustness_suite` with `THOTH_MOCK_*` gates. |
| **C6** | **Cognitive metrics** | 📋 | Per-goal quantitative record + time-series analysis — see **§ C6** below. Feeds **C3**, **C4**, **C7**, papers/benchmarks. |
| **C7** | **Runtime latency** | 📋 | Production goal execution speed — see **§ C4/C7**. Parallel retrieval, prompt size, batching, inference settings, plan scheduling, LLM step timeouts vs hardware. |

**Dependencies:** C3 benefits from C1/C2 (reflection currently retriggers on failed LLM/timeouts, not just bad answers). **C4** and **C7** are independent — do not mix mock-fast-CI work with runtime optimization. **C6** should start early (append-only logging) and deepen as C1–C7 land — metrics make every subsequent tuning iteration measurable.

#### C4 & C7 — Latency (developer vs runtime)

Two different engineering problems; separate solutions and success criteria.

| | **C4 — Developer & CI** | **C7 — Runtime / production** |
|--|---------------------------|-------------------------------|
| **Goal** | Fast feedback while coding; cheap regression in CI | Faster real goal execution for users |
| **Examples** | `THOTH_MOCK_LLM`, tiny test corpus, cached embeddings, no re-index when paths unchanged, `run_test_suite` dev tier vs full tier, parallel ctest | Parallel retrieval where safe, prompt/context trimming, step batching, Ollama `num_predict` / model choice, ExecutiveController scheduling, embedding batch API |
| **Measures** | CI wall time, developer iteration loop | Per-goal `total_wall_clock_ms`, step latencies (via **C6**) |
| **Risk** | Mocks drift from production behavior — keep full Ollama path as nightly/regression | Optimizations must not break GRAG quality (**C2**) or plan correctness (**C1**) |

#### C6 — Cognitive metrics

Move beyond pass/fail: record **quantitative metrics for every goal execution**, persist them append-only, and aggregate over hundreds of runs.

**Per-goal fields (target schema):**

| Metric | Purpose |
|--------|---------|
| `planning_time_ms` | LLM plan generation latency |
| `retrieval_time_ms` | RETRIEVAL step wall time |
| `llm_synthesis_time_ms` | LLM step wall time |
| `retrieved_chunk_count` | Context volume delivered to synthesis |
| `grag_alpha` | Directional blend at retrieval (from GRAG diagnostics) |
| `trajectory_score` | ExecutiveController outcome score |
| `reflection_count` | Reflection replan cycles consumed |
| `final_success_score` | Plan history / past_plans success signal |
| `total_tokens` | LLM token usage (plan + synthesis; embed if available) |
| `total_wall_clock_ms` | Goal start → PLAN_COMPLETED/FAILED |

**Questions metrics should answer over time:**

- Is planning getting faster?
- Is retrieval quality improving (alpha, chunk count, downstream success)?
- Does reflection trigger less often after tuning?
- Did a change increase average latency or token cost?

**Implementation notes:**

- **Partial data today:** `StepMetricsRepository`, `grag_benchmark.jsonl`, `decision_trace.jsonl` (reflection, trajectory events), step `latency_ms` in `WorkflowEngine` — not yet one unified per-goal row.
- **Deliverables:** `cognitive_metrics.jsonl` (or SQLite `goal_metrics` table), emit on `PLAN_COMPLETED` / `PLAN_FAILED`; optional `scripts/plot_cognitive_metrics.py` or export for GUI; wire token counts from `LLMInterface` when available.
- **Consumers:** C3 A/B runs, C4/C7 regression checks, `benchmark_results.md` archive, thesis/paper figures.

---

### 1. Verification & audit refresh

| ID | Task | Status | Notes |
|----|------|--------|-------|
| **V1** | Re-run `TEST_SUITE.md` TC-01–TC-07 | 🔶 | **Headless:** `run_test_suite` ✅ 2026-06-27 (all 7). **Manual GUI pass:** pending (scores panel TC-05, chat UX) |
| **V2** | Refresh `audit.md` | ✅ | 2026-06-18 — shell gate fixed, portable paths, verification checklist |
| **V3** | Re-upload **`MYPAPER.md`** to Zenodo | ⏸️ | Deferred — more benchmark runs first (see **C2**) |

---

### 2. Phase 3 — Memory stability (finish internals)

**Phase status:** 🔶 Partial — `improvements.md` Phase 3

| ID | Task | Status | Notes |
|----|------|--------|-------|
| **M1** | Pruning: summarize-before-archive | 📋 | LLM summary of pruned turns before raw delete; searchable warm tier |
| **M2** | Pruning: age-based trigger | 📋 | Policy from Step 3.1 (e.g. 30-day threshold) |
| **M3** | Pruning: admin `/prune` command | 📋 | Manual trigger + `DecisionTraceLogger` pruning events |
| **M4** | Pruning: `MemoryPruner::restore(session_id, range)` | 📋 | On-demand historical replay; transactional SQLite |
| **M5** | Vector store benchmark scaffold | 📋 | Step 3.4: ingest/latency/memory contract tests at 10k/50k/100k chunks; dual-write stub (disabled) |

**Done already (don't redo):** `FactStore`, `MemoryPruner` hot-tier auto-prune, `IVectorStore` / `FlatVectorStore` wrapper.

---

### 3. Phase 4 — Advanced reasoning & GRAG

**Phase status:** 🔶 Partial — `improvements.md` Phase 4

| ID | Task | Status | Notes |
|----|------|--------|-------|
| **G1** | Trajectory tuning (Step 4.5) | 🔶 | Overlaps **C2** — $w_t=0.2$ active; trajectory-disambiguation bucket **−0.037** in one run |
| **G2** | Hierarchical subgoal trees (Step 4.4) | 📋 | `GoalNode`, active subgoal embedding in `GragScorer`; single root $G$ today |
| **G3** | Model upgrade path (Step 4.7) | 📋 | Audit `LLMInterface` for model-specific assumptions; `ModelConfig` + migration playbook |
| **G4** | Trace replay vs SQLite resume | 🔶 | Document-only acceptable: trace is observability; `resume_from_plan()` is authoritative |

**Done already:** Scientific mode, `StrategyEngine`, `GraphRefiner`, `TrajectoryBuilder` wiring.

---

### 4. Benchmarks & automated testing

| ID | Task | Source | Notes |
|----|------|--------|-------|
| **B1** | Research-paper corpus: 30 hardened cases | `new_corpus_tests.md` | 6 per paper × 3 case types; per-paper and per-type breakdown |
| **B2** | Automate critical manual suite signals | ✅ | `tests/run_test_suite.cpp` + `check_baseline.py` (2026-06-27); extend per **C5** |
| **B3** | Reduce test log noise | unit tests | Repeated `[Memory] Phase 4: Migrating embedding schema v1→v2` per fixture |
| **B4** | Compiler warnings (~14 on debug build) | build output | Unused params in stubs/GUI; sign compare in `PlanExecutionPanel.cpp` |

---

### 5. NODE execution harness

| ID | Task | Status | Notes |
|----|------|--------|-------|
| **N1** | Decide: implement or defer | 📋 | `NODE.md` + `build_node.md` are spec-only; no `/core/node` in repo |
| **N2** | If defer: one-line status in `README.md` / `NODE.md` | 📋 | Ensure nothing claims NODE is operational |

**Default recommendation:** defer until Phase 3–4 memory/GRAG items above are closed.

---

### 6. UI polish (after core stable)

From `ui_improvements.md` §11–§12 — research console shell exists; these are enhancements:

| Priority | Item |
|----------|------|
| 1 | Tool output renderers (`summarize_text` first) |
| 2 | Step Execution debug mode (manual step → observe) |
| 3 | Retrieval Inspector chunk drill-down + alpha/magnitude color cues |
| 4 | Execution Trace unified timeline |
| 5 | GRAG vector visualizer + interactive plan graph |
| 6 | GUI sync test harness (backend writes → panel assertions) |

---

### 7. Self-building — last (optional future expansion)

**Do not start until Tier 2–4 are in good shape.** Phase 5 in `improvements.md` is **not scheduled**.

| ID | Task | Status | Notes |
|----|------|--------|-------|
| **S1** | `code_modify` / `apply_diff` | 🚫 stub | Returns prototype error; read + `project_analyze` + `run_tests` work |
| **S2** | Self-correction loop in production goals | 📋 | Harness exists; not a closed autonomous edit-verify loop |
| **S3** | Phase 5 close-out criteria | 📋 | See `improvements.md` Phase 5 — only after S1–S2 |

---

## Suggested session order

```
Done    Headless TEST_SUITE TC-01–TC-07 (2026-06-27)
Done    executeLLM + plan templates + workflow_engine fixes (2026-06-27)
Done    V2 — audit.md refreshed (2026-06-18)
Done    B2 — run_test_suite + check_baseline (2026-06-27)

Now     C1 — planning quality (GUI goals, reuse pollution, schema adherence)
Next 1  C6 — cognitive metrics logging (append-only; enables all tuning work)
Next 2  C2 — retrieval ranking + benchmark on hardened corpus
Next 3  C3 — reflection A/B measurement (does replan improve outcomes?)
Next 4  C4 — developer & CI latency (fast test path, mocks, slim corpus)
Next 5  C7 — runtime latency (production execution hot paths)
Next 6  C5 — robustness / failure scenario tests
Next 7  V1 manual GUI pass (TC-05 scores panel, chat UX)
Next 8  M1–M4 — finish memory pruning pipeline
Next 9  G1 / G2 — trajectory tuning or subgoal trees
Later   Tier 6 UI polish
Last    Tier 7 self-building / apply_diff (owner discretion)
External V3 — Zenodo MYPAPER re-upload when C2 benchmark stable
```

---

## Quick reference — partial vs not started

| Item | Today |
|------|-------|
| End-to-end cognitive loop (RETRIEVAL → LLM → PLAN_COMPLETED) | ✅ headless 2026-06-27 |
| Headless TEST_SUITE 7/7 | ✅ ~40 min with Ollama |
| Manual TEST_SUITE (GUI) | 🔶 pending |
| Planning / retrieval / reflection quality | 📋 **C1–C3** |
| Per-goal cognitive metrics (time-series) | 📋 **C6** |
| Developer / CI latency (fast tests) | 📋 **C4** |
| Runtime latency (production goals) | 📋 **C7** |
| Robustness test coverage | 📋 **C5** |

---

## Doc map (where to read)

| Need | File |
|------|------|
| Full phase specs | `improvements.md` |
| What's actually shipped | `completed_improvements_log.md` |
| GRAG implementation truth | `GRAG.md`, `benchmark_results.md` |
| Cognate / executive truth | `cognate.md`, `PLAN.md` |
| GRAG paper (Zenodo) | `MYPAPER.md` |
| Cognate paper (Zenodo) | `COGNATE_V2.md` |
| Manual test protocol | `TEST_SUITE.md`, `TESTING.md` |
| UI backlog | `ui_improvements.md` §11–§12 |

---

*Update this file when starting or finishing a tier. Append summaries to `completed_improvements_log.md`; do not mark phases complete in `improvements.md` until close-out criteria pass.*
