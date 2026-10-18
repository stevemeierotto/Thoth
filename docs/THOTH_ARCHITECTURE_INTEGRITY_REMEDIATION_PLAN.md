# Thoth architecture integrity review and remediation plan

**Date:** 2026-10-15
**Status:** Planning only. No source, configuration, protocol, or database was changed. No test, goal, network request, or engineering pilot was run.
**Experiment:** EGAR-LX v0.1 has not started. No sample size is selected.

This plan is the implementation proposal for confirmed problems that affect Cognate goal execution, EGAR acquisition, graph reinforcement, tool containment, and experimental isolation. It is not implementation approval.

## 1. Repository instructions reviewed

| Document | Role in this review |
|---|---|
| Repository-root `AGENTS.md` | Authoritative agent guide. Planning gate applies. Protocol files are not revised here. `docs/improvements.md` and `docs/completed_improvements_log.md` were not overwritten. |
| `docker/seed_rag/AGENTS.md` | RAG corpus copy of the guide, aligned for retrieval. It is not the editing authority. Its status text is shorter than the root guide and was not treated as current product truth. |

No other nested `AGENTS.md` files were found.

Documents read after the guides:

- `docs/EGAR_LINUX_DIAGNOSTIC_AB_PROTOCOL.md`
- `docs/EGAR_LX_EXPERIENCE_PATHWAY_INVESTIGATION.md`
- `docs/EGAR_LX_SAMPLE_SIZE_PLANNING_REPORT.md`
- `docs/EGAR_LX_FAMILY_DIVERSITY_FEASIBILITY_REPORT.md`

The sample-size and family-diversity reports do not change the architecture findings. They remain planning arithmetic. No sample size is adopted here.

Decision D2 in the protocol (approved 2026-10-14) says the experiment tests EGAR as currently implemented and does not enhance injection, thresholds, strategy promotion, or the planner. Section 4 approves keeping the current `success_score` heuristic and forbids writing external grades back into EGAR. Section 5 approves fixed historical evidence and says tool-restriction enforcement is not authorized by that approval. This plan does not edit that protocol. Where a confirmed defect would change what Arm B stores or retrieves, the item is flagged for a separate protocol decision.

## 2. Current architecture summary

A diagnostic goal, if started through `ExecutiveController::execute_goal`, uses the same Cognate path as any other goal. There is no diagnostic-goal type.

1. `execute_goal` (`external/basic_agent/src/executive_controller.cpp` lines 439–500) embeds the goal, may append one similar past plan, and calls `LLMPlanner::create_plan`.
2. `create_plan` (`llm_planner.cpp` lines 169–277) may add one strategy and one trajectory snippet, then asks the model for a plan. `PlanValidator::validateAndRepair` is called with `allow_tool_steps = false`. A rejected plan is retried once, then replaced by the C++ fallback `RETRIEVAL` then `LLM` (`plan_validator.cpp` lines 46–67 and 78–85).
3. `decide_transition` runs ready steps through `WorkflowEngine`. A normal finish computes `calculate_trajectory_score`, may reflect when that score is below 0.6, and otherwise calls `store_plan_history`.
4. `store_plan_history` (lines 1685–1756) writes `past_plans`, a cognate plan, and a trajectory, calls `reinforce_plan_graph`, runs `StrategyEngine::processTrajectories`, and may call `reinforce_plan_graph` a second time.
5. The next goal can inject at most one past plan, one trajectory snippet, and one strategy. `revise_plan` does not receive that block (`llm_planner.cpp` lines 314–329).

GRAG chunk scores can also change when `graph_edges` weights change (`grag_scorer.cpp` lines 197–225). That path does not put experience text in the planner prompt.

The approved external 0–4 diagnostic rubric is not this internal score. Section 4 of the protocol already says they stay separate.

## 3. Tool-permission enforcement findings

### 3.1 What the Cognate planner will accept

**Confirmed.** Both `LLMPlanner::create_plan` and `LLMPlanner::revise_plan` pass `allow_tool_steps = false` (`llm_planner.cpp` lines 276–277 and 359–360). `PlanValidator::validateAndRepair` rejects any step whose type is `TOOL` or whose payload contains `tool` or `tool_name` (`plan_validator.cpp` lines 18–26 and 78–84). The default plan template also says not to emit `TOOL` steps (`prompt_factory.cpp` lines 17–25). The validator is the interlock. The template sentence is not.

`tests/unit_tests.cpp` lines 2963–2968 already assert that a `TOOL` plan fails `assessGeneratedPlan` when `allow_tool_steps` is false.

**Confirmed for the goal path used by EGAR-LX.** A planner-generated `web_scrape` step does not reach `WorkflowEngine` on this path. It is a validation failure. If both generations fail validation, the fallback plan is retrieval plus synthesis and also contains no tool step.

**Cannot be established statically.** Whether a live model will still emit a `TOOL` step. That does not matter for execution on this path, because the validator rejects it before dispatch. Whether a workspace copy of `plan_generation.tmpl` could weaken the instruction is also irrelevant to execution: `buildPlanPrompt` loads that file if present (`prompt_factory.cpp` lines 480–503), but `allow_tool_steps` stays false in code. No `agent_workspace` copy of the template was present in the tree at review time. `ensureDefaultTemplatesExist` can write one at runtime (`prompt_factory.cpp` lines 534–550). That write was not performed here.

### 3.2 What still executes a tool if a plan already contains one

**Confirmed.** The validator is not consulted again at dispatch.

