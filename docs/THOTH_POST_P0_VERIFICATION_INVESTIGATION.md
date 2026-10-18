# Thoth Post-P0 Verification Investigation

**Date**: 2026-10-15  
**Status**: Investigation and planning only. No remediation is approved.  
**Authorization**: Steve authorized a read-only investigation of three issues found during the P0 architecture-correctness implementation. Production code, tests, configuration, databases, and the EGAR-LX protocol were not modified.

This report checks the current implementation. It does not treat the P0 completion notes as evidence.

---

## 1. Scope and authorization

Authorized:

- Read source, tests, documentation, Git history, and the existing full-suite log `/tmp/thoth-p0-core-tests.log`.
- Static path analysis.
- Create this report.

Not authorized, and not done:

- Modify production code, tests, configuration, databases, or `docs/EGAR_LINUX_DIAGNOSTIC_AB_PROTOCOL.md`.
- Modify `docs/THOTH_ARCHITECTURE_INTEGRITY_REMEDIATION_PLAN.md` or the other EGAR-LX reports.
- Run Thoth goals, new tests, benchmarks, network requests, the engineering pilot, or EGAR-LX acquisition or evaluation.

The P0 changes remain in the working tree: `external/basic_agent/include/executive_controller.h`, `external/basic_agent/src/executive_controller.cpp`, and `tests/unit_tests.cpp`. This investigation did not edit them.

---

## 2. Repository instructions reviewed

- Root `AGENTS.md` (planning gate, protocol lock, build-failure rule). Nested `docker/seed_rag/AGENTS.md` is a RAG corpus copy and is not the authority for this work.
- `docs/THOTH_ARCHITECTURE_INTEGRITY_REMEDIATION_PLAN.md` (P0 polarity and set-lifetime design).
- `docs/EGAR_LINUX_DIAGNOSTIC_AB_PROTOCOL.md` (mechanical `success_score` stays; graph learning is part of Arm B as implemented; no protocol amendment in this task).
- Current controller, workflow, GRAG, graph, memory, and planner sources listed below.
- `docs/GRAG.md` § Dynamic Graph Learning (logistic reward and penalty; it does not say aging increments counters).

---

## 3. Investigation A — Retrieval diagnostics and graph reinforcement

### Classification

**Confirmed integration defect.** Diagnostics are generated with the JSON fields the controller reads, then omitted from the retrieval step result. No other production path fills `active_sets_`. A normal successful goal therefore does not reinforce graph edges.

This is not an optional feature. `handle_step_completion` and `GraphRefiner` are implemented consumers. The missing step is attaching an object that already exists.

### Data flow

| Step | Location | What happens |
| --- | --- | --- |
| 1. Retrieval step | `WorkflowEngine::executeRetrieval` (`external/basic_agent/src/workflow_engine.cpp`) | Calls `rag_pipeline_->retrieveRelevant`. |
| 2. GRAG execution | `RAGPipeline::retrieveRelevant` (`external/basic_agent/src/rag.cpp`, about lines 40–261) | Builds a `GragDiagnostics` object and copies it to `outDiagnostics`. |
| 3. Breakdown fields | `GragScorer::rescore` (`external/basic_agent/src/grag_scorer.cpp`, about lines 161–166 and 228) | Sets `sb.code_text = chunk.code` and pushes the breakdown. |
| 4. JSON schema | `ScoreBreakdown::to_json` and `GragDiagnostics::to_json` (`external/basic_agent/include/grag_diagnostics.h`, lines 19–44 and 76–99) | Emits `code_text`, `file_name`, `symbol`, `final_score`, and a `breakdowns` array. |
| 5. Side channels | `rag.cpp` `logGragBenchmark` (about lines 264–279) and the `RETRIEVAL_DIAGNOSTICS` event | Persist or emit the full diagnostics object. They do not write the step result. |
| 6. Step result | `workflow_engine.cpp` about lines 600–631 | Copies chunks as `{file, content}`, plus `grag_alpha`, `grag_routing_mode`, and `retrieved_chunk_count`. Does not set `result.data["diagnostics"]`. The STRICT empty path also omits it. |
| 7. Completion | `ExecutiveController::handle_step_completion` (`external/basic_agent/src/executive_controller.cpp`, about lines 1145–1220) | Assigns `step.result = result.data`. Pushes `active_sets_` only when `step.result` contains `diagnostics.breakdowns` with non-empty `code_text`. |
| 8. Reinforcement | `ExecutiveController::reinforce_plan_graph` (same file) | Returns immediately when `active_sets_.size() < 2`, after clearing the vector (P0). Otherwise builds edges and calls `GraphRefiner::refineFromTrajectory`. |
| 9. Edge write | `GraphRefiner::refineFromTrajectory` (`external/basic_agent/src/graph_refiner.cpp`, lines 17–51) | Updates weight and one counter, then `memory_->addEdge`. |

