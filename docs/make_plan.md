# make_plan.md
# Thoth — ExecutiveController Implementation Guide
# For: Gemini Implementation Session
# Status: Authoritative build instructions

---

## Context

You are implementing the **ExecutiveController** system for the Thoth project.

Thoth is a C++ AI agent application with a wxWidgets GUI. The core agent lives in `external/basic_agent/` and compiles to `libbasic_agent.so`. The GUI links against it. **Do not touch GUI files** (`src/MainFrame.cpp`, `src/AgentInterface.cpp`, `src/VisualizationFrame.cpp`) unless specifically instructed.

Before writing any code, read `AGENTS.md` in full. It defines all naming conventions, file locations, and critical rules. Key points:

- All new source files go in `external/basic_agent/src/`
- All new headers go in `external/basic_agent/include/`
- Use `snake_case` for filenames, `PascalCase` for class names
- JSON library is `nlohmann/json` at `external/basic_agent/include/json.hpp` — do NOT introduce a second JSON library
- All new source files must be added to `external/basic_agent/CMakeLists.txt`
- MIT license headers must be preserved in all files

---

## What You Are Building

The **ExecutiveController** is the cognitive orchestration core of Thoth.

Its identity in one sentence:

> A resumable, observable, goal-driven state machine that executes a Plan over a runtime environment.

It does NOT:
- Contain UI logic
- Know about wxWidgets
- Directly call the LLM
- Directly execute tools
- Directly manipulate GRAG internals

It ONLY orchestrates. Everything else plugs into it.

---

## Critical Architectural Rules

1. **ExecutiveController never calls the LLM directly.** It calls `IPlanner`.
2. **ExecutiveController never executes tools directly.** It dispatches through `ToolRegistry`.
3. **All state transitions must be logged** to `decision_trace.jsonl`.
4. **Plan must be serializable to JSON** — resume capability depends on this.
5. **Events are abstract** — not wxWidgets events. The UI subscribes via `std::function` callback.
6. **Scientific mode uses Strategy Pattern** — not a nested state machine.

---

## Phase 1 — Data Structures and Interfaces (DO THIS FIRST, NO LOOP YET)

### Files to create:

- `external/basic_agent/include/plan.h`
- `external/basic_agent/include/iplanner.h`
- `external/basic_agent/include/iexecution_mode.h`
- `external/basic_agent/include/controller_event.h`
- `external/basic_agent/src/plan.cpp`

---

### plan.h

Define all enums and structs. Everything else depends on these being stable.

```cpp
// StepType — what kind of work this step does
enum class StepType {
    TOOL,       // Execute a registered tool via ToolRegistry
    RETRIEVAL,  // RAG or GRAG retrieval
    LLM,        // LLM reasoning call via IPlanner
    NODE        // Future: execute a NODE graph node by node_id
};

// StepStatus — lifecycle state of a single step
enum class StepStatus {
    PENDING,
    RUNNING,
    SUCCESS,
    FAILED,
    SKIPPED
};

// PlanStatus — lifecycle state of the whole plan
enum class PlanStatus {
    ACTIVE,
    COMPLETED,
    FAILED,
    ABORTED
};

// StepFailurePolicy — per-step behavior on failure
struct StepFailurePolicy {
    int max_retries = 1;
    bool abort_on_failure = false;
    bool revise_plan_on_failure = false;
};

// PlanStep — a single unit of work inside a Plan
struct PlanStep {
    std::string id;                  // UUID string
    std::string description;         // Human-readable label
    StepType type;
    nlohmann::json payload;          // Tool args, retrieval config, node_id, etc.
    StepStatus status = StepStatus::PENDING;
    int retry_count = 0;
    StepFailurePolicy failure_policy;
    nlohmann::json result;           // Structured output after execution
    std::string reasoning;           // Why this step exists (for trace logging)
    int64_t started_at_ms = 0;
    int64_t completed_at_ms = 0;

    nlohmann::json to_json() const;
    static PlanStep from_json(const nlohmann::json& j);
};

// Plan — the full goal execution plan
struct Plan {
    std::string id;                  // UUID string
    std::string goal;
    std::vector<PlanStep> steps;
    size_t current_index = 0;
    PlanStatus status = PlanStatus::ACTIVE;
    int64_t created_at_ms = 0;
    int64_t updated_at_ms = 0;

    nlohmann::json to_json() const;
    static Plan from_json(const nlohmann::json& j);
};
```

**Requirement:** `to_json()` and `from_json()` must be fully implemented in `plan.cpp`. Plan serialization is not optional — resume capability depends on it.

---

### iplanner.h

```cpp
class IPlanner {
public:
    virtual Plan create_plan(const std::string& goal) = 0;
    virtual Plan revise_plan(const Plan& current_plan,
                             const nlohmann::json& step_result,
                             const std::string& revision_reason) = 0;
    virtual ~IPlanner() = default;
};
```