`WorkflowEngine::executeStepAsync` switches on `step.type` and calls `executeTool` for `StepType::TOOL` (`workflow_engine.cpp` lines 289–291). `executeRetrieval` also calls `executeTool` when the payload has a tool name (lines 488–490). `executeTool` resolves the name in `ToolRegistry`, and if `confirmed` is absent it sets `confirmed` to true (lines 421–425). `validateInput` checks required fields and a few JSON types. It does not enforce `additionalProperties: false` (lines 758–795), so the injected `confirmed` field does not by itself reject `web_scrape`.

`ToolRegistry::executeTool` (`tools.cpp` lines 50–71) checks `requires_confirmation()` and then calls `ITool::execute`. It does not consult `Config::enable_tools`, an allowlist, or `ConstraintChecker`.

`ExecutiveController::decide_transition` does call `ConstraintChecker`, but every `TOOL` step is labeled `tool_call` (lines 1041–1051). `ConstraintChecker::check_action` applies the domain list only when the action type is `network_request`, and the tool-call cap only when the payload contains `current_call_count` (`constraint_checker.cpp` lines 33–50). Nothing in the engine sets `current_call_count`. The cap of 20 therefore does not fire on this path. The blocked-domain list (`internal.google.com`, `secret.com`) does not apply to executive tool steps.

`Config::enable_tools` gates whether chat prompts include the tool list (`prompt_factory.cpp` lines 140 and 159–160). The checked-in `external/basic_agent/config.json` sets `enable_tools` to true and does not mention `allow_shell_exec`. The C++ default of `allow_shell_exec` is false (`config.cpp` line 32; `config.h` line 43). Disabling tools in the planner prompt would not stop `WorkflowEngine` from running a tool step that was already on a plan.

Paths that can still reach `executeTool`:

- A stored plan resumed through `resume_from_plan` (`executive_controller.cpp` lines 573–644). Resume does not re-run `PlanValidator`.
- Any caller that constructs a `Plan` with a `TOOL` step and executes it.
- The chat path, which is not `execute_goal`.

### 3.3 `web_scrape` and HTTP

**Confirmed implementation.** `WebScrapeTool::execute` (`web_scrape_tool.cpp` lines 57–125) performs `curl_easy_perform` unless `THOTH_MOCK_SCRAPE=true`. There is no scheme allowlist, no host allowlist, and no `allow_shell_exec` check. `CURLOPT_FOLLOWLOCATION` is set. `requires_confirmation()` returns false (line 25).

**Confirmed for Cognate goals.** That HTTP call is not reached from a plan produced by the current `LLMPlanner`, because the validator rejects the step first.

**Confirmed for chat.** `CommandProcessor::processToolCall` (`command_processor.cpp` lines 1073–1118) maps `web_scrape` to `network_request` and then calls `ToolRegistry::executeTool`. The domain check is a substring search against two hard-coded names. Any other URL is allowed. Chat does not auto-set `confirmed`, but `web_scrape` does not require it. This path runs from the conversational turn (`command_processor.cpp` line 500), not from the goal-banner Run path.

**Configuration-dependent.** `THOTH_MOCK_SCRAPE=true` returns fixed text and does not call curl. That variable is a test switch, not an experimental containment control.

**Not established statically.** Whether the linked libcurl build allows `file://` or other non-HTTP schemes. Thoth does not set `CURLOPT_PROTOCOLS`. Redirect targets after `CURLOPT_FOLLOWLOCATION` are also unrestricted in this code.

### 3.4 Shell

**Confirmed.** The planner-facing shell tools are `run_tests` and `code_modify` operation `build`. Both call `popen` only after `config_ && config_->allow_shell_exec` (`run_tests_tool.cpp` lines 79–91; `code_modify_tool.cpp` lines 118–132). With a null config pointer, or with the default `allow_shell_exec == false`, they return an error and do not call `popen`. `ToolRegistry::setConfig` is wired from the plugin (`basic_agent_plugin.cpp` line 141) and replaces those two tools with config-bearing instances (`tools.cpp` lines 35–38). `testAllowShellExecGate` in `tests/unit_tests.cpp` (line 5529) covers the false setting. It was not run in this review.

**Confirmed for a shell step on the goal path.** The validator rejects the step before dispatch. If a shell step were dispatched anyway, the default configuration would still refuse `popen`.

**Configuration-dependent.** `allow_shell_exec: true` in the loaded config would allow those two `popen` calls. The checked-in engine `config.json` does not set the flag, so the compiled default remains false unless some other loader sets it. This review did not start a process to print the runtime value.

**Not a goal-step shell, recorded so it is not mistaken for one.** `index_manager.cpp` line 683 and `git_metadata.cpp` line 48 call `popen` for git metadata. They are not `ToolRegistry` tools and are not gated by `allow_shell_exec`. Whether an EGAR-LX process invokes them depends on indexing and startup, which was not executed.

`code_modify` operation `read` is not shell. It reads a file under the project root when the path check passes (`code_modify_tool.cpp` lines 91–117). `revert` copies a backup file (lines 144–158). `apply_diff` is the known stub. None of these run on a validator-accepted Cognate plan. `project_analyze` walks a directory and reads source text (`project_analyze_tool.cpp` lines 54–80). Its `root_path` join does not use the same `..` rejection as `code_modify`. That matters only if the tool is actually dispatched.

### 3.5 Gmail and other network

**Confirmed.** `gmail_read_messages` returns mock JSON and does not call curl (`gmail_read_messages_tool.cpp` lines 31–76). `gmail_read_labels` does call curl against `https://gmail.googleapis.com/gmail/v1/users/me/labels` (`gmail_read_labels_tool.cpp` lines 72–83). It requires `credential_id` and `requires_confirmation() == true`. On the goal path the validator blocks the step. On the chat path, curl runs only if the model emits a confirmed tool call that passes the two-domain filter. The chat constraint check sets the Gmail URL to the literal `google.com` (`command_processor.cpp` lines 1097–1099), which is not on the block list.

