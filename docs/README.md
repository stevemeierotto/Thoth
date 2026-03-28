# Thoth: An Experience-Aware Cognitive Agent System

> *"It does not merely chain prompts; it operates on a continuous cognitive loop where past successes and failures directly inform future actions."*

---

## Overview

Thoth is an autonomous agent platform built on the **Cognate Architecture** — a disciplined, industrial-grade framework for **Experience-Guided Autonomous Reasoning**. It separates the concerns of Planning, Execution, Memory, and Action into clean abstractions, then wires them together into a self-correcting, self-building cognitive loop.

The system is housed in a professional **3-Column Research Console** and operates as a fully functional **Autonomous Cognitive Orchestrator** with empirically validated retrieval, crash-resilient execution, and thread-safe concurrency.

---

## Key Capabilities

### 🧠 GRAG — Goal-Relative Adaptive Graph Retrieval
The cognitive retrieval engine at the heart of Thoth. Unlike standard RAG, GRAG treats retrieval as **navigation through knowledge space toward a goal**.

- **Directional Retrieval:** Scores documents by their alignment with the vector `D = G − C` (Goal minus Current State).
- **Adaptive Blending:** Dynamically adjusts the balance between semantic similarity and goal-directed retrieval.
- **Adaptive Graph Learning:** Dynamically reinforces knowledge relationships based on execution success using a logistic learning rule ($lr=0.2$).
- **Multi-Index Routing:** Automatically routes across `PLAN_AWARE`, `GOAL_ONLY`, and `CONVERSATIONAL` modes.

### ⚙️ ExecutiveController — The Cognitive Spine
A resilient, observable, and **thread-safe** state machine that orchestrates plan execution.

- **Full Lifecycle Management:** `PLANNING → EXECUTING_STEP → OBSERVING_RESULT → REVISING_PLAN → COMPLETED`.
- **Thread-Safety Hardening:** Protected via `std::mutex` and `std::recursive_mutex` (for LLM calls) across all shared state to ensure UI and background agent stability.
- **Dynamic Plan Revision:** Plans are automatically repaired mid-execution after step failures using process history.
- **Crash Recovery:** Full execution state is persisted to SQLite in real-time, allowing goals to be resumed via `resume_from_plan()`.

### 💾 Hybrid Memory & Learning
- **Episodic Memory (SQLite):** Persists the full execution trajectory of every goal.
- **Trajectory Viewer:** A hierarchical UI component that displays real-time and historical execution steps directly from the database.
- **Strategy Engine:** Analyzes execution trajectories to surface repeating successful patterns and promotes them to reusable **Strategies**.
- **FactStore:** A persistent store for structured world knowledge.

### 🛠️ Tool System & Security
- **9 Tools** across file analysis, web scraping, Gmail, and code analysis.
- **Mandatory Confirmation:** `requires_confirmation()` enforced across the `ITool` interface for all risky operations.
- **ConstraintChecker:** Global security policy enforcement integrated into both chat and autonomous execution paths.

---

## The Research Console UI
Thoth features a modernized **3-Column Layout** built with `wxAuiManager`:
- **Left Pane:** Session management and Knowledge Base (RAG) control.
- **Center Pane:** Unified Chat and Plan Execution visualizer.
- **Right Pane:** Real-time GRAG Diagnostics and Strategy hints.
- **Bottom Tray:** Tabbed access to Trajectories, Knowledge Graph stats, and live Decision Logs.

---

## Observability

### `decision_trace.jsonl`
High-level logical flow and reasoning transparency. Managed by `DecisionTraceLogger` with schema versioning and thread-safe appending.

### `grag_benchmark.jsonl`
Low-level mathematical performance monitoring: alpha weights, direction magnitudes, and graph activations.

### Explainable Retrieval (`ScoreBreakdown`)
Every retrieved chunk carries a `ScoreBreakdown` providing "Why this document?" transparency across Query, Goal, Trajectory, Keyword, and Graph signals.

---

## Roadmap

| Status | Item |
|---|---|
| ✅ Complete | Cognate Phases 9–12 (Dynamic Planning, Tool Re-enablement) |
| ✅ Complete | Cognitive Spine Thread-Safety & Concurrency Hardening |
| ✅ Complete | Adaptive Graph Learning (Phase 5.6) |
| ✅ Complete | 3-Column Research Console UI & Trajectory Viewer |
| ✅ Complete | Crash Recovery & SQLite Persistence |
| 📋 Planned | Phase 3: Memory Stability (Pruning, Archiving) |
| 📋 Planned | Phase 4: Advanced Reasoning (Scientific Mode, Subgoal Trees) |
| 📋 Planned | Phase 5: Self-Building Capability (Code Modification) |

---

## Workspace Structure

```
agent_workspace/
├── rag/                   # Sandboxed RAG corpus
├── memory.db              # SQLite episodic/semantic memory
├── retrieval_config.json  # Locked GRAG weight configuration
logs/
├── decision_trace.jsonl   # Behavioral & state transition logs
└── grag_benchmark.jsonl   # Retrieval math performance logs
```

---

*Thoth is built like industrial software — observable, resumable, disciplined, and testable.*
