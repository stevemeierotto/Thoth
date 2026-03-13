# Thoth: Next Steps Roadmap

**Version:** 1.0
**Status:** Authoritative — Successor phases to Cognate Phases 1–8 and Basic Agent Stabilization (continues from improvements.md Phases 3–5; this file covers Phases 9–12)

---

## Rules for This Document

1. Completed work must be moved to `docs/completed_improvements_log.md`.
2. Architecture references: `PLAN.md`, `GRAG.md`, `cognate.md`, `architectural_facts.md`.
3. Before implementing any phase, read both the relevant spec AND `architectural_facts.md`. Status takes precedence — do not re-implement or regress completed work.
4. Roadmap items must not contradict architecture documents.
5. **Gemini must pause for confirmation between every step and every phase.**

---

## How to Use This Document (For Gemini)

You are implementing the Thoth next-phase roadmap.

Before making any changes:
- Read `AGENTS.md` in full.
- Read the architecture document referenced in the phase you are implementing.
- Read `architectural_facts.md` to confirm what is already implemented.
- Confirm all naming conventions: `snake_case` filenames, `PascalCase` class names.
- New source files → `external/basic_agent/src/`
- New headers → `external/basic_agent/include/`
- Do NOT touch GUI files.
- Do NOT add new external dependencies unless explicitly stated.
- Add all new `.cpp` files to `external/basic_agent/CMakeLists.txt`.

**At every step:**
- Output only what the step requests.
- Explain reasoning clearly.
- Keep implementation minimal and testable.
- **Wait for explicit confirmation before proceeding to the next step.**

**At every phase boundary:**
- Summarize what was completed.
- List all files created or modified.
- **Wait for explicit confirmation before starting the next phase.**

---

## Phase Order

| # | Phase | Priority |
|---|---|---|
| 9 | LLM-Driven Step Generation | Critical |
| 10 | Resume Completeness | High |
| 11 | Dynamic Plan Revision | High |
| 12 | Extended Agent & Tool Re-enablement | Medium |

---

---

# Phase 9 — LLM-Driven Step Generation

**Architecture references:** `PLAN.md`, `cognate.md`

**Goal:** Replace the hardcoded 3-step scaffold with a fully dynamic planning loop where the LLM generates, parses, and executes arbitrary plan steps. This is the primary unlock for full agent autonomy.

> **Note for Gemini:** The `LLMPlanner`, `IPlanner`, and `Plan` data structures are already implemented. Do not re-implement them. This phase adds a robust dynamic JSON parser and wires it into the existing planner output. Read `architectural_facts.md` and `cognate.md` before touching any planner or controller code.

---

## Step 9.1 — Audit the Current Scaffold (NO CODE YET)

**Description:**
Audit the existing `LLMPlanner::createInitialPlan()` implementation to precisely document where the 3-step scaffold is enforced and what the LLM output currently looks like.

Define:
- The exact location(s) in `LLMPlanner` where fixed steps are generated
- The current prompt template used for plan generation
- The expected JSON schema for a dynamic plan (list all required and optional fields per step)
- All places in `ExecutiveController` or `WorkflowEngine` that make assumptions about step count or step shape

**Output required:**
- Audit summary (inline — no code)
- Proposed JSON schema for a dynamic `PlanStep` (fields: `step_id`, `step_type`, `description`, `tool_name` (optional), `payload` (optional), `timeout_ms` (optional), `depends_on` (optional))
- List of all files that will need to change in Steps 9.2–9.4

> ⏸ **PAUSE — Wait for confirmation before Step 9.2**

---

## Step 9.2 — Dynamic Plan JSON Parser

**Description:**
Implement a robust parser that converts raw LLM JSON output into a validated `Plan` object with an arbitrary number of steps.