Inference HTTP (`inference_http.cpp`, `llm_interface.cpp`) is the model backend. The protocol allows that network. It is not case-evidence retrieval.

### 3.6 Discrepancy with the 2026-10-15 pathway investigation

`docs/EGAR_LX_EXPERIENCE_PATHWAY_INVESTIGATION.md` section 7 said a plan-named tool is executed, and that `web_scrape` is incompatible if a plan calls it. The second sentence remains true of `WebScrapeTool` itself. The first sentence is not true of the current Cognate planner: `allow_tool_steps` is hardcoded false, and `tests/unit_tests.cpp` already covers that rejection. The earlier report inspected `executeTool` and did not follow `parsePlanWithValidation`. This review follows both.

The planner can still *propose* a prohibited step in model text. The controller, through the validator, rejects it before tool dispatch. That is not a broken goal-path boundary. The remaining gap is that dispatch, chat, and resume do not share that rejection.

### 3.7 Classification

| Claim | Class |
|---|---|
| A current `LLMPlanner` goal cannot execute `web_scrape` or `popen` | Confirmed for plans that pass through `create_plan` / `revise_plan` |
| Default config prevents `run_tests` and `code_modify build` from calling `popen` | Confirmed, and configuration-dependent if `allow_shell_exec` is later set true |
| `WebScrapeTool` will HTTP if something calls it | Confirmed implementation; not reached by the current planner |
| Chat `processToolCall` can still HTTP via `web_scrape` | Confirmed if that chat path is used |
| Executive `ConstraintChecker` is a network allowlist | Not supported. Domain checks are not applied to executive tool steps, and the call counter is never set |
| `enable_tools: false` is an execution interlock | Not supported. It changes prompt assembly |

No live HTTP request or shell-tool action was used to test these statements.

## 4. EGAR success-scoring findings

**Confirmed.** `calculate_trajectory_score` (`executive_controller.cpp` lines 1448–1463) is a mechanical heuristic.

- Empty plans return 0.
- The start value is 1 when the caller passes `plan_completed_successfully`, and 0 otherwise. The caller sets that flag from “every step status is `SUCCESS` and the plan is non-empty” (`decide_transition`, lines 891–902 and 980).
- Each recorded trajectory step with a non-empty `error` subtracts 0.1.
- If the plan was fully successful and `revisions_count_ > 0`, add 0.1. `revisions_count_` increments on plan revision (line 1258), which is a different counter from reflection.
- The result is clamped to [0, 1].

It does not read the LLM answer. An LLM step is successful when generation returns non-empty, non-error text (`workflow_engine.cpp` lines 722–736). An incorrect diagnosis that finishes that way scores 1.0. That matches the earlier investigation.

**What the score is for.** It is the controller’s record of mechanical completion. It is stored on `past_plans`, cognate plans, and trajectories (`store_plan_history`, lines 1692, 1710, and 1721). It is copied to `grag_metrics` as `final_success_score` (line 1752). Retrieval uses it as an eligibility and ranking feature:

- Past plans need `success_score >= 0.6`, with +0.1 cosine when the score is at least 0.8 (`memory.cpp` lines 733–755; floors in `plan_reuse_config.h` lines 21–29).
- Trajectories use the same 0.8 boost and a 0.55 cosine floor (`memory.cpp` lines 203–220).
- Strategy promotion averages it. Promotion needs at least three trajectories in the table, a repeated pattern of at least two steps, pattern count at least 3, and mean score at least 0.8 (`strategy_engine.cpp` lines 26–30 and 80–84).

Graph reinforcement does **not** use this value. It uses `PlanStatus`, below.

Reflection uses the value: below 0.6, and fewer than `max_reflections_` cycles, `decide_transition` stores history and replans (`executive_controller.cpp` lines 906–922). `revise_plan` is the in-step failure revision and does not receive the EGAR block. Reflection calls `create_plan` again, but the goal string is the clean goal plus a reflection suffix, and the comment at line 925 says plan-reuse injection is not reapplied there. Strategy and trajectory lookup inside `create_plan` still run, because `create_plan` does its own retrieval (`llm_planner.cpp` lines 190–223). The earlier report’s sentence that `revise_plan` omits experience remains true. Reflection is not `revise_plan`.

**Not a defect.** The score is not the external rubric. Protocol section 4 already requires that separation. Changing the heuristic so that it grades diagnostic truth would change which rows Arm B can inject. That is an estimand change, not a correctness fix. It is listed under P2 and is not recommended before the baseline.

**Genuine inconsistency.** The same finish path tells the controller `COMPLETED` and tells the graph `not PlanStatus::COMPLETED`. Those are different signals. See section 5. The score itself is applied consistently to storage, ranking, promotion, and reflection.

## 5. Graph reinforcement findings

### 5.1 Status never becomes `COMPLETED` on the normal finish path

**Confirmed defect.**

`PlanStatus` defaults to `ACTIVE` (`plan.h` line 82). `create_plan`, the fallback plan, and `revise_plan` set `ACTIVE` (`llm_planner.cpp` lines 135, 175, and 388; `plan_validator.cpp` line 49). The only production assignment found of a terminal `PlanStatus` on the controller path is `PlanStatus::FAILED` when resume refuses to replay an interrupted non-retrieval step (`executive_controller.cpp` line 696).

`decide_transition` sets `ControllerState::COMPLETED` or `ControllerState::FAILED` (line 980). It emits `PLAN_COMPLETED` or `PLAN_FAILED` (line 995). It does not assign `current_plan_.status`.

`abort` sets `ControllerState::ABORTED` and does not call `store_plan_history` (lines 553–565). It also does not set `PlanStatus::ABORTED`.