### Producer–consumer mismatch

The field names match. The controller comment at the consumer says the breakdown must contain `code_text`, and `ScoreBreakdown::to_json` does contain `code_text`.

The mismatch is omission:

- Producer of the object: `GragScorer` and `RAGPipeline`.
- Producer of the step result: `WorkflowEngine::executeRetrieval`.
- Consumer: `ExecutiveController::handle_step_completion`.

`executeRetrieval` keeps `GragDiagnostics` in a local variable and copies only the scalar GRAG fields onto `result.data`. A search of production sources found no other assignment of a `diagnostics` object onto a retrieval step result, and no other push into `active_sets_`.

### Can a normal goal reinforce edges?

No, on the current production path.

- A successful retrieval step does not populate `active_sets_`, because the required object is absent.
- `reinforce_plan_graph` therefore takes the `size() < 2` return and writes no edge.
- The default validated plan is one `RETRIEVAL` step followed by one `LLM` step. Even after diagnostics are attached, one retrieval step yields one set, and reinforcement still does nothing.
- Two same-goal sets are possible only when one plan attempt completes two successful retrieval steps that both carry non-empty `code_text` breakdowns. The validator allows that shape. The default template does not emit it.

### Did the P0 tests bypass this gap?

Yes. `tests/unit_tests.cpp` `ExecutiveControllerP0TestAccess::pushSet` inserts synthetic chunk hashes and then calls `decide_transition` / `reinforce_plan_graph` directly. Those tests do not call `executeRetrieval` and do not require a `diagnostics` object. They correctly test P0 status polarity and set lifetime. They do not prove that a live retrieval step can create an edge.

---

## 4. Investigation B — Memory aging and graph counts

### Classification

**Confirmed pre-existing defect.** One goal outcome is stored as a larger counter than the refiner incremented. P0 did not introduce the SQL. P0 made the success branch reachable when two or more chunk sets exist, so the same defect now applies to rewarded edges as well as penalized ones. With Investigation A unfixed, normal retrieval still never reaches this path.

### Trace

1. `GraphRefiner::refineFromTrajectory` copies an edge, increments `success_count` or `failure_count` once, and passes that absolute record to `Memory::addEdge` (`graph_refiner.cpp` lines 28–51).
2. `SQLiteMemoryRepository::addEdge` (`sqlite_memory_repository.cpp` lines 615–634) inserts the row. On `PRIMARY KEY (from_id, to_id)` conflict it runs:

   `success_count = graph_edges.success_count + excluded.success_count`  
   `failure_count = graph_edges.failure_count + excluded.failure_count`  
   `weight = excluded.weight`

   The caller passes an absolute count. The conflict clause treats that value as a delta.
3. A first insert of a new edge is correct: there is no conflict, so the stored count is 1.
4. A later refine of that same edge loads the stored count, adds 1 in memory, then the SQL adds the new total to the stored total. One additional outcome increases the stored count by more than 1.
5. `ExecutiveController::store_plan_history` calls `reinforce_plan_graph`, then `Memory::processMemoryAging` (`executive_controller.cpp` lines 1723–1745).
6. `Memory::processMemoryAging` (`memory.cpp` about lines 228–287) multiplies every weight by `0.995`, drops edges below `0.02`, and calls `repo->addEdge(edge)` with the loaded counters still set. The comment at the call is “Update weight”. The SQL adds the counters again. One fresh outcome therefore becomes `success_count == 2` or `failure_count == 2` before any second goal.
7. The second `reinforce_plan_graph` call when `success_score > 0` (lines 1751–1753) does not add another increment after P0, because the first call already cleared `active_sets_`.

