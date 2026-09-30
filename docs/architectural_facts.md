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

**Verified lifecycle ordering (2026-09-29):** the controller sets `COMPLETED` or `FAILED`, stores plan history, and releases its lock before it emits `PLAN_COMPLETED` or `PLAN_FAILED`. Terminal controller state can become externally observable before the corresponding terminal event callback is delivered.

**Verified default retrieval scope (2026-09-29):** goal retrieval admits only `session_attachment` chunks whose owner matches `Memory::getActiveSessionId()`. Indexed content is not automatically eligible. With ALP flags unset, corpus create registers attachment ownership and then indexes. ALP document/session links supersede that route only when `THOTH_ALP_ENABLED` and `THOTH_ALP_TX_INDEX` are both set. An empty scoped retrieval can still be reported as a successful retrieval step.

**Observed, not reclassified (2026-09-29):** on the verified two-step success path, `PLAN_CREATED` was emitted with controller state `IDLE`, and `current_index` stayed 0.

**Verified soft-timeout delivery (2026-09-30):** the opt-in production timeout chain used one deterministic TOOL step with the real 30000 ms budget. At expiry, the tool was still executing and no `STEP_FAILED` had been delivered. `WorkflowEngine::executeStepAsync` constructs the timeout result but destruction of its inner `std::async` future joins the late work before the outer future becomes ready. The controller subsequently stores the timeout failure, discards the late success, and emits `STEP_FAILED` in `OBSERVING_RESULT` with `next_action=continue`, followed by terminal `FAILED` / `PLAN_FAILED`. This is existing soft-timeout/join-before-delivery behavior, not a cancellation repair.

**Verified timeout terminal path (2026-09-30):** all seven frozen timeout oracles passed. This fixture executed once, with no retry, revision, or reflection. Metrics recorded `reflection_skip_reason=timeout_failure`, trajectory score 0, and the unchanged reflection limit 2. One past plan at score 0 and one trajectory containing the timeout error remained; the session's active plan was removed and no strategy was promoted. This differs from ordinary failure → reflection → replacement plan and flag-gated failure → revision of the same plan (§6). The check does not cover a timeout with revision enabled, other policy combinations, or crash/resume. Observed values `PLAN_CREATED` state `IDLE`, final `current_index` 0, and serialized `plan_status` 0 were not reclassified. Validation limits and the separate default-suite environment/MTCP failures are recorded in `completed_improvements_log.md`, 2026-09-30.

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

**Authoritative resume:** `Memory` / SQLite stores the serialized `Plan`; `ExecutiveController::resume_from_plan()` restores execution. Trace log supplements debugging only. `getActivePlan(session)` returns the latest `active_plans` row for that session. `plan_id` is the primary key; the session column is not unique.

**Verified default failure recovery (2026-09-29):** a schema-generated plan leaves `revise_plan_on_failure` false. The verified recovery is failed execution, a trajectory score below 0.6, `REFLECTION_REPLAN`, `create_plan`, a replacement plan, then `COMPLETED`. The verified run reflected once at score 0. Both the failed attempt and the successful attempt remained in `past_plans` and `trajectories`. `PLAN_CREATED` state `IDLE` and `current_index` 0 were observed and were not reclassified.

**Verified reflection replacement (2026-09-29):** `active_plans` holds the in-flight plan. When reflection replaces that plan, the failed attempt is already stored in `past_plans` and `trajectories`. The controller then persists the recovery plan and, still holding its lock and before `PLAN_CREATED`, deletes only the retired plan id from `active_plans`. Terminal `PLAN_COMPLETED` then removes the recovery plan id. After that completion the session has no resumable active plan. Before this repair, the retired failed row stayed in `active_plans` and could become the resume target once the recovery row was deleted. History tables were already separate and were not the defect.

**Verified flag-gated plan revision (2026-09-29):** `revise_plan_on_failure` stays false unless the parser reads it from the model JSON. When the failing synthesis step carries the flag, a real production synthesis failure emits `STEP_FAILED` with `next_action=revise` and enters `REVISING_PLAN`. `LLMPlanner::revise_plan` receives the LLM-facing existing plan plus the failed-step data. A successful revision keeps the original plan id, replaces the active-plan steps in place, and emits `PLAN_REVISED`. Execution then resumes on the revised steps and can reach `COMPLETED`. The failed step stays in the one combined trajectory. That verified run stored one past plan, emitted no reflection, and promoted no strategy. `PLAN_CREATED` state `IDLE` and final `current_index` 0 were observed and were not reclassified. This is a different mechanism from the reflection replacement above.

---

## 7. GRAG Logging Separation

Two independent logs:

| File | Source | Purpose |
|------|--------|---------|
| `grag_benchmark.jsonl` | `RAGPipeline` | Retrieval math (alpha, direction magnitude, scores) |
| `decision_trace.jsonl` | `DecisionTraceLogger` | Agent behavior, state transitions, tool outcomes |

No cross-dependency; correlatable via shared `request_id` when present.

---

## 8. UI Sidebar & Observability Architecture

Mandatory patterns for MainFrame (see also `AGENTS.md`):

1. **Left Knowledge Base:** `m_leftSidebar` is a permanent `wxScrolledWindow` — never hidden. Sections use `AddCollapsiblePane`; toggle events must call `FitInside()` + `m_auiManager.Update()`.
2. **Observability:** one right AUI pane hosting `m_observabilityNotebook` (tabs: Cognitive State, Plan Execution, GRAG Diagnostics, Strategy Engine). Each tab gets full column height; add future panels as new tabs. `CloseButton(false)`.
3. **System State:** bottom AUI `wxNotebook` (RAG / Trajectories / Experiments / Graph / Logs) — stays bottom-docked.
4. **AUI flags:** `.PaneBorder(true)` on side docks; left sidebar `.CloseButton(false)`.
5. **Layout:** Left sidebar sizer must be assigned before adding collapsible panes.

---

*For alignment backlog and doc/code gaps, see [`cursor_list.md`](cursor_list.md).*
