# Thoth — Implementation Status & Architecture Audit Notes

**Last Updated:** 2026-06-17  
**Purpose:** Implementation facts, audit conclusions, and known gaps. For security and tool audits, see [`audit.md`](audit.md).

---

## 1. ExecutiveController Event Emission

Every `emit_event()` call logs via `DecisionTraceLogger` (`log_to_trace()` inside `emit_event()`).

| EventType | ControllerState at Emission | log_to_trace() | plan.updated_at_ms before emit |
|-----------|----------------------------|----------------|-------------------------------|
| PLAN_CREATED | PLANNING | Yes | No (cosmetic gap) |
| MODE_SWITCHED | Current | Yes | No |
| STATE_CHANGED | Current | Yes | No |
| STEP_STARTED | EXECUTING_STEP | Yes | No |
| STEP_COMPLETED | OBSERVING_RESULT | Yes | No |
| STEP_FAILED / STEP_RETRYING | OBSERVING_RESULT | Yes | No |
| PLAN_REVISED | REVISING_PLAN | Yes | No |
| PLAN_COMPLETED | COMPLETED | Yes | No |
| PLAN_ABORTED | ABORTED | Yes | No |

**Open gap:** `plan.updated_at_ms` is updated at the end of `decide_transition()`, not before each emission. Cosmetic only.

---

## 2. Thread Safety

**Status as of 2026-06-17: ✅ Protected**

- `ExecutiveController` shared state is guarded by `mutex_` (`std::lock_guard`) across public methods and the run loop (hardened 2026-03-22; see `completed_improvements_log.md`).
- `stop_requested_` remains `std::atomic<bool>`.
- P1.3 observability fix (2026-06-16): plan-reuse logging must not call `emit_event()` while holding `mutex_` — events are deferred until after unlock.

**Historical note:** Pre-March 2026 audits flagged unprotected access to `current_plan_`, `state_`, and `event_callback_`. That gap is closed in current code.

---

## 3. Decision Trace Logging

- `ExecutiveController` and `CommandProcessor` use `DecisionTraceLogger` (schema version 1.0 envelope).
- Append pattern is managed by the logger (not raw concurrent `std::ios::app` on shared handles).
- `decision_trace.jsonl` is for **observability**; crash resume uses SQLite plan persistence (`resume_from_plan()`), not trace replay.

---

## 4. Event Ordering (Success Path)

For a successful final step, emission order is:

`STEP_STARTED` → `STEP_COMPLETED` → `PLAN_COMPLETED`

When the controller loop runs, a `STATE_CHANGED` may precede that sequence in the same iteration.

---

## 5. Metadata Shape (DecisionTraceLogger)

Controller events appear inside the versioned envelope. Key mappings:

| Logical Field | JSON Location | Source |
|---------------|---------------|--------|
| Event type | `stages[0].name` | Mapped from `EventType` |
| Controller state | `stages[0].summary` | `event.controller_state_name` |
| Plan ID | `stages[0].metadata.plan_id` | `event.plan_id` |
| Step ID | `stages[0].metadata.step_id` | `event.step_id` |
| Payload | `stages[0].metadata.metadata` | `event.metadata` |

---

## 6. Resume Compatibility

**Trace replay alone cannot fully reconstruct controller state** — missing full plan JSON on some events, step results, and explicit `current_index` on every entry.

**Authoritative resume:** `Memory` / SQLite stores the serialized `Plan`; `ExecutiveController::resume_from_plan()` restores execution. Trace log supplements debugging only.

---

## 7. GRAG Logging Separation

Two independent logs:

| File | Source | Purpose |
|------|--------|---------|
| `grag_benchmark.jsonl` | `RAGPipeline` | Retrieval math (alpha, direction magnitude, scores) |
| `decision_trace.jsonl` | `DecisionTraceLogger` | Agent behavior, state transitions, tool outcomes |

No cross-dependency; correlatable via shared `request_id` when present.

---

## 8. UI Sidebar Architecture (Stable & Scrollable)

Mandatory pattern for MainFrame sidebars (see also `AGENTS.md`):

1. **Containers:** `m_leftSidebar` and `m_rightSidebar` are permanent `wxScrolledWindow` objects — never hidden.
2. **AUI flags:** `.CloseButton(false)`, `.MaximizeButton(false)`, `.PaneBorder(true)`.
3. **AddCollapsiblePane:** All sections use the helper; toggle events must call `FitInside()` + `m_auiManager.Update()`.
4. **Layout:** Sidebar sizers must be assigned before adding collapsible panes.

---

*For alignment backlog and doc/code gaps, see [`cursor_list.md`](cursor_list.md).*
