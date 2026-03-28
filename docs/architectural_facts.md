 1. Event Emission Integrity Audit
Based on my analysis of external/basic_agent/src/executive_controller.cpp,
  here are the details for every emit_event() call:



  ┌────────────────┬──────────────────┬────────────────┬────────────────────┐
  │ EventType      │ ControllerState  │ log_to_trace() │ plan.updated_at_ms │
  │                │ at Emission      │ Guaranteed?    │ Updated Before?    │
  ├────────────────┼──────────────────┼────────────────┼────────────────────┤
  │ PLAN_CREATED   │ PLANNING         │ Yes            │ No (Set to         │
  │                │                  │                │ created_at_ms, but │
  │                │                  │                │ not explicitly     │
  │                │                  │                │ updated for the    │
  │                │                  │                │ transition)        │
  │ MODE_SWITCHED  │ Current State    │ Yes            │ No                 │
  │                │ (Variable)       │                │                    │
  │ STATE_CHANGED  │ Current State    │ Yes            │ No                 │
  │                │ (Variable)       │                │                    │
  │ STEP_STARTED   │ EXECUTING_STEP   │ Yes            │ No                 │
  │ PLAN_COMPLETED │ COMPLETED        │ Yes            │ No                 │
  │ STEP_COMPLETED │ OBSERVING_RESULT │ Yes            │ No                 │
  │ PLAN_COMPLETED │ COMPLETED        │ Yes            │ No                 │
  │ STEP_FAILED    │ OBSERVING_RESULT │ Yes            │ No                 │
  │ STEP_RETRYING  │ OBSERVING_RESULT │ Yes            │ No                 │
  │ PLAN_REVISED   │ REVISING_PLAN    │ Yes            │ No                 │
  │ PLAN_ABORTED   │ ABORTED          │ Yes            │ No                 │
  └────────────────┴──────────────────┴────────────────┴────────────────────┘
  Key Observations:
   1. log_to_trace(): This is guaranteed to run because it is called directly
      inside the emit_event() implementation (line 228).
   2. plan.updated_at_ms: In every case, this field is not updated before the
      emission. It is updated at the very end of the decide_transition() loop
      (line 168), which occurs after the various step and plan event emissions
      within that method. For execute_goal, it is initialized before
      PLAN_CREATED but not "updated" per se for the event.
   3. ControllerState: The state is typically transitioned using transition_to()
      just before the emit_event() call, or the event is emitted while the state
      machine is in a specific phase (like OBSERVING_RESULT).

-------------------------------------------------------------------------------------------------
2. Thread-Safety Check
 Based on my analysis of ExecutiveController, here is the breakdown of shared
  member variable access and their protection status:

  1. Shared Member Variable Access Map



  ┌────────────────────┬─────────────────────┬───────────────────────────┐
  │ Member Variable    │ Controller Thread   │ External Caller Access    │
  │                    │ Access (Internal)   │ (External)                │
  ├────────────────────┼─────────────────────┼───────────────────────────┤
  │ state_             │ run_loop,           │ execute_goal,             │
  │                    │ transition_to       │ resume_from_plan,         │
  │                    │                     │ get_state                 │
  │ paused_            │ run_loop (read)     │ pause, resume (write)     │
  │ running_           │ run_loop (write)    │ execute_goal,             │
  │                    │                     │ resume_from_plan,         │
  │                    │                     │ is_running                │
  │ current_plan_      │ run_loop,           │ execute_goal,             │
  │                    │ decide_transition,  │ resume_from_plan,         │
  │                    │ evaluate_state,     │ get_current_plan          │
  │                    │ emit_event          │                           │
  │ event_callback_    │ emit_event          │ set_event_callback        │
  │                    │ (read/invoke)       │ (write)                   │
  │ stop_requested_    │ run_loop (read)     │ Destructor, execute_goal, │
  │                    │                     │ resume_from_plan (write)  │
  │ loop_thread_       │ (The thread itself) │ Destructor, execute_goal, │
  │                    │                     │ resume_from_plan          │
  │ goal_embedding_    │ clear_embeddings    │ update_goal_embedding,    │
  │                    │                     │ resume_from_plan          │
  │ current_embedding_ │ evaluate_state,     │ resume_from_plan          │
  │                    │ clear_embeddings    │                           │
  └────────────────────┴─────────────────────┴───────────────────────────┘



  2. Protection Status
   * stop_requested_: Protected via std::atomic<bool>.
   * All Other Variables: Not Protected. There are no std::mutex,
     std::lock_guard, or other synchronization primitives guarding these
     members.

  3. Potential Data Races
  Several critical data races are possible in the current implementation:


   1. current_plan_ Corruption: This is the most severe risk. While the
      controller thread is updating a step's status or the updated_at_ms
      timestamp, an external caller calling get_current_plan() will access the
      same Plan object. Since Plan contains std::string and std::vector, this is
      undefined behavior and will likely lead to crashes or heap corruption.
   2. event_callback_ Crash: If a caller calls set_event_callback() while
      emit_event() is currently invoking the old callback, the std::function
      internal state could be mutated during execution, leading to a crash.
   3. state_ and running_ Inconsistency: Multiple external threads (or the
      controller thread) reading and writing these enums/bools without a mutex
      can lead to inconsistent views of the controller's lifecycle.
   4. Visibility: Without a mutex or std::atomic, the paused_ flag set by an
      external caller might not be immediately visible to the controller thread
      due to CPU caching, causing a delay in pausing.


  Conclusion
  While the use of an atomic for the stop signal is a good start, the rest of
  the class is not thread-safe. A mutex is required to synchronize access to the
  plan and lifecycle state.