Requirements:
- New class: `PlanParser`
- Input: raw string (LLM response, may contain markdown fences or preamble — strip safely)
- Output: `std::optional<Plan>` (returns `std::nullopt` on parse failure)
- Validates all required fields per step using the schema defined in Step 9.1
- Assigns deterministic `step_id` values if the LLM omits them (`plan_id + "-step-" + index`)
- Logs a structured `PARSE_FAILED` event via `DecisionTraceLogger` on any validation error, including the raw LLM output and the specific field that failed
- Must handle: extra fields (ignore gracefully), missing optional fields (apply defaults), malformed JSON (return `nullopt`)
- No external JSON libraries — use the existing JSON handling pattern in the codebase

**Output required:**
- `plan_parser.h` and `plan_parser.cpp`
- Unit tests: valid multi-step plan, missing required field, malformed JSON, markdown-fenced input, empty step array

> ⏸ **PAUSE — Wait for confirmation before Step 9.3**

---

## Step 9.3 — Dynamic Prompt Template

**Description:**
Update the `LLMPlanner` prompt template to instruct the model to generate a fully dynamic, variable-length plan in the validated JSON schema.

Requirements:
- Update the plan generation prompt template in `agent_workspace/prompt_templates/`
- Prompt must include: the exact JSON schema, a worked example with 3+ steps of different types, explicit instruction that step count is variable (not fixed)
- Prompt must instruct the model to output JSON only — no preamble, no explanation
- Do NOT change any planner logic in this step — only the template
- Add a new template variable `{available_tools}` that is populated from `ToolRegistry` when tools are enabled (no-op when tools are disabled per `Config::enable_tools`)

**Output required:**
- Updated prompt template file
- Confirmation that the existing `PromptFactory` variable substitution system handles `{available_tools}` correctly (or a minimal fix if it does not)

> ⏸ **PAUSE — Wait for confirmation before Step 9.4**

---

## Step 9.4 — Wire Parser into LLMPlanner

**Description:**
Replace the scaffold logic in `LLMPlanner::createInitialPlan()` with a call to `PlanParser`, making dynamic step generation the default path.

Requirements:
- `LLMPlanner::createInitialPlan()` calls `PlanParser::parse()` on the LLM response
- On `nullopt`: retry the LLM call once with an augmented prompt that includes the parse error, then return a `PLAN_FAILED` status if the second attempt also fails
- On success: pass the parsed `Plan` directly to the `ExecutiveController` — no step count assumptions
- Remove the scaffold — do not keep it as a fallback
- Existing plan persistence (SQLite `plans` table) must continue to work unchanged

**Output required:**
- Updated `llm_planner.cpp`
- Updated `llm_planner.h` if interface changes are required
- Unit tests: verify end-to-end plan creation produces a variable-length `Plan` object via the parser

> ⏸ **PAUSE — Wait for confirmation before Phase 9 close-out**

---

## Phase 9 Close-Out

Before moving to Phase 10:
- Run full build and unit tests
- Execute a goal end-to-end: confirm the generated plan has a variable number of steps driven by the LLM
- Inspect `decision_trace.jsonl`: verify no `PARSE_FAILED` events on a clean run
- Confirm plan is persisted correctly in SQLite `plans` table
- Confirm all existing Cognate Phase 1–8 baseline checks still pass

**Summarize:**
- Files created
- Files modified
- Any deferred items

> ⏸ **PAUSE — Confirm Phase 9 complete before starting Phase 10**

---

---

# Phase 10 — Resume Completeness

**Architecture references:** `architectural_facts.md` (§6 Resume Compatibility), `PLAN.md`

**Goal:** Ensure the `decision_trace.jsonl` log and the SQLite persistence layer contain enough information to fully reconstruct and resume a plan after a crash or restart, eliminating the four known gaps identified in `architectural_facts.md §6`.

> **Note for Gemini:** The `resume_from_plan()` method and `active_plans` SQLite table already exist. This phase enriches the data written at each lifecycle event — it does not redesign the resume mechanism. Read `architectural_facts.md §6` in full before writing any code.

---

## Step 10.1 — Audit Current Resume Gaps (NO CODE YET)