`reinforce_plan_graph` passes 1.0 only when `current_plan_.status == PlanStatus::COMPLETED`, and otherwise 0.0 (lines 1428–1430). `GraphRefiner::refineFromTrajectory` treats a value below 0.8 as failure and multiplies the weight by `(1 - learning_rate)` with `learning_rate` 0.2 (`graph_refiner.cpp` lines 20–39; `graph_refiner.h` line 31). A new edge starts at 0.1 (controller line 1418), so a penalty moves it to 0.08, then the clamp at 0.01 does not bind.

`run_cognate_benchmark.cpp` line 90 treats `PlanStatus::COMPLETED` as benchmark success. On this path that flag stays false even when the controller emits `PLAN_COMPLETED`. The EGAR-LX protocol does not use that harness. The mismatch is still evidence that plan status and controller completion have diverged.

Stored trajectory JSON copies `current_plan_.status` into `final_status` (`executive_controller.cpp` line 1713). After a normal success that integer remains `ACTIVE` (0), while `success_score` can be 1. Cognate plan rows store `static_cast<int>(state_)` (line 1709), and `ControllerState::COMPLETED` is 6, not `PlanStatus::COMPLETED` (1). Readers of those two columns are not looking at the same enum.

### 5.2 When the penalty actually runs

**Confirmed.** `reinforce_plan_graph` returns immediately when `active_sets_.size() < 2` (line 1388). That early return does not clear `active_sets_`. The clear is only after a refinement attempt (line 1433).

`active_sets_` grows only after a successful `RETRIEVAL` step whose diagnostics contain chunk text (lines 1176–1210). A standard one-retrieval, one-LLM success therefore does not update any edge at finish time.

The second call (lines 1732–1734) runs only when `success_score > 0`. If the first call refined, it already cleared the sets, so the second call returns immediately. If the first call returned early because there were fewer than two sets, the second call also returns early. The second call does not reward anything.

`execute_goal` does not clear `active_sets_` (lines 458–472). The vector is process memory. A later goal in the same controller keeps the previous goal’s retrieval sets. Two successive one-retrieval goals can therefore satisfy `size >= 2` on the second finish and create edges between chunk sets from different goals. Those edges then receive 0.0 because plan status is still not `COMPLETED`.

How often a single goal produces two retrieval chunk-sets was not measured. Static analysis cannot count it. The cross-goal accumulation does not require that measurement: one set per goal is enough if the process is reused.

On failure, abort, and timeout:

- A finished plan with any non-success step takes `ControllerState::FAILED`, still stores history, and would pass 0.0 into the refiner if at least two sets exist. Penalty on mechanical failure matches the refiner’s failure branch. It is not distinguishable today from penalty on mechanical success.
- `abort` does not reinforce and does not store a trajectory.
- A timeout fails the step (`workflow_engine.cpp` lines 386–394). The plan then follows the failed-step policy. If the plan later finishes through `decide_transition`, storage and the 0.0 graph value behave as any other unfinished-success plan. Soft-timeout cancellation remains the known unfinished join behavior in `AGENTS.md`. It was not re-investigated as an EGAR defect.

`store_plan_history` also calls `Memory::processMemoryAging` (line 1726), which multiplies every edge weight by 0.995 and may delete edges below 0.02 (`memory.cpp` lines 265–281). That decay runs on both successful and failed stores. It is existing aging policy, not the polarity bug. A disposable per-case database limits it to that case.

### 5.3 Intended behavior

`GraphRefiner`’s header says reinforcement follows “the success of a completed plan” (`graph_refiner.h` lines 24–27). `docs/GRAG.md` says the logistic rule rewards successful trajectories and penalizes failures. The controller’s own branch intends 1.0 on `PlanStatus::COMPLETED` and 0.0 otherwise. The missing assignment makes the reward branch unreachable on a normal goal. That is a defect, not a deliberate choice to penalize success.

Docs that call graph learning “operational” describe the refiner math. They do not show a production assignment of `PlanStatus::COMPLETED`. The code wins.

### 5.4 Experimental effect

If acquisition and evaluation each use a new process, and each plan has only one retrieval set, this bug writes no edges. Arm B’s graph channel is then empty for that reason. If acquisition reuses one process, or a goal retrieves twice, the snapshot can contain penalized edges, including edges that join different acquisition cases. GRAG adds `graph_weight * graph_score` to chunk scores. Those edges are inside the approved Arm B snapshot and can change rankings without any planner sentence.

Fixing the status assignment changes that behavior: a mechanically successful multi-set goal would reward edges instead of penalizing them. That changes what Arm B can retrieve. It is still a correctness fix. It needs an explicit protocol note because D2 says the experiment tests the current implementation. See section 12.

## 6. Experience storage and reuse findings

The earlier investigation’s storage-versus-injection table matches the current code.

| Channel | Persisted | Later planner sees | Discarded from the prompt |
|---|---|---|---|
| Past plan | Full plan JSON in `past_plans.outline` (`store_plan_history` lines 1688–1700) | Goal text, score, step count, and `TYPE(step_id)` via `sanitizePlanOutline` (`goal_text_utils.h` lines 79–107; `build_plan_reuse_context` lines 1922–1932) | Step results, tool arguments, and the written answer |
| Trajectory | Goal, initial plan, each step’s description, tool, result JSON, and error, revisions, `final_status`, `success_score`, embedding (`trajectory.cpp` lines 43–66). LLM result JSON includes the model text | Prior goal and `success_score` only (`llm_planner.cpp` lines 213–218) | The diagnosis paragraph and tool payloads |
| Strategy | Step-type or tool-name pattern and a success rate (`strategy_engine.cpp` lines 41–105) | Description, pattern JSON, rate (`llm_planner.cpp` lines 58–65) | The answer text. A pattern such as `RETRIEVAL->LLM` is not a Linux root cause |
| Graph | Edges between retrieval chunk hashes | No text. Weights change GRAG scores | Not applicable |

