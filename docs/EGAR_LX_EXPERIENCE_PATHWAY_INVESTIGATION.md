# EGAR-LX v0.1 — Experience pathway investigation

**Date:** 2026-10-15
**Status:** Read-only static investigation. No goal was executed. No sample size is selected.
**Protocol and planning reports were not modified.**

This report answers the Section 4 gate in `docs/EGAR_LINUX_DIAGNOSTIC_AB_PROTOCOL.md`: how a diagnostic goal writes trajectories, plans, and `success_score`, and whether those records can meet the existing retrieval and promotion thresholds. Findings are from the current source. They are not runtime observations.

Labels used below:

- **Confirmed:** the cited code does this.
- **Inference:** a conclusion that follows from those paths, not from a run.
- **Unresolved:** the source does not settle it, and it was not executed.

## 1. Scope and method

Documents read: `docs/EGAR_LINUX_DIAGNOSTIC_AB_PROTOCOL.md`, `docs/EGAR_LX_SAMPLE_SIZE_PLANNING_REPORT.md`, and `docs/EGAR_LX_FAMILY_DIVERSITY_FEASIBILITY_REPORT.md`. They require separate acquisition and evaluation cases, Arm A empty databases, Arm B disposable copies of one hashed snapshot, identical non-experience configuration, fixed pre-cutoff evidence, no live Linux or sealed-answer access, and passive measurement. External 0–4 grades are not `success_score`.

The trace follows `ExecutiveController::execute_goal` through planning, step execution, history storage, and the next goal’s planner injection. No Thoth process was started.

## 2. Files and functions inspected

| Role | Location |
|---|---|
| Goal entry | `external/basic_agent/src/executive_controller.cpp` — `execute_goal` (line 439), `decide_transition` (line 872), `abort` (line 553) |
| Score | `calculate_trajectory_score` (line 1448), `store_plan_history` (line 1685) |
| Graph update | `reinforce_plan_graph` (line 1387), `external/basic_agent/src/graph_refiner.cpp` — `refineFromTrajectory` (line 17) |
| Planner injection | `external/basic_agent/src/llm_planner.cpp` — `selectRelevantStrategy` (line 28), `create_plan` (line 169), `revise_plan` (line 314) |
| Prompt budget | `external/basic_agent/src/prompt_factory.cpp` — `assemblePlannerPrompt` (line 74), `buildPlanPrompt` (line 476) |
| Thresholds | `external/basic_agent/include/plan_reuse_config.h`, `external/basic_agent/include/planner_injection_config.h` |
| Retrieval | `external/basic_agent/src/memory.cpp` — `retrieveSimilarPlans` (line 725), `retrieveSimilarTrajectories` (line 197), `Memory::clear` (line 192) |
| Promotion | `external/basic_agent/src/strategy_engine.cpp` — `processTrajectories` (line 21) |
| Outline injected | `external/basic_agent/include/goal_text_utils.h` — `sanitizePlanOutline` (line 80) |
| Step execution | `external/basic_agent/src/workflow_engine.cpp` — `executeTool` (line 412), `executeLLM` (line 648) |
| Stored shape | `external/basic_agent/src/trajectory.cpp` — `Trajectory::to_json` (line 43) |
| Schema | `external/basic_agent/src/sqlite_memory_repository.cpp` (lines 55–226) |
| Shell and network tools | `run_tests_tool.cpp`, `code_modify_tool.cpp`, `web_scrape_tool.cpp` |
| Logs | `executive_controller.cpp` `log_to_trace` (line 1501), `logger.cpp` (line 30), `generation_call_log.cpp`, `grag_metrics.cpp` |

## 3. End-to-end path

**Confirmed.** A goal enters `ExecutiveController::execute_goal`. The controller embeds the goal, optionally appends one similar past plan to the goal string, and calls `LLMPlanner::create_plan`. The planner may add one strategy and one trajectory snippet, then asks the model for a plan. `revise_plan` does not receive that experience block.