Rules for implementors:
- Must use strict JSON schema when prompting the LLM
- Must reject malformed plan output (throw or return error plan)
- Must use low temperature
- Must never execute tools inside the planner

---

### iexecution_mode.h

```cpp
// Forward declare to avoid circular dependency
class ExecutiveController;

class IExecutionMode {
public:
    virtual void step(ExecutiveController& controller) = 0;
    virtual std::string mode_name() const = 0;
    virtual ~IExecutionMode() = default;
};
```

Two concrete implementations will be created later:
- `StandardExecutionMode` — normal tool/retrieval/LLM step execution
- `ScientificExecutionMode` — hypothesis-evaluate-iterate loop (Phase 5, not now)

---

### controller_event.h

```cpp
enum class EventType {
    PLAN_CREATED,
    STEP_STARTED,
    STEP_COMPLETED,
    STEP_FAILED,
    STEP_RETRYING,
    PLAN_REVISED,
    PLAN_COMPLETED,
    PLAN_ABORTED,
    PLAN_FAILED,
    STATE_CHANGED,
    MODE_SWITCHED
};

struct ControllerEvent {
    EventType type;
    std::string plan_id;
    std::string step_id;       // Empty string if not step-specific
    std::string controller_state_name;
    nlohmann::json metadata;   // Flexible payload for UI and logging
    int64_t timestamp_ms;
};

// Callback type — UI, CLI, and trace logger all subscribe via this
using EventCallback = std::function<void(const ControllerEvent&)>;
```

---

## Phase 2 — ExecutiveController Core Loop

### Files to create:

- `external/basic_agent/include/executive_controller.h`
- `external/basic_agent/src/executive_controller.cpp`

---

### executive_controller.h

```cpp
enum class ControllerState {
    IDLE,
    PLANNING,
    EXECUTING_STEP,
    OBSERVING_RESULT,
    REVISING_PLAN,
    SCIENTIFIC_MODE,
    COMPLETED,
    ABORTED,
    FAILED
};

class ExecutiveController {
public:
    explicit ExecutiveController(
        std::shared_ptr<IPlanner> planner,
        std::shared_ptr<ToolRegistry> tool_registry
    );

    // Primary entry point — drives the full goal to completion
    void execute_goal(const std::string& goal);

    // Pause/resume support
    void pause();
    void resume();
    bool is_running() const;

    // Resume from a persisted plan (loaded from SQLite or JSON)
    void resume_from_plan(const Plan& plan);

    // Observability — subscribe to lifecycle events
    void set_event_callback(EventCallback callback);

    // Mode switching (called internally, but exposed for testing)
    void set_execution_mode(std::unique_ptr<IExecutionMode> mode);

    // State inspection (for UI highlighting and trace logging)
    ControllerState get_state() const;
    const Plan& get_current_plan() const;

private:
    // The main loop — runs until COMPLETED, ABORTED, or FAILED
    void run_loop();

    // Called each iteration — updates C embedding when GRAG is integrated
    void evaluate_state();

    // Decide and perform the next transition
    void decide_transition();

    // Dispatch a single PlanStep to the appropriate executor
    nlohmann::json dispatch_step(PlanStep& step);

    // Transition helpers
    void transition_to(ControllerState new_state);
    void emit_event(const ControllerEvent& event);
    void log_to_trace(const ControllerEvent& event);

    std::shared_ptr<IPlanner> planner_;
    std::shared_ptr<ToolRegistry> tool_registry_;
    std::unique_ptr<IExecutionMode> execution_mode_;
    EventCallback event_callback_;

    Plan current_plan_;
    ControllerState state_ = ControllerState::IDLE;
    bool paused_ = false;
    bool running_ = false;

    // GRAG stubs — populated in Phase 4
    std::vector<float> goal_embedding_;    // G
    std::vector<float> current_embedding_; // C
};
```

---

### executive_controller.cpp — The Loop

The core loop logic must follow this structure exactly:

