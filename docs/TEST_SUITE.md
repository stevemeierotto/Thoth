# TEST_SUITE.md
# Thoth Basic Agent — Repeatable Pipeline Test Suite
# Run this suite before merging any change to the basic_agent subsystem.

---

## How to Use This Document

**GUI quick pass:** see **[TEST_SUITE_GUI_CHECKLIST.md](TEST_SUITE_GUI_CHECKLIST.md)** (~30 min, Observability-focused).

Each test case is self-contained. You run the agent, perform the described
action, then check the specified log signals. A test passes only when ALL
signals for that test are confirmed.

**Log files to have open:**
- `decision_trace.jsonl` — agent behavioral flow
- `grag_benchmark.jsonl` — retrieval math
- Look for `routing_decision` entries inside decision_trace

**Tip:** Run `tail -f decision_trace.jsonl | python3 -m json.tool` in a
terminal while testing for live readable output.

---

## TC-01: Plain Chat, No Goal Active

**Purpose:** Confirm the agent responds naturally when no goal is set,
uses standard retrieval, and does not call any tools.

**Steps:**
1. Start the agent fresh (no prior goal in session).
2. Type: `What is GRAG?`
3. Wait for response.

**Pass Signals:**

In `routing_decision` log entry:
```json
{
  "goal_active": false,
  "routing_mode": "CONVERSATIONAL"
}
```

In agent response:
- Natural language answer. No JSON tool_call block present.
- No `gmail_read_labels` or other tool invocation in logs.

In `grag_benchmark.jsonl` (if a retrieval entry appears):
```json
{
  "scoring_type": "cosine",
  "alpha": 0.0
}
```

**Fail Conditions:**
- `goal_active: true` when no goal was set → processQuery sync bug
- JSON tool_call present in response → prompt guardrail not working
- `routing_mode: PLAN_AWARE` → false positive goal detection

---

## TC-02: Set Goal — Verify Plan Structure

**Purpose:** Confirm that setting a goal produces a plan with a RETRIEVAL
step as the first step, not just an LLM stub.

**Steps:**
1. Start fresh session.
2. Set goal: `/goal "Analyze the ExecutiveController and summarize its state machine"`
3. Wait for plan to execute (watch for PLAN_COMPLETED in trace).

**Pass Signals:**

In `decision_trace.jsonl`, find the PLAN_CREATED entry and confirm
the plan contains at minimum 2 steps. Then confirm:

```json
{ "name": "STEP_STARTED", "metadata": { "step_type": "RETRIEVAL" } }
```
appears BEFORE:
```json
{ "name": "STEP_STARTED", "metadata": { "step_type": "LLM" } }
```

Full required event sequence:
```
PLAN_CREATED
STEP_STARTED (RETRIEVAL)
STEP_COMPLETED
STEP_STARTED (LLM)
STEP_COMPLETED
PLAN_COMPLETED
```

**Fail Conditions:**
- Only LLM steps in plan → DefaultPlanner fix did not land
- PLAN_CREATED followed immediately by PLAN_COMPLETED with no steps
- RETRIEVAL step appears after LLM step

---

## TC-03: GRAG Math Verification

**Purpose:** Confirm that the RETRIEVAL step uses directional scoring
(not plain cosine similarity) when a goal is active.

**Steps:**
Same as TC-02. After PLAN_COMPLETED, check `grag_benchmark.jsonl`.

**Pass Signals:**

Find the benchmark entry that corresponds to the plan's RETRIEVAL step
(match by timestamp or request_id). Confirm:

```json
{
  "scoring_type": "grag",
  "alpha": 1.0,
  "direction_magnitude": "> 0.5"
}
```

The `direction_magnitude` confirms goal embedding ≠ state embedding,
meaning the direction vector is meaningful (not collapsed).

Acceptable range for `alpha`: 0.3 – 1.0 (depends on goal/state distance).
A value of exactly 0.0 is a failure.

**Fail Conditions:**
- `scoring_type: "cosine"` → goal embedding not reaching RAGPipeline
- `alpha: 0.0` → direction vector collapsed or goal not set
- `direction_magnitude: 0.0` → goal and state embeddings are identical
- No entry in grag_benchmark.jsonl → RETRIEVAL step never called RAGPipeline

---

