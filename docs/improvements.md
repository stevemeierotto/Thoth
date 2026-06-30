# Thoth Improvements Roadmap

**Version:** 2.0
**Status:** Authoritative — Single Source of Truth for All Upcoming Work

---

## Rules for This Document

1. Completed work must be moved to `docs/completed_improvements_log.md`.
2. Architecture specifications live in: `PLAN.md`, `GRAG.md`, `NODE.md`, `cognate.md`.
3. Implementation status lives in: `GRAG.md` (spec + audit), `completed_improvements_log.md`, `architectural_facts.md`, and `benchmark_results.md`. Before implementing any work that touches GRAG or ExecutiveController, read both the relevant spec AND the relevant status file. Status takes precedence on what already exists — do not re-implement or regress completed work.
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

**Legend:** ✅ Complete · 🔶 Partial · 📋 Not started · 🚫 Stub

| # | Phase | Priority | Status |
|---|---|---|---|
| 3 | Memory Stability | High | 🔶 Partial |
| 4 | Advanced Reasoning | High | 🔶 Partial |
| 5 | Self-Building Capability | — | 📋 Future expansion (optional) |
| 9 | LLM-Driven Step Generation | Critical | ✅ Complete (see completed_improvements_log.md) |
| 10 | Resume Completeness | High | ✅ Complete (see completed_improvements_log.md) |
| 11 | Dynamic Plan Revision | High | ✅ Complete (see completed_improvements_log.md) |
| 12 | Extended Agent & Tool Re-enablement | Medium | ✅ Complete (see completed_improvements_log.md) |

**Active work:** Remaining Phase 3–4 items are listed below with per-step status. **Phase 5 (Self-Building)** is an optional future expansion the owner may revisit — not scheduled active work. Benchmark case design: [`new_corpus_tests.md`](new_corpus_tests.md). See `completed_improvements_log.md` and `cursor_list.md`.

---

---

# Phase 3 — Memory Stability

**Phase status:** 🔶 Partial

| Step | Status | Notes |
|------|--------|-------|
| 3.1 Pruning design | ✅ | `PruningPolicy`, `archived_turns` schema in code |
| 3.2 Pruning implementation | 🔶 | Runtime + GUI trim wired (2026-06-16); summarize/age/admin/range restore open |
| 3.3 Fact Store | ✅ | `FactStore` + `store_fact` tool |
| 3.4 IVectorStore | 🔶 | `IVectorStore` + `FlatVectorStore`; full benchmark contract tests not done |

**Architecture references:** `GRAG.md`, `memory_summary.md`

**Goal:** Prevent memory bloat, add structured world knowledge, and define the vector store scalability path.

---

## Step 3.1 — Memory Pruning: Policy Design

**Status:** ✅ Complete — see `completed_improvements_log.md` (2026-03-05 Memory Stability).

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

---

## Step 3.2 — Memory Consolidation (M1): Implementation

**Status:** ✅ **M1 verified** (2026-06-26) — M1.5 gate passed. See **`episodic_memory_benchmark.md`**.

**Description:**
Memory **consolidation** (design term; code: `MemoryPruner` until rename) transforms hot → warm + cold when count threshold exceeded.

**M1 deliverables:**
- `SummaryGenerator` → `EpisodicMemory` → `EpisodicMemoryRenderer` (structured source of truth; JSON is persistence only)
- `warm_memory` + `warm_memory_embeddings` tables; atomic transaction (LLM outside txn; hot delete only after DB success)
- Embed **canonical structured text**, not rendered prose
- GRAG merge-before-score for documents + warm memories (`MemoryScope::SESSION`)
- Fix `archived_at_ms` timestamp bugs; `derived_from_hash` integrity
- GUI: consolidate via agent pipeline (no silent JSON drops)
- Unit tests with `THOTH_MOCK_EPISODIC=1`

**Output:**
- `memory_architecture.md`, `summary_generator.*`, `episodic_memory.*`
- Updated `memory_pruner.cpp`, `sqlite_memory_repository.cpp`, `rag.cpp`
- Unit tests: consolidate at threshold, warm row + cold archive, GRAG recall

