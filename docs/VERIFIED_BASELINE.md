# VERIFIED_BASELINE.md
# Thoth Basic Agent — Verified Behavior Baseline
# Date: 2026-03-12
# Status: LOCKED — Do not modify without running all tests in TEST_SUITE.md

> **Supersession note (2026-06-17):** This baseline remains the regression contract for
> TC-01–TC-06 pipeline behavior below. **Cognate V2** work (scientific mode, strategy
> injection, reflection replan, LLM planner, plan reuse, etc.) post-dates this lock.
> Re-run `TEST_SUITE.md` TC-01–TC-07 and full `ctest` before treating pass dates as current.
> Baseline §1 references `DefaultPlanner`; production may use `LLMPlanner` — re-verify
> TC-02 plan structure on the next manual baseline pass.

---

## What This Document Is

This is the authoritative record of verified, working behavior for the Thoth
Basic Agent cognitive spine. It was established after the stabilization pass
that fixed:

- DefaultPlanner missing RETRIEVAL steps
- processQuery ignoring active goal context (goal_active always false)
- Goal embedding not synced to the UI/chat thread
- Tool hallucinations from missing system prompt guardrails

Any future change that breaks the behaviors listed here is a regression.

---

## 1. Verified Pipeline: End-to-End Flow

The following full pipeline has been confirmed operational:

```
User sets goal
    ↓
ExecutiveController transitions: IDLE → PLANNING → EXECUTING_STEP
    ↓
DefaultPlanner generates Plan with:
    Step 1: RETRIEVAL (mode=grag, index=auto)
    Step 2: LLM
    ↓
RETRIEVAL step fires RAGPipeline
    ↓
RAGPipeline detects goal_active=true → routing_mode=PLAN_AWARE
    ↓
GRAG directional scoring executes (alpha > 0, magnitude > 0)
    ↓
LLM step receives retrieved context
    ↓
ExecutiveController transitions: EXECUTING_STEP → OBSERVING_RESULT → COMPLETED
    ↓
User asks follow-up question in chat (goal still active)
    ↓
processQuery detects active goal → injects goal embedding
    ↓
Chat retrieval uses PLAN_AWARE mode with GRAG scoring
    ↓
UI receives RETRIEVAL_DIAGNOSTICS event → scores panel updates
```

---

## 2. Verified Log Signals

### 2.1 decision_trace.jsonl — Required Events Per Goal Execution

Every goal execution MUST produce these events in this order:

| # | Event | Controller State | Notes |
|---|-------|-----------------|-------|
| 1 | STATE_CHANGED | PLANNING | Goal received |
| 2 | PLAN_CREATED | PLANNING | Plan with ≥2 steps |
| 3 | STATE_CHANGED | EXECUTING_STEP | |
| 4 | STEP_STARTED | EXECUTING_STEP | type=RETRIEVAL |
| 5 | STEP_COMPLETED | OBSERVING_RESULT | |
| 6 | STEP_STARTED | EXECUTING_STEP | type=LLM |
| 7 | STEP_COMPLETED | OBSERVING_RESULT | |
| 8 | PLAN_COMPLETED | COMPLETED | |

If STEP_STARTED for a RETRIEVAL step (event #4) is missing, the planner
fix did not land.

### 2.2 grag_benchmark.jsonl — Required Fields Per Retrieval

Every RETRIEVAL step MUST produce a benchmark entry with:

```json
{
  "scoring_type": "grag",
  "alpha": "> 0.0",
  "direction_magnitude": "> 0.0",
  "indexes_used": ["codebase_index", "..."]
}
```

If `alpha` is 0.0 or `scoring_type` is "cosine", the goal embedding
was not synced to the RAGPipeline for that call.

### 2.3 routing_decision log — Required Fields Per Chat Turn

Every chat turn while a goal is active MUST log:

```json
{
  "event": "routing_decision",
  "goal_active": true,
  "routing_mode": "PLAN_AWARE",
  "indexes_selected": ["codebase_index"]
}
```

If `goal_active` is false during an active goal, the processQuery
sync fix did not land.

---

## 3. Verified Behavioral Constraints

### 3.1 No Tool Hallucinations
The agent must NOT call tools (gmail_read_labels or any other) in
response to general knowledge questions or goal-status queries.

Verified prompt guardrail in effect:
- System prompt states: respond in natural language unless a tool is
  strictly necessary.
- getToolList() uses conditional language (not imperative).

### 3.2 Goal Embedding Persistence
The goal embedding set when a goal starts must remain available to:
- The ExecutiveController background thread (for plan steps)
- The CommandProcessor UI thread (for chat turns)

Both must use the same goal embedding for GRAG scoring.

### 3.3 Plan Structure
DefaultPlanner must always generate a plan with at minimum:
- One RETRIEVAL step as the first step
- One LLM step as the final step

A plan with only LLM steps is a regression.

---

## 4. Known Limitations (Not Regressions)

These are known gaps that are NOT failures — they are documented future work:

1. Resume from trace log is incomplete (authoritative resume uses SQLite).
2. plan.updated_at_ms is not updated before event emission (cosmetic).
3. Scientific mode is **implemented** (2026-03-28; see `make_cognate.md` and
   `completed_improvements_log.md`) but **not covered** by this baseline's TC suite.

---

## 5. Baseline Test Results

Confirmed passing on: **2026-03-12 (pre–Cognate V2 — historical record)**

| Test | Result | Log Signal |
|------|--------|-----------|
| TC-01: Plain chat, no goal | PASS | routing_mode=CONVERSATIONAL |
| TC-02: Set goal, verify RETRIEVAL step | PASS | STEP_STARTED type=RETRIEVAL in trace |
| TC-03: GRAG math active | PASS | alpha=1.0, magnitude ~1.0 (graph_weight=0.3 active) |
| TC-04: Chat during active goal | PASS | routing_mode=PLAN_AWARE in routing log |
| TC-05: UI scores panel updates | PASS | RETRIEVAL_DIAGNOSTICS event received |
| TC-06: No tool hallucination | PASS | No tool_call JSON in response |

---

## 6. Files That Implement This Behavior

| Behavior | Source File |
|----------|------------|
| RETRIEVAL step generation | external/basic_agent/src/default_planner.cpp |
| Goal injection in chat | external/basic_agent/src/command_processor.cpp |
| Goal embedding sync | external/basic_agent/src/rag.cpp |
| System prompt guardrail | external/basic_agent/src/prompt_factory.cpp |
| GRAG scoring math | external/basic_agent/src/rag.cpp |
| Event emission | external/basic_agent/src/executive_controller.cpp |
| Routing diagnostic log | external/basic_agent/src/command_processor.cpp |

---

End of Baseline