**Description:**
Map the four known resume gaps from `architectural_facts.md §6` to specific lines in the codebase.

Define:
- Exact `emit_event()` call sites that are missing metadata (file + line number)
- The `current_plan_.to_json()` method: confirm it exists and produces complete output, or flag what it is missing
- The `current_index` field: confirm how it is tracked internally and whether it is accessible at each emit site
- The `step.result` field: confirm its type and where it is populated after `dispatch_step()` returns

**Output required:**
- Gap-to-code mapping table (inline — no code)
- Confirmation that `Plan::to_json()` and `Plan::from_json()` are sufficient for round-trip reconstruction, or a list of missing fields

> ⏸ **PAUSE — Wait for confirmation before Step 10.2**

---

## Step 10.2 — Enrich PLAN_CREATED and PLAN_REVISED Events

**Description:**
Add the full serialized plan to `PLAN_CREATED` and `PLAN_REVISED` event metadata so a replayer can reconstruct the plan structure from the log.

Requirements:
- `PLAN_CREATED` metadata must include: `current_plan_.to_json()` output
- `PLAN_REVISED` metadata must include: `current_plan_.to_json()` output (post-revision state)
- `current_index` must be included in both events
- Do not change the outer `DecisionTraceLogger` envelope schema — add fields inside `stages[0].metadata` only
- Verify the enriched events still pass `check_baseline.py`

**Output required:**
- Updated `executive_controller.cpp` (emit sites for `PLAN_CREATED` and `PLAN_REVISED` only)
- Unit test: verify `PLAN_CREATED` event contains parseable plan JSON with all steps

> ⏸ **PAUSE — Wait for confirmation before Step 10.3**

---

## Step 10.3 — Enrich STEP_COMPLETED and All Step Events

**Description:**
Add `step.result` and `current_index` to step-level events so execution state is unambiguous at every point in the log.

Requirements:
- `STEP_COMPLETED` metadata must include: `step.result` (full payload, not truncated), `current_index`
- `STEP_FAILED` metadata must include: `step.error_message`, `current_index`
- `STEP_RETRYING` metadata must include: `retry_count`, `current_index`
- `STEP_STARTED` metadata must include: `current_index` (for replay ordering)
- All other events (`PLAN_COMPLETED`, `PLAN_ABORTED`) must include: `current_index`
- If `step.result` is large (> 4KB), store a SHA-256 hash and truncated preview; log full result to a separate `step_results/` directory keyed by `step_id`

**Output required:**
- Updated `executive_controller.cpp` (all remaining emit sites)
- Unit test: simulate a 3-step plan completion and verify every event in the trace contains `current_index`

> ⏸ **PAUSE — Wait for confirmation before Step 10.4**

---

## Step 10.4 — Resume Smoke Test

**Description:**
Write an integration test that validates crash recovery end-to-end using the enriched trace log.

Requirements:
- Test scenario: a 3-step plan where the process is interrupted after step 2 completes
- Recovery path: parse `decision_trace.jsonl`, reconstruct the `Plan` from `PLAN_CREATED` metadata, identify the last completed `current_index`, call `resume_from_plan()` with the reconstructed plan
- Assert: step 3 executes and completes correctly; no duplicate execution of steps 1 or 2
- Test must be deterministic (no real LLM calls — use a mock planner and mock step dispatcher)

**Output required:**
- Integration test in `tests/` covering the above scenario
- Any minimal fixes to `resume_from_plan()` required to accept a plan reconstructed from JSON (do not redesign the method)

> ⏸ **PAUSE — Wait for confirmation before Phase 10 close-out**

---

## Phase 10 Close-Out

Before moving to Phase 11:
- Run full build and unit tests including the new integration test
- Manually inspect a `decision_trace.jsonl` from a goal run — verify all four previously missing fields are present
- Run `check_baseline.py` — confirm 100% compliance
- Confirm `resume_from_plan()` succeeds on the reconstructed plan in the integration test

