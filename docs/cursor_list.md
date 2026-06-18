# Thoth Working Backlog

**Last updated:** 2026-06-18  
**Purpose:** Active todo list for the next development sessions. Specs live in `improvements.md`; finished work is logged in `completed_improvements_log.md`.

**Baseline locked:** P0–P2 alignment pass complete (2026-06-17) — reflection test green, P1 security/plan-reuse/pruning shipped, docs corpus-qualified. Details in `completed_improvements_log.md`.

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
| Plan history reuse + observability events | ✅ |
| `allow_shell_exec` tool gating | ✅ |
| Memory hot-tier prune + session scoping + GUI trim | 🔶 core wired; warm/cold tier open |
| Trajectory $w_t$ in scoring path | 🔶 config 0.2; mixed lift on trajectory-disambiguation cases |
| Unit tests | ✅ full suite green (~78s, 2026-06-16) |
| Doc alignment P2.1–P2.6 | ✅ |

---

## Active backlog

### 1. Verification & audit refresh

| ID | Task | Status | Notes |
|----|------|--------|-------|
| **V1** | Re-run `TEST_SUITE.md` TC-01–TC-07 manually | 🔶 | **Automated:** `ctest` + audit grep ✅ 2026-06-18. **Manual GUI pass:** pending (Ollama + app) |
| **V2** | Refresh `audit.md` | ✅ | 2026-06-18 — shell gate fixed, portable paths, verification checklist |
| **V3** | Re-upload **`MYPAPER.md`** to Zenodo | ⏸️ | Deferred — more benchmark runs first |

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
| **G1** | Trajectory tuning (Step 4.5) | 🔶 | $w_t=0.2$ active; `TRAJECTORY_DISAMBIGUATES` bucket showed **−0.037** in one run — see `plan_reuse_tuning.md`, `benchmark_results.md` |
| **G2** | Hierarchical subgoal trees (Step 4.4) | 📋 | `GoalNode`, active subgoal embedding in `GragScorer`; single root $G$ today |
| **G3** | Model upgrade path (Step 4.7) | 📋 | Audit `LLMInterface` for model-specific assumptions; `ModelConfig` + migration playbook |
| **G4** | Trace replay vs SQLite resume | 🔶 | Document-only acceptable: trace is observability; `resume_from_plan()` is authoritative |

**Done already:** Scientific mode, `StrategyEngine`, `GraphRefiner`, `TrajectoryBuilder` wiring.

---

### 4. Benchmarks & automated testing

| ID | Task | Source | Notes |
|----|------|--------|-------|
| **B1** | Research-paper corpus: 30 hardened cases | `new_corpus_tests.md` | 6 per paper × 3 case types; per-paper and per-type breakdown |
| **B2** | Automate critical manual suite signals | `TEST_SUITE.md`, `TESTING.md` | Integration tests for `routing_mode`, alpha, RETRIEVAL-before-LLM ordering |
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
Done    V2 — audit.md refreshed (2026-06-18)
Done    V1 partial — ctest + audit grep (2026-06-18)
Next 1   V1 manual — TEST_SUITE TC-01–TC-07 with GUI (combine with benchmark score runs)
Next 2   M1–M4 — finish memory pruning pipeline
Next 3   G1 — trajectory benchmark tuning (plan_reuse_tuning.md)
Next 4   B1 — 30-case research-paper GRAG benchmark (new_corpus_tests.md)
Next 5   B2 + B4 — integration tests for manual suite + clean warnings
Next 6   G2 or G3 — subgoal trees OR model upgrade path (pick one phase boundary)
Later    Tier 6 UI polish
Last     Tier 7 self-building / apply_diff (owner discretion)
External V3 — Zenodo MYPAPER re-upload when ready
```

---

## Quick reference — partial vs not started

| Item | Today |
|------|-------|
| Hot-tier memory prune | ✅ wired |
| Warm/cold prune + restore | 📋 |
| Vector store benchmarks | 📋 |
| Trajectory $w_t$ tuning | 🔶 active, needs benchmark win on trajectory cases |
| Hierarchical subgoals | 📋 |
| NODE runtime | 📋 spec only |
| Manual TEST_SUITE re-verified | 🔶 automated 2026-06-18; GUI TC pending |
| `audit.md` current | ✅ 2026-06-18 |
| Self-building / `apply_diff` | 🚫 stub — **last** |

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
