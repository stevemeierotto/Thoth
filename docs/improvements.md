# Thoth Improvements Roadmap

**Version:** 2.0
**Status:** Authoritative — Single Source of Truth for All Upcoming Work

---

## Rules for This Document

1. Completed work must be moved to `docs/completed_improvements_log.md`.
2. Architecture specifications live in: `PLAN.md`, `GRAG.md`, `NODE.md`, `cognate.md`.
3. Implementation status lives in: `grag_summery.md`, `completed_improvements_log.md`, `architectural_facts.md`. Before implementing any work that touches GRAG or ExecutiveController, read both the relevant spec AND the relevant status file. Status takes precedence on what already exists — do not re-implement or regress completed work.
4. Roadmap items must not contradict architecture documents.
5. This roadmap progresses: core infrastructure → observability → capabilities → memory → research.
6. **Gemini must pause for confirmation between every step and every phase.**

---

## How to Use This Document (For Gemini)

You are implementing the Thoth improvement roadmap.

Before making any changes:
- Read `AGENTS.md` in full.
- Read the architecture document referenced in the phase you are implementing.
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
- List files created or modified.
- **Wait for explicit confirmation before starting the next phase.**

---

## Phase Order

| # | Phase | Priority | Status |
|---|---|---|---|
| 3 | Memory Stability | High | 📋 Planned |
| 4 | Advanced Reasoning | High | 📋 Planned |
| 5 | Self-Building Capability | Medium | 📋 Planned |
| 9 | LLM-Driven Step Generation | Critical | ✅ Complete (see completed_improvements_log.md) |
| 10 | Resume Completeness | High | ✅ Complete (see completed_improvements_log.md) |
| 11 | Dynamic Plan Revision | High | ✅ Complete (see completed_improvements_log.md) |
| 12 | Extended Agent & Tool Re-enablement | Medium | ✅ Complete (see completed_improvements_log.md) |

---

---

# Phase 3 — Memory Stability

**Architecture references:** `GRAG.md`, `memory_summery.md`

**Goal:** Prevent memory bloat, add structured world knowledge, and define the vector store scalability path.

---

## Step 3.1 — Memory Pruning: Policy Design (NO CODE YET)

**Description:**
Design the pruning and archival policy before writing any implementation.

Define:
- **Hot tier**: recent N raw turns (configurable, default: last 50 messages)
- **Warm tier**: compressed summaries of older sessions
- **Cold tier**: archived raw logs (kept for audit, not used in retrieval)

Policy triggers:
- Session message count exceeds threshold
- Session age exceeds threshold (e.g., 30 days)
- Manual trigger via admin command

Preservation rules:
- Key decisions, tasks, and tool results must be preserved in summary before pruning raw turns
- Summary must be searchable via RAG

**Output required:**
- Design document (inline, no code)
- Proposed SQLite schema additions: `archived_turns` table
- Proposed `PruningPolicy` struct definition (header only)

> ⏸ **PAUSE — Wait for confirmation before Step 3.2**

---

## Step 3.2 — Memory Pruning: Implementation

**Description:**
Implement the pruning and archival pipeline based on the approved design from Step 3.1.

Requirements:
- New class: `MemoryPruner`
- Triggered automatically on session write when thresholds are exceeded
- Uses `DecisionTraceLogger` to log pruning events
- Audit metadata per archived turn: `archived_at_ms`, `summary_version`, `source_range`
- Restore path: `MemoryPruner::restore(session_id, range)` for on-demand historical replay
- All operations must be transactional (SQLite)

**Output required:**
- `memory_pruner.h` and `memory_pruner.cpp`
- Updated `SQLiteMemoryRepository` to integrate pruning triggers
- Unit tests: prune triggers at threshold, restore returns correct turns

> ⏸ **PAUSE — Wait for confirmation before Step 3.3**

---

## Step 3.3 — Structured Fact Store

**Description:**
Implement a dedicated `facts` SQLite table for structured world knowledge.

This is distinct from episodic conversation memory. It is for persistent, verifiable facts about the project and environment.

Schema:
```sql
CREATE TABLE facts (
  key TEXT PRIMARY KEY,
  value TEXT NOT NULL,
  confidence REAL DEFAULT 1.0,
  source TEXT,
  last_updated_ms INTEGER NOT NULL
);
```