Caps: strategy 512 characters, trajectory-plus-plan text 512, plan-reuse block 1024 (`planner_injection_config.h` lines 25–27). If the core rules, schema, and goal consume the budget, `assemblePlannerPrompt` clears the whole experience block and sets `experience_dropped` (`prompt_factory.cpp` lines 95–104). Capping is not the same flag as a full drop.

When nothing matches, planning still runs. An empty database injects nothing. There is no EGAR disable flag. No injection is a valid Arm B observation under the protocol.

Experience can change a later plan without copying the prior answer, because the injected text is a goal, a mechanical score, and a step-type outline or strategy pattern. It cannot transfer the diagnostic paragraph itself. That is the implemented design. Protocol section 2 already describes those payloads and Decision D2 keeps them.

`Memory::clear` deletes session messages only (`memory.cpp` lines 192–195). It is not an Arm A reset.

Classification:

- **A. Defects.** Graph status and `active_sets_` lifetime (section 5). Trajectory `final_status` staying `ACTIVE` after controller completion is the same status defect, visible in stored JSON.
- **B. Deliberate limits.** Compressed injection, one item per channel, `revise_plan` without the EGAR block, mechanical `success_score`, promotion floors, and no EGAR off switch. Do not “fix” these to raise diagnostic scores.
- **C. Possible later enhancements.** Injecting a bounded lesson that is not the sealed answer; separating a diagnostic grade from `success_score`. Both change the estimand. Not in the first package.
- **D. Observability gaps.** Graph updates log a stdout line only (`graph_refiner.cpp` lines 59–64). The prompt body after capping is not stored; byte counts are. Nothing records whether the model used the injected text. `experience_dropped` does not by itself explain a case that selected nothing.

One correction to the earlier report: reflection calls `create_plan`, so strategy and trajectory retrieval can run on the reflection goal string. The reflection suffix is part of that string (`executive_controller.cpp` lines 926–944). Plan-reuse text is not appended again at the controller. This is narrower than “reflection sees no experience.”

## 7. State-isolation findings

Experience-bearing state that can affect a later goal:

| State | Where it lives | Survives a new case if |
|---|---|---|
| `past_plans`, `trajectories`, `strategies`, `graph_nodes`, `graph_edges`, `episode_steps`, `plans`, `facts`, `step_metrics`, `warm_memory`, sessions, messages, summaries | `memory.db`, schema in `sqlite_memory_repository.cpp` lines 55–226 | The same file is opened, or a copy of it is opened |
| In-memory trajectory index | Built in `Memory::loadTrajectoryIndex` from SQLite at `Memory` construction (`memory.cpp` lines 41 and 289–304) | The process keeps the old `Memory` object after the file is replaced |
| `active_sets_` | `ExecutiveController` member | The same controller runs the next goal |
| Goal, current, and trajectory embeddings | Controller and `RAGPipeline` | The same process starts another goal without a new pipeline. `execute_goal` does replace the goal embedding for the new goal |
| `rag_index.bin` and the corpus | Shared retrieval files | They are shared on purpose. A write during a case changes both arms |
| Retrieval weights | `retrieval_config.json` | The file differs between arms |
| Logs | `decision_trace.jsonl`, `app_log.jsonl`, `grag_metrics.jsonl`, `generation_calls.jsonl` | A later process reads them. They are not the SQLite snapshot. The indexer extension list was not re-audited by executing the indexer |
| Chat messages | `messages` plus `CommandProcessor` writes on the chat path (lines 1035–1038) | The goal path does not itself add chat turns. A Send during the run would |
| Facts | `store_fact` | Only if that tool runs. The planner path currently rejects the tool step. No production reader of `facts` into the planner was found in the pathway investigation, and this review did not find a new one |
| Tool registry and config | Process singletons | A process started with `allow_shell_exec` true, or `THOTH_MOCK_SCRAPE`, differs from one that was not |

The protocol’s Arm A / Arm B file strategy is sufficient only when all of the following hold:

1. Each case is a new process, or a full reconstruction of `Memory` and `ExecutiveController`, bound to that case’s database file.
2. Arm A’s new database has no rows the planner or GRAG will read. The protocol still requires an inspection of a freshly initialized database. This review did not open `memory.db` and did not count rows.
3. Arm B is a copy of the frozen snapshot, opened for that case only. Evaluation writes stay in the copy.
4. `rag_index.bin`, the corpus, weights, model, and tool configuration are identical and are not written by the case.
5. Replacing `memory.db` on disk while an old process keeps running is not treated as isolation. `loadTrajectoryIndex` will not rerun by itself.
6. `active_sets_` must not carry chunk sets across acquisition goals. A new process does that today. A long-lived acquisition process does not. That is independent of copying `memory.db` between evaluation cases.

`Memory::clear` is not sufficient. Copying only part of `memory.db` is not sufficient. Sharing one process across Arm A and Arm B is not sufficient.

## 8. Existing test coverage

Tests were located and not executed.

