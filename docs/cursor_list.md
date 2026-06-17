# Thoth Alignment Checklist — Cursor Fresh Start

**Created:** 2026-06-13  
**Last updated:** 2026-06-17  
**Purpose:** Prioritized list of fixes needed so code, tests, and documentation match the claims made across `docs/`, thesis papers, and `AGENTS.md`. Use this as the working backlog when returning to the project.

**Status:** P0 and P1 resolved; P2.1–P2.6 doc alignment done (2026-06-17). **Phase 5 self-building = optional future expansion.**

**How to use:**
1. Work top-to-bottom by priority tier.
2. After each code fix: `cmake --build --preset build-debug` and `ctest --output-on-failure`.
3. After each doc fix: update `completed_improvements_log.md` only when work is actually done (do not mark complete prematurely).
4. Treat `completed_improvements_log.md` + `audit.md` as the honesty layer; narrative docs (`README.md`, Zenodo papers) may lag — check `cursor_list.md` first.

---

## Priority 0 — Regressions & Broken Verification

**All items resolved 2026-06-13.** See `completed_improvements_log.md` (2026-06-13 entry).

### ✅ P0.1 — `testReflectionLoop` (RESOLVED 2026-06-13)

| | |
|---|---|
| **Was** | `./tests/thoth-unit-tests` failed: `testReflectionLoop: failed to trigger reflection (call count: 1)`. |
| **Root cause** | **Test design flaw**, not an ExecutiveController regression. The mock plan used only `StepType::LLM` steps; `WorkflowEngine::executeLLM()` always succeeds → trajectory score **1.0** → reflection threshold (**0.6**) never reached. |
| **Fix applied** | First mock plan uses a failing `StepType::NODE` step; reflection replan uses `LLM`. Test strengthened to assert `Reflection:` goal text, two `PLAN_CREATED` events (with async polling), `COMPLETED` state, and plan id `reflection-plan-2`. Documented in `TESTING.md`. |
| **Controller note** | `basic_agent` scoring hardening (`calculate_trajectory_score(bool)`) was already on `main`; no additional production change required for this failure mode. |

### ✅ P0.2 — Unit test suite green (RESOLVED 2026-06-13)

| | |
|---|---|
| **Was** | One failing test blocked “baseline locked” claims. |
| **Fix applied** | After P0.1, `ctest --output-on-failure` passes 100% (~64s locally). |
| **Remaining** | Optional: add “last verified” date to `VERIFIED_BASELINE.md`; wire `ctest` into CI (see P3.3 / P2.2). |

---

## Priority 1 — Code vs. Documented Capabilities (Core Claims)

These are features described as complete (or thesis-critical) but are stubbed, disabled, or not wired.

### ✅ P1.1 — Trajectory-aware GRAG (`w_t`) (RESOLVED 2026-06-15, Option A)

| | |
|---|---|
| **Was** | `agent_workspace/retrieval_config.json` set `"trajectory": 0.0` despite code and docs assuming $w_t=0.2$. |
| **Fix applied** | Activated `trajectory: 0.2` in local `retrieval_config.json` (gitignored runtime config). Full GRAG benchmark: overall nDCG **+0.042**; `TRAJECTORY_DISAMBIGUATES` bucket **−0.037** — see `benchmark_results.md` and `plan_reuse_tuning.md`. |
| **Remaining doc fix** | Align `GRAG.md` §3 audit note and `AGENTS.md` status with active $w_t$; note mixed trajectory-case lift. |

### P1.2 — `code_modify` / `apply_diff` stub — **FUTURE EXPANSION (not scheduled)**

| | |
|---|---|
| **Priority** | **None for now** — Self-building is an optional idea the owner may try later; not on the active roadmap. |
| **Reality** | `code_modify_tool.cpp` returns `"Unified diff application not fully implemented in v1.0 prototype."` Read + `project_analyze` + `run_tests` work. |
| **If resumed later** | Implement unified diff apply per `improvements.md` Phase 5 spec, or keep stub and document as experimental only. |
| **Doc stance** | Mark Phase 5 as future expansion everywhere; do not imply self-building is in progress. |

### ✅ P1.3 — Plan history reuse (RESOLVED 2026-06-15)

