# Thoth Improvements (Active)

> **Superseded by [`improvements.md`](improvements.md)** — historical roadmap snapshot from 2026-03-04.
> Finished work is recorded in [`completed_improvements_log.md`](completed_improvements_log.md).

Last reset: 2026-03-04

Finished work has been archived in `docs/completed_improvements_log.md`.

## Current Focus

- Vector Store Scalability.
- Workflow & Tool Orchestration.

## Active Improvement Queue

1. [M3 - Validation] Add more comprehensive tool integration tests (e.g., malformed tool result handling).
2. [M1 - Core] Implement asynchronous tool execution for long-running operations.

## Notes

- Keep this file limited to active and upcoming items only.
- Move completed items into `docs/completed_improvements_log.md`.

## AI Agent Roadmap Addendum (2026-03-04)

### Scope Context

- Current architecture uses a hybrid memory model:
	- Episodic memory in SQLite (sessions, messages, summaries)
	- Semantic memory via summaries + RAG retrieval
	- Working memory assembled per request
	- Vector index in `rag_index.bin` with pluggable similarity metrics
	- **Cognitive Retrieval**: See `GRAG.md` for full architectural specification. All retrieval-related enhancements must align with GRAG definitions.
- Current runtime constraints:
	- Small Qwen model selected due to current hardware limits
	- Hardware upgrade planned later

---

### 1) Plan Vector Store Scalability Path

**Problem statement**
- Flat `rag_index.bin` is simple and fast at small scale but will degrade as chunk counts approach or exceed ~100k.
- Rebuild, search latency, and maintenance overhead increase with dataset size.

**Recommended solution**
- Define a staged migration plan to a scalable vector backend (evaluate LanceDB and ChromaDB first).
- Keep current interface stable so backend can be switched with minimal changes.

**Priority level**
- Medium

**Implementation notes**
- Introduce `VectorStore` backend interface contract tests.
- Add benchmark gates: ingest throughput, p50/p95 retrieval latency, memory usage, persistence behavior.
- Run side-by-side evaluation on representative corpora before cutover.
- Support dual-write or export/import tooling to reduce migration risk.

---

### 2) Implement Memory Pruning and Archiving

**Problem statement**
- Retaining all episodic turns indefinitely increases storage growth, retrieval noise, and prompt assembly overhead.
- Long-running sessions need lifecycle controls to preserve relevance.

**Recommended solution**
- Add policy-driven pruning and archival pipeline that summarizes older turns and removes low-value raw history from hot storage.
- Keep searchable archives and summary continuity to preserve long-term context.

**Priority level**
- High

**Implementation notes**
- Define retention tiers: hot (recent raw turns), warm (compressed summaries), cold (archived raw logs).
- Add scheduled compaction job per session or size threshold.
- Preserve key facts/tasks/decisions before pruning raw turns.
- Add audit metadata (`archived_at`, `summary_version`, `source_range`).
- Add restore path for on-demand historical replay.

---

### 3) Design Workflow & Tool Orchestration

**Problem statement**
- While `ToolRegistry` (v1.0) exists, the agent lacks advanced orchestration (multi-step operations, robust retries, stateful chains).

**Recommended solution**
- Build a workflow engine on top of the Tool System with:
	- workflow state machine (steps, guards, transitions, failure handlers).
	- checkpointing for chained operations.
	- deterministic test harness for multi-step failure scenarios.

**Priority level**
- High

---

### 4) Plan Model Upgrade Path (Post-Hardware Refresh)

**Problem statement**
- Current small Qwen model is a constraint-driven choice and may limit complex tool-calling reliability and reasoning depth.

**Recommended solution**
- Document and implement a model abstraction + migration playbook so larger-model adoption is low-risk when hardware is upgraded.

**Priority level**
- Low (until hardware changes)

---

## Suggested Execution Order

1. Tool Registry integration into query flow
2. Tool-aware prompting (prompt_factory)
3. Memory pruning + archiving
4. Vector store scalability evaluation
5. Model upgrade cutover (after hardware upgrade)

## Toolchain Roadmap

### 1) Web Scraping Tool

- **Priority**: High
- **Notes**: Support both static and JS-rendered content; feed output into RAG ingestion.

### 2) Coding Agent Tool

- **Priority**: High
- **Notes**: File diff awareness, iterative loop controls, and sandbox boundaries.

### 3) Trading Tool

- **Priority**: Low
- **Notes**: Position limits, risk caps, and paper trading mode.

### 4) Gmail Integration Tool (Expanded)

- **Priority**: Medium
- **Status**: `gmail_read_labels` implemented.
- **Next**: Expand to `gmail_read_messages`, `gmail_send_message`, and `gmail_search`.
