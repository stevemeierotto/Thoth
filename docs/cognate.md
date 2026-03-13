# cognate.md
# Thoth Cognitive Executive System — Architecture Summary

---

## 1. What the Cognitive Executive System Is

The Cognitive Executive System is the reasoning and orchestration layer that sits above individual tools and retrieval.

It is what makes Thoth an **agent** rather than a chatbot.

Where GRAG answers the question *"what knowledge do I need right now?"*, the Cognitive Executive System answers the question *"what should I do, and in what order, and what do I do when things go wrong?"*

It is composed of four tightly integrated components:

- **Plan** — the structured representation of a goal broken into executable steps
- **IPlanner** — the interface responsible for creating and revising plans using the LLM
- **WorkflowEngine** — the execution harness that dispatches each step to the correct subsystem
- **ExecutiveController** — the state machine that drives the entire loop from goal to completion

These four components form the cognitive spine of Thoth.

---

## 2. What Has Already Been Implemented

The foundational layer is complete and stabilized.

### ExecutiveController (Stable)

The `ExecutiveController` is a resumable, observable, goal-driven state machine. It runs an asynchronous loop that drives plan execution from start to finish, or until it aborts or is interrupted.

The state machine has these explicit states:

```
IDLE → PLANNING → EXECUTING_STEP → OBSERVING_RESULT → REVISING_PLAN → COMPLETED
                                                                      → ABORTED
                                                                      → FAILED
```

What is confirmed working:
- Thread-safe lifecycle management via `std::mutex`
- `std::atomic<bool>` stop signal for clean shutdown
- Pause and resume capability
- `resume_from_plan()` for mid-plan restart
- Full event emission on every state transition
- `DecisionTraceLogger` integration — all events written to `decision_trace.jsonl` with schema versioning
- GRAG wiring — `goal_embedding_` and `current_embedding_` are live members, passed to `RAGPipeline`
- Unit tests covering: plan serialization, lifecycle (success, retry, abort, revise), event tracing

### Plan & PlanStep (Stable)

`Plan` and `PlanStep` are fully defined and serializable.

`PlanStep` carries per-step failure policy:
- `max_retries`
- `abort_on_failure`
- `revise_plan_on_failure`

`Plan` carries:
- `goal`
- `steps[]`
- `current_index`
- `status`
- `created_at_ms` / `updated_at_ms`
- `to_json()` / `from_json()` for persistence

### IPlanner Interface (Stable)

`IPlanner` is a pure virtual interface. `ExecutiveController` never calls the LLM directly — it always calls through `IPlanner`. This allows the planning model to be swapped, tested with stubs, or replaced with a different reasoning strategy without touching the controller.

```cpp
class IPlanner {
public:
    virtual Plan create_plan(const std::string& goal) = 0;
    virtual Plan revise_plan(const Plan&, const StepResult&) = 0;
};
```

### Event System (Stable)

`ExecutiveController` emits abstract `ControllerEvent` structs. The UI, CLI, and trace logger all subscribe via a single callback — the controller itself knows nothing about wxWidgets or any display layer.

Confirmed event types:
`PLAN_CREATED`, `PLAN_COMPLETED`, `PLAN_REVISED`, `PLAN_ABORTED`,
`STEP_STARTED`, `STEP_COMPLETED`, `STEP_FAILED`, `STEP_RETRYING`,
`STATE_CHANGED`, `MODE_SWITCHED`

---

## 3. What Is Not Yet Built

The foundational scaffold is in place, but the cognitive upgrades that give it real intelligence are still ahead.

### WorkflowEngine (Missing)

Today, `ExecutiveController` dispatches steps directly. There is no dedicated `WorkflowEngine` to centralize step dispatch, async execution, retry policy enforcement, and metrics collection. This needs to be extracted into its own class so the controller loop stays clean.

### Execution Modes / Strategy Pattern (Missing)

`PLAN.md` defines the Strategy Pattern for pluggable execution modes:

```cpp
class IExecutionMode {
public:
    virtual void step(ExecutiveController&) = 0;
};
```

Two modes are planned:
- `StandardExecutionMode` — normal plan execution
- `ScientificExecutionMode` — hypothesis-driven iteration loop

Neither exists yet. `ExecutiveController` currently has a single hardcoded execution path. Scientific mode will be activated by goal classification and swaps the mode pointer without touching the controller loop itself.

### Goal Hierarchy / Subgoal Trees (Missing)

Right now the controller drives toward a single flat goal embedding. The `grag_upgrade.md` defines the `GoalNode` tree structure where the active subgoal — not the root goal — drives GRAG directional retrieval. This dramatically tightens retrieval focus during multi-phase goals.