The controller then runs steps through `WorkflowEngine`. Retrieval steps call GRAG. Tool steps call `ToolRegistry`. LLM steps generate a written answer from the step prompt and prior step results. GRAG’s current-episode trajectory weight `wt` is forced to 0 until `TrajectoryBuilder` has at least three episode steps (`plan_reuse_config.h`, `TrajectoryReuse::kMinEpisodeStepsForEmbedding`). Otherwise `wt` is the configured value, typically −0.05.

When no step is still pending, `decide_transition` computes `success_score` and calls `store_plan_history`. That writes a past plan, a cognate plan, and a trajectory, then runs `StrategyEngine::processTrajectories`. `abort` persists the active plan and emits `PLAN_ABORTED`. It does not call `store_plan_history`.

**Inference.** A diagnostic goal uses this same path. There is no separate diagnostic-goal type in the controller.

## 4. Experience creation

**Confirmed.** A reusable record is written when a plan reaches the end of `decide_transition`, including a failed plan, and again when a low score triggers reflection before the retry. The stored trajectory JSON includes the goal, the initial plan, each recorded step’s description, tool, result JSON, and error, revisions, final status, `success_score`, and the goal embedding (`trajectory.cpp` lines 43–66). An LLM step’s result JSON includes the model text (`workflow_engine.cpp` lines 731–736).

What later goals are shown is narrower than what is stored:

| Channel | Stored | Injected into the next planner prompt |
|---|---|---|
| Past plan | Full plan JSON in `past_plans.outline` | Goal text, numeric score, and `TYPE(step_id)` outline only (`sanitizePlanOutline`, `build_plan_reuse_context` lines 1922–1932) |
| Trajectory | Full step results, including the written answer | Prior goal text and `success_score` only (`llm_planner.cpp` lines 213–218) |
| Strategy | Step-type or tool-name pattern such as `RETRIEVAL->LLM`, plus a success rate | That pattern, the description, and the rate (`formatStrategyContext`, lines 58–65) |
| Graph | Edges between retrieval chunk hashes | Not text. Edge weights change GRAG scores (`grag_scorer.cpp` line 207) |

**Confirmed.** Promotion requires at least three stored trajectories in total, and a repeated step pattern of at least two steps whose average `success_score` is at least 0.8 (`strategy_engine.cpp` lines 26–30 and 80–84). Plan reuse requires `success_score` ≥ 0.6 and cosine similarity, after a +0.1 boost if the score is ≥ 0.8, of at least 0.55 (`memory.cpp` lines 733–755). Trajectory injection uses the same 0.55 floor after the same 0.8 boost (`memory.cpp` lines 203–220). Strategy injection requires cosine similarity of at least 0.40 (`planner_injection_config.h` line 33). Each channel injects at most one item.

**Confirmed.** A written diagnostic answer can be stored without executing a repair, because an LLM step succeeds when the model returns non-error text. Failures and timeouts that finish the plan loop are stored with a reduced score. A crash or `abort` before `store_plan_history` does not create the trajectory row.

**Inference.** Four acquisition cases cannot meet the three-trajectory promotion rule unless reflection or retries write extra trajectories, and even then the pattern must repeat with mean score ≥ 0.8. A four-case acquisition can still store plans and trajectories that a later goal may inject if similarity clears 0.55. No injection is a normal outcome when the database is empty or similarity is below the floor. That matches the approved “no injection observed” category. It is not evidence that EGAR code was disabled. There is no EGAR disable flag on this path.

## 5. Internal success score

**Confirmed.** `ExecutiveController::calculate_trajectory_score` (lines 1448–1463) is a heuristic, not a model grade and not external feedback.

- Start at 1.0 if every step status is `SUCCESS` and the plan is non-empty. Otherwise start at 0.0.
- Subtract 0.1 for each recorded step whose error string is non-empty.
- Add 0.1 if the plan was fully successful and at least one reflection revision occurred.
- Clamp to the closed interval from 0 to 1.