`PRAGMA foreign_keys` is not enabled in this repository file. Edge rows can persist without matching `graph_nodes` rows. That is separate from the counter bug.

### What the counters affect

- GRAG ranking uses `edge.weight` inside `GragScorer`. It does not read `success_count` or `failure_count`.
- Pruning uses weight `< 0.02` inside `processMemoryAging`.
- Experience retrieval and EGAR injection use trajectory `success_score` and cosine similarity, not these counters.
- `SQLiteMemoryRepository::getGraphStatistics` sums the counters for the graph panel (`sqlite_memory_repository.cpp` lines 730–739; `src/GraphPanel.cpp`).

Weight decay (`0.995`) does change future graph scores. The doubled counters do not, except as displayed statistics and as inputs to the next absolute-plus-delta update.

### Intent versus documentation

`docs/GRAG.md` describes the logistic rule (learning rate `0.2`): reward success, penalize failure. It does not say aging should increment counters. The aging function’s own behavior is weight decay, a dormancy penalty, and pruning. Passing the loaded counters through an additive upsert is inconsistent with that.

### Relation to P0

The additive conflict clause and the aging upsert exist in the pre-P0 sources. Before P0, `reinforce_plan_graph` passed `0.0` unless `PlanStatus` was `COMPLETED`, and the normal finish path left the plan `ACTIVE`, so a successful goal penalized edges when sets existed. Those penalties were then doubled by the same upsert. P0 makes a successful multi-set goal pass `1.0`, so the reward branch and its doubled `success_count` are now reachable. Investigation A means that branch is still not reached by a normal retrieval step.

The P0 polarity test observed this: the refiner log showed weight `0.2800` and one success, and the assertion had to accept `success_count >= 1` because aging stored `2`. That observation is from the earlier approved P0 run. This investigation did not re-run it.

### Smallest correction (not implemented)

Change the `ON CONFLICT` clause to assign `excluded.success_count` and `excluded.failure_count` instead of adding them.

Both callers pass absolute counts:

- `GraphRefiner::refineFromTrajectory`
- `Memory::processMemoryAging`

No caller passes a delta. After the assignment change, a repeat refine stores the incremented absolute count, and aging rewrites the same count while still replacing `weight` and `last_used_ms`.

An equivalent narrower change is a weight-and-timestamp `UPDATE` used only by aging, leaving `addEdge` additive. That would fix aging and leave the refiner’s second update of an existing edge still doubled. The conflict-clause assignment fixes both.

---

## 5. Investigation C — Planner-hash failure

### Classification

**Confirmed obsolete test expectation. Not a P0 regression.** The planner behavior matches a later test that was added for the LLM-facing revision boundary. The failing assertion still hashes the older `Plan::to_json()` serialization. The suite log does not split which clause of a three-part `if` failed; static comparison shows the serialization clause fails for this fixture, and the two ceilings do not enter prompt construction.

### Evidence

Full-suite artifact from the approved P0 run, not re-executed here:

- `/tmp/thoth-p0-core-tests.log` prints `mtcp wrapper hash not stable across ceilings`.
- The process exited `1` with `1 test(s) failed.`
- The targeted `THOTH_P0_STATUS=1` run had exited `0` before that.

Assertion (`tests/mtcp_generation_checks.inc` lines 325–328), introduced in parent commit `fc83836` (2026-09-28):

```cpp
if (hash_512.empty() || hash_512 != hash_2048 || hash_512 != Thoth::sha256Hex(
        prompts->buildRevisionPrompt(plan.goal, plan.to_json().dump(), failed.dump(), nullptr))) {
    std::cerr << "mtcp wrapper hash not stable across ceilings\n";
```

