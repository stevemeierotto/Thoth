# Thoth Alignment Checklist — Cursor Fresh Start

**Created:** 2026-06-13  
**Purpose:** Prioritized list of fixes needed so code, tests, and documentation match the claims made across `docs/`, thesis papers, and `AGENTS.md`. Use this as the working backlog when returning to the project.

**How to use:**
1. Work top-to-bottom by priority tier.
2. After each code fix: `cmake --build --preset build-debug` and `ctest --output-on-failure`.
3. After each doc fix: update `completed_improvements_log.md` only when work is actually done (do not mark complete prematurely).
4. Treat `completed_improvements_log.md` + `audit.md` as the honesty layer; marketing docs (`README.md`, `gemini_thoughts.md`) lag behind reality.

---

## Priority 0 — Regressions & Broken Verification (Fix First)

These directly contradict “verified / passing” claims and block a trustworthy fresh start.

### P0.1 — `testReflectionLoop` fails (reflection loop regression)

| | |
|---|---|
| **Claim** | `completed_improvements_log.md` (2026-03-29): “Reflection Loop & Success Scoring Hardening” verified; `completed_improvements_log.md` (2026-03-22): “passed full suite of unit tests including … reflection cycles.” |
| **Reality** | Build succeeds; `./tests/thoth-unit-tests` fails with `testReflectionLoop: failed to trigger reflection (call count: 1)`. Log shows `[DEBUG] Low success score (0), triggering reflection cycle 1` but `MockReflectionPlanner::call_count` never reaches 2. |
| **Likely area** | `external/basic_agent/src/executive_controller.cpp` — `decide_transition()` reflection block (~lines 391–424): second `planner_->create_plan()` may not run, may race with `run_loop()`, or replanned steps never re-enter execution. |
| **Fix** | Trace `execute_goal` → `run_loop` → `decide_transition` for a single-step LLM plan that fails scoring; ensure reflection calls `create_plan` a second time and the new plan is executed; re-run test until green. |
| **Doc follow-up** | If the 2026-03-29 log entry overstates verification, add a correction note to `completed_improvements_log.md`. |

### P0.2 — Unit test suite must pass before any “baseline locked” claim

| | |
|---|---|
| **Claim** | `VERIFIED_BASELINE.md`, `TEST_SUITE.md`, `make_cognate.md` imply stable, merge-ready behavior. |
| **Reality** | Only one automated test fails today, but the suite takes ~80s and is not run in CI by default in this checklist context. |
| **Fix** | Fix P0.1; confirm `ctest --output-on-failure` is 100% green; optionally add a one-line “last verified” date to `VERIFIED_BASELINE.md`. |

---

## Priority 1 — Code vs. Documented Capabilities (Core Claims)

These are features described as complete (or thesis-critical) but are stubbed, disabled, or not wired.

### P1.1 — Trajectory-aware GRAG (`w_t`) disabled in production config

| | |
|---|---|
| **Claim** | `GRAG.md`, `MYPAPER.md`, `benchmark_results.md` (Mar 14 runs), `README.md`: trajectory term $w_t=0.2$ is part of the scoring model; Phase 4.5 in `improvements.md` describes trajectory-aware retrieval. |
| **Reality** | `GragScorer::rescore()` supports `trajectory_embedding` and `config.wt`, but `agent_workspace/retrieval_config.json` sets `"trajectory": 0.0`. Default `Config` uses `wt=0.2` until overridden by that file. |
| **Fix (choose one path)** | **A)** Activate: set `trajectory` to `0.2` in `retrieval_config.json`, verify with GRAG benchmark and unit tests. **B)** Defer: keep `0.0` but downgrade claims in `README.md`, `COGNATE_V2.md`, and benchmark notes that assume active $w_t$. |
| **Doc fix** | Align `GRAG.md` §3 audit note with chosen path; update `AGENTS.md` “Current System Status” section. |

### P1.2 — `code_modify` / `apply_diff` is a stub (Self-Building not real)

