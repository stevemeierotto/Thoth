Stabilization Plan: Observability-First
You're right to pause behavioral changes. Based on the audit findings in gemini_thoughts.md and architectural_facts.md, here's what's actually broken and how to fix it without touching behavior:

The Real Problem
You have three disconnected flows that should be one observable pipeline:
User Input
    ↓
CommandProcessor::processQuery   ← always CONVERSATIONAL, ignores active goal
    ↓
RAGPipeline                      ← defaults to conversations_index
    ↓
DefaultPlanner                   ← only generates LLM stub steps, never RETRIEVAL
    ↓
ExecutiveController              ← runs fine but retrieval never fires
GRAG is implemented and verified through Phase 8. It's just never being called during goal execution.

Phase 1 — Logging First (No Behavior Changes)
Before touching anything else, instrument the path so you can see what's happening:
1. Request-scoped trace ID
Every user message should generate a request_id that flows through CommandProcessor → RAGPipeline → ExecutiveController. Right now you can't correlate grag_benchmark.jsonl events to decision_trace.jsonl events for the same request.
2. Add a routing_diagnostic log entry at the top of processQuery
Log exactly what mode it selected and why:
json{
  "event": "routing_decision",
  "request_id": "...",
  "goal_active": false,
  "routing_mode": "CONVERSATIONAL",
  "indexes_selected": ["conversations_index"],
  "reason": "no active goal detected"
}
```
This one log line will immediately tell you if goal injection is working or not.

**3. Add a `planner_plan_dump` log in `DefaultPlanner::create_plan`**
Log the full plan JSON the moment it's created. Right now you can't see what steps were generated unless you attach a debugger.

---

### Phase 2 — Fix the Two Root Causes (Minimal, Surgical)

Once logging confirms the flow, two small fixes stabilize everything:

**Fix A — `DefaultPlanner` must emit a RETRIEVAL step**

The planner stub is the single biggest gap. A plan for any goal should minimally be:
```
Step 1: RETRIEVAL  (mode=grag, index=auto)
Step 2: LLM        (summarize/act on retrieved context)
This one change makes GRAG fire during every goal execution. No architectural change needed.
Fix B — processQuery must check for an active goal
cpp// Before building RAG context:
if (controller->get_state() != ControllerState::IDLE) {
    auto plan = controller->get_current_plan();
    rag_context.goal = plan.goal;
    rag_context.goal_present = true;
    rag_context.routing_mode = PLAN_AWARE;
}
This is ~5 lines and makes conversational chat during an active goal benefit from directional scoring. The routing_diagnostic log from Phase 1 will confirm it works immediately.

Phase 3 — Tool Hallucination (System Prompt Fix Only)
From gemini_thoughts.md section 6, the fix is purely in PromptFactory — no logic changes:
Add to the default system prompt:

"Respond in natural language unless you need to perform an action. Only use the JSON tool format when a tool is strictly necessary."

Soften getToolList() header from imperative ("To call a tool, respond with...") to conditional ("If you need to call a tool, use this format...").
This costs nothing to revert if it changes behavior unexpectedly.

Stabilization Checklist
PriorityChangeRiskConfirms1Add routing_diagnostic logZeroWhat mode is selected2Add planner_plan_dump logZeroWhat steps are generated3Add shared request_id across logsZeroCross-log correlation4DefaultPlanner adds RETRIEVAL stepLowGRAG fires5processQuery injects active goalLowDirectional scoring in chat6System prompt tool-use guardLowEliminates hallucinated calls
Do phases 1–3 (logging) first and share what you see. The logs will tell you exactly which of fixes 4–6 are actually needed and in what order. Don't change behavior until the logs confirm the diagnosis.