```
execute_goal(goal):
    transition_to(PLANNING)
    current_plan_ = planner_->create_plan(goal)
    emit PLAN_CREATED event
    transition_to(EXECUTING_STEP)
    run_loop()

run_loop():
    while state_ != COMPLETED && state_ != ABORTED && state_ != FAILED:
        if paused_: sleep briefly, continue

        evaluate_state()     // GRAG hook — stub for now, just log
        decide_transition()  // Core dispatch logic

decide_transition():
    step = current_plan_.steps[current_plan_.current_index]

    transition_to(EXECUTING_STEP)
    emit STEP_STARTED

    result = dispatch_step(step)

    transition_to(OBSERVING_RESULT)

    if result["status"] == "success":
        step.status = SUCCESS
        emit STEP_COMPLETED
        current_plan_.current_index++
        if current_plan_.current_index >= steps.size():
            transition_to(COMPLETED)
            emit PLAN_COMPLETED
        else:
            transition_to(EXECUTING_STEP)

    else:  // failure
        step.retry_count++
        emit STEP_FAILED

        if step.retry_count <= step.failure_policy.max_retries:
            emit STEP_RETRYING
            // loop will retry this step

        else if step.failure_policy.revise_plan_on_failure:
            transition_to(REVISING_PLAN)
            current_plan_ = planner_->revise_plan(current_plan_, result, "step failed")
            emit PLAN_REVISED
            transition_to(EXECUTING_STEP)

        else if step.failure_policy.abort_on_failure:
            transition_to(ABORTED)
            emit PLAN_ABORTED

        else:
            step.status = FAILED
            current_plan_.current_index++
            // continue to next step

dispatch_step(step):
    switch step.type:
        TOOL:      look up tool in tool_registry_, validate input, execute, return result
        RETRIEVAL: stub — return success with empty chunks for now
        LLM:       call planner_ with step payload (or stub)
        NODE:      stub — return success, log "NODE dispatch not yet implemented"
```

---

### evaluate_state() — GRAG Stub (Phase 1)

For now, implement as a no-op that just logs:

```cpp
void ExecutiveController::evaluate_state() {
    // GRAG integration point — Phase 4
    // When GRAG lands: update current_embedding_ C here
    // Compare with goal_embedding_ G
    // If direction alignment drops below threshold, trigger REVISING_PLAN
    log_to_trace({ EventType::STATE_CHANGED, ... });
}
```

This stub keeps the hook in place without blocking Phase 1 compilation.

---

## Phase 3 — StandardExecutionMode

### File to create:
- `external/basic_agent/include/standard_execution_mode.h`
- `external/basic_agent/src/standard_execution_mode.cpp`

```cpp
class StandardExecutionMode : public IExecutionMode {
public:
    void step(ExecutiveController& controller) override;
    std::string mode_name() const override { return "standard"; }
};
```

`step()` implementation: pull the current step from the controller's plan, dispatch it, handle result. This encapsulates the step execution strategy so `ScientificExecutionMode` can swap in later without touching `ExecutiveController`.

---

## Phase 4 — Trace Logging (All Events)

Every `emit_event()` call must also write to `decision_trace.jsonl`.

Format per entry:

```json
{
  "timestamp_ms": 1234567890,
  "event_type": "STEP_STARTED",
  "plan_id": "uuid",
  "step_id": "uuid",
  "controller_state": "EXECUTING_STEP",
  "metadata": {}
}
```

Use the existing `decision_trace.jsonl` append pattern already in the codebase. Do not invent a new logging mechanism.

---

## Phase 5 — Unit Tests

Add to `tests/unit_tests.cpp`:

1. **Plan serialization round-trip** — `to_json()` → `from_json()` → fields match
2. **Stub planner produces valid plan** — `create_plan()` returns non-empty steps
3. **Controller reaches COMPLETED** — single-step plan with stub tool succeeds
4. **Retry logic** — step fails N times, retries correctly, then fails definitively
5. **abort_on_failure** — controller transitions to ABORTED state correctly
6. **revise_plan_on_failure** — planner's `revise_plan()` is called on failure
7. **Event callback fires** — all expected events emitted in correct order

Use stub implementations of `IPlanner` and `ITool` for all tests. Do not require a live Ollama connection.

---

## Build Checklist

After each phase:

```bash
cmake --build --preset debug
./tests/thoth-unit-tests
```

Both must pass before moving to the next phase.

Add all new `.cpp` files to `external/basic_agent/CMakeLists.txt` under the `SOURCES` list.

---

## What NOT to Build Yet

- ScientificExecutionMode (Phase 5 in thoth.md — deferred)
- GRAG embedding calls (Phase 4 in thoth.md — stubs only)
- NODE graph execution (Phase 6 in thoth.md — stub dispatch only)
- UI event wiring to wxWidgets (separate task)
- Multi-agent coordination (future)

The stubs must exist as compile-time hooks but must not be implemented.

---

## File Summary

| File | Location | Purpose |
|---|---|---|
| `plan.h` / `plan.cpp` | `include/` / `src/` | Plan, PlanStep, enums, serialization |
| `iplanner.h` | `include/` | IPlanner pure virtual interface |
| `iexecution_mode.h` | `include/` | IExecutionMode strategy interface |
| `controller_event.h` | `include/` | ControllerEvent struct + EventCallback type |
| `executive_controller.h` / `.cpp` | `include/` / `src/` | Core state machine and loop |
| `standard_execution_mode.h` / `.cpp` | `include/` / `src/` | Default step execution strategy |

---

## Guiding Principle

> Do not write ExecutiveController until Plan and PlanStep are stable.
> Do not add GRAG until the loop runs cleanly.
> Do not add Scientific mode until Standard mode is verified.
> Stubs are correct. Skipping phases is not.

---

End of make_plan.md