**Summarize:**
- Files created
- Files modified
- Any deferred items

> ⏸ **PAUSE — Confirm Phase 10 complete before starting Phase 11**

---

---

# Phase 11 — Dynamic Plan Revision

**Architecture references:** `PLAN.md`, `cognate.md`

**Goal:** Replace the `revise_plan` stub with a real-time correction mechanism that uses trajectory failure data to instruct the LLM to generate a repaired plan mid-execution.

> **Note for Gemini:** The `REVISING_PLAN` controller state, `PLAN_REVISED` event, and trajectory storage are already implemented. This phase implements the revision logic that runs inside that state. Do not change the state machine transitions — only the behavior that executes during `REVISING_PLAN`.

---

## Step 11.1 — Revision Trigger Policy Design (NO CODE YET)

**Description:**
Define precisely what conditions cause a revision and what context is passed to the LLM for repair.

Define:
- **Trigger conditions** (must define all three):
  - Step failure after max retries exceeded
  - Step result is semantically invalid (define what "invalid" means — schema check or LLM evaluation?)
  - External revision request (manual trigger via `ExecutiveController::request_revision()`)
- **Revision context payload** — the exact fields passed to the LLM for repair:
  - Original goal
  - Current plan (full JSON)
  - Failed step details (step_id, error_message, result)
  - Last N trajectory steps (define N, default 5)
  - Relevant past trajectories from GRAG (top 2)
- **Scope of revision**: full plan replacement vs. tail-only (only revise steps from `current_index` onward)
- **Revision limit**: maximum revision count per plan before aborting (default 3)

**Output required:**
- Policy design document (inline — no code)
- Proposed `RevisionContext` struct definition (header only)

> ⏸ **PAUSE — Wait for confirmation before Step 11.2**

---

## Step 11.2 — RevisionContext and Prompt Template

**Description:**
Implement the `RevisionContext` struct and the revision prompt template.

Requirements:
- New struct: `RevisionContext` (per approved design from Step 11.1)
- New prompt template: `plan_revision.tmpl` in `agent_workspace/prompt_templates/`
- Template variables: `{goal}`, `{current_plan_json}`, `{failed_step_json}`, `{trajectory_context}`, `{past_trajectories}`
- Template must instruct the LLM to output a replacement plan in the same JSON schema as `PlanParser` expects (Phase 9)
- Template must explicitly instruct: only revise from the failed step onward — do not repeat completed steps
- `PromptFactory` must be updated to support rendering the new template

**Output required:**
- `revision_context.h`
- `plan_revision.tmpl`
- Updated `PromptFactory` to render revision template
- Unit test: verify template renders correctly with a known `RevisionContext`

> ⏸ **PAUSE — Wait for confirmation before Step 11.3**

---

## Step 11.3 — Revision Engine Implementation

**Description:**
Implement the revision logic that runs when `ExecutiveController` enters `REVISING_PLAN` state.

Requirements:
- New class: `PlanRevisionEngine`
  - Input: `RevisionContext`
  - Output: `std::optional<Plan>` (revised plan, or `nullopt` on failure)
  - Calls `LLMPlanner` with the revision prompt
  - Uses `PlanParser` (Phase 9) to parse the response
  - On parse failure: retries once with an error-augmented prompt
  - On second failure: returns `nullopt` — caller must abort the plan
- `ExecutiveController::decide_transition()` must call `PlanRevisionEngine` when in `REVISING_PLAN` state, replacing the current stub
- Revision count tracked in `Plan` (new field: `revision_count`) — abort if limit exceeded
- `PLAN_REVISED` event must fire with the full revised plan JSON (Phase 10 enrichment is prerequisite)

**Output required:**
- `plan_revision_engine.h` and `plan_revision_engine.cpp`
- Updated `executive_controller.cpp` to call the engine during `REVISING_PLAN`
- Updated `plan.h` / `plan.cpp` to track `revision_count`
- Unit tests: successful revision produces a valid plan, max revision limit triggers abort