Requirements:
- New class: `FactStore` with methods: `upsert`, `get`, `search`, `delete`
- `search` must use substring matching on `key` and `value`
- Accessible from `CommandProcessor` as a retrieval source (alongside RAG)
- Tools may write facts via a new `store_fact` tool (implement the tool here too)
- Tool name: `store_fact`, input: `{ "key": string, "value": string, "confidence": float, "source": string }`

**Output required:**
- `fact_store.h` and `fact_store.cpp`
- `store_fact_tool.h` and `store_fact_tool.cpp`
- Unit tests: upsert, retrieval, search

> ⏸ **PAUSE — Wait for confirmation before Step 3.4**

---

## Step 3.4 — Vector Store Scalability Path

**Description:**
Define and implement the abstraction layer allowing `IndexManager` to swap vector backends.

This step defines the interface and implements the migration path — it does NOT switch the backend yet.

Requirements:
- New interface: `IVectorStore` (pure virtual)
  - Methods: `insert`, `search`, `delete_chunk`, `flush`, `chunk_count`
- Wrap current flat `rag_index.bin` implementation behind `IVectorStore`
- New class: `FlatVectorStore` (existing behavior, wrapped)
- Add benchmark contract tests:
  - Ingest throughput
  - p50/p95 retrieval latency
  - Memory usage at 10k / 50k / 100k chunks
- Document the evaluated candidates: LanceDB, ChromaDB
- Add dual-write_stub (disabled by default) for future migration

**Output required:**
- `i_vector_store.h` interface
- `flat_vector_store.h` and `flat_vector_store.cpp` (wrapped existing)
- Benchmark test scaffold
- Migration design notes (inline comments or small design doc)

> ⏸ **PAUSE — Wait for confirmation before Phase 3 close-out**

---

## Phase 3 Close-Out

Before moving to Phase 4:
- Run full build and unit tests
- Verify `step_metrics` SQLite table is populated after a goal execution
- Verify `facts` table exists and `store_fact` tool is accessible via tool registry
- Confirm `IVectorStore` builds cleanly and existing RAG behavior is unchanged

**Summarize:**
- Files created
- Files modified
- Any deferred items

> ⏸ **PAUSE — Confirm Phase 3 complete before starting Phase 4**

---

---

# Phase 4 — Advanced Reasoning

**Architecture references:** `PLAN.md`, `GRAG.md`, `cognate.md`, `README.md`

**Goal:** Implement structured scientific reasoning, strategy reuse, and advanced GRAG retrieval modes.

---

## Step 4.1 — Scientific Reasoning Engine: ProblemState Design (NO CODE YET)

**Description:**
Design the `ProblemState` structure and hypothesis lifecycle before writing implementation.

Define:
```cpp
struct ProblemState {
    std::string problem_description;
    std::vector<std::string> hypotheses;
    std::vector<std::string> constraints;
    std::vector<std::string> unknowns;
    std::vector<std::string> rejected_paths;
    std::string current_hypothesis;
    int iteration_count;
};
```

Define the scientific reasoning loop:
1. Generate hypotheses (LLM call via `IPlanner`)
2. Extract constraints
3. Evaluate feasibility (score each hypothesis)
4. Generate alternatives for low-score hypotheses
5. Score options on: feasibility, cost realism, risk, novelty
6. Iterate until stable (convergence threshold configurable)

Define convergence criteria:
- No new hypotheses added in last 2 iterations
- Top hypothesis score > 0.8

**Output required:**
- `problem_state.h` header (struct + enums only)
- Written loop design (pseudocode or description)

> ⏸ **PAUSE — Wait for confirmation before Step 4.2**

---

## Step 4.2 — Scientific Reasoning Engine: Implementation

**Description:**
Implement the scientific reasoning loop using the Strategy Pattern (per `PLAN.md`).

Requirements:
- New class: `ScientificExecutionMode` implementing `IExecutionMode`
- Activated when `ExecutiveController` classifies goal as scientific
- Goal classification logic: keyword matching + LLM classification call
- Each iteration updates `ProblemState`
- `ProblemState` must be persisted to SQLite (new table: `problem_states`)
- Hypothesis scores returned as structured comparison table
- No single-shot answers — loop must run minimum 2 iterations