**Deferred (M3–M4):** `/prune`, range restore, async queue, background repair, warm merge lifecycle; token/pressure triggers (enum reserved).

---

## Step 3.2b — Memory Consolidation (M2): Age-Based Policy

**Status:** ✅ **Complete** (2026-06-29)

**Deliverables:**
- `ConsolidationReason` bitmask + `ConsolidationDecision` (structured policy with diagnostics)
- `Clock` / `SystemClock` / `FakeClock` — injectable time for policy + tests
- `config.json` → `memory.max_hot_messages`, `memory.max_hot_age_days`, `memory.prune_batch_size`
- `evaluatePolicy()` OR triggers: HOT_COUNT | SESSION_INACTIVE | OLDEST_MESSAGE
- `consolidateIfNeeded()` batch loop (cap 5, no-progress guard, deferred trace)
- Startup stale-session discovery (no LLM); consolidate active only; defer others until access
- Timestamp-preserving `loadConversation()` + GUI sync
- Unit tests M2-01–M2-08

**Output:**
- `consolidation_policy.*`, `clock.*`
- Updated `memory_pruner.*`, `memory.cpp`, `basic_agent_plugin.cpp`, `MainFrame.cpp`

---

## Step 3.3 — Structured Fact Store

**Status:** ✅ Complete — see `completed_improvements_log.md` (2026-03-05 Memory Stability).

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

---

## Step 3.4 — Vector Store Scalability Path

**Status:** 🔶 Partial — `IVectorStore` and `FlatVectorStore` implemented; ingest/latency/memory benchmark contract tests and dual-write stub remain open.

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

---

## Phase 3 Close-Out

**Status:** 🔶 Partial — not all steps complete. Historical checklist:

Before moving to Phase 4:
- ✅ Run full build and unit tests
- ✅ Verify `facts` table exists and `store_fact` tool is accessible via tool registry
- ✅ Confirm `IVectorStore` builds cleanly and existing RAG behavior is unchanged
- 🔶 Verify `step_metrics` populated after goal execution (infrastructure exists; manual verify optional)
- 🔶 Pruning: summarize-before-archive, age policy, admin restore path

**Deferred:** Vector store benchmark contract suite; pruning warm-tier summarization.

> Phase 3 remains **partial**. Remaining work tracked in Step 3.2 and 3.4 above.

---

---

# Phase 4 — Advanced Reasoning

**Phase status:** 🔶 Partial

| Step | Status | Notes |
|------|--------|-------|
| 4.1 ProblemState design | ✅ | `problem_state.h` + loop design |
| 4.2 Scientific execution mode | ✅ | `ScientificExecutionMode`; Cognate V2 (2026-03-28) |
| 4.3 Strategy memory / hints | ✅ | `StrategyEngine` + planner injection (not separate `StrategyMemory` class) |
| 4.4 Hierarchical subgoals | 📋 | Not implemented |
| 4.5 Trajectory awareness | 🔶 | `TrajectoryBuilder` + scoring path; $w_t=0.2$ local; mixed benchmark lift |
| 4.6 Adaptive graph learning | ✅ | `GraphRefiner` operational |
| 4.7 Model upgrade path | 📋 | Design/doc step not closed out |

**Architecture references:** `PLAN.md`, `GRAG.md`, `cognate.md`, `README.md`

**Goal:** Implement structured scientific reasoning, strategy reuse, and advanced GRAG retrieval modes.

---

## Step 4.1 — Scientific Reasoning Engine: ProblemState Design

**Status:** ✅ Complete — see `completed_improvements_log.md` (2026-03-28 Cognate V2).

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

---

## Step 4.2 — Scientific Reasoning Engine: Implementation

**Status:** ✅ Complete — see `completed_improvements_log.md` (2026-03-28 Cognate V2).

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

---

## Step 4.3 — Strategy Memory & Tool Metrics

**Status:** ✅ Complete — see `completed_improvements_log.md` (2026-03-28 strategy promotion; planner injection).

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

---

## Step 4.4 — GRAG Upgrade: Hierarchical Goals

**Status:** 📋 Not started — see `GRAG.md` audit note; `cursor_list.md` P1.4.

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

---

## Step 4.5 — GRAG Upgrade: Trajectory Awareness