`hash_512` and `hash_2048` come from `LLMPlanner::revisionWrapperSha256` after `GenerationBudget::setForTests` ceilings of 512 and 2048.

Production hash (`external/basic_agent/src/llm_planner.cpp` lines 418–431):

- `buildRevisionPromptText` calls `buildRevisionPrompt` with `existing_plan.toLlmFacingJson().dump()`.
- `revisionWrapperSha256` hashes that text.

`Plan::to_json` (`external/basic_agent/src/plan.cpp` lines 114–126) emits `plan_id`, `goal`, `status`, timestamps, and `steps` with runtime fields. `Plan::toLlmFacingJson` (lines 94–111) emits `{ "plan": [ {step_id, step_type, description, ...} ] }`. The fixture in the MTCP test sets status, failure policy, result, and an integer step type, so the two dumps differ.

`PromptFactory::buildRevisionPrompt` (`prompt_factory.cpp` lines 506–531) sizes the prompt with `max(config.maxContextLength, kMinPlanPromptBudget)`. `kMinPlanPromptBudget` is 8192. `GenerationBudget` is not read there. It changes generation `num_predict` on LLM calls, which this hash function does not make. With a stable template, `hash_512` and `hash_2048` are the same string hash. The failure message still prints when only the third clause fails.

Parent commit `aeac1f9` (2026-09-29, “Record the LLM-facing revision boundary in the core tests”) added `testPlanLlmFacingRevisionBoundary` in `tests/unit_tests.cpp` (about lines 2807–2903). That test expects `revisionWrapperSha256` to equal the SHA-256 of the prompt built from `toLlmFacingJson()`, and expects production `to_json()` to remain the runtime record. Submodule commit `eed2f12` (2026-09-29, “Render the existing plan in the parser schema during revision”) is an ancestor of the current `external/basic_agent` HEAD `24a0398`. The MTCP assertion was not updated when that boundary landed.

P0 edits `executive_controller.cpp` and the P0 tests. It does not call `revisionWrapperSha256`, `buildRevisionPrompt`, or `GenerationBudget`.

### What the hash is for

`revisionWrapperSha256` is the identity of the revision prompt wrapper: goal text, the LLM-facing plan JSON, and the failed-step JSON, with a null metrics pointer. Ceiling stability means that changing `GenerationBudget` does not change that wrapper. The current implementation satisfies that, because the budget is not an input. Equality with `plan.to_json()` was true on 2026-09-28 and stopped being the production input on 2026-09-29.

The assertion should keep the 512-versus-2048 equality. The expected digest should use `toLlmFacingJson().dump()`, which is what `testPlanLlmFacingRevisionBoundary` already locks. That is a test correction. It is not a planner change and not a weakening of the ceiling check.

This investigation did not re-run the assertion, so it does not have a fresh split of the three `||` clauses. The static inequality between `to_json()` and `toLlmFacingJson()` is sufficient to fail the test as written.

---

## 6. Confirmed facts versus unresolved questions

### Confirmed

- Retrieval diagnostics are generated, logged, and event-emitted, and they use the field names the controller reads.
- `WorkflowEngine::executeRetrieval` does not attach them to `result.data`.
- `active_sets_` is filled only from that missing object.
- A normal retrieval step does not reinforce graph edges.
- P0 regression tests inject synthetic sets and bypass this wiring.
- One retrieval set is not enough to form an edge, by the existing `size() < 2` rule.
- `addEdge` adds absolute counters on conflict. Aging and a second refine of an existing edge both inflate counts.
- Counters are not GRAG score inputs. Weights are.
- The hash failure predates P0 and disagrees with the LLM-facing revision test added the next day.

### Unresolved without a further approved run

- Whether any live plan in the EGAR corpus emits two retrieval steps. The code path allows it. The default template does not.
- A fresh split of the hash assertion’s three clauses. Static analysis already explains the failure.

---

## 7. Architectural and experimental implications

### Cognate goal execution