**Output required:**
- `scientific_execution_mode.h` and `scientific_execution_mode.cpp`
- `problem_state.cpp` with SQLite persistence
- Updated `ExecutiveController` to activate `ScientificExecutionMode` on classification
- Unit tests: loop runs minimum iterations, convergence halts loop

> ⏸ **PAUSE — Wait for confirmation before Step 4.3**

---

## Step 4.3 — Strategy Memory & Tool Metrics

**Description:**
Expand the Plan History system to prefer historically successful strategies.

Requirements:
- New class: `StrategyMemory`
- Reads from `step_metrics` table (implemented in Phase 1)
- Computes per-tool success rates and average latency
- Exposes ranked list of preferred tools/strategies to `IPlanner`
- `Planner::createInitialPlan()` must accept a `StrategyHints` struct
- Plans generated with hints must prefer high-success tools when multiple options exist
- Hint influence must be configurable (weight 0.0–1.0)

**Output required:**
- `strategy_memory.h` and `strategy_memory.cpp`
- Updated `IPlanner` interface to accept `StrategyHints`
- Unit tests: verify strategy hints influence plan tool selection

> ⏸ **PAUSE — Wait for confirmation before Step 4.4**

---

## Step 4.4 — GRAG Upgrade: Hierarchical Goals

**Description:**
Implement subgoal tree support per `grag_upgrade.md` Upgrade #1.

Requirements:
- New struct: `GoalNode` with: `description`, `embedding`, `children[]`, `status`
- `Planner` generates subgoal tree alongside plan
- `ExecutiveController` activates one subgoal at a time
- `GragScorer::rescore()` uses `active_subgoal_embedding` instead of root goal embedding
- Root goal embedding still retained for top-level direction
- Subgoal transitions logged to `decision_trace.jsonl`

**Output required:**
- `goal_node.h` (struct definition)
- Updated `GragScorer::rescore()` signature to accept optional active subgoal embedding
- Updated `ExecutiveController` to manage subgoal activation
- Unit tests: verify directional scoring uses active subgoal, not root goal

> ⏸ **PAUSE — Wait for confirmation before Step 4.5**

---

## Step 4.5 — GRAG Upgrade: Trajectory Awareness

**Description:**
Implement progress-aware retrieval per `grag_upgrade.md` Upgrade #2.

Requirements:
- New class: `TrajectoryBuilder`
  - Input: last N `EpisodeStep` records (N configurable, default 7)
  - Output: single trajectory embedding `T`
- Structured trajectory input for embedding:
  ```json
  {
    "recent_actions": ["..."],
    "failures": ["..."],
    "progress_summary": "..."
  }
  ```
- Updated retrieval scoring:
  ```
  score = wq * cosine(Q, chunk)
        + wd * cosine(D, chunk)
        + wt * cosine(T, chunk)
  ```
  Default weights: `wq=0.4`, `wd=0.4`, `wt=0.2` (configurable)
- `ExecutiveController` updates trajectory embedding after every step
- `GragDiagnostics` must include trajectory weight and magnitude

New SQLite table: `episode_steps`
- Fields: `episode_id`, `goal_id`, `step_index`, `state_summary`, `action_taken`, `result_status`, `embedding_blob`, `timestamp_ms`

**Output required:**
- `trajectory_builder.h` and `trajectory_builder.cpp`
- Updated `GragScorer::rescore()` to accept trajectory embedding
- Updated `GragDiagnostics` struct
- Unit tests: verify trajectory shifts retrieval scores vs. baseline

> ⏸ **PAUSE — Wait for confirmation before Step 4.6**

---

## Step 4.6 — GRAG Hardening: Adaptive Graph Learning

**Status: [COMPLETE]**
Dynamic graph edge learning is now fully operational. Edge weights are adjusted via `GraphRefiner` based on execution success/failure using a logistic learning rule (learning_rate=0.2).

**Completed:**
- ✅ Dynamic edge weight adjustment based on successful execution trajectories (`GraphRefiner::refineFromTrajectory`)
- ✅ Graph density metrics added to `GragDiagnostics` and logged in `grag_benchmark.jsonl`:
  - Total nodes and edges
  - Average edge weight
  - Graph activations per retrieval
  - Maximum graph contribution