> ⏸ **PAUSE — Wait for confirmation before Step 11.4**

---

## Step 11.4 — Revision Integration Test

**Description:**
Write an end-to-end integration test that validates the full revision lifecycle.

Requirements:
- Test scenario: a 4-step plan where step 2 fails (inject a mock failure), triggers revision, and the revised plan (steps 2–4 replaced) executes to completion
- Assert: `revision_count` is 1, the `PLAN_REVISED` event is present in the trace with valid JSON, steps 3–4 of the original plan are NOT executed, revised steps complete successfully
- Test must be deterministic (mock LLM and mock dispatcher)
- Separately: test the abort path — step 2 fails, revision fails, plan is aborted with `PLAN_ABORTED` event

**Output required:**
- Integration tests in `tests/` covering both scenarios above

> ⏸ **PAUSE — Wait for confirmation before Phase 11 close-out**

---

## Phase 11 Close-Out

Before moving to Phase 12:
- Run full build and unit tests including integration tests
- Execute a real goal that forces a revision (or simulate via a known-failing step)
- Inspect `decision_trace.jsonl`: verify `PLAN_REVISED` contains full revised plan JSON and `current_index`
- Confirm `revision_count` is persisted in the SQLite `plans` table
- Run `check_baseline.py` — confirm 100% compliance

**Summarize:**
- Files created
- Files modified
- Any deferred items

> ⏸ **PAUSE — Confirm Phase 11 complete before starting Phase 12**

---

---

# Phase 12 — Extended Agent & Tool Re-enablement

**Architecture references:** `TOOLS.md`, `basic_agent_design_goal.md`, `PLAN.md`

**Goal:** Re-enable the tool system after the three Basic Agent criteria from `basic_agent_design_goal.md` are verified met. Introduce the first production-grade extended tools and validate the agent operates safely and correctly with tools active.

> **Note for Gemini:** Tools must NOT be re-enabled until all three Basic Agent gate criteria below are explicitly confirmed. Verify each criterion before beginning Step 12.1.

**Basic Agent Gate Criteria (must all be confirmed before Phase 12):**
1. Zero-Chunk Handling: the agent correctly returns `success=false` and reports inability to find information rather than hallucinating (verified in `WorkflowEngine` — confirm still passing).
2. Context Integrity: the agent does not leak internal prompt structure (tool schemas, system prompt fragments) into conversational output.
3. Resumption Stability: `ExecutiveController` can resume any RAG-based plan from SQLite without state corruption (validated by Phase 10 integration tests).

---

## Step 12.1 — Basic Agent Gate Verification (NO CODE YET)

**Description:**
Formally verify all three Basic Agent gate criteria before any tool is re-enabled.

Requirements:
- Run `check_baseline.py` — confirm 100% pass
- Run the Phase 10 resume integration test — confirm pass
- Manually execute 3 queries where no RAG context exists — confirm all return clean "cannot find information" responses with no hallucination
- Inspect 5 recent conversation outputs — confirm no system prompt fragments or tool schema text appears in responses

**Output required:**
- Gate verification checklist (inline — results of each check above)
- If any criterion fails: stop, fix, and re-run before proceeding

> ⏸ **PAUSE — Wait for confirmation before Step 12.2**

---

## Step 12.2 — Tool System Re-enablement

**Description:**
Set `Config::enable_tools` to `true` by default and verify the existing tool prompt injection pipeline behaves correctly with the current toolset.

Requirements:
- Set `Config::enable_tools = true` in the constructor
- Run the full build and all unit tests
- Verify `PromptFactory::getToolList()` produces a clean, correct tool list with no extraneous content
- Verify the agent's conversational output does not include raw tool schema text when no tool call is needed
- Confirm all currently registered tools appear in the system prompt exactly once

**Output required:**
- Updated `Config` constructor (single-line change)
- Confirmation that all existing tool tests pass with tools enabled