| | |
|---|---|
| **Was** | `Memory::retrieveSimilarPlans()` returned `{}`; injection never fired. |
| **Fix applied** | Embedding-based retrieval over `past_plans` (v2); executive + reflection injection; observability events; `docs/plan_reuse_tuning.md`. Deadlock fix (2026-06-16): observability no longer calls `emit_event()` under `mutex_`. `ctest` 100% green (~78s). |

### P1.4 — Hierarchical subgoal trees not implemented

| | |
|---|---|
| **Claim** | Referenced as planned in `GRAG.md`, `improvements.md` Step 4.4, `README.md` roadmap. |
| **Reality** | Single root goal embedding for entire plan duration (`GRAG.md` audit note is correct). |
| **Fix** | Code: implement `GoalNode` tree per roadmap **or** doc-only: ensure all “complete” docs do not imply this exists. |
| **Doc fix** | `cognate.md` §5 “Future Frontiers” is accurate; `README.md` “Planned” row is accurate; no change needed unless something claims it is done. |

### ✅ P1.5 — `allow_shell_exec` bypass (RESOLVED 2026-06-15)

| | |
|---|---|
| **Was** | `run_tests` and `code_modify build` used `popen` without checking `Config::allow_shell_exec`. |
| **Fix applied** | Tools gate on config via `ToolRegistry::setConfig()`; unit test `testAllowShellExecGate`. Default remains **deny** unless `config.json` enables shell. |
| **Remaining doc fix** | Move from “known gap” to “fixed” in `audit.md` and `architectural_facts.md`. |

### ✅ P1.6 — Memory pruning not integrated into runtime (RESOLVED 2026-06-16)

| | |
|---|---|
| **Was** | `MemoryPruner` class existed but was never constructed or called; `Memory::activeSessionId` stuck on `default_session`; `chat_sessions.json` grew unbounded. |
| **Fix applied** | `Memory::setActiveSessionId()` wired from plugin; `MemoryPruner` init + auto-prune after `addMessage()`; shared limits in `memory_pruning_config.h`; GUI trims sessions on load/save to hot-tier cap (50). |
| **Remaining** | `summarize_before_pruning`, age trigger, `/prune` admin command, range-based restore — deferred. |

### P1.7 — NODE runtime not built

| | |
|---|---|
| **Claim** | `NODE.md`, `build_node.md`, `AGENTS.md`: NODE as execution harness / future PlanStep type. |
| **Reality** | Specification and build guide only; no `/core/node` implementation in repo. |
| **Fix** | Either start `build_node.md` Phase 1 **or** keep as future work and ensure no doc says NODE is operational. |
| **Doc fix** | `README.md` describes NODE accurately; `NODE.md` is spec only. |

---

## Priority 2 — Documentation Consistency & Missing Files

Fix doc drift so the next session does not read conflicting sources.

### ✅ P2.1 — Broken / missing doc references (RESOLVED 2026-06-17)

| | |
|---|---|
| **`grag_summery.md`** | Removed broken reference; `improvements.md` rule 3 now points to `GRAG.md`, `architectural_facts.md`, `completed_improvements_log.md`, `benchmark_results.md`. |
| **`basic_agent_design_goal.md`** | Phase 12 in `improvements.md` now references `VERIFIED_BASELINE.md` + `PLAN.md`. |
| **`COGNATE.md` / `cognate.md`** | No stub added — canonical filename is `cognate.md`; listed in `INDEX.md`. |
| **`memory_summery.md`** | Renamed to `memory_summary.md`; link updated in `improvements.md`. |

### ✅ P2.2 — Stale “verified” documents (RESOLVED 2026-06-17)

| | |
|---|---|
| **`VERIFIED_BASELINE.md`** | Supersession banner; §4 scientific mode updated; §5 marked pre–Cognate V2 historical. |
| **`architectural_facts.md`** | Rewritten: current thread-safety status, removed agent monologue; kept audit facts §1–§8. |
| **`AGENTS.md`** | Last updated 2026-06-17; GRAG trajectory + Current System Status refreshed (P1, Cognate V2). |
| **`INDEX.md`** | Roadmap wording updated; points to `cursor_list.md` for next-work priority. |

### ✅ P2.3 — `improvements.md` phase table vs `completed_improvements_log.md` (RESOLVED 2026-06-17)