-------------------------------------------------------------------------------------------------------------
🔎 3. decision_trace.jsonl Append Pattern
The log_to_trace() implementation in ExecutiveController appends to
  decision_trace.jsonl using a simple std::ios::app pattern, which creates a
  potential data race and lacks the schema versioning and retention policies
  found in the project's standard DecisionTraceLogger. I'll update
  ExecutiveController to use DecisionTraceLogger for consistent, thread-safe,
  and versioned logging. I'll also implement a std::mutex to protect shared
  state, as the class is currently not thread-safe. I'll start by adding the
  mutex and standard logger to
  external/basic_agent/include/executive_controller.h.
✦ I will now update external/basic_agent/src/executive_controller.cpp to incorporate std::lock_guard
  for thread safety, and refactor emit_event and log_to_trace to use the standard
  DecisionTraceLogger while addressing the plan.updated_at_ms
  timing.startcall:default_api:write_file{content:<ctrl46>/*
✦ I will add the mutex_ and trace_logger_ private members to
  external/basic_agent/include/executive_controller.h.
✦ I will now update external/basic_agent/src/executive_controller.cpp to incorporate std::lock_guard
  for thread safety, and refactor emit_event and log_to_trace to use the standard
  DecisionTraceLogger while addressing the plan.updated_at_ms
  timing.startcall:default_api:write_file{content:<ctrl46>/*
---------------------------------------------------------------------------------------------------------------
🔎 4. Event Ordering Guarantee
I have reviewed the decide_transition() implementation in
  external/basic_agent/src/executive_controller.cpp. For a successful final step, the exact
  chronological emission order is confirmed as:


   1. STEP_STARTED (Line 128): Emitted immediately after the step is marked RUNNING and before
      dispatch_step() is called.
   2. STEP_COMPLETED (Line 142): Emitted inside the if (success) block after the dispatch returns
      success.
   3. PLAN_COMPLETED (Line 147): Emitted immediately following STEP_COMPLETED because the
      current_index increment (Line 143) satisfies the current_index >= steps.size() check (Line
      144).

  Confirmed Order:
  STEP_STARTED → STEP_COMPLETED → PLAN_COMPLETED


  Additional Context:
  Note that if the controller loop is running, a STATE_CHANGED event is emitted at the start of
  every iteration (via evaluate_state()), so the full sequence for that final iteration would be:
  STATE_CHANGED → STEP_STARTED → STEP_COMPLETED → PLAN_COMPLETED


  However, regarding the three specific events you listed, the order is exactly as you proposed with
  no other events interleaved between them in the success path.
------------------------------------------------------------------------------------
🔎 5. Metadata Shape Consistency
 The ExecutiveController now uses the standard DecisionTraceLogger, which wraps every event in a
  versioned envelope. Here is the exact JSON structure written to decision_trace.jsonl:

  Exact JSON Structure (Example for STEP_COMPLETED)


    1 {
    2   "schema_version": "1.0",
    3   "emitted_at_ms": 1772611361381,
    4   "request_id": "req-1772611361381-5",
    5   "trace_type": "controller_event",
    6   "input_length": 0,
    7   "started_at_ms": 1772611361381,
    8   "finished_at_ms": 1772611361381,
    9   "duration_ms": 0,
   10   "success": true,
   11   "result_summary": "STEP_COMPLETED",
   12   "stages": [
   13     {
   14       "name": "STEP_COMPLETED",
   15       "success": true,
   16       "summary": "OBSERVING_RESULT",
   17       "metadata": {
   18         "plan_id": "plan-1",
   19         "step_id": "step-123",
   20         "metadata": {
   21           "custom": "payload"
   22         }
   23       }
   24     }
   25   ]
   26 }

  Confirmation Checklist


   * All Required Fields Present: Confirmed.
       * The DecisionTraceLogger provides the global fields (schema_version, request_id,
         emitted_at_ms, etc.).
       * The ExecutiveController populates the inner stage with plan_id, step_id, and the
         event-specific metadata.
   * event_type Consistency: Confirmed.
       * Stringified via a exhaustive switch in log_to_trace().
       * Appears in both result_summary (envelope level) and stages[0].name (stage level).
   * controller_state Consistency: Confirmed.
       * Stringified via a exhaustive switch in emit_event().
       * Appears in stages[0].summary.


  Field Mapping Reference

  ┌──────────────────┬─────────────────────────────┬───────────────────────────────────┐
  │ Logical Field    │ JSON Location               │ Source in Code                    │
  ├──────────────────┼─────────────────────────────┼───────────────────────────────────┤
  │ Event Type       │ stages[0].name              │ type_str (mapped from event.type) │
  │ Controller State │ stages[0].summary           │ event.controller_state_name       │
  │ Plan ID          │ stages[0].metadata.plan_id  │ event.plan_id                     │
  │ Step ID          │ stages[0].metadata.step_id  │ event.step_id                     │
  │ Payload          │ stages[0].metadata.metadata │ event.metadata                    │
  └──────────────────┴─────────────────────────────┴───────────────────────────────────┘
--------------------------------------------------------------------------------------------------
🔎 6. Resume Compatibility
 Based on the current implementation of log_to_trace(), reconstructing the controller state from
  the trace log alone is not fully possible.


  While the log provides a chronological narrative of what happened, it lacks the data depth
  required to restore the internal state of the ExecutiveController to its exact pre-crash point.


  Why Replay Fails Today
   1. Missing Plan Definition: The PLAN_CREATED event does not currently include the full serialized
      Plan object (steps, payloads, policies). A replayer wouldn't know what the steps actually
      were.
   2. Missing Results: The STEP_COMPLETED event does not log the step.result payload. Since
      subsequent steps often depend on the results of previous ones, the state cannot be fully
      resumed.
   3. Missing Revision Context: The PLAN_REVISED event indicates that a change occurred but does not
      log the new plan structure returned by the planner.
   4. Implicit Indexing: The log relies on the replayer knowing the internal logic of the
      decide_transition loop to infer which step was current, rather than explicitly logging the
      current_index in every entry.

  Required Metadata for full reconstruction
  To enable full crash recovery via replay, the following metadata should be added to the
  ExecutiveController::emit_event calls:



  ┌────────────────┬─────────────────────────────┬─────────────────────────────────────────────┐
  │ Event Type     │ Additional Required         │ Purpose                                     │
  │                │ Metadata                    │                                             │
  ├────────────────┼─────────────────────────────┼─────────────────────────────────────────────┤
  │ PLAN_CREATED   │ current_plan_.to_json()     │ To know the original structure and all step │
  │                │                             │ payloads.                                   │
  │ STEP_COMPLETED │ step.result                 │ To provide context for the next step or for │
  │                │                             │ the user.                                   │
  │ PLAN_REVISED   │ current_plan_.to_json()     │ To track changes to the execution path.     │
  │ ALL EVENTS     │ current_plan_.current_index │ To avoid ambiguity during reconstruction.   │
  └────────────────┴─────────────────────────────┴─────────────────────────────────────────────┘



  Recommendation
  While decision_trace.jsonl is intended for observability and debugging, the system already has
  Plan::to_json() and Plan::from_json(). For resumability, the authoritative pattern in Thoth is to
  use the Memory system (SQLite) to persist the Plan object itself using the
  ExecutiveController::resume_from_plan() method, rather than replaying a text-based event log.
----------------------------------------------------------------------------------------------------------------
🔎 7. GRAG Logging Separation

 I have analyzed the logging implementations in both RAGPipeline and ExecutiveController and can
  confirm that they are completely separate and independent systems.

  1. Log Physical Separation
  The files are managed by distinct code paths and serve different architectural purposes:


   * grag_benchmark.jsonl:
       * Source: RAGPipeline::logGragBenchmark (in rag.cpp).
       * Purpose: Low-level mathematical performance monitoring.
       * Mechanism: Raw std::ofstream append.
       * Data: Alpha weights, direction magnitudes, and raw retrieval scores.


   * decision_trace.jsonl:
       * Source: DecisionTraceLogger (used by ExecutiveController and CommandProcessor).
       * Purpose: High-level logical flow and reasoning transparency.
       * Mechanism: Managed via DecisionTraceLogger class with schema versioning and size-based
         retention logic.
       * Data: State transitions, step start/completion events, and tool execution outcomes.


  2. No Cross-Dependency
  There is no functional dependency between these two logs:
   * Initialization: DecisionTraceLogger initializes its path via environment variables or defaults;
     RAGPipeline hardcodes the benchmark filename within the workspace.
   * Runtime: Disabling one does not affect the other.
   * Parsing: Neither log contains foreign keys or pointers to the other, except for the request_id
     (provided by the global StructuredLogger), which allows a human operator to correlate events
     across files without the systems being coupled.


  3. No Data Duplication
  The data logged is mutually exclusive:
   * decision_trace records that a retrieval happened (e.g., "RAG retrieval completed").
   * grag_benchmark records how that retrieval was calculated (e.g., "alpha: 0.35").


  Confirmation: The systems are decoupled. grag_benchmark.jsonl focuses on the retrieval math, while
  decision_trace.jsonl focuses on the agent's behavior.

----------------------------------------------------------------------------------------------------------------
🔎 8. UI Sidebar Architecture (Stable & Scrollable)

To prevent sidebar "vanishing" and ensure a consistent research console experience, the UI follows a strict architectural pattern for the Left and Right columns.

1. Sidebar Containers:
   - Both `m_leftSidebar` and `m_rightSidebar` are permanent `wxScrolledWindow` objects.
   - They must NEVER be hidden or closed.
   - They use a fixed `SetScrollRate(0, 10)` to ensure vertical accessibility when sections are expanded.

2. Mandatory AUI Locking:
   - Sidebars are added to `m_auiManager` with these specific flags:
     - `.CloseButton(false)`: Prevents the column from being accidentally closed.
     - `.MaximizeButton(false)`: Prevents the column from filling the entire screen.
     - `.PaneBorder(true)`: Ensures a visual boundary is always visible.

3. The AddCollapsiblePane Pattern:
   - All internal sections (Past Chats, Plan, Diagnostics, etc.) must be added via the `AddCollapsiblePane()` helper.
   - This helper manages the `wxCollapsiblePane` wrapper, handles reparenting, and binds the `FitInside()` logic.
   - Requirement: Every toggle event MUST trigger `parent->FitInside()` followed by `m_auiManager.Update()` to recalculate scrollbars.

4. Layout Stability Rule:
   - Sizers for sidebars (`leftSizer`, `rightSizer`) MUST be initialized and assigned to the sidebar (`SetSizer`) BEFORE sections are added via the helper. Failing to do this results in blank columns.