| | |
|---|---|
| **Claim** | `README.md` roadmap, `improvements.md` Phase 5, early `completed_improvements_log.md` entries: self-building / diff application. |
| **Reality** | `code_modify_tool.cpp` returns `"Unified diff application not fully implemented in v1.0 prototype."` |
| **Fix** | Implement unified diff apply with temp-file atomic write (per `improvements.md` Step 5.3) **or** clearly mark Phase 5 as blocked/stub everywhere. |
| **Doc fix** | `README.md` still lists Phase 5 as planned — good — but remove any wording implying diff apply works today; `audit.md` is accurate, `gemini_thoughts.md` is not (claims Phase 3 self-building “complete”). |

### P1.3 — Plan history reuse is a placeholder

| | |
|---|---|
| **Claim** | `cognate.md`, `GRAG.md`, `AGENTS.md`: “Plan History Reuse” / `retrieveSimilarPlans` accelerates planning. |
| **Reality** | `Memory::retrieveSimilarPlans()` in `memory.cpp` is a placeholder returning `{}` (also triggers `-Wunused-parameter` warnings). `ExecutiveController::execute_goal` calls it but gets nothing. |
| **Fix** | Implement embedding-based plan retrieval in `SQLiteMemoryRepository` + `Memory::retrieveSimilarPlans`, or remove injection from `execute_goal` and downgrade docs. |

### P1.4 — Hierarchical subgoal trees not implemented

| | |
|---|---|
| **Claim** | Referenced as planned in `GRAG.md`, `improvements.md` Step 4.4, `README.md` roadmap. |
| **Reality** | Single root goal embedding for entire plan duration (`GRAG.md` audit note is correct). |
| **Fix** | Code: implement `GoalNode` tree per roadmap **or** doc-only: ensure all “complete” docs do not imply this exists. |
| **Doc fix** | `cognate.md` §5 “Future Frontiers” is accurate; `README.md` “Planned” row is accurate; no change needed unless something claims it is done. |

### P1.5 — `allow_shell_exec` bypass

| | |
|---|---|
| **Claim** | `AGENTS.md`, `audit.md`: known gap — config flag bypassed. |
| **Reality** | `run_tests` and `code_modify build` use shell execution without checking `allow_shell_exec`. |
| **Fix** | Enforce flag in tools before `popen` / build; add unit test. |
| **Doc fix** | Move from “known gap” to “fixed” in `audit.md` and `architectural_facts.md` when done. |

### P1.6 — Memory pruning not integrated into runtime

| | |
|---|---|
| **Claim** | `improvements.md` Phase 3.2: auto-trigger on session write; `completed_improvements_log.md` mentions Memory Stability. |
| **Reality** | `MemoryPruner` class exists (`memory_pruner.cpp`) and has unit test coverage references, but it is **not** wired into `SQLiteMemoryRepository` session-write path (no production call sites outside tests). |
| **Fix** | Integrate pruning triggers per Phase 3.2 spec **or** mark Phase 3.2 as “class only, not integrated” in roadmap. |
| **Doc fix** | Update `improvements.md` Phase 3 status table — several Phase 3 items are partial, not “Planned” from scratch. |

### P1.7 — NODE runtime not built

| | |
|---|---|
| **Claim** | `NODE.md`, `build_node.md`, `AGENTS.md`: NODE as execution harness / future PlanStep type. |
| **Reality** | Specification and build guide only; no `/core/node` implementation in repo. |
| **Fix** | Either start `build_node.md` Phase 1 **or** keep as future work and ensure no doc says NODE is operational. |
| **Doc fix** | `README.md` and `gemini_thoughts.md` describe NODE as existing harness — soften to “specified, not implemented.” |

---

## Priority 2 — Documentation Consistency & Missing Files

Fix doc drift so the next session does not read conflicting sources.

### P2.1 — Broken / missing doc references