| | |
|---|---|
| **Was** | Phases 3–5 all 📋 Planned despite substantial completion. |
| **Fix applied** | Phase table + per-step rollups; status headers on each step; close-outs marked partial with ✅/🔶/📋 checklist. |

### ✅ P2.4 — Overclaiming in narrative docs (RESOLVED 2026-06-17)

| | |
|---|---|
| **`README.md`** | Known gaps table; Cognate benchmark 0.00\* caveat; Phase 3 partial; removed "self-building loop" wording. |
| **`COGNATE_V2.md`** | Fixed "depeloth" → "depth"; mock-environment footnote for 51× metric (Zenodo — owner approved). |
| **`MYPAPER.md`** | §3.4 Adaptive Graph Learning marked **Implemented** (`GraphRefiner`). |
| **`cognate.md`** | §5 updated: graph learning done; self-building = optional future expansion. |
| **`gemini_thoughts.md`** | Removed in P2.5 (was briefly updated in P2.4). |

### ✅ P2.5 — Scratch / duplicate docs (RESOLVED 2026-06-17)

| | |
|---|---|
| **Deleted** | `tmp.md`, `tmp2.md`, `tmp_ui.md`, `tmp_tools.md`, `tmp_science.md`, `qwen_reply.md`, `TEST.md`, `gemini_thoughts.md` — unique content merged first. |
| **Merged into** | `ui_improvements.md` §9–§12 (research console, GRAG troubleshooting, future UI, priorities); `TESTING.md` (manual concurrency tests + `TEST_SUITE.md` pointer). |
| **Linked** | `HOWTO.md` and `new_corpus_tests.md` from `INDEX.md`; `benchmark_results.md` / `improvements.md`. |
| **Kept** | `old_improvements.md` with supersession banner. |

### ✅ P2.6 — `benchmark_results.md` interpretation (RESOLVED 2026-06-17)

| Issue | Fix |
|-------|-----|
| Early Mar 9 run shows +0.200 nDCG; later corpus runs show +0.019–0.060 | **`benchmark_results.md`:** “How to Read These Runs” table; **`GRAG.md` §5:** canonical vs sandbox; **`AGENTS.md`:** corpus-qualified claims |
| **`MYPAPER.md` (Zenodo):** stale 563-chunk table / +0.220 | §4 aligned to Mar 14 hardened 100-case run (+0.041 mean, +0.202 goal-disambiguation); re-upload Zenodo when ready |
| Cognate benchmark `Success Rate 0.00*` | Already footnoted in P2.4 (`COGNATE_V2.md`, `README.md`) |

---

## Priority 3 — Code Quality & Hardening (Non-blocking but real)

### P3.1 — Compiler warnings (14 on debug build)

| File | Warning |
|------|---------|
| `embedding_engine.cpp` | Unused `path` in `saveState` / `loadState` |
| `memory.cpp` | ~~Unused params in `retrieveSimilarPlans` placeholder~~ — fixed in P1.3 |
| `code_modify_tool.cpp` | Unused params in `applyDiff` stub |
| `ChatSessionDataViewModel.cpp` | Unused params in read-only model methods |
| `PlanExecutionPanel.cpp` | Sign compare; unused `stepId` |
| `GraphPanel.cpp` | Unused `event` in handlers |

**Fix:** `(void)param;`, `[[maybe_unused]]`, or implement the stub bodies — especially clean up placeholders when implementing P1.3/P1.2.

### P3.2 — `retrieveSimilarPlans` and embedding migration noise

Every test run logs `[Memory] Phase 4: Migrating embedding schema from v1 to v2...` repeatedly.  

**Fix:** Use isolated temp DB paths (tests already do) but avoid redundant migration per test fixture; reduces log noise and test time.

### P3.3 — Manual test suite not automated

`TEST_SUITE.md` TC-01–TC-07 are manual log-inspection tests.  

**Fix:** Longer term — automate critical signals (`routing_mode`, `alpha`, RETRIEVAL-before-LLM ordering) as integration tests; note in `TESTING.md`.

### P3.4 — Hardcoded paths in `audit.md`

Audit references `/home/steve/Thoth` as project root.  

**Fix:** Use `FileHandler::getProjectRoot()` wording in docs; avoid machine-specific paths in permanent docs.

---

## Priority 4 — Roadmap Work (Aligned claims, not yet promised as done)