Confirmed: goals still complete. P0 still records `PlanStatus::COMPLETED` or `FAILED` before reinforcement. Missing diagnostics do not change step success or `success_score`.

### GRAG retrieval

Confirmed: chunk ranking for the step still happens. The score breakdown is computed and then dropped from the step result. Ranking weights are unchanged by the omission.

### Graph reinforcement

Confirmed: the normal path does not write edges. The P0 reward branch is latent until a step result carries two same-goal diagnostic sets. Cross-goal leakage remains closed by the P0 clear.

Hypothetical, only if diagnostics are later attached and a plan retrieves twice: a successful goal would reward edges (weight `0.1 → 0.28` before aging decay) instead of the pre-P0 penalty. That is the estimand change already recorded for P0. It is not active on the standard one-retrieval plan.

Graph reinforcement is not evidence that diagnostic answers improve.

### EGAR acquisition and reuse

Confirmed: episodic `success_score`, plan-history reuse, and trajectory injection do not read graph counters or `active_sets_`. The diagnostics gap does not change what those channels store. Strategy promotion still uses trajectory scores.

### EGAR-LX A/B and a reproducible baseline

Confirmed today: Arm B’s graph channel does not learn from a normal retrieval step, in either arm, because the step result never supplies chunk sets. A baseline frozen now would freeze that inert channel. P0 polarity would not yet appear in `graph_edges` for standard plans.

Hypothetical if Investigation A is fixed and plans stay one-retrieval: still no edges. The baseline graph stays empty for that plan shape.

Hypothetical if Investigation A is fixed and any official case has two retrieval steps: Arm B would start writing rewarded edges, then aging would both decay weights and double counters. That changes future graph-augmented retrieval for later cases in the same process and database. It would require an explicit experimental note before baseline freeze. It would not, by itself, change the protocol’s mechanical `success_score`.

The counter defect does not change GRAG rank. It does change graph-panel totals and the stored history of an edge. Fixing it before any edges are written does not move a baseline that has no edges. Fixing it after edges exist changes stored counts only.

The hash failure does not affect goal execution, retrieval, or the experimental baseline. It blocks a clean full-suite signal.

---

## 8. Recommended remediation package

Three separate corrections. None is an optional improvement of ranking or scoring. Do not implement until Steve approves a subset.

### R1 — Attach retrieval diagnostics to the step result

- **Root cause.** `executeRetrieval` drops `GragDiagnostics` when it builds `result.data`.
- **Evidence.** `workflow_engine.cpp` result construction versus `grag_diagnostics.h` `to_json` versus `handle_step_completion`.
- **Change.** On the non-error retrieval paths, set `result.data["diagnostics"]` from `diagnostics.to_json()`. Do not invent new fields. Do not change ranking, `k` selection, or the `size() < 2` rule.
- **Before.** Retrieval succeeds, `active_sets_` stays empty, no edge.
- **After.** One retrieval step stores one set and still writes no edge. Two successful retrieval steps in the same attempt can write an edge, and P0 polarity applies.
- **Baseline.** Changes Arm B only for multi-retrieval goals. Protocol review is required before that behavior is part of a frozen baseline. Standard one-retrieval plans do not change.
- **Risk.** Larger step-result JSON, including chunk text already held in `code_text`. Event and benchmark copies already contain that text.

### R2 — Store absolute edge counters

- **Root cause.** `addEdge` adds `excluded` counters to stored counters, while both callers pass absolute counts. Aging then upserts the same record.
- **Evidence.** `sqlite_memory_repository.cpp` conflict clause; `graph_refiner.cpp` increment-then-add; `memory.cpp` `processMemoryAging`.
- **Change.** On conflict, assign `success_count` and `failure_count` from `excluded`. Keep `weight = excluded.weight`.
- **Before.** A new edge is inserted as 1, then aging stores 2. A later refine adds the new total to the old total.
- **After.** One outcome stays 1 after aging. Weight still decays by `0.995`.
- **Baseline.** Does not change GRAG weights’ update rule. It stops counter drift. Protocol review is needed only if the owner treats counters as part of the estimand. `docs/GRAG.md` describes weights, not aging increments.
- **Risk.** Any unseen caller that relied on delta addition would store a smaller count. The only callers in this tree pass absolute counts.

