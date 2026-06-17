# Thoth: An Experience-Aware Cognitive Agent System

> *"It does not merely chain prompts; it operates on a continuous cognitive loop where past successes and failures directly inform future actions."*

---

## Overview

Thoth is an autonomous agent platform built on the **Cognate V2 Architecture** — a disciplined, industrial-grade framework for **Experience-Guided Autonomous Reasoning**. It separates the concerns of Planning, Execution, Memory, and Action into clean abstractions, then wires them into a self-correcting cognitive loop.

The system has evolved into a **Scientific Research Console**, designed to provide empirical proof of autonomous learning, strategy adoption, and iterative problem-solving for advanced thesis validation.

---

## Key Capabilities

### ⚙️ The Cognitive Spine (Cognate V2)
The heart of Thoth is a resilient, observable, and thread-safe state machine that orchestrates multi-mode execution.

- **Scientific Reasoning Engine**: A hypothesis-driven iterative loop that traverses four stages: *Hypothesis Generation → Constraint Extraction → Feasibility Evaluation → Final Selection*.
- **Convergence Logic**: Automatical detection of reasoning stability ($\Delta < 0.05$ confidence) to prevent infinite loops and ensure high-integrity outcomes.
- **Dynamic Plan Revision**: Plans are automatically repaired mid-execution after step failures using process history.
- **Crash Recovery**: Full execution state is persisted to SQLite in real-time, allowing goals to be resumed via `resume_from_plan()`.

### 🧠 GRAG — Goal-Relative Adaptive Graph Retrieval
The cognitive retrieval engine that treats knowledge access as **navigation toward a goal**.

- **Directional Retrieval**: Scores documents by alignment with the vector `D = G − C` (Goal minus Current State).
- **Adaptive Graph Learning**: Dynamically reinforces knowledge relationships based on execution success using a logistic learning rule ($lr=0.2$).
- **Multi-Index Routing**: Automatically routes across `PLAN_AWARE`, `GOAL_ONLY`, and `CONVERSATIONAL` modes.

### 💾 Autonomous Learning & Strategy Engine
Thoth doesn't just remember; it learns how to perform better over time.

- **Strategy Engine 2.0**: Rigorously extracts semantic patterns from trajectories. High-success patterns are promoted to first-class **Strategies** once they hit the **80% success / 3-run threshold**.
- **Experience-Guided Planning**: Learned strategies and relevant past trajectories are explicitly injected into the LLM planning prompt to improve decomposition accuracy.
- **Episodic Memory (SQLite)**: Persists the full "Plan vs. Reality" trajectory of every goal ever attempted.

### 🛠️ Tool System & Security
- **9 Tools** across file analysis, web scraping, Gmail, and code analysis.
- **Mandatory Confirmation**: `requires_confirmation()` enforced across the `ITool` interface for all risky operations.
- **ConstraintChecker**: Global security policy enforcement integrated into both chat and autonomous execution paths.

---

## Research Console UI
Thoth features a stable, **3-Column Layout** with a "locked" collapsible sidebar architecture:
- **Left Sidebar**: Knowledge Base management and collapsible "Past Chats" history.
- **Center Workspace**: Unified Chat and Plan Execution visualizer.
- **Right Sidebar**: Observability stack including **GRAG Diagnostics**, **Strategy Hints**, and the **Cognitive Loop Graph**.
- **System State Tray**: Hierarchical **Trajectory Viewer** showing "Initial Plan vs. Reality" and live reasoning logs.

---

## Observability & Thesis Validation

### Cognitive Loop Graph
A real-time directed graph visualization that highlights which part of the architecture is currently active (e.g., glowing green when the Scientific Reasoning Engine is iterating).

### `run_cognate_benchmark`
A dedicated research tool for performing comparative analysis (Standard vs. Scientific mode) and measuring the "Learning Curve" of strategy adoption.

> **Benchmark caveat:** Cognate benchmark runs in `benchmark_results.md` report **0.00\*** task success rates in the mock environment. Metrics such as reasoning depth, SCR, and strategy promotion remain meaningful; end-to-end task completion is **not** implied by those runs.

### Explainable Retrieval (`ScoreBreakdown`)
Every retrieved chunk carries a `ScoreBreakdown` providing "Why this document?" transparency across Query, Goal, Trajectory, Keyword, and Graph signals.

---

## Roadmap

| Status | Item |
|---|---|
| ✅ Complete | Cognate V2: Scientific Reasoning & Iterative Loops |
| ✅ Complete | Strategy Engine 2.0: Autonomous Promotion & Injection |
| ✅ Complete | Cognitive Loop Graph & "Plan vs. Reality" Visualization |
| ✅ Complete | Stable UI Sidebar Architecture & AddCollapsiblePane helper |
| ✅ Complete | Adaptive Graph Learning & Multi-Index Routing |
| 🔶 Partial | Phase 3: Memory Stability (hot-tier prune wired; summarize/age restore open) |
| 🔮 Future expansion | Phase 5: Self-Building (optional — owner may try later; `apply_diff` stub today) |
| 📋 Planned | Subgoal Trees (Hierarchical Decomposition) |

---

## Known gaps

Authoritative detail: [`GRAG.md`](GRAG.md) §1 audit box, [`audit.md`](audit.md), [`cursor_list.md`](cursor_list.md).

| Area | Status |
|------|--------|
| Hierarchical subgoals | Not implemented |
| Trajectory weight $w_t$ | Active locally (0.2); mixed lift on some benchmark buckets |
| `code_modify` / `apply_diff` | Stub — self-building is optional future expansion, not scheduled |
| NODE harness | Spec only (`NODE.md`) — not built in repo |
| Trace-log resume | Observability only; SQLite is authoritative for crash resume |

---

## Workspace Structure

```
agent_workspace/
├── rag/                   # Sandboxed RAG corpus
├── memory.db              # SQLite episodic/semantic/strategy memory
├── retrieval_config.json  # Locked GRAG weight configuration
logs/
├── decision_trace.jsonl   # Behavioral logs (includes Strategy Injection events)
└── grag_benchmark.jsonl   # Retrieval math performance logs
```

---

*Thoth is built like industrial software — observable, resumable, disciplined, and testable.*
