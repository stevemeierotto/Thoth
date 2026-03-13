# wxWidgets UI Modernization Suggestions

This document outlines several strategies to modernize the user interface of the Thoth Control Panel, which is built using the wxWidgets framework.

## 1. Modernize the Layout with `wxAuiManager` (COMPLETED)

The current UI structure is made dynamic and professional using the `wxAuiManager`.

*   **Strategy:** Replaced the basic layout system with `wxAuiManager`.
*   **Current Layout:**
    *   **Left Pane:** A dockable pane for the chat session list (`m_chatList`).
    *   **Center Pane:** The main area for the chat display (`m_chatDisplay`) and user input (`m_inputCtrl`).
    *   **Bottom Pane:** A dockable pane for RAG file management.
*   **Benefit:** This approach provides a flexible, modern, IDE-like feel, allowing users to resize and rearrange panes.

## 2. Use More Advanced Widgets (COMPLETED)

Advanced widgets have been implemented to improve functionality and appearance.

*   **Chat Display:** Replaced `wxStyledTextCtrl` with a custom-drawn **`wxScrolledWindow`** containing bubble panels, providing a modern chat-like interface.
*   **Session List:** The session list uses a **`wxDataViewCtrl`** with a custom `ChatSessionDataViewModel`, providing a professional model-view-controller integration.

## 3. Achieve a Custom Look and Feel (COMPLETED)

The UI has moved beyond default native widgets with custom-drawn components.

*   **Custom Chat Bubbles:** Implemented `ChatMessagePanel` which uses `wxGraphicsContext` to draw modern, rounded chat bubbles with anti-aliasing and distinct themes for user and agent messages.
*   **Visual Feedback:** Added an "Agent is thinking..." indicator to provide real-time status updates during LLM processing.
*   **Dynamic Container:** Replaced the flat text display with a `wxScrolledWindow` that dynamically manages bubble panels.
*   **HiDPI Readiness:** The use of `wxGraphicsContext` ensures that custom UI elements scale cleanly on high-resolution displays.

## 4. Ensure UI Responsiveness During Agent Processing (COMPLETED)

The application remains fully responsive during long-running AI operations.

*   **Problem:** Blocking calls to the `AgentInterface` on the main UI thread would freeze the application.
*   **Solution:** Agent communication is handled via background threads in `AgentInterface`.
    1.  When the user sends a message, `OnSend` triggers the agent's asynchronous processing.
    2.  The background thread generates the response.
    3.  Upon completion, the `onResponse` callback uses `wxTheApp->CallAfter` to safely update the UI from the main thread.
*   **Benefit:** The UI remains fully responsive. Status bar updates provide visual feedback that processing is underway.

# UI Improvements — Addendum
# Added: 2026-03-03
# Append this section to ui_improvements.md

---

## 5. GRAG Diagnostics Panel

### Motivation

The GRAG system logs retrieval diagnostics to `grag_benchmark.jsonl` on every
query. This data is only useful if visible. A diagnostics panel surfaces it
in real time so the developer can observe whether GRAG is activating, how
strongly it is steering retrieval, and whether retrieved chunks are improving.

### Recommended Widget

A dockable `wxAuiManager` pane, positioned below or beside the chat panel.
Collapsible so it does not clutter normal use. Visible by default only when
a goal is active.

### Contents

**Retrieval Mode Indicator**

A small status badge showing the current scoring type:
- Grey pill: `RAG` — no goal active, standard cosine scoring
- Blue pill: `GRAG BLENDED` — goal active, mixed scoring
- Green pill: `GRAG` — fully directional, alpha near 1.0

This updates after every query response.

**Alpha Gauge**

A horizontal progress bar, 0.0 to 1.0, labeled "Directional Strength".
- 0.0 = pure RAG behavior
- 1.0 = fully goal-directed

Color gradient: grey at 0 → blue at 0.5 → green at 1.0. Gives instant
visual sense of how strongly GRAG is steering without reading numbers.

**Direction Magnitude**