The score is stored on `past_plans`, `plans`, and `trajectories`, and copied into `grag_metrics.jsonl` as `final_success_score`. It changes ranking: scores ≥ 0.6 are eligible for plan reuse, and scores ≥ 0.8 receive the +0.1 similarity boost and are the promotion average. It does not measure technical correctness of a diagnosis.

**Confirmed.** An incorrect diagnosis can receive 1.0. The LLM step marks success from a non-empty, non-error generation (`workflow_engine.cpp` lines 724–736). The heuristic never reads the answer text.

**Confirmed disagreement with a completion-based graph story.** `reinforce_plan_graph` (line 1429) passes 1.0 to `GraphRefiner` only when `current_plan_.status == PlanStatus::COMPLETED`. `decide_transition` sets `ControllerState` to `COMPLETED` or `FAILED`. It does not assign `PlanStatus::COMPLETED`. The only production assignment of `PlanStatus::FAILED` found on this path is the interrupted-step resume helper (line 696). `GraphRefiner` treats a score below 0.8 as failure (line 20). On a normal finish, the graph update therefore receives 0.0 whenever at least two retrieval chunk-sets exist. The second `reinforce_plan_graph` call in `store_plan_history` (line 1732) runs only after `active_sets_` has been cleared, so it returns immediately.

**Inference.** Positive graph reinforcement is not what a successfully finished diagnostic goal does today. Edges that do get updated are penalized. This is a code fact, not a runtime measurement of how often two chunk-sets exist.

## 6. Retrieval and injection

**Confirmed.** Retrieval is automatic inside `execute_goal` and `create_plan`. It is not a separate operator action. The stages are not the same event:

| Stage | What the code does | Where it is visible |
|---|---|---|
| Present in storage | Rows in `past_plans`, `trajectories`, `strategies`, `graph_edges` | SQLite, not a prompt |
| Retrieved | Similarity search over those rows, or GRAG over the corpus | Planner logs if injection follows; GRAG diagnostics on retrieval steps |
| Selected | At most one plan, one trajectory, and one strategy above the floors | `PLAN_REUSE_INJECTION`, `TRAJECTORY_INJECTION`, `STRATEGY_INJECTION` |
| Injected | Text appended to the plan prompt, unless the experience budget is zero | `PLANNER_CONTEXT_ASSEMBLY` field `experience_dropped` |
| Used in reasoning | Not recorded | The model may ignore injected text. No field records use |

**Confirmed.** If the core rules, schema, and goal consume the planner budget, `assemblePlannerPrompt` clears the experience block and sets `experience_dropped` (lines 95–104). Individual sections are also capped: strategy 512 characters, trajectory-plus-plan text 512, plan-reuse block 1024 (`planner_injection_config.h`). `experience_dropped` is true only when the entire block is removed, not when it is merely capped.

**Confirmed.** A strategy can be absent from the prompt when dropped, while still having been selected. Graph weights can change GRAG rankings with no experience sentence in the planner prompt. `revise_plan` omits strategy, trajectory, and plan reuse (`llm_planner.cpp` lines 322–335).

**Confirmed.** An empty experience database injects nothing and still runs planning. EGAR code remains active.

## 7. Fixed-evidence compatibility

| Behavior | Classification | Evidence |
|---|---|---|
| LLM step writes a hypothesis, recommended commands, or a remedy as text and does not execute them | Compatible as implemented | `executeLLM` returns the generation. It does not run the recommended commands |
| Score and storage do not need a live repair or an external grade | Compatible as implemented | Heuristic in `calculate_trajectory_score`. No reviewer input |
| GRAG reads whatever corpus and `rag_index.bin` the process was given | Compatible only under existing documented settings | The code has no pre-cutoff check. A corpus that contains post-cutoff or sealed text would be retrieved |
| Planner may emit `TOOL` steps | Potentially incompatible | `executeTool` runs any registered tool the plan names, and sets `confirmed` to true (lines 423–425) |
| `web_scrape` performs HTTP GET | Confirmed incompatible if a plan calls it | `web_scrape_tool.cpp` uses libcurl. No `allow_shell_exec` gate |
| `run_tests` and `code_modify` build use `popen` | Confirmed incompatible if a plan calls them and `allow_shell_exec` is true | Both tools refuse when `allow_shell_exec` is false |
| `code_modify` can read workspace files when invoked | Potentially incompatible | The tool exists and is not banned by the diagnostic-goal path |
| Graph and trajectory writes during acquisition | Compatible as implemented for the acquisition phase | `store_plan_history` is the approved experience write. Evaluation must use a copy so later cases do not keep those writes |
| Sealed answers are not fetched by the controller | Compatible as implemented, with a corpus caveat | Nothing in this path opens a sealed-key store. A sealed document that has been indexed would be an ordinary GRAG chunk |