```cpp
struct GoalNode {
    std::string description;
    Embedding embedding;
    std::vector<GoalNode> children;
    GoalStatus status;
};
```

`ExecutiveController` must learn to activate one subgoal at a time and pass `active_subgoal_embedding` to `GragScorer` instead of the root.

### Trajectory Awareness (Missing)

`ExecutiveController` does not yet maintain an episode log or trajectory embedding. After each step completes, the trajectory should be updated so GRAG can incorporate recent progress — including failures — into retrieval scoring. This prevents the agent from repeating approaches that already failed.

### Resume Completeness Gap (Known Issue)

`resume_from_plan()` exists, but the trace log does not yet contain enough data to fully reconstruct state after a crash. The `PLAN_CREATED` event does not serialize the full plan, and `STEP_COMPLETED` does not include the step result. Full crash recovery requires SQLite-backed plan persistence as the authoritative resume source, not trace log replay.

### Scientific Reasoning Engine (Missing)

`ProblemState` and the hypothesis scoring loop do not yet exist. The controller has a `SCIENTIFIC_MODE` state in the enum, but nothing activates it. This requires:
- `ProblemState` struct (hypotheses, constraints, unknowns, rejected paths)
- Goal classification to detect scientific tasks
- `ScientificExecutionMode` implementing the iterative hypothesis loop
- SQLite persistence for `problem_states`

### Strategy Memory (Missing)

The controller executes plans but does not yet learn from them. `StrategyMemory` will read `step_metrics` to prefer historically successful tools and plan patterns. The planner will accept `StrategyHints` so prior experience influences new plan generation.

---

## 4. How the Cognitive Loop Works (Complete Vision)

When fully built, one goal execution cycle looks like this:

```
User submits goal
        ↓
ExecutiveController receives goal string
        ↓
IPlanner::create_plan()
  → StrategyMemory injects historical hints
  → Planner decomposes goal into GoalNode tree + PlanStep list
        ↓
ExecutiveController activates first subgoal
  → goal_embedding = active_subgoal_embedding
  → current_embedding = structured state embedding
        ↓
[Loop begins]
        ↓
evaluate_state()
  → GRAG re-scores retrieval using D = G_active - C + trajectory T
  → Check if plan revision is needed
        ↓
decide_transition()
  → IExecutionMode::step() dispatches via WorkflowEngine
  → WorkflowEngine routes to: ToolRegistry / RAGPipeline / NodeGraph
  → StepResult returned
        ↓
OBSERVING_RESULT
  → Step metrics logged
  → Trajectory embedding updated
  → If step success → advance current_index
  → If step failure → retry, revise, or abort per StepFailurePolicy
        ↓
If all steps complete → advance to next subgoal → repeat
If root goal complete → COMPLETED
```

Each iteration is logged, observable, resumable, and GRAG-guided.

---

## 5. Relationship to GRAG

GRAG is the retrieval layer. The Cognitive Executive System is the reasoning layer.

They interact at two points:

1. **Before each step** — `evaluate_state()` updates the current state embedding `C` and compares it against the active subgoal embedding `G`. GRAG uses `D = G - C` plus trajectory `T` to retrieve the most directionally relevant context.

2. **Plan revision** — if GRAG retrieval reveals that the current plan is misaligned with the goal direction, `ExecutiveController` transitions to `REVISING_PLAN` and calls `IPlanner::revise_plan()`.

GRAG does not make decisions. The controller does.

---

## 6. Relationship to NODE System

`NODE.md` defines the Node System as an execution harness for workflow composition and subsystem testing. In the current architecture, `PlanStep.type` can be `TOOL`, `RETRIEVAL`, or `LLM`. Eventually it will also accept `NODE`, where the payload contains a `node_id` pointing to a predefined node graph.

`ExecutiveController` will not know the difference. `WorkflowEngine` handles the dispatch. This is the critical design move that keeps the controller clean while the node system evolves independently.

---

## 7. Summary

The Cognitive Executive System is the difference between Thoth being a reactive chatbot and being a goal-directed agent.

What exists today is a solid, thread-safe, observable scaffold: a working state machine, clean interfaces, GRAG wiring, and full trace logging.

What comes next turns that scaffold into a genuine cognitive loop: pluggable execution modes, subgoal trees, trajectory awareness, scientific reasoning, and strategy memory that learns from experience.

GRAG tells Thoth what to look at.
The Cognitive Executive System tells Thoth what to do with it.