| File | Problem | Fix |
|------|---------|-----|
| `improvements.md` | References `grag_summery.md` — **file does not exist** | Create `grag_summery.md` as a short status companion to `GRAG.md`, **or** replace all references with `GRAG.md` + `architectural_facts.md` |
| `INDEX.md`, `improvements.md` | Reference `basic_agent_design_goal.md` — **deleted** (per git status) | Remove links; point to `VERIFIED_BASELINE.md` + `PLAN.md` instead |
| User/docs convention | `COGNATE.md` expected; actual file is `cognate.md` (case-sensitive on Linux) | Add `docs/COGNATE.md` symlink or one-line stub pointing to `cognate.md`; update `INDEX.md` |
| `memory_summery.md` | Filename typo (“summery”) | Rename to `memory_summary.md` when convenient; update links |

### P2.2 — Stale “verified” documents

| File | Stale content | Fix |
|------|---------------|-----|
| `VERIFIED_BASELINE.md` | §4 item 3: “Scientific mode is not implemented” | Update: Scientific mode was implemented March 28 2026 (`make_cognate.md`, `completed_improvements_log.md`); add note that baseline TC tests are pre–Cognate V2 |
| `VERIFIED_BASELINE.md` | “Confirmed passing on 2026-03-12” | Add “superseded by Cognate V2 work; re-run TC-01–TC-07 + full unit tests” banner |
| `architectural_facts.md` | §2 reads as **pre-fix** thread-safety audit with inline “I will now update…” agent notes | Replace with current status (mutex added per completed log 2026-03-22) or move raw audit to `docs/audit.md` appendix |
| `AGENTS.md` | Last updated 2026-03-10 | Refresh “Current System Status”, Cognate V2 completion, reflection/scientific/strategy sections |
| `INDEX.md` | Last updated 2026-03-10 | Refresh phase table, add `cursor_list.md`, fix deleted-file links |

### P2.3 — `improvements.md` phase table contradicts `completed_improvements_log.md`

| Phase | Table says | Log says |
|-------|------------|----------|
| 3 Memory Stability | 📋 Planned | FactStore, IVectorStore, MemoryPruner **partially** implemented |
| 4 Advanced Reasoning | 📋 Planned | Scientific mode, Strategy Engine, GraphRefiner **complete**; subgoals & $w_t$ **not** |
| 5 Self-Building | 📋 Planned | Tools exist; `apply_diff` stub |

**Fix:** Rewrite Phase 3–5 table with substates: ✅ Complete / 🔶 Partial / 📋 Not started. Move finished Phase 4 items to `completed_improvements_log.md` and trim duplicate step text from `improvements.md`.

### P2.4 — Overclaiming in narrative docs

| File | Issue | Fix |
|------|-------|-----|
| `gemini_thoughts.md` | States self-building “Phase 3 Roadmap complete”, NODE operational, all Phases 1–12 complete without stub caveats | Add “as of” date and link to `audit.md` / this file |
| `README.md` | Strong Cognate V2 / learning claims — mostly fair | Add short “Known gaps” footnote linking `GRAG.md` audit box |
| `COGNATE_V2.md` | Typo: “depeloth” → “depth” in §7 Conclusion | Fix typo |
| `COGNATE_V2.md` | Presents SCR / 51× depth as established fact | Add footnote: mock benchmark environment (`benchmark_results.md` notes 0.00 success rate in mock runs) |
| `cognate.md` | §2 says Scientific mode in strategy pattern — OK | §5 future items accurate |
| `MYPAPER.md` | §3.4 “Adaptive Graph Learning (Planned Extension)” | Mark **Complete** per `GRAG.md` §6 item 3 |

### P2.5 — Scratch / duplicate docs (cleanup)

These confuse a fresh start. Either archive under `docs/archive/` or merge into authoritative files:

| File | Recommendation |
|------|------------------|
| `tmp.md`, `tmp2.md`, `tmp_ui.md`, `tmp_tools.md`, `tmp_science.md` | Archive or delete after extracting any unique requirements into `ui_improvements.md` / `improvements.md` |
| `old_improvements.md` | Keep as archive; add banner “superseded by improvements.md” |
| `qwen_reply.md` | External LLM review notes — move to `docs/archive/` or delete |
| `new_corpus_tests.md` | Active benchmark design — keep, link from `improvements.md` or `benchmark_results.md` |
| `HOWTO.md` | User-facing; good — link from `INDEX.md` under “For users” |
| `TEST.md` | Overlaps `TEST_SUITE.md` / `TESTING.md` | Merge manual concurrency protocol into `TESTING.md`; deprecate `TEST.md` |