Track in `improvements.md` only — do **not** mark complete until implemented:

1. **Phase 3 completion** — pruning integration, archival restore path, vector store benchmark scaffold  
2. **Phase 4.4** — hierarchical subgoal trees  
3. **Phase 4.5** — trajectory weight tuning (config active; refine $w_t$ / T embedding quality per benchmark)  
4. **Phase 5 Self-Building** — optional future expansion *(not scheduled)*  
5. **`new_corpus_tests.md`** — 30-case research-paper disambiguation benchmark  
6. **Cross-agent / multi-agent** — mentioned in `PLAN.md`, not scoped in roadmap  
7. **GUI polish** — future items in `ui_improvements.md` §11–§12 (step debug mode, tool renderers, etc.)

---

## Suggested Session Order (updated 2026-06-16)

```
Done    P0.1 + P0.2 — reflection test fixed; ctest green
Done    P1.1 — trajectory wt=0.2 activated (local config); benchmark run archived
Done    P1.3 — plan history reuse + observability + plan_reuse_tuning.md
Done    P1.5 — allow_shell_exec enforced in shell tools
Done    P1.6 — memory pruning + session scoping + chat_sessions.json hot-tier trim
Done    P2.1 — broken doc references fixed (no COGNATE.md stub)
Done    P2.2 — VERIFIED_BASELINE, architectural_facts, AGENTS, INDEX refreshed
Done    P2.3 — improvements.md phase table aligned with completed log
Done    P2.4 — narrative doc honesty (README, COGNATE_V2, MYPAPER, cognate; gemini_thoughts removed)
Done    P2.5 — scratch docs deleted; content merged into ui_improvements + TESTING
Done    P2.6 — benchmark interpretation (benchmark_results, GRAG.md, MYPAPER.md, AGENTS.md)
Next 1  Re-run TEST_SUITE.md TC-01–TC-07 manually; update VERIFIED_BASELINE.md date
Next 2  P1.7 NODE harness OR doc-only deferral
Backlog  Phase 5 self-building / `apply_diff` (future expansion — owner may try later)
```

---

## Authoritative Source Hierarchy (use when docs conflict)

1. **Code** — always wins  
2. **`audit.md`** + **`completed_improvements_log.md`** (2026-03-30 audit note)  
3. **Specs:** `GRAG.md` (live implementation spec), `PLAN.md`, `TOOLS.md`, `cognate.md`
4. **Roadmap:** `improvements.md`
5. **Zenodo / thesis narrative:** `MYPAPER.md` (GRAG paper), `COGNATE_V2.md` (Cognate paper) — edit locally; re-upload Zenodo when changed
6. **Marketing:** `README.md`

---

## Quick Reference — Claim vs. Reality Matrix

| Feature | Claimed | Actual (2026-06-16) |
|---------|---------|---------------------|
| GRAG directional core | ✅ Complete | ✅ Works |
| GRAG graph learning | ✅ Complete | ✅ GraphRefiner active |
| GRAG trajectory $w_t$ | ✅ In formula / benchmarks | ✅ Config **0.2** (local); mixed lift on trajectory cases |
| Cognate executive loop | ✅ Complete | ✅ Reflection loop verified via `testReflectionLoop` |
| Scientific mode | ✅ Complete | ✅ Implemented (partial unit test coverage) |
| Strategy promotion | ✅ Complete | ✅ Verified in benchmarks |
| Plan history reuse | ✅ Described | ✅ `retrieveSimilarPlans` + injection + events |
| Shell exec security | ✅ Config gate | ✅ `allow_shell_exec` enforced (P1.5) |
| Crash resume | ✅ SQLite authoritative | ✅ Yes |
| Trace log resume | ✅ Phase 10 | 🔶 Observability only |
| Self-building / apply_diff | 📋 Future expansion | 🚫 Stub; harness only (`project_analyze`, read) |
| Memory pruning | 📋 Planned | ✅ Hot-tier auto-prune + GUI trim (P1.6) |
| NODE harness | Described as active | ❌ Spec only |
| Unit tests | ✅ Passing claimed | ✅ Full suite green (2026-06-16, ~78s) |

---

*Update this file as items are completed. When a section is fully resolved, move a summary entry to `completed_improvements_log.md` and strike through the row here.*
