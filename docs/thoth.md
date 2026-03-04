# THOTH — Executive Scientific Agent Roadmap

Version: 1.0  
Purpose: This document defines the staged implementation plan for transforming the current agent into **Thoth**, a transparent executive system capable of structured planning, self-building, and scientific/engineering innovation.

This file is written as a task-oriented implementation guide for Copilot and future development sessions.

---

# CORE DESIGN PRINCIPLES

1. Thoth must be transparent (no black-box execution).
2. All tool usage must go through ToolRegistry.
3. Planning and execution must be separated.
4. Scientific reasoning must be a cognitive subsystem, not a single tool.
5. Thoth must eventually help build and improve itself.
6. Architecture > Prompt tricks.

---

# STAGE 1 — EXECUTIVE FOUNDATION (Step-Based Agent)

Goal: Transform Thoth into a structured step-by-step executive agent.

---

## 1.1 Create Plan Data Structures

Create new files:

- `plan.h`
- `plan.cpp`

Define:

```cpp
struct PlanStep {
    std::string id;
    std::string tool_name;
    nlohmann::json input;
    std::string status; // pending | running | success | failed
    nlohmann::json result;
    std::string reasoning;
};

struct Plan {
    std::string id;
    std::string goal;
    std::vector<PlanStep> steps;
    std::string status; // active | completed | failed
};

Requirements:

Steps must not execute automatically.

Status must update after each execution.

Plan must be serializable to JSON.

1.2 Create Planner Interface

Create:

planner.h

planner.cpp

Define:

class Planner {
public:
    Plan createInitialPlan(const std::string& goal);
    Plan revisePlan(const Plan& current_plan,
                    const nlohmann::json& feedback);
};

Implementation rules:

Use strict JSON schema when prompting LLM.

Reject malformed plan outputs.

Temperature must be low.

No tool execution inside planner.

1.3 Create Workflow Engine

Create:

workflow_engine.h

workflow_engine.cpp

Define:

class WorkflowEngine {
public:
    nlohmann::json executeStep(PlanStep& step);
};

Rules:

All tool execution must go through ToolRegistry.

Validate input schema before execution.

Validate output schema after execution.

Update step status properly.

Log all execution to decision_trace.jsonl.

1.4 Add Executive Controller

Create:

executive_controller.h

executive_controller.cpp

Define:

class ExecutiveController {
public:
    void executeGoal(const std::string& goal);
};

Execution loop:

Create plan.

Execute one step.

Evaluate result.

Ask planner whether to:

Continue

Insert new step

Revise remaining steps

Abort

Must be step-by-step and observable.

STAGE 2 — PERSISTENT WORLD STATE (SQLite Migration)

Goal: Upgrade memory from JSON files to SQLite.

2.1 Create Database Layer

Create:

database.h

database.cpp

Implement:

SQLite connection manager

Safe transactions

Prepared statements

2.2 Create Tables

Tables required:

sessions
messages
goals
plans
plan_steps
facts

Example schema:

CREATE TABLE goals (
  id TEXT PRIMARY KEY,
  description TEXT,
  status TEXT,
  priority INTEGER,
  created_at_ms INTEGER,
  updated_at_ms INTEGER
);

Plans must be persisted.
Steps must be persisted.

2.3 Implement Fact Store

Facts table:

CREATE TABLE facts (
  key TEXT PRIMARY KEY,
  value TEXT,
  confidence REAL,
  last_updated_ms INTEGER
);

Used for structured world knowledge.
Not conversational memory.

STAGE 3 — THOTH BUILDS THOTH (Meta-Development Tools)

Goal: Thoth can modify and improve its own codebase.

3.1 Coding Agent Tool

Create tool:

Tool name: code_modify

Capabilities:

Read file

Apply unified diff

Write file

Return compile/test result

Must:

Run in sandboxed environment

Reject dangerous commands

Return structured error output

3.2 Build Tool: run_tests

Capabilities:

Run unit tests

Capture output

Return pass/fail status

3.3 Build Tool: project_analyze

Capabilities:

Scan source directory

List classes

Map file dependencies

Return structured project graph

This will allow Thoth to reason about its own architecture.

STAGE 4 — GOAL-DIRECTED RETRIEVAL (GRAG)

Goal: Move beyond flat RAG.

4.1 Add Goal Embedding Support

Compute:

current_state_embedding

goal_embedding

Store temporarily in working memory.

4.2 Implement Directional Scoring

Replace flat similarity:

Old:
cosine(query, chunk)

New:
direction = goal_embedding - current_embedding
score = cosine(chunk_embedding, direction)

Select top-K by directional alignment.

4.3 Multi-Index Retrieval

Create separate indexes for:

Conversations

Codebase

Scientific knowledge

Plans

Tool documentation

Select index based on plan step type.

STAGE 5 — SCIENTIFIC REASONING ENGINE

Goal: Enable structured innovation mode.

5.1 Create ProblemState

Create:

problem_state.h

problem_state.cpp

Define:

struct ProblemState {
    std::string problem_description;
    std::vector<std::string> hypotheses;
    std::vector<std::string> constraints;
    std::vector<std::string> unknowns;
    std::vector<std::string> rejected_paths;
};

Must be persistent during reasoning loop.

5.2 Implement Scientific Mode

Inside ExecutiveController:

If goal classified as scientific:

Activate loop:

Generate hypotheses

Extract constraints

Evaluate feasibility

Generate alternatives

Score options

Iterate until stable

Each pass updates ProblemState.

No single-shot answers.

5.3 Add Evaluation Scoring

Each hypothesis must be scored on:

Feasibility

Energy efficiency

Cost realism

Risk level

Novelty

Return structured comparison table.

STAGE 6 — SELF-IMPROVEMENT SYSTEM

Goal: Thoth adapts over time.

6.1 Tool Performance Metrics

Track per tool:

Success rate

Latency

Error frequency

Store in SQLite.

6.2 Strategy Memory

Store:

Successful plan templates

Failed plan templates

Execution time statistics

Planner should prefer historically successful strategies.

6.3 Prompt Versioning

Store:

Prompt versions

Success metrics

Regression results

Allow safe rollback.

DEVELOPMENT ORDER (DO NOT SKIP)

Stage 1 — Executive foundation

Stage 2 — SQLite migration

Stage 3 — Coding tools

Stage 4 — Directional retrieval

Stage 5 — Scientific engine

Stage 6 — Self-improvement

Skipping stages will destabilize architecture.

FINAL OBJECTIVE

Thoth must become:

A transparent executive agent

Capable of persistent goal execution

Able to modify and improve its own codebase

Capable of structured scientific exploration

Eventually adaptive and self-optimizing

This document defines the blueprint.

Implementation must prioritize architecture integrity over rapid feature addition.