### P2.6 — `benchmark_results.md` interpretation

| Issue | Fix |
|-------|-----|
| Early Mar 9 run shows +0.200 nDCG; later corpus runs show +0.019–0.060 | Docs should cite **which corpus / case mix** when claiming “+0.200 lift” — `GRAG.md` §5 should note early sandbox vs hardened 100-case suite |
| Cognate benchmark `Success Rate 0.00*` | Already footnoted — ensure `COGNATE_V2.md` and `README.md` do not imply end-to-end task success without that caveat |

---

## Priority 3 — Code Quality & Hardening (Non-blocking but real)

### P3.1 — Compiler warnings (14 on debug build)

| File | Warning |
|------|---------|
| `embedding_engine.cpp` | Unused `path` in `saveState` / `loadState` |
| `memory.cpp` | Unused params in `retrieveSimilarPlans` placeholder |
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
3. **Phase 4.5** — activate trajectory weight end-to-end (config + diagnostics + benchmarks)  
4. **Phase 5** — real `apply_diff`, build/revert automation  
5. **`new_corpus_tests.md`** — 30-case research-paper disambiguation benchmark  
6. **Cross-agent / multi-agent** — mentioned in `PLAN.md`, not scoped in roadmap  
7. **GUI polish** — `tmp_ui.md` research console menu items (Step Execution debug mode, etc.)

---

## Suggested Fresh-Start Session Order

For your first week back:

```
Day 1   Fix P0.1 (reflection test) → green ctest
Day 2   P2.1–P2.3 doc alignment (quick wins, no code)
Day 3   Decide P1.1 trajectory weight: activate or doc downgrade
Day 4   P1.3 plan history reuse OR remove from execute_goal
Day 5   Re-run TEST_SUITE.md TC-01–TC-07 manually; update VERIFIED_BASELINE.md date
Week 2  P1.2 apply_diff OR formally defer Phase 5 with honest README
```

---

## Authoritative Source Hierarchy (use when docs conflict)

1. **Code** — always wins  
2. **`audit.md`** + **`completed_improvements_log.md`** (2026-03-30 audit note)  
3. **Specs:** `GRAG.md`, `PLAN.md`, `TOOLS.md`, `cognate.md`  
4. **Roadmap:** `improvements.md`  
5. **Marketing / thesis narrative:** `README.md`, `COGNATE_V2.md`, `MYPAPER.md`, `gemini_thoughts.md`  

---

## Quick Reference — Claim vs. Reality Matrix

| Feature | Claimed | Actual (2026-06-13) |
|---------|---------|---------------------|
| GRAG directional core | ✅ Complete | ✅ Works |
| GRAG graph learning | ✅ Complete | ✅ GraphRefiner active |
| GRAG trajectory $w_t$ | ✅ In formula / benchmarks | 🔶 Code yes; config **0.0** |
| Cognate executive loop | ✅ Complete | 🔶 Reflection test **failing** |
| Scientific mode | ✅ Complete | ✅ Implemented (re-verify tests) |
| Strategy promotion | ✅ Complete | ✅ Verified in benchmarks |
| Plan history reuse | ✅ Described | ❌ Placeholder returns empty |
| Crash resume | ✅ SQLite authoritative | ✅ Yes |
| Trace log resume | ✅ Phase 10 | 🔶 Observability only |
| Self-building / apply_diff | ✅ / 📋 Mixed | ❌ Stub |
| Memory pruning | 📋 Planned | 🔶 Class only |
| NODE harness | Described as active | ❌ Spec only |
| Unit tests | ✅ Passing claimed | ❌ 1 failure |

---

*Update this file as items are completed. When a section is fully resolved, move a summary entry to `completed_improvements_log.md` and strike through the row here.*