> ⏸ **PAUSE — Wait for confirmation before Step 12.3**

---

## Step 12.3 — `web_scrape` Tool

**Description:**
Implement the `web_scrape` tool per the Toolchain Roadmap in `improvements.md`.

Requirements:
- Tool name: `web_scrape`
- Input schema: `{ "url": string, "mode": "static" | "js_rendered" (optional, default "static") }`
- Output: `{ "url": string, "title": string, "content": string, "retrieved_at_ms": int }`
- Static mode: HTTP fetch with a reasonable timeout (10s), extract main text content (strip scripts, styles, nav)
- JS-rendered mode: stub only in this step — return `{ "error": "js_rendered not yet implemented" }` — do not add a headless browser dependency
- Output must be fed into the RAG ingestion pipeline (`IndexManager`) as a new chunk with source metadata `{ "type": "web", "url": "...", "retrieved_at_ms": ... }`
- Must sanitize URL (reject non-HTTP/HTTPS, reject internal network ranges: 127.x, 10.x, 192.168.x, 172.16–31.x)
- Timeout enforced — no hanging requests

**Output required:**
- `web_scrape_tool.h` and `web_scrape_tool.cpp`
- Registration in `ToolRegistry`
- Unit tests: valid URL fetch and ingestion, blocked internal URL rejection, timeout behavior

> ⏸ **PAUSE — Wait for confirmation before Step 12.4**

---

## Step 12.4 — Gmail Extended Tools

**Description:**
Implement the next three Gmail tools to complete the Gmail integration suite. `gmail_read_labels` is already implemented — build on that foundation.

Requirements:
- Tool name: `gmail_read_messages`
  - Input: `{ "label_id": string, "max_results": int (optional, default 10) }`
  - Output: `{ "messages": [ { "id": string, "subject": string, "from": string, "date": string, "snippet": string } ] }`
- Tool name: `gmail_search`
  - Input: `{ "query": string, "max_results": int (optional, default 10) }`
  - Output: same shape as `gmail_read_messages`
- Tool name: `gmail_send_message`
  - Input: `{ "to": string, "subject": string, "body": string }`
  - Output: `{ "message_id": string, "status": "sent" | "failed", "error": string (optional) }`
  - `gmail_send_message` must require explicit user confirmation before sending — implement a `requires_confirmation: true` flag on the `ITool` interface and enforce it in `ToolRegistry::execute()`
- All three tools must follow `TOOLS.md v1.0`

**Output required:**
- `gmail_read_messages_tool.h/.cpp`, `gmail_search_tool.h/.cpp`, `gmail_send_message_tool.h/.cpp`
- Updated `ITool` interface with `requires_confirmation` flag
- Updated `ToolRegistry::execute()` to enforce confirmation gate
- Registration of all three tools
- Unit tests for each tool

> ⏸ **PAUSE — Wait for confirmation before Phase 12 close-out**

---

## Phase 12 Close-Out

Final validation before marking this roadmap cycle complete:
- Run full build and all unit tests
- Execute a goal that uses `web_scrape` — verify the result is indexed and retrievable via RAG in a follow-up query
- Execute a Gmail label read — verify structured output
- Attempt a `gmail_send_message` — verify confirmation gate blocks execution until confirmed
- Run `check_baseline.py` — confirm 100% compliance

**Summarize:**
- Files created
- Files modified
- Any deferred items
- Recommended next roadmap additions

> ⏸ **PAUSE — Confirm Phase 12 complete. Roadmap cycle complete.**

---

## Notes

- This file covers Phases 9–12, succeeding the Cognate Phase 1–8 work logged in `completed_improvements_log.md`.
- On completion of each phase, move its close-out summary to `docs/completed_improvements_log.md`.
- `improvements.md` (Phases 3–5) runs in parallel — coordinate on shared files (`executive_controller.cpp`, `llm_planner.cpp`, `ToolRegistry`) to avoid conflicts.