- ✅ Enhanced logging in `GraphRefiner` to track weight updates, edge creation/updates, and learning progress

---

## Step 4.7 — Model Upgrade Path

**Description:**
Define the abstraction layer and migration playbook for adopting larger models.

This is a design and documentation step — no major code changes.

Requirements:
- Audit `LLMInterface` for any hard-coded model-specific assumptions
- Define a `ModelConfig` struct: `{ model_name, context_window, supports_tool_calls, temperature, max_tokens }`
- Move model selection to runtime config (already partially done — verify and complete)
- Document migration playbook:
  - How to test a new model against current prompt templates
  - How to verify tool call format compatibility
  - Rollback procedure

**Output required:**
- Updated `LLMInterface` with `ModelConfig` abstraction
- Migration playbook as inline comments or a small `.md` file

> ⏸ **PAUSE — Wait for confirmation before Phase 4 close-out**

---

## Phase 4 Close-Out

Final validation before marking roadmap complete:
- Run full build and unit tests
- Run a scientific goal end-to-end: verify loop iterations and `ProblemState` persistence
- Run a standard goal: verify subgoal tree is generated and GRAG uses active subgoal
- Check `grag_benchmark.jsonl` — confirm trajectory weight field is present
- Confirm strategy hints are visible in planner trace logs

**Summarize:**
- Files created
- Files modified
- Any deferred items
- Recommended next roadmap additions

> ⏸ **PAUSE — Confirm Phase 4 complete. Roadmap cycle complete.**

---

---

# Phase 5 — Self-Building Capability

**Architecture references:** `PLAN.md`, `TOOLS.md`

**Priority: Medium**

**Goal:** Give Thoth the ability to read, modify, build, and test its own codebase through the standard tool interface.

> **Note for Gemini:** All tools in this phase must implement `ITool` per `TOOLS.md v1.0`. All execution must be sandboxed. No tool may directly access memory or SQLite.

---

## Step 5.1 — `project_analyze` Tool

**Description:**
Implement a tool that scans the Thoth source directory and returns a structured project graph.

Requirements:
- Tool name: `project_analyze`
- Input schema: `{ "root_path": string }` (defaults to `./` if not provided)
- Output: structured JSON containing:
  - File list with paths and sizes
  - Class names extracted from headers
  - File dependency map (which headers each `.cpp` includes)
- Must complete within 10 seconds on the current codebase
- Read-only — must not write or modify any files

**Output required:**
- `project_analyze_tool.h` and `project_analyze_tool.cpp`
- Registration in `ToolRegistry`
- Unit test: verify output schema on a known directory structure

> ⏸ **PAUSE — Wait for confirmation before Step 5.2**

---

## Step 5.2 — `run_tests` Tool

**Description:**
Implement a tool that executes the Thoth unit test suite and returns structured results.

Requirements:
- Tool name: `run_tests`
- Input schema: `{ "filter": string (optional) }` — allows running a subset by name pattern
- Output: `{ "total": int, "passed": int, "failed": int, "failures": [ { "name": string, "message": string } ] }`
- Must capture stdout/stderr from test runner
- Timeout: 60 seconds max
- Must NOT allow arbitrary command injection via the `filter` field — sanitize input strictly

**Output required:**
- `run_tests_tool.h` and `run_tests_tool.cpp`
- Registration in `ToolRegistry`
- Unit test: verify structured output parsing on a known test result

> ⏸ **PAUSE — Wait for confirmation before Step 5.3**

---

## Step 5.3 — `code_modify` Tool (Read + Diff)

**Description:**
Implement the read and diff-application capability of the coding agent tool.

This step implements read and diff only — no build trigger yet.

Requirements:
- Tool name: `code_modify`
- Input schema:
  ```json
  {
    "operation": "read" | "apply_diff",
    "file_path": string,
    "unified_diff": string (required for apply_diff)
  }
  ```
- `read`: returns file contents as a string
- `apply_diff`: applies a unified diff to the target file, returns success/failure with line-level error detail
- File path must be validated against an allowlist (only paths under the project root)
- Reject paths containing `..` or absolute paths outside project root
- Atomic write: apply to a temp file first, rename on success