### R3 — Align the MTCP wrapper expectation with the LLM-facing plan

- **Root cause.** `testMtcpRevisionWrapperStable` still hashes `plan.to_json()` after revision prompts moved to `toLlmFacingJson()`.
- **Evidence.** `tests/mtcp_generation_checks.inc` lines 325–328; `llm_planner.cpp` lines 418–431; `testPlanLlmFacingRevisionBoundary`; commits `fc83836`, `eed2f12`, `aeac1f9`.
- **Change.** Build the expected prompt with `plan.toLlmFacingJson().dump()`. Keep the check that the 512 and 2048 ceiling hashes are equal and non-empty. Split the error text so a ceiling mismatch is not reported as a serialization mismatch.
- **Before.** The assertion fails on current planner behavior.
- **After.** The assertion matches production and the existing facing-JSON test.
- **Baseline.** No product or experimental change.
- **Risk.** None on the agent. The ceiling check remains.

R1 and R2 are product defects. R3 is a test defect. Shipping R1 without R2 would persist doubled counters on every reinforced edge. Shipping R2 without R1 does not create edges.

---

## 9. Proposed regression tests

Do not add these until the corresponding correction is approved.

**R1**

- A retrieval result that already contains `diagnostics.breakdowns[0].code_text` is stored on the step and pushes one `active_sets_` entry. This can be a controller-level result object, as the P0 tests do, plus one workflow-level test that the JSON from `GragDiagnostics::to_json` is present on `executeRetrieval`’s `result.data`.
- Two such results in one goal, then a successful finish, write one edge with weight above the initial `0.1` after R2 (or `success_count >= 1` if R2 is not in the same change).
- One retrieval result still writes no edge.
- A second goal does not reuse the first goal’s set (existing P0 isolation test).

**R2**

- Refine one new edge with success, run `processMemoryAging` once: `success_count == 1`, `failure_count == 0`, weight equals the refiner weight times `0.995` (about `0.28 * 0.995`).
- Refine the same edge again with success: `success_count == 2`, not `1 + 2`.
- Failure path: one refine plus aging leaves `failure_count == 1` and weight below `0.1`.

**R3**

- `revisionWrapperSha256` at ceilings 512 and 2048 is equal.
- That digest equals `sha256Hex(buildRevisionPrompt(..., toLlmFacingJson().dump(), ...))`.
- It does not equal the digest of the `to_json()` prompt for a plan that carries runtime fields.
- `testPlanLlmFacingRevisionBoundary` stays as the schema lock.

**Unchanged**

- P0 terminal status, polarity, isolation, and `calculate_trajectory_score` tests.
- No change to `success_score`, injection floors, or GRAG weights.

---

## 10. Additional verification requiring authorization

Not required to classify the three issues. Request approval before any of these runs:

1. One mocked workflow retrieval against a tiny in-memory index, asserting whether `result.data` contains `diagnostics`. This would confirm the static omission at runtime. It is redundant with the source trace.
2. Re-run only `testMtcpRevisionWrapperStable` under a debugger or a temporary print of the three booleans, to show the log line is the `to_json` clause. Static inequality already shows that clause fails. Do not change the test to do this.
3. Do not run diagnostic goals, the pilot, or an A/B until Steve decides whether R1 is in the baseline. A baseline recorded before R1 freezes an inert graph channel. A baseline recorded after R1 changes behavior only for goals that retrieve twice.

---

## 11. Files created or modified

Created:

- `docs/THOTH_POST_P0_VERIFICATION_INVESTIGATION.md` (this file).

Not modified:

- Production sources, tests, configuration, databases, `docs/EGAR_LINUX_DIAGNOSTIC_AB_PROTOCOL.md`, and `docs/THOTH_ARCHITECTURE_INTEGRITY_REMEDIATION_PLAN.md`.