A single numeric readout labeled "Goal Distance". This is `||G - C||` — how
far the current state is from the goal embedding. Helps the user understand
whether progress is being made across a session. Should decrease over time
as a goal is pursued successfully.

**Retrieved Chunk Score List**

A small `wxListCtrl` showing the top-K chunks returned by the last retrieval:
- Chunk preview (first 60 characters)
- Final score (after GRAG re-ranking)
- Source file name

Clicking a chunk row expands it inline or opens a detail popup showing the
full chunk text and both its RAG score and GRAG score side by side. This is
the core comparison tool for evaluating whether GRAG is improving retrieval.

**Per-Query History Sparkline**

A simple line graph (last 20 queries) showing alpha over time. Built with
`wxGraphicsContext`, same pattern as the chat bubble rendering. Lets the
developer see at a glance whether goal-directedness is increasing, stable,
or erratic across a session.

### Implementation Notes

- Panel reads from `GragDiagnostics` struct populated by `GragScorer::rescore()`
- Data reaches the UI via the existing `onResponse` callback pattern —
  extend the response payload to include a `diagnostics` field
- Do not read `grag_benchmark.jsonl` at runtime — use the in-memory
  `GragDiagnostics` struct passed through the pipeline
- Panel must remain functional with diagnostics showing `scoring_type: "rag"`
  when no goal is active — do not hide the panel entirely, show the grey RAG
  badge so the user knows GRAG is standing by

---

## 6. Active Goal Display

### Motivation

GRAG activates when a goal embedding is present. The user needs to always
know whether a goal is active, what it is, and be able to clear it.

### Recommended Widget

A slim banner bar between the session list and the chat display. Hidden when
no goal is active. When a goal is set:

- Shows goal text (truncated to one line with tooltip for full text)
- Shows goal status badge: `ACTIVE`, `COMPLETED`, `ABORTED`
- A small [×] button to clear the goal (returns to plain RAG mode)
- A small [✎] button to revise the goal text

Background color shifts subtly (light blue tint) when a goal is active so
the user always has peripheral awareness of GRAG mode.

### Implementation Notes

- Goal state comes from `ExecutiveController::get_current_plan()`
- Banner subscribes to `ControllerEvent` callbacks —
  `PLAN_CREATED`, `PLAN_COMPLETED`, `PLAN_ABORTED` drive show/hide/status
- Clearing the goal must call a method that resets goal and current
  embeddings to empty vectors, which AUTO mode will interpret as RAG fallback

---

## 7. ExecutiveController State Visualizer

### Motivation

`ExecutiveController` is a state machine with explicit states:
`IDLE`, `PLANNING`, `EXECUTING_STEP`, `OBSERVING_RESULT`, `REVISING_PLAN`,
`SCIENTIFIC_MODE`, `COMPLETED`, `ABORTED`, `FAILED`.

These transitions are currently only visible in `decision_trace.jsonl`.
Surfacing them in the UI makes the agent transparent and debuggable.

### Recommended Widget

A horizontal step-progress strip across the top of the chat area, similar
to a stepper component. Each `PlanStep` in the current plan is shown as a
node in the strip:

- Grey circle: PENDING
- Pulsing blue circle: RUNNING (animated)
- Green circle with checkmark: SUCCESS
- Red circle with ×: FAILED
- Orange circle: RETRYING

Clicking any node opens a detail popup showing:
- Step description
- Step type (TOOL / RETRIEVAL / LLM / NODE)
- Input payload
- Result payload
- Timing (execution_time_ms)
- Retry count

This is the n8n-style visual harness described in `NODE.md`. It does not
need to be interactive in v1 — read-only observation is sufficient and
valuable. Interactive node editing comes later.

### Implementation Notes

- Subscribes to `ControllerEvent` via `set_event_callback()` on
  `ExecutiveController` — specifically `STEP_STARTED`, `STEP_COMPLETED`,
  `STEP_FAILED`, `STEP_RETRYING`, `PLAN_REVISED`
- Events carry `step_id` and `plan_id` — use these to update the correct
  node in the strip without a full redraw
