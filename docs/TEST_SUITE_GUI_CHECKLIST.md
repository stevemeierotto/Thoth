# TEST_SUITE — GUI Manual Checklist (V1)

**Purpose:** Repeatable GUI pass for `TEST_SUITE.md` TC-01–TC-07 without re-reading the full spec each session.  
**Headless equivalent:** `./build/debug/tests/run_test_suite --dev` (fast) or `--full` (Ollama).  
**Last GUI pass:** 2026-06-29 (Steve — observability confirmed working)

---

## Before you start

| Item | Path / action |
|------|----------------|
| Build | `cmake --build --preset build-debug --target thoth-control-panel` |
| Ollama | Running with embed + chat models (`nomic-embed-text`, configured LLM) |
| Logs (tail in terminals) | `agent_workspace/decision_trace.jsonl`, `agent_workspace/grag_benchmark.jsonl`, `logs/chat_rag.jsonl`, `logs/cognitive_metrics.jsonl` |
| GUI panels | **Observability** → Plan Execution, GRAG Diagnostics (user-confirmed working 2026-06-29) |

**Note:** `decision_trace.jsonl` lives under **`agent_workspace/`**, not `logs/`.

---

## Quick GUI checklist (~30 min)

| # | Action in GUI | Watch in Observability | Log / file check |
|---|---------------|------------------------|------------------|
| **TC-01** | New session → chat: `What is GRAG?` | Graph → CONVERSATIONAL | `logs/chat_rag.jsonl` — turn logged; no active goal |
| **TC-02** | **Agent → Run Goal** or `goal: Analyze the ExecutiveController…` | Plan Execution: RETRIEVAL then LLM steps appear | `decision_trace.jsonl` — PLAN_CREATED → STEP_STARTED (RETRIEVAL before LLM) |
| **TC-03** | Same goal run as TC-02 | GRAG Diagnostics: alpha > 0 | `grag_benchmark.jsonl` — `alpha > 0`, `direction_magnitude > 0`; or `cognitive_metrics.jsonl` — `grag_alpha` |
| **TC-04** | While goal running (or banner visible), chat: `What indexes does GRAG use?` | GRAG panel updates on chat retrieval | Chat uses goal-scoped retrieval (PLAN_AWARE) — see `chat_rag.jsonl` grounding |
| **TC-05** | Same as TC-04 | **GRAG Diagnostics** shows non-zero scores + chunk list | User visual confirm (no stale panel) |
| **TC-06** | With goal active, ask: `How does GRAG calculate alpha?` | No tool steps in Plan Execution | Response is prose only — no `tool_call` JSON in chat |
| **TC-07** | After PLAN_COMPLETED, chat: `What did you find?` | Goal banner still visible | Post-completion chat turn logged; goal context retained in session |

---

## Pass / fail table (2026-06-29)

| Test | Description | GUI pass? | Evidence |
|------|-------------|-----------|----------|
| TC-01 | Plain chat, conversational | ✅ | `chat_sessions.json` — "What is GRAG?" session; C2 chat RAG validated 2026-06-27 |
| TC-02 | Goal → RETRIEVAL before LLM | ✅ | GUI goal `Tell me about Thoth.` — `step_count=2`, completed; headless 7/7 |
| TC-03 | GRAG math (alpha, magnitude) | ✅ | `cognitive_metrics.jsonl` — `grag_alpha=1.0`, 5 chunks; `grag_benchmark` — mag ≈ 0.78 |
| TC-04 | Chat during active goal | ✅ | Session `active_goal` persisted; post-goal chat in `chat_rag.jsonl` |
| TC-05 | Observability / scores panel | ✅ | **User confirmed** GRAG + Plan panels update during goal/chat |
| TC-06 | No tool hallucination | ✅ | Chat history — prose answers, no tool JSON in GUI session |
| TC-07 | Goal persists after completion | ✅ | `active_goal` retained; "What did you find?" in `chat_rag.jsonl` after goal |

**Headless regression (same contract):** `run_test_suite --dev` 7/7 (2026-06-29); `ctest -L fast` 3/3.

---

## GUI goal vs chat (common confusion)

| You type | Path | Observability Plan panel | Cognitive metrics row |
|----------|------|--------------------------|------------------------|
| Plain chat | `processQuery` | Unchanged (last plan or None) | No new row |
| `goal: …` or `/goal …` | ExecutiveController | Updates on PLAN_CREATED | Yes — on PLAN_COMPLETED |
| **Agent → Run Goal** | `executeGoal` | Updates | Yes |
| **Revise Goal** (banner) | `executeGoal` | Updates | Yes |

---

## After the pass

1. Export metrics: **Benchmarks → Export Cognitive Metrics…** (JSONL or CSV).
2. Optional: `python3 scripts/summarize_cognitive_metrics.py --last 5`
3. Record date in `VERIFIED_BASELINE.md` §5.2 and `completed_improvements_log.md`.
4. Mark **V1** done in `cursor_list.md`.

---

## Fail triage (quick)

| Symptom | Likely cause |
|---------|----------------|
| Plan panel empty during goal | Used plain chat instead of `goal:` / Run Goal |
| `grag_alpha: 0` in metrics | Dev/TfIdf tier or mock path — use full Ollama + GUI, not `--dev` |
| No `decision_trace.jsonl` | Wrong path — use `agent_workspace/decision_trace.jsonl` |
| TC-05 panel stale | Switch session or run new retrieval (chat or RETRIEVAL step) |

---

*Spec detail: [TEST_SUITE.md](TEST_SUITE.md). Automated driver: `tests/run_test_suite.cpp`.*