| Area | What exists | Gap |
|---|---|---|
| Goal completion | Unit tests wait for `ControllerState::COMPLETED` and `PLAN_COMPLETED`, and some check `success_score` near 1 or 0 (`tests/unit_tests.cpp` around lines 3740, 4871, 4890, 5356) | No assertion that `Plan::status` becomes `PlanStatus::COMPLETED` or `FAILED` when the controller finishes |
| Graph polarity | Refiner unit coverage was not found. Logging is stdout | No test that a successful two-set goal rewards edges, that a failed goal penalizes them, or that sets do not leak across `execute_goal` |
| Internal score | Reflection and spine tests depend on the heuristic indirectly | No direct test that a wrong-but-successful LLM answer scores 1, or that the score ignores answer text. A direct test is optional because the protocol keeps this heuristic |
| Experience creation | Tests store and read past plans and trajectories | They do not lock the injected field set (goal, score, outline only) |
| Experience retrieval | `retrieveSimilarPlans` thresholds are tested (around line 5583) | Trajectory floor and strategy cosine floor are not a single EGAR-LX regression |
| Experience injection | Planner logs and prompt metrics exist in product code | No test that `revise_plan` omits the block while reflection’s `create_plan` may include strategy and trajectory text |
| Tool permissions | `testAllowShellExecGate`; `assessGeneratedPlan(..., false)` rejects `TOOL` (line 2963) | No test that `execute_goal` never dispatches `web_scrape`. No test that chat `web_scrape` is denied. No test that resume revalidates tools |
| Isolation | Episodic tests use temporary databases | No test that a second `execute_goal` on one controller does not link the first goal’s chunk hashes |

`testCognitiveSpine` (`unit_tests.cpp` line 7515) calls `web_scrape` on the registry with `THOTH_MOCK_SCRAPE=true`. It does not prove goal-path containment, and it does not prove live HTTP.

## 9. Confirmed defects versus design limitations

**Confirmed defects**

1. Normal goal completion never sets `PlanStatus::COMPLETED`, so `reinforce_plan_graph` cannot take its reward branch.
2. `active_sets_` survives an early return and survives the start of the next `execute_goal`, so chunk sets can join across goals in one process and then be penalized.
3. Stored trajectory `final_status` stays `ACTIVE` after controller success. Same missing assignment.
4. Dispatch and chat do not enforce the planner’s tool ban. This is a real second path, not a failure of the planner ban itself.

**Design limitations, not defects**

- `success_score` measures mechanical completion.
- Injection omits the diagnostic answer.
- Promotion and similarity floors can yield no injection.
- `revise_plan` omits experience.
- `Memory::clear` is a message clear.
- Graph aging decays every stored edge on history write.
- There is no EGAR off switch. Arm A is an empty store.

**Not confirmed**

- That a standard one-retrieval goal writes any graph edge.
- That a fresh database is empty of planner-visible rows.
- The cosine distribution of Linux family goals.
- Runtime `allow_shell_exec` after all loaders run.
- libcurl’s default protocol set.

## 10. Prioritized remediation plan

No item below is authorized to implement until Steve approves that item.

### P0 — Architectural correctness

#### P0-A. Set terminal `PlanStatus` from the controller outcome

**Problem.** The graph reward branch is unreachable on a normal finish, and stored `final_status` does not match controller completion.

**Evidence.** Section 5.1. `decide_transition` lines 980–995 versus `reinforce_plan_graph` lines 1428–1430. No production write of `PlanStatus::COMPLETED` exists.

**Why a defect.** The refiner and the controller branch are written to reward `PlanStatus::COMPLETED`. The assignment is missing. This is not an experimental tuning change.

**Correction.** When `decide_transition` enters the terminal branch, set `current_plan_.status` to `PlanStatus::COMPLETED` if the same condition that emits `PLAN_COMPLETED` is true, and to `PlanStatus::FAILED` otherwise, before `store_plan_history`. On `abort`, set `PlanStatus::ABORTED` before persist. Do not pass `calculate_trajectory_score` into the refiner. The existing 1.0 / 0.0 branch then matches its own condition. Keep `GraphRefiner`’s 0.8 threshold unchanged.

**Files.** `ExecutiveController::decide_transition`, `ExecutiveController::abort`, and the status field those functions already persist. Tests in `tests/unit_tests.cpp`. `run_cognate_benchmark.cpp` will start reporting success when the controller completes; that is the intended reading of line 90, and it should be called out in the test note rather than specially patched.

**Expected behavior.** A mechanically successful goal with at least two retrieval sets rewards edges. A mechanically failed goal with at least two sets still penalizes them. A one-set goal still updates nothing, until P0-B stops those sets from accumulating.

**Tests.** Drive a controller with two successful retrieval results and assert `PlanStatus::COMPLETED` and a weight increase. Drive a failed plan and assert `PlanStatus::FAILED` and a weight decrease. Assert trajectory `final_status` matches. Do not require a live model.

**Risks.** Acquisition snapshots created after the fix can contain rewarded edges where today’s code would contain penalized edges or no edges. GRAG rankings on Arm B can change. Plans resumed from older rows that were left `ACTIVE` are unchanged until they run again.

**Estimand.** Yes. Arm B graph contents can change. Planner text does not change.

**Protocol.** Needs a written note that the pre-baseline graph polarity fix is accepted and that D2’s “as currently implemented” refers to the code after this fix, not to the unreachable reward branch. Do not edit the protocol in the implementation patch unless that note is separately approved.

**Order.** First, together with P0-B. No dependency on scoring or tools.

#### P0-B. Bound `active_sets_` to the goal that produced them

**Problem.** Retrieval sets accumulate across goals in one process and can be refined together.

**Evidence.** Early return at line 1388 does not clear. `execute_goal` lines 458–472 do not clear. Push is at line 1210. Clear is only at line 1433 after a real refinement.

**Why a defect.** Edges are defined as links inside a trajectory. Joining two goals’ chunk sets is cross-case graph contamination. The protocol’s per-case database copy does not reset this vector.

**Correction.** Clear `active_sets_` at the start of `execute_goal` and at the start of a reflection attempt before the new plan runs. On the early return in `reinforce_plan_graph`, clear as well when the function decides there is nothing to refine, so a one-set goal cannot leak. Do not invent edges for single-set goals.

