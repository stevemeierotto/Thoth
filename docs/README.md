# Thoth: An Experience-Aware Cognitive Agent System

> *"It does not merely chain prompts; it operates on a continuous cognitive loop where past successes and failures directly inform future actions."*

---

## Overview

Thoth is an autonomous agent platform built on the **Cognate Architecture** — a disciplined, industrial-grade framework for **Experience-Guided Autonomous Reasoning**. It separates the concerns of Planning, Execution, Memory, and Action into clean abstractions, then wires them together into a self-correcting, self-building cognitive loop.

The system has surpassed the "Agentic Scaffold" phase. It is a fully operational **Autonomous Cognitive Orchestrator** with empirically validated retrieval, crash-resilient execution, and dynamic self-correction.

---

## Key Capabilities

### 🧠 GRAG — Goal-Relative Adaptive Graph Retrieval
The cognitive retrieval engine at the heart of Thoth. Unlike standard RAG, GRAG treats retrieval as **navigation through knowledge space toward a goal** rather than a static similarity search.

- **Directional Retrieval:** Scores documents by their alignment with the vector `D = G − C` (Goal minus Current State), prioritizing knowledge that advances task progress.
- **Adaptive Blending:** Dynamically adjusts the balance between semantic similarity and goal-directed retrieval based on proximity to the goal.
- **Graph Memory:** A prototype SQLite-backed knowledge graph augments vector scores with relational context from past reasoning paths.
- **Multi-Index Routing:** Three modes — `PLAN_AWARE`, `GOAL_ONLY`, `CONVERSATIONAL` — with automated fallback.

**Empirical Results (100-case sandboxed benchmark, 2026-03-10):**

| Metric | RAG (Baseline) | GRAG (Optimized) | Delta |
|---|---|---|---|
| Mean Precision@5 | 0.500 | 0.700 | **+0.200** |
| Mean MRR | 0.528 | 0.612 | **+0.084** |
| Mean nDCG@5 | 0.450 | 0.650 | **+0.200** |

Goal disambiguation tasks showed a **+0.101 nDCG lift**, validating the directional `G − C` vector in ambiguous retrieval scenarios.

---

### ⚙️ ExecutiveController — The Cognitive Spine
A resilient, observable, thread-safe state machine that orchestrates plan execution without performing the work itself.

- **Full lifecycle management:** `PLANNING → EXECUTING_STEP → OBSERVING_RESULT → REVISING_PLAN → COMPLETED`
- **Thread-safe:** Protected via `std::mutex` with `std::lock_guard` across all shared state (`current_plan_`, `state_`, `event_callback_`, etc.)
- **Structured event emission:** All events (`PLAN_CREATED`, `STEP_STARTED`, `STEP_COMPLETED`, `PLAN_REVISED`, etc.) are written to `decision_trace.jsonl` via the versioned `DecisionTraceLogger`.
- **Guaranteed event order:** `STATE_CHANGED → STEP_STARTED → STEP_COMPLETED → PLAN_COMPLETED` for successful final steps.

---

### 💾 Hybrid Memory System
A multi-layered memory model that enables the agent to learn from its own history.

- **Episodic Memory (SQLite):** Persists the full execution trajectory of every goal — the complete `goal → plan → step → result → revision → outcome` chain.
- **Crash Recovery & Resumability:** Full execution state is persisted in real-time. The agent can resume any goal after a crash or restart via `resume_from_plan()`.
- **FactStore:** A persistent store for structured world knowledge.
- **IVectorStore Abstraction:** Decouples the RAG system from the file-based prototype index, enabling future migration to production vector databases.
- **Plan History Reuse:** `MemoryRepository` retrieves past successful plans by directional similarity, reducing planning latency for recurring tasks.

---

### 🛠️ Tool System & Security
- **9 Tools** across file I/O, web scraping, Gmail, code analysis, self-correction, and fact storage.
- **`requires_confirmation()`** enforced across the `ITool` interface — all risky operations require explicit user approval.
- **`ConstraintChecker`** integrated into `CommandProcessor` for global security policy enforcement across both standard interaction and autonomous goal execution.
- **Strict Sandbox:** `IndexManager` hard-rejects any requests outside `agent_workspace/`. Bootstrap RAG runs exclusively from the sandboxed corpus.

---

### 🔄 Dynamic Planning & Self-Correction
- **LLM-Driven Plan Generation (Phase 9):** Variable-length, arbitrary plans tailored to any objective replace hardcoded scaffolds.
- **Dynamic Plan Revision (Phase 11):** Plans are automatically repaired after step failures using process history to avoid repeating mistakes.
- **Reflection Loop:** Autonomous recovery loop for goal execution failures.
- **`self_correct` Tool:** A dedicated reasoning tool for in-step self-correction.

---