**Output required:**
- `code_modify_tool.h` and `code_modify_tool.cpp`
- Registration in `ToolRegistry`
- Unit tests for: read, valid diff apply, invalid diff rejection, path traversal rejection

> ⏸ **PAUSE — Wait for confirmation before Step 5.4**

---

## Step 5.4 — `code_modify` Tool (Build + Revert)

**Description:**
Extend `code_modify` with build verification and automatic revert on failure.

New operations:
- `build`: runs `cmake --build --preset debug`, returns structured result
- `revert`: restores the file from backup taken before last `apply_diff`

Requirements:
- Before any `apply_diff`, back up the original file to a temp location
- After `apply_diff`, optionally auto-trigger build (configurable)
- If build fails, revert automatically and return failure with build output
- Revert must be idempotent

**Output required:**
- Updated `code_modify_tool.cpp`
- Unit test: verify revert behavior on a simulated build failure

> ⏸ **PAUSE — Wait for confirmation before Phase 5 close-out**

---

## Phase 5 Close-Out

Before moving to next Roadmap iteration:
- Run full build and unit tests
- Manually verify: ask agent to analyze the project — confirm structured output
- Manually verify: ask agent to read a source file — confirm it returns contents
- Confirm all tools registered in `ToolRegistry` and appear in system prompt

**Summarize:**
- Files created
- Files modified
- Any deferred items

> ⏸ **PAUSE — Confirm Phase 5 complete.**

---

## Toolchain Roadmap (Ongoing — Not Phased)

These tools can be implemented independently of the phase structure above. They must comply with `TOOLS.md v1.0`.

### Web Scraping Tool
- **Priority:** High
- **Notes:** Support both static and JS-rendered content. Feed output into RAG ingestion pipeline.
- **Tool name:** `web_scrape`

### Trading Tool
- **Priority:** Low
- **Notes:** Position limits, risk caps, paper trading mode required before live use.
- **Tool name:** `place_order`

### Gmail Integration (Expanded)
- **Priority:** Medium
- **Status:** `gmail_read_labels` and `gmail_read_messages` implemented.
- **Next tools:** `gmail_send_message`, `gmail_search`

---

# Phase 9 — LLM-Driven Step Generation

**Status:** ✅ Complete — See `completed_improvements_log.md` for details

**Note:** This phase was completed. The implementation details are preserved below for reference, but work has been completed and moved to the completed log.

**Architecture references:** `PLAN.md`, `cognate.md`

**Goal:** Replace the hardcoded 3-step scaffold with a fully dynamic planning loop where the LLM generates, parses, and executes arbitrary plan steps. This is the primary unlock for full agent autonomy.

---

# Phase 10 — Resume Completeness

**Status:** ✅ Complete — See `completed_improvements_log.md` for details

**Note:** This phase was completed. The implementation details are preserved below for reference, but work has been completed and moved to the completed log.

**Architecture references:** `architectural_facts.md` (§6 Resume Compatibility), `PLAN.md`

**Goal:** Ensure the `decision_trace.jsonl` log and the SQLite persistence layer contain enough information to fully reconstruct and resume a plan after a crash or restart, eliminating the four known gaps identified in `architectural_facts.md §6`.

---

# Phase 11 — Dynamic Plan Revision

**Status:** ✅ Complete — See `completed_improvements_log.md` for details

**Note:** This phase was completed. The implementation details are preserved below for reference, but work has been completed and moved to the completed log.

**Architecture references:** `PLAN.md`, `cognate.md`

**Goal:** Replace the `revise_plan` stub with a real-time correction mechanism that uses trajectory failure data to instruct the LLM to generate a repaired plan mid-execution.

---

# Phase 12 — Extended Agent & Tool Re-enablement

**Status:** ✅ Complete — See `completed_improvements_log.md` for details

**Note:** This phase was completed. The implementation details are preserved below for reference, but work has been completed and moved to the completed log.

**Architecture references:** `TOOLS.md`, `basic_agent_design_goal.md`, `PLAN.md`

**Goal:** Re-enable the tool system after the three Basic Agent criteria from `basic_agent_design_goal.md` are verified met. Introduce the first production-grade extended tools and validate the agent operates safely and correctly with tools active.