**Unresolved.** Whether a model given the approved diagnostic instruction will actually emit `web_scrape` or a shell tool. The instruction is not a code interlock.

## 8. Experience-bearing state

**Confirmed.** `memory.db` is opened at `<workspace>/memory.db` (`memory.cpp` line 27, `memory_repository_factory.cpp` line 20). Tables created in `sqlite_memory_repository.cpp` include `past_plans`, `trajectories`, `strategies`, `graph_nodes`, `graph_edges`, `plans`, `active_plans`, `episode_steps`, `facts`, `plan_embeddings`, `sessions`, `messages`, `summaries`, `warm_memory`, and `step_metrics`.

**Confirmed.** SQLite is not the whole process image. `Memory` keeps an in-memory trajectory index loaded from those rows. The controller and RAG pipeline hold goal, current-state, and trajectory embeddings for the running goal. `Memory::clear` deletes session messages only (lines 192–195). It does not clear trajectories, plans, strategies, or edges.

**Confirmed.** `rag_index.bin` and the files under the RAG corpus are shared retrieval state, not the acquisition snapshot. Copying only `memory.db` leaves them shared, which is what the protocol requires, provided both arms use the same corpus and weights. Edge weights inside the database can still change which chunks GRAG prefers.

**Inference.** A faithful Arm B copy is a new process, or a reload, opened on a copy of `memory.db` taken after acquisition. A long-running process that keeps the old `Memory` object will keep the old index even if the file on disk is replaced. Logs written beside the database (`decision_trace.jsonl`, `app_log.jsonl`, `grag_metrics.jsonl`) are not in the SQLite snapshot. The indexer accepts `.txt`, `.md`, `.epub`, `.pdf`, and C/C++ sources, not `.jsonl` (`index_manager.h` lines 152–154), so those logs are not GRAG chunks under the current extension list.

**Unresolved.** Whether a diagnostic plan will call `store_fact` or write chat `messages`. The executive path does not do so itself. A tool step could.

## 9. Passive observability

| Question | Class | Where |
|---|---|---|
| Plan-reuse selection | Directly observable now | `decision_trace.jsonl` event `PLAN_REUSE_INJECTION`, and `app_log.jsonl` |
| Strategy and trajectory selection, similarity, drop flag | Directly observable now | `app_log.jsonl` events `STRATEGY_INJECTION`, `TRAJECTORY_INJECTION`, `PLANNER_CONTEXT_ASSEMBLY` (`experience_dropped`, byte counts) |
| Final internal score | Directly observable now | `PLAN_HISTORY_STORED` metadata and `grag_metrics.jsonl` `final_success_score` |
| Row written | Directly observable now | `trajectories`, `past_plans`, `strategies` in `memory.db` |
| LLM and plan generation calls | Directly observable now | `logs/generation_calls.jsonl` |
| Tool name and step outcome | Directly observable now | `step_metrics` and the trajectory JSON `tool` / `result` / `error` |
| Retrieval diagnostics | Directly observable now | `decision_trace.jsonl` event `RETRIEVAL_DIAGNOSTICS` when emitted |
| Graph weight change | Inferable, not a structured event | stdout line `[GraphRefiner] Refined ...` only (`graph_refiner.cpp` lines 59–64) |
| Experience text actually left in the prompt after capping | Inferable from byte counts | The prompt body is not written to `PLANNER_CONTEXT_ASSEMBLY` |
| Model used the injected text | Not currently observable | No citation or attention record |
| Injection absent because nothing qualified, versus injection dropped | Inferable by combining the three injection flags with `experience_dropped` | A missing log line does not prove either case if logging failed |
| Whether two chunk-sets existed so edges were updated | Uncertain without execution | `active_sets_` is in memory and cleared after the attempt |