## TC-04: Chat Turn During Active Goal

**Purpose:** Confirm that a user question asked while a goal is still
active uses PLAN_AWARE mode with GRAG scoring, not plain CONVERSATIONAL.

**Steps:**
1. Set a goal: `/goal "Review the GRAG retrieval implementation"`
2. Wait for plan execution to begin (STATE_CHANGED → EXECUTING_STEP).
3. Immediately type a chat question: `What indexes does GRAG use?`
4. Wait for response.

**Pass Signals:**

In `routing_decision` log for that chat turn:
```json
{
  "goal_active": true,
  "routing_mode": "PLAN_AWARE",
  "indexes_selected": ["codebase_index"]
}
```

In `grag_benchmark.jsonl` for that chat turn:
```json
{
  "scoring_type": "grag",
  "alpha": "> 0.0"
}
```

**Fail Conditions:**
- `goal_active: false` despite active goal → goal embedding sync bug
- `routing_mode: "CONVERSATIONAL"` → processQuery fix not working
- `alpha: 0.0` in benchmark → embedding not reaching RAG for chat call

---

## TC-05: UI Scores Panel Updates

**Purpose:** Confirm the UI retrieval diagnostics panel receives and
displays scores from a GRAG retrieval during chat.

**Steps:**
Same as TC-04. After the chat response appears:

**Pass Signals:**
- The RAG scores panel in the UI shows updated chunk scores.
- Scores are non-zero.
- Panel does not show stale data from a previous session.

In logs, confirm a `RETRIEVAL_DIAGNOSTICS` event was emitted
(search decision_trace for this event type).

**Fail Conditions:**
- Scores panel empty or unchanged → UI callback not receiving events
- `RETRIEVAL_DIAGNOSTICS` absent from trace → event not emitted

---

## TC-06: No Tool Hallucination

**Purpose:** Confirm the agent never calls tools for questions that
do not require tool use.

**Steps:**
With a goal active, ask each of these questions one at a time:
1. `Will the scientific reasoning mode work?`
2. `What is the current state of the controller?`
3. `How does GRAG calculate alpha?`

**Pass Signals for each:**
- Response is natural language prose.
- No JSON block with `tool_call` key appears in the response.
- No tool invocation appears in `decision_trace.jsonl` for these requests.
- Specifically: `gmail_read_labels`, `stock_quote`, or any other tool
  name must NOT appear in logs for these queries.

**Fail Conditions:**
- Any tool invocation triggered by a general question → prompt
  guardrail insufficient, add heuristic tool detection in processToolCall

---

## TC-07: Goal Persistence After Plan Completes

**Purpose:** Confirm the goal context persists for chat after the
background plan finishes executing (PLAN_COMPLETED).

**Steps:**
1. Set a goal and wait for PLAN_COMPLETED in trace.
2. After completion, ask: `What did you find?`

**Pass Signals:**
In `routing_decision` for the post-completion chat turn:
```json
{
  "goal_active": true,
  "routing_mode": "PLAN_AWARE"
}
```

The goal should remain active until explicitly cleared or a new
session starts.

**Fail Conditions:**
- `goal_active: false` after PLAN_COMPLETED → goal cleared too early

---

## Quick Reference: Pass/Fail Summary Table

Use this after running all tests. All must pass before merging changes.

| Test | Description | Pass? | Notes |
|------|-------------|-------|-------|
| TC-01 | Plain chat, no goal, conversational mode | | |
| TC-02 | Goal sets plan with RETRIEVAL step first | | |
| TC-03 | GRAG math: alpha > 0, magnitude > 0.5 | | |
| TC-04 | Chat during active goal uses PLAN_AWARE | | |
| TC-05 | UI scores panel updates from GRAG retrieval | | |
| TC-06 | No tool hallucination on general questions | | |
| TC-07 | Goal persists after PLAN_COMPLETED | | |

**All 7 must pass. If any fail, do not proceed to next feature work.**

---

## How to Hand This to Gemini

When starting a new development session, include this file and say:

> "Before making any changes, confirm the test suite in TEST_SUITE.md
> still passes. After making changes, run it again. Do not merge
> anything that introduces a regression against these tests."

This keeps Gemini anchored to verified behavior as it builds new features.

---

End of Test Suite