**Status:** 🔶 Partial — infrastructure complete; $w_t=0.2$ in local `retrieval_config.json`; executive zeroes weight when trajectory empty. Tuning doc: `plan_reuse_tuning.md`.

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

---

## Step 4.6 — GRAG Hardening: Adaptive Graph Learning

**Status:** ✅ Complete — see `completed_improvements_log.md` (2026-03-05 GRAG Phase 8 / GraphRefiner).
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

**Status:** 📋 Not started — design/documentation step.

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

---

## Phase 4 Close-Out

**Status:** 🔶 Partial — not all steps complete. Historical checklist:

Final validation:
- ✅ Run full build and unit tests
- ✅ Run scientific goal: loop iterations and `ProblemState` persistence (Cognate V2 tests)
- ✅ Strategy hints visible in planner trace / injection events
- ✅ `grag_benchmark.jsonl` includes graph and trajectory fields
- 📋 Standard goal with subgoal tree (Step 4.4 not implemented)
- 📋 Model upgrade playbook (Step 4.7)

**Deferred:** Hierarchical subgoals (4.4); full trajectory tuning (4.5); model migration playbook (4.7).

> Phase 4 remains **partial**. Core Cognate V2 reasoning path is operational.

---

---

# Phase 5 — Self-Building Capability *(optional future expansion)*

**Phase status:** 📋 **Not scheduled** — possible expansion the owner may try later. Existing tools (`project_analyze`, `run_tests`, `code_modify` read) remain registered; **`apply_diff` is a stub** and is not on the active roadmap.

> **Owner note (2026-06-17):** Self-building is an idea for later, not current development priority. Steps below are retained as a **reference spec** if this direction is picked up again.

| Step | Status | Notes |
|------|--------|-------|
| 5.1 `project_analyze` | ✅ | Registered; unit tested |
| 5.2 `run_tests` | ✅ | Registered; `allow_shell_exec` gate (P1.5) |
| 5.3 `code_modify` read | ✅ | Read operation works |
| 5.3 `apply_diff` | 🚫 Stub | Not planned until owner schedules Phase 5 |
| 5.4 build + revert | 📋 | Future expansion only |

**Architecture references:** `PLAN.md`, `TOOLS.md`

**Priority: Medium**

**Goal:** Give Thoth the ability to read, modify, build, and test its own codebase through the standard tool interface.

> **Note for Gemini:** All tools in this phase must implement `ITool` per `TOOLS.md v1.0`. All execution must be sandboxed. No tool may directly access memory or SQLite.

---

## Step 5.1 — `project_analyze` Tool

**Status:** ✅ Complete — see `completed_improvements_log.md` (Self-Building prototype).

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

---

## Step 5.2 — `run_tests` Tool

**Status:** ✅ Complete — see `completed_improvements_log.md`; shell gating in P1.5.

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

---

## Step 5.3 — `code_modify` Tool (Read + Diff)

**Status:** 🔶 Partial — **read** ✅; **apply_diff** 🚫 stub. Phase 5 is **optional future expansion** — not active work (`cursor_list.md` P1.2).

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

---

## Step 5.4 — `code_modify` Tool (Build + Revert)

**Status:** 📋 Not started — depends on working `apply_diff` (Step 5.3).

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

---

## Phase 5 Close-Out

**Status:** 🔶 Partial — not all steps complete. Historical checklist:

Before moving to next Roadmap iteration:
- ✅ Run full build and unit tests
- ✅ `project_analyze` and `run_tests` registered and in tool prompt
- ✅ `code_modify` read verified
- 🚫 `apply_diff` not functional (stub)
- 📋 Build + revert automation (Step 5.4)

**Deferred:** Unified diff apply (P1.2 low priority); build/revert pipeline.

> Phase 5 is **optional future expansion**, not active roadmap work. Harness tools exist; code modification is not a current goal.

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

**Architecture references:** `TOOLS.md`, `PLAN.md`, `VERIFIED_BASELINE.md`

**Goal:** Re-enable the tool system after the cognitive spine baseline criteria in `VERIFIED_BASELINE.md` are verified met. Introduce the first production-grade extended tools and validate the agent operates safely and correctly with tools active.