Missing logs do not prove an event did not occur.

## 10. Implications for acquisition cases

**Inference, from the confirmed injection limits.**

- A case can produce a stored trajectory without a live repair. The reusable prompt text is the goal, a score, and a step-type outline, not the diagnosis paragraph. Cases that differ only in the wording of the final answer will not differ in what the next planner is shown, except through similarity of the goal embedding and the score.
- Promotion wants the same step-type or tool pattern at least three times with mean score ≥ 0.8. Variation that changes the step pattern works against promotion. Variation that keeps `RETRIEVAL` then `LLM`, for example, can meet the pattern rule. That pattern is not a Linux strategy. It is a controller step sequence.
- An incorrect but completed LLM answer is scored like a correct one. Acquisition can store and later inject a high-scoring wrong episode. The injected text still omits the wrong answer, but the high score makes retrieval more likely, and the stored JSON retains the answer for any later reader of the database.
- External grades are not required for writes and must not be written back into `success_score` if the approved estimand is to stay independent. The code will not do that write by itself.
- Acquisition must not put evaluation solutions into the goal text. `cleanGoalForStorage` strips the plan-reuse suffix. It does not detect a sealed diagnosis pasted into the goal. The stored goal string is exactly what the next trajectory injection prints.
- Sharing a family between acquisition and evaluation is a protocol rule, not a code rule. Near-duplicate goals will have similar embeddings and are the ones most likely to clear 0.55.
- Empty injection remains a valid Arm B observation when similarity or promotion thresholds are missed. A four-case acquisition should be expected to miss promotion of a strategy unless extra trajectories are written.

No acquisition count is chosen.

## 11. Confirmed limitations

- `success_score` is plan-mechanics, not diagnostic truth.
- Planner injection does not include the diagnostic answer or the tool payloads.
- `revise_plan` does not see experience.
- `Memory::clear` does not reset EGAR rows.
- Positive graph reinforcement is not wired to controller completion, because `PlanStatus::COMPLETED` is not set on this path.
- `web_scrape` is an unconditional network tool if the plan calls it.
- Shell tools run only when `allow_shell_exec` is true, and the planner is still allowed to request them.
- There is no code marker that distinguishes “EGAR idle because the database was empty” from “EGAR idle because similarity failed,” except by reading the selection logs together with the database counts.

## 12. Unresolved questions

- Whether a real diagnostic plan emits a tool step.
- How often retrieval produces two or more chunk-sets, and therefore how often the graph penalty runs.
- Whether `store_fact` or chat rows appear in an acquisition database.
- The numerical distribution of goal-embedding cosines between same-family Linux goals. The 0.55 floor may pass or block them. That was not measured.
- Whether a process can switch `memory.db` without restarting. The path is fixed at workspace open.

## 13. Decisions that need approval

1. Accept this static trace as the Section 4 investigation record, or require a later runtime trace under a separate authorization. This report does not mark the protocol gate closed, because the protocol file was not edited.
2. Decide whether acquisition is still useful if the reusable prompt content is goal text, a mechanical score, and a step-type outline, while the full answer remains only in SQLite.
3. Decide whether a tool-step ban has to be implemented before any pilot. Today the ban is an instruction, not an interlock. `web_scrape` is the sharp case.
4. Decide whether Arm isolation is a fresh process per case bound to a workspace copy of `memory.db`, with the shared corpus and `rag_index.bin` held identical and not written by the case.
5. Do not treat internal `success_score` as the external 0–4 outcome, and do not promote graph-edge success from the documentation alone. The controller path penalizes edges on a normal finish.

No sample size, acquisition count, or experimental run is authorized by this report.