- Strip resets and redraws when `PLAN_CREATED` fires
- Strip must be scrollable horizontally for plans with many steps
- Render with `wxGraphicsContext` for consistent anti-aliasing

---

## 8. RAG File Slot Enhancements

### Motivation

Testing GRAG accuracy requires controlled RAG file loading — starting with
one file and incrementally adding up to four. The current file slot UI
should surface enough information to make this testing workflow smooth.

### Recommended Additions

**Per-Slot Chunk Count**

After a file is indexed, show the number of chunks it produced beside the
filename label. Gives the user immediate feedback on whether the file was
large enough to be useful and helps explain retrieval behavior.

**Per-Slot Index Status Badge**

- Grey: empty slot
- Yellow spinner: indexing in progress
- Green: indexed and ready
- Red: indexing failed (with tooltip showing error)

**Slot Enable/Disable Toggle**

A small checkbox per slot that temporarily excludes that file from retrieval
without removing it from the slot. Essential for controlled GRAG testing —
lets the user isolate which files are contributing to a retrieval result
without re-dropping files.

**Active Retrieval Highlight**

When the GRAG diagnostics panel shows the retrieved chunk list, highlight
which RAG file slot each chunk came from using a color-coded left border
that matches a color indicator on the slot label. Visual correlation between
source and result.

### Implementation Notes

- Chunk count comes from `IndexManager` after indexing completes — expose
  a `getChunkCount(file_path)` method if not already available
- Slot enable/disable must propagate to `IndexManager` before each query —
  disabled slots are excluded from the retrieval candidate set
- Color coding per slot: use a fixed palette of four distinct colors,
  assigned in slot order (slot 1 = blue, slot 2 = green, slot 3 = orange,
  slot 4 = purple)

---

## Cross-Cutting Implementation Note

All new panels and widgets must follow the existing patterns:

- Dockable via `wxAuiManager`
- Data reaches UI through callbacks and response payloads, never by reading
  log files at runtime
- All UI updates posted via `wxTheApp->CallAfter` from background threads
- Custom drawing uses `wxGraphicsContext` for HiDPI consistency
- No direct access to `GragScorer`, `ExecutiveController`, or any core
  agent class from GUI code — always bridged through `AgentInterface`

---

End of Addendum

---

## Strategic UI Recommendations & Bridging with the Backend Roadmap

*Critique of the backend-focused `improvements.md` plan and its implications for the UI.*

The project's backend roadmap (`improvements.md`) is comprehensive from a technical standpoint but currently lacks any connection to the user-facing `ThothControlPanel` application. The following points should be addressed to create a cohesive product strategy.

### 1. The Critical Gap: UI/UX Integration is Missing

The backend plan details powerful capabilities (e.g., "Web Scraping Tool," "Coding Agent Tool") but does not specify how a user will interact with them. The GUI is the entire point of user interaction and must be planned alongside the backend.

*   **Recommendation:** A new **"UI/UX Integration Strategy"** section should be added to the main roadmap. This section must map each backend feature to a user story and a corresponding UI design. This is the perfect place to incorporate the specific wxWidgets suggestions (like using `wxAuiManager` for new panes or `wxStyledTextCtrl` for code diffs) to realize the new functionality.

### 2. Observability vs. User Feedback

The backend plan focuses on logging for developers. A parallel effort is needed to design how the agent communicates its status to the user through the UI.

*   **Recommendation:** For each new tool or agent capability, define the "User Feedback" requirements. What does success, failure, or a work-in-progress state look like from the user's perspective? This involves designing specific UI elements to convey this information, not just logging it to a file.

### 3. Holistic End-to-End Testing

The current testing strategy in the backend plan is missing a crucial layer.

*   **Recommendation:** A plan for **End-to-End (E2E) Testing** is needed. This involves creating a test suite that can drive the wxWidgets application itself (e.g., by simulating UI events) to verify that the entire workflow—from a button click in the GUI to the backend processing and back to the UI display—is working correctly.