**Files.** `executive_controller.cpp` (`execute_goal`, reflection branch in `decide_transition`, `reinforce_plan_graph`).

**Expected behavior.** Chunk hashes from goal N are not refined with hashes from goal N+1. A new process and a reused process behave the same.

**Tests.** Two sequential goals on one controller, one retrieval set each. Assert no edge is written between their hashes. One goal with two sets still refines, using the polarity from P0-A.

**Risks.** Low. Goals that only accidentally formed edges by accumulation will stop doing so.

**Estimand.** Yes, if acquisition would have reused one process. The fix removes a contamination path. It does not add new experience text.

**Protocol.** Same note as P0-A. The approved isolation story should say “new process or reconstructed controller,” and after this fix a reused controller no longer carries retrieval sets. Database isolation rules stay as written.

**Order.** Same patch as P0-A. The tests need both.

### P1 — Experimental readiness

#### P1-A. Execution-layer denial for tools the planner already rejects

**Problem.** `PlanValidator` blocks tools on `create_plan` and `revise_plan`. `WorkflowEngine`, resume, and chat do not.

**Evidence.** Section 3.2–3.5.

**Why this is not P0.** The goal path EGAR-LX will use already rejects `TOOL` steps. Calling the security boundary broken because the model can emit the word `web_scrape` would be the mistake the review was told to avoid. Leaving a live curl implementation behind that validator is still a containment gap if a resumed plan, a future `allow_tool_steps = true`, or a chat Send is used during a case.

**Correction.** In `WorkflowEngine::executeTool` and in the retrieval branch that forwards to it, refuse network and shell tools unless a single explicit config flag is set. Default that flag false. For the EGAR-LX profile the flag stays false, which matches today’s planner. Specifically deny `web_scrape`, `gmail_read_labels`, `run_tests`, and `code_modify` build at dispatch, before curl or `popen`, and log the denial on the step result. Do not rely on prompt text. Do not auto-set `confirmed` for those tools. Keep `PlanValidator` as it is.

A smaller alternative, if the owner wants less product surface change: refuse the same tools only when an experiment environment variable is set, and require that variable in the EGAR-LX run book. The product default would remain the current curl implementation for non-experiment chat. That alternative must still be enforced in the dispatcher, not in the prompt.

**Files.** `workflow_engine.cpp`, `tools.cpp` only if the registry is the chosen choke point, `config.h` / `config.cpp` if a flag is added, `tests/unit_tests.cpp`.

**Expected behavior.** A hand-built `TOOL` step named `web_scrape` fails closed on the goal dispatcher under the EGAR-LX setting and does not call curl. `run_tests` remains denied when `allow_shell_exec` is false even if the new flag is forgotten.

**Tests.** Dispatcher test with a fake or unconfigured curl: a `TOOL` step named `web_scrape` returns an error and does not require network. A validator test already covers planner rejection; keep it. A resume test: a stored plan with a `TOOL` step is failed closed or revalidated before dispatch.

**Risks.** Chat users who rely on `web_scrape` would lose it if the default is product-wide. That is why the flag default needs an owner decision. Gmail label reads would be denied on the goal path, which the planner already forbids.

**Estimand.** No change to experience text or scores if plans were already tool-free. A denied tool step cannot become a high-scoring mechanical success via live HTTP. That protects the fixed-evidence rule.

**Protocol.** Section 5 already says tool-restriction enforcement was not authorized by the rubric approval. Implementing P1-A needs its own approval. The protocol should then say the interlock is the dispatcher, not the sentence “Do NOT emit TOOL steps.” No rubric or sample-size change.

**Order.** After P0, before any pilot. Independent of P0 code, but do not start the pilot between them if the owner wants both.

#### P1-B. Make graph updates observable without changing weights

**Problem.** Edge changes are a stdout line. The protocol’s injection record does not include them, but the estimand includes graph edges.

**Evidence.** `graph_refiner.cpp` lines 59–64. Protocol section 2 lists graph edges as part of the combined effect.

**Correction.** Log one structured event per refinement: success flag, edge count, created, updated, mean delta. Do not log chunk text. Do not change the learning rate.

**Files.** `graph_refiner.cpp` or the controller call site, using `StructuredLogger` or the existing decision trace. Tests assert the event fields.

**Estimand.** No, if the payload is logs only.

**Protocol.** Optional extra measurement. Not a change to the primary score. Can be noted as instrumentation. The protocol already allows recording measurements that do not change prompts or tool outputs.

**Order.** With P0-A so the new polarity is visible in the pilot logs.

#### P1-C. Isolation run book, still without collecting cases

**Problem.** Copying `memory.db` is not the whole process image. Section 7.

**Correction.** Document, in the experimental procedure when the owner next amends it, that each case starts a new process on a case-private database; that Arm A emptiness is a recorded row count for trajectories, strategies, past plans, graph edges, and facts; and that corpus and `rag_index.bin` hashes are compared before the case and after it. Do not perform that inspection in this planning task.

**Estimand.** No code change.

**Protocol.** The fresh-schema count is already an unresolved gate in protocol section 2. This item does not close it.

**Order.** Before the pilot. Can be written with the P0 protocol note.

### P2 — Optional architecture improvements

These are not required to correct a confirmed defect. They are not recommended before the baseline. Each one changes what Arm B learns or what the score means, and would need a protocol amendment under Decision D2 and section 4.

#### P2-A. Do not replace `success_score` with the external rubric

Keep the heuristic. If a later experiment wants a diagnostic grade inside EGAR, store it in a new field and do not overwrite `success_score`. Otherwise promotion and the 0.6 / 0.8 floors change meaning and the approved estimand moves.