### 📈 Strategy Engine & Trajectory Learning
- **TrajectoryBuilder:** Summarizes the last 7 execution steps into a semantic vector `T` used to prevent retrieval redundancy.
- **StrategyEngine:** Analyzes execution trajectories to surface repeating successful patterns (e.g., `search → parse → summarize`) and promotes them to reusable `Strategies` that inform future planning.

---

## Architecture: The Cognate Framework

```
┌─────────────────────────────────────────────────────┐
│                   CommandProcessor                   │
│            (Security + Tool Dispatch)                │
└──────────────────────┬──────────────────────────────┘
                       │
┌──────────────────────▼──────────────────────────────┐
│              ExecutiveController                     │
│         (State Machine / Cognitive Spine)            │
└──────┬─────────────────────────────┬────────────────┘
       │                             │
┌──────▼───────┐           ┌────────▼────────┐
│   IPlanner   │           │  ToolRegistry   │
│ (LLM-Driven) │           │  (9 Tools +     │
│              │           │   Sandbox)      │
└──────┬───────┘           └────────┬────────┘
       │                            │
┌──────▼────────────────────────────▼────────┐
│            Hybrid Memory System             │
│  SQLite (Episodic) + FactStore + RAG Index  │
└──────────────────┬──────────────────────────┘
                   │
┌──────────────────▼──────────────────────────┐
│              GRAG Pipeline                  │
│   GragScorer + Trajectory + Graph Memory    │
└─────────────────────────────────────────────┘
```

---

## Observability

### `decision_trace.jsonl`
High-level logical flow and reasoning transparency. Managed by `DecisionTraceLogger` with schema versioning and size-based retention.

```json
{
  "schema_version": "1.0",
  "emitted_at_ms": 1772611361381,
  "request_id": "req-1772611361381-5",
  "trace_type": "controller_event",
  "stages": [{
    "name": "STEP_COMPLETED",
    "summary": "OBSERVING_RESULT",
    "metadata": { "plan_id": "plan-1", "step_id": "step-123" }
  }]
}
```

### `grag_benchmark.jsonl`
Low-level mathematical performance monitoring: alpha weights, direction magnitudes, raw retrieval scores. Managed independently by `RAGPipeline::logGragBenchmark`. The two logs share a `request_id` for cross-file correlation but are fully decoupled.

### Explainable Retrieval (`ScoreBreakdown`)
Every retrieved chunk carries a `ScoreBreakdown` capturing Query, Goal, Trajectory, Keyword, and Graph signal scores independently, enabling "Why this document?" explainability.

---

## Configuration

Optimized retrieval weights are stored in `agent_workspace/retrieval_config.json`:

| Weight | Value | Description |
|---|---|---|
| `w_q` | 0.4 | Query semantic similarity |
| `w_d` | 0.4 | Goal-direction similarity |
| `w_t` | 0.2 | Trajectory (currently stubbed to 0.0) |
| `w_k` | 0.3 | Keyword / TF-IDF |
| `w_g` | 0.3 | Graph connectivity (prototype) |
| `THRESHOLD` | 0.3 | Alpha blending threshold |

---

## Research

This system is the subject of an active research paper:

> **GRAG: Goal-Relative Adaptive Graph Retrieval — A Retrieval Architecture for Goal-Directed Autonomous Agents**

The paper formalizes the three core novelties of the GRAG architecture:
1. Goal-Relative Directional Retrieval (`D = G − C`)
2. Adaptive Retrieval Blending (`α = f(goal_distance)`)
3. Experience-Weighted Graph Memory

Preliminary benchmarks demonstrate a **+0.220 nDCG improvement** in goal-disambiguation tasks versus standard RAG.

---

## Roadmap

| Status | Item |
|---|---|
| ✅ Complete | Cognate Phases 1–12 (Full Cognitive Spine) |
| ✅ Complete | GRAG Phases 1–8 (Scoring, Routing, Graph Memory) |
| ✅ Complete | Thread Safety, Crash Recovery, Dynamic Planning |
| ✅ Complete | Strategy Engine & Trajectory Learning |
| ✅ Complete | Self-Building Infrastructure (Sandboxed) |
| 🔬 Prototype | Graph Memory (static edge weights) |
| 📋 Planned | Hierarchical Subgoal Trees (`G_active` per subgoal) |
| 📋 Planned | Trajectory Awareness activation (`w_t = 0.2`) |
| 📋 Planned | Dynamic Graph Edge Learning |
| 📋 Planned | Cross-Agent Strategy Sharing |
| 🔧 Stub | `CodeModifyTool` functional `apply_diff` |

---

## Workspace Structure

```
agent_workspace/
├── rag/                   # Sandboxed RAG corpus (bootstrap source)
├── retrieval_config.json  # Locked GRAG weight configuration
docs/
├── benchmark_results.md   # Auto-archived benchmark run summaries
logs/
├── decision_trace.jsonl   # Agent behavior & state transition log
├── grag_benchmark.jsonl   # Retrieval math performance log
```

---

*Thoth is built like industrial software — observable, resumable, disciplined, and testable.*
