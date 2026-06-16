# Thoth Alignment Checklist — Cursor Fresh Start

**Created:** 2026-06-13  
**Last updated:** 2026-06-16  
**Purpose:** Prioritized list of fixes needed so code, tests, and documentation match the claims made across `docs/`, thesis papers, and `AGENTS.md`. Use this as the working backlog when returning to the project.

**Status:** P0 and P1.1 / P1.3 / P1.5 / **P1.6** resolved (2026-06-16). Next focus: P1.7 and Priority 2 doc drift. **P1.2 deferred (low priority).**

**How to use:**
1. Work top-to-bottom by priority tier.
2. After each code fix: `cmake --build --preset build-debug` and `ctest --output-on-failure`.
3. After each doc fix: update `completed_improvements_log.md` only when work is actually done (do not mark complete prematurely).
4. Treat `completed_improvements_log.md` + `audit.md` as the honesty layer; marketing docs (`README.md`, `gemini_thoughts.md`) lag behind reality.

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

### P1.2 — `code_modify` / `apply_diff` is a stub (Self-Building not real) — **LOW PRIORITY / DEFERRED**

| | |
|---|---|
| **Priority** | **Low** — owner deferred; no action until Phase 5 is explicitly scheduled. |
| **Claim** | `README.md` roadmap, `improvements.md` Phase 5, early `completed_improvements_log.md` entries: self-building / diff application. |
| **Reality** | `code_modify_tool.cpp` returns `"Unified diff application not fully implemented in v1.0 prototype."` |
| **Fix (when resumed)** | Implement unified diff apply with temp-file atomic write (per `improvements.md` Step 5.3) **or** clearly mark Phase 5 as blocked/stub everywhere. |
| **Doc fix (optional)** | `audit.md` is already accurate; `README.md` lists Phase 5 as planned — sufficient for now. |

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
4. **Phase 5** — real `apply_diff`, build/revert automation *(low priority — P1.2 deferred)*  
5. **`new_corpus_tests.md`** — 30-case research-paper disambiguation benchmark  
6. **Cross-agent / multi-agent** — mentioned in `PLAN.md`, not scoped in roadmap  
7. **GUI polish** — `tmp_ui.md` research console menu items (Step Execution debug mode, etc.)

---

## Suggested Session Order (updated 2026-06-16)

```
Done    P0.1 + P0.2 — reflection test fixed; ctest green
Done    P1.1 — trajectory wt=0.2 activated (local config); benchmark run archived
Done    P1.3 — plan history reuse + observability + plan_reuse_tuning.md
Done    P1.5 — allow_shell_exec enforced in shell tools
Done    P1.6 — memory pruning + session scoping + chat_sessions.json hot-tier trim
Next 1  P2.1–P2.3 doc alignment (INDEX.md, improvements.md phase table, broken links)
Next 2  Re-run TEST_SUITE.md TC-01–TC-07 manually; update VERIFIED_BASELINE.md date
Next 3  P1.7 NODE harness OR doc-only deferral
Backlog P1.2 apply_diff (low priority — deferred)
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
| Self-building / apply_diff | ✅ / 📋 Mixed | ❌ Stub |
| Memory pruning | 📋 Planned | ✅ Hot-tier auto-prune + GUI trim (P1.6) |
| NODE harness | Described as active | ❌ Spec only |
| Unit tests | ✅ Passing claimed | ✅ Full suite green (2026-06-16, ~78s) |

---

*Update this file as items are completed. When a section is fully resolved, move a summary entry to `completed_improvements_log.md` and strike through the row here.*