#### P2-B. Richer experience injection

Injecting findings, commands, or remedies would transfer more than the approved payload (goal, score, step-type outline, strategy pattern). That can also transfer a wrong diagnosis into the next prompt, which today’s injector avoids. Not a defect.

#### P2-C. Pass the trajectory score into `GraphRefiner`

Possible, but it is a second policy. P0-A restores the binary branch already in the controller. Using 0.8 on the trajectory score would also reward some partially penalized mechanical scores. Do not do both.

#### P2-D. Strategy patterns that name Linux actions

Promotion today keys off step type or tool name. With tools rejected, the pattern is `RETRIEVAL->LLM`. That cannot teach a filesystem-specific procedure. Changing promotion would be a new learning channel. D2 forbids it for this experiment.

#### P2-E. Clear or rebuild indexes inside a long-lived process

A supported reload API would make in-place database swaps safe. The simpler experimental control is a new process per case. Build a reload API only if the owner rejects per-case processes.

## 11. Proposed regression tests

Run only after implementation approval. Not run for this plan.

1. **Terminal status.** Successful mock plan ends with `ControllerState::COMPLETED`, `PlanStatus::COMPLETED`, `PLAN_COMPLETED`, and trajectory `final_status` completed. Failed mock plan sets `PlanStatus::FAILED`. Abort sets `PlanStatus::ABORTED` and does not call `store_plan_history`.
2. **Graph polarity.** Two retrieval sets, all steps successful: edge weight increases and `success_count` increments. Same topology with a failed step: weight decreases and `failure_count` increments. One retrieval set: no edge write.
3. **Set lifetime.** Two sequential goals, one set each, same controller: no cross-goal edge. Reflection that stores a mid-goal history does not leave the first attempt’s sets attached to a different goal.
4. **Score unchanged.** All-success plan scores 1.0. One recorded error subtracts 0.1. Answer text is not an input. This guards P0 from silently “improving” the heuristic.
5. **Planner tool ban, already present.** Keep the `assessGeneratedPlan(..., false)` rejection test.
6. **Dispatcher deny, with P1-A.** A constructed `web_scrape` step returns a logged denial and does not perform HTTP. `run_tests` with `allow_shell_exec` false still returns the existing error.
7. **Injection contract.** A stored trajectory with a long answer injects only goal and score. `buildRevisionPrompt` receives empty strategy and past-experience arguments. This locks the approved payload rather than expanding it.

## 12. Experimental protocol implications

Do not amend `docs/EGAR_LINUX_DIAGNOSTIC_AB_PROTOCOL.md` in the implementation of this plan unless Steve approves the amendment separately.

| Proposed change | Protocol effect |
|---|---|
| P0-A and P0-B | Changes graph edges Arm B may retrieve. Does not change planner payload, thresholds, or the external rubric. Conflicts with a literal reading of D2 until the owner accepts the fix as pre-baseline correctness. Required note: baseline SHA is after the fix; penalized-on-success is not the behavior under test. |
| P1-A | Matches the already approved fixed-evidence rule by enforcing it in the dispatcher. Section 5 said that enforcement was not part of the rubric approval. Needs its own approval. No statistical change. |
| P1-B | Instrumentation only, if logs stay outside the retriever. |
| P1-C | Closes no numerical gate by itself. The fresh-database row count remains required before a pilot. |
| P2 items | Would change learning or scoring. Out of scope until a later protocol version. |

The external 0–4 rubric, pair design, and “do not lower thresholds because promotion missed” rule stay as approved. Incorrect mechanical scores remain in the snapshot. They are not filtered to help Arm B.

The pathway investigation’s tool conclusion should not be copied forward as “a diagnostic goal will HTTP.” The current planner will not dispatch that step. The curl implementation is still present behind the validator.

## 13. Unresolved questions requiring owner decisions

1. Accept P0-A and P0-B as pre-baseline correctness even though graph weights on multi-retrieval successes will move from penalty-or-noop to reward? This is the Decision D2 exception.
2. Should P1-A deny network and shell tools for every goal, or only when an EGAR-LX flag is set? Product-wide denial affects chat `web_scrape`.
3. Is the experimental unit of isolation a new process per case? If yes, P0-B is still worth fixing, and P2-E is unnecessary.
4. Should the fresh-database row count be the next read-only task before any code change? It is already an open protocol gate and was not done here.
5. Should graph events be added to the approved injection record, or kept as engineering logs?
6. Is the compressed injection payload accepted as the experience Arm B is allowed to use? The earlier pathway report asked this. The protocol text already describes that payload. A yes confirms that P2-B stays deferred.

No sample size is requested.

## 14. Recommended first implementation package

Approve one package, and nothing else, on the next implementation decision:

**Package: P0 graph terminal status and retrieval-set lifetime.**

- Implement P0-A and P0-B only.
- Add regression tests 1, 2, 3, and 4 from section 11.
- Do not change `calculate_trajectory_score`.
- Do not change injection, promotion floors, or planner prompts.
- Do not change tool registration in that package.
- Do not edit the EGAR-LX protocol in that package. The approval sentence itself should record that the owner accepts the graph-polarity estimand change and that the protocol note will be a separate edit.

After that package is verified, the next approval decision is P1-A (dispatcher denial) plus P1-B (structured graph log), still without a pilot and still without case collection.

P2 stays unapproved.

---

**Files created by this review:** `docs/THOTH_ARCHITECTURE_INTEGRITY_REMEDIATION_PLAN.md`

**Files modified:** none besides that new plan.

**Not done:** source edits, configuration edits, protocol edits, tests, benchmarks, goals, network requests, snapshots, case collection, and the engineering pilot.
