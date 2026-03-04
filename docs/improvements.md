# Thoth Improvements (Active)

Last reset: 2026-03-01

Finished work has been archived in `docs/completed_improvements_log.md`.

## Current Focus

- Add new improvements below as we decide what to build next.

## Active Improvement Queue

1. [M1 - Core Integration] Integrate `ToolRegistry::instance()` into `command_processor.cpp` for runtime tool execution.
2. [M1 - Core Integration] Expose `getAvailableTools()` in prompt-building flow so tool metadata can be injected into model prompts.
3. [M2 - Observability] Add tool invocation trace logging (`tool_name`, `input_schema`, `result_status`, latency) to decision/ops logs.
4. [M3 - Validation] Add registry integration tests (tool lookup, malformed JSON input, missing tool error, malformed tool output).

## Notes

- Keep this file limited to active and upcoming items only.
- Move completed items into `docs/completed_improvements_log.md`.

## AI Agent Roadmap Addendum (2026-03-01)

### Scope Context

- Current architecture uses a hybrid memory model:
	- Episodic memory in JSON stores: \`memory.json\`, \`chat_sessions.json\`, \`decision_trace.jsonl\`
	- Semantic memory via summaries + RAG retrieval
	- Working memory assembled per request
	- Vector index in \`rag_index.bin\` with pluggable similarity metrics
	- **Cognitive Retrieval**: See \`GRAG.md\` for full architectural specification. All retrieval-related enhancements must align with GRAG definitions.
- Current runtime constraints:
	- Small Qwen model selected due to current hardware limits
	- Hardware upgrade planned later

---

### 1) Migrate Episodic Memory from JSON to SQLite

**Problem statement**
- `memory.json` and `chat_sessions.json` will degrade in read/write performance and reliability as conversation/session volume increases.
- JSON file mutation is fragile under concurrent writes and makes partial updates expensive.

**Recommended solution**
- Migrate episodic memory persistence to SQLite while preserving current data model semantics.
- Keep `decision_trace.jsonl` as append-only operational trace (or migrate later if needed), while moving mutable conversation/session records into normalized SQLite tables.

**Priority level**
- High

**Implementation notes**
- Add schema (example): `sessions`, `messages`, `summaries`, `memory_meta`.
- Implement repository/DAO abstraction so storage backend is swappable.
- Create one-time migration tool from existing JSON files to SQLite.
- Add write transactions and indexes (`session_id`, `created_at_ms`, `updated_at_ms`).
- Add compatibility mode fallback for legacy JSON read during migration window.

---

### 2) Plan Vector Store Scalability Path

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

### 3) Implement Memory Pruning and Archiving

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

### 4) Design Tool and Workflow Declaration System

**Problem statement**
- The agent lacks a formal, extensible mechanism to define, register, select, and orchestrate tools/workflows.
- Without this foundation, advanced capabilities (multi-step operations, robust retries, stateful chains) remain brittle.

**Recommended solution**
- Build a first-class tool/workflow declaration framework with:
	- declarative tool schemas
	- central registry
	- planner/selector logic
	- workflow engine with retries, compensation, and state passing

**Priority level**
- High

**Implementation notes**
- Define tool manifest fields: `name`, `description`, `inputs`, `outputs`, `capabilities`, `timeout`, `idempotency`, `retry_policy`.
- Add runtime registry with validation and policy gating before execution.
- Implement workflow state machine (steps, guards, transitions, failure handlers).
- Add checkpointing for chained operations so partial progress is recoverable.
- Record per-step execution telemetry in decision trace for debugging/audit.
- Add deterministic test harness for tool selection and mid-workflow failure scenarios.
- Deferred integration tasks after initial registry refactor:
	- Wire `ToolRegistry` into `command_processor.cpp` dispatch path.
	- Surface `getAvailableTools()` output to `prompt_factory.cpp` for tool-aware prompting.
	- Add strict JSON contract checks at registry boundaries (invalid input/tool output).
	- Add compatibility layer for future API-backed and local tools without changing registry core.

---

### 5) Plan Model Upgrade Path (Post-Hardware Refresh)

**Problem statement**
- Current small Qwen model is a constraint-driven choice and may limit complex tool-calling reliability and reasoning depth.
- Ad hoc model swaps later can break prompts, tool integration, and latency expectations.

**Recommended solution**
- Document and implement a model abstraction + migration playbook so larger-model adoption is low-risk when hardware is upgraded.

**Priority level**
- Low (until hardware changes)

**Implementation notes**
- Create model provider interface with capability flags (context size, tool-calling format, structured output reliability).
- Externalize prompts/templates by model profile.
- Define acceptance suite: reasoning quality, tool-call correctness, latency/cost envelope, regression checks.
- Add staged rollout plan: canary sessions -> partial rollout -> full cutover.
- Keep rollback path and per-model configuration presets.

---

## Suggested Execution Order

1. Tool/workflow declaration system (foundational)
2. SQLite episodic memory migration
3. Memory pruning + archiving
4. Vector store scalability evaluation and trigger thresholds
5. Model upgrade cutover (after hardware upgrade)

## Toolchain Roadmap

### Tool Interface Standard (Applies to All Tools)

- Canonical flow: Tool Name -> Input Schema -> Execution -> Structured Output -> Back to Agent.
- Tool contract must be API-agnostic so the agent runtime does not depend on whether execution is local or external.
- All tools should register through the same declaration interface once the tool/workflow system is implemented.
- MCP compatibility should be a design requirement for future plug-and-play interoperability.

---

### 1) Web Scraping Tool

**Tool name**
- Web Scraping Tool

**Implementation approach (custom vs existing API/MCP)**
- Custom implementation owned internally for targeted domain scraping.
- Optionally complemented by a general MCP web search tool for broad discovery tasks.

**Recommended libraries or services**
- `BeautifulSoup` for static HTML extraction.
- `Playwright` for dynamic rendering and scripted interactions.

**Key implementation notes**
- Support both static pages and JavaScript-rendered content in one tool contract.
- Normalize output into chunk-ready documents with URL/source metadata for indexing.
- Feed tool output directly into the RAG ingestion pipeline so scraped knowledge is searchable.
- Add guardrails for robots.txt policy, rate limits, and domain allowlists.
- Store fetch status and parse diagnostics in structured output for traceability.

**Dependencies on other improvements**
- Depends on the tool/workflow declaration system for registration, invocation, and retries.
- Benefits from memory pruning/index lifecycle work to prevent unbounded knowledge growth.

**Priority**
- High

---

### 2) Coding Agent Tool

**Tool name**
- Coding Agent Tool

**Implementation approach (custom vs existing API/MCP)**
- Custom implementation, extending existing file manipulation capabilities.
- Use tools like Aider and Continue as reference patterns for coding loop design.

**Recommended libraries or services**
- Local process/sandbox runtime for controlled code execution.
- Unified diff/patch engine for file change awareness and safe apply flows.

**Key implementation notes**
- Implement code execution sandbox boundaries (filesystem scope, command policy, timeout).
- Add model-facing error feedback loop: compile/test/runtime errors returned as structured observations.
- Add file diff awareness so the agent reasons over changed hunks instead of full file snapshots.
- Include iterative loop controls (attempt limits, rollback hooks, stop conditions).
- Emit structured outputs for each cycle: action, result, diagnostics, and next-step hints.

**Dependencies on other improvements**
- Hard dependency on tool/workflow declaration system for consistent registration and orchestration.
- Depends on guardrail/policy framework for safe command execution.
- Benefits from model upgrade path for stronger multi-step reasoning when hardware improves.

**Priority**
- High

---

### 3) Trading Tool

**Tool name**
- Trading Tool

**Implementation approach (custom vs existing API/MCP)**
- Custom wrapper around existing exchange APIs.
- Exchange APIs handle market connectivity; wrapper provides agent-facing interface and safety enforcement.

**Recommended libraries or services**
- Alpaca API (stocks).
- Coinbase API or Binance API (crypto).

**Key implementation notes**
- Must support paper trading mode as default for development/testing.
- Require explicit confirmation steps before any live order execution.
- Enforce position limits and risk caps at tool layer (not only in agent prompts/policies).
- Add idempotency keys, order-state reconciliation, and failure/retry handling.
- Log every action with structured audit fields (intent, confirmation, order params, result).

**Dependencies on other improvements**
- Must be deferred until tool/workflow declaration system is complete.
- Must be deferred until guardrail architecture and policy enforcement are complete.
- Should leverage decision trace and logging foundations for compliance and auditability.

**Priority**
- Low (deferred until guardrails and tool system are complete)

---

### 4) Gmail Integration Tool

**Tool name**
- Gmail Integration Tool

**Implementation approach (custom vs existing API/MCP)**
- Evaluate existing Gmail MCP server first.
- If MCP coverage is insufficient, implement custom wrapper using official Gmail REST API with OAuth2.

**Recommended libraries or services**
- Official Gmail API + OAuth2 flow.
- Gmail MCP server (first-choice evaluation path).

**Key implementation notes**
- Prioritize least-privilege scopes and explicit consent flow.
- Normalize message/list/send actions into shared structured output schema.
- Add token lifecycle handling (refresh, revocation, secure storage boundaries).
- Include retry/backoff behavior for quota and transient API failures.
- Keep implementation contract identical whether provider is MCP-backed or direct API-backed.

**Dependencies on other improvements**
- Depends on tool/workflow declaration system for pluggable registration.
- Depends on security policy and secret handling to protect OAuth credentials and message data.

**Priority**
- Medium

---

### Toolchain Sequencing Notes

1. Finish tool/workflow declaration system first (foundational interface and orchestration).
2. Implement Web Scraping and Coding Agent tools against the unified contract.
3. Evaluate Gmail MCP coverage, then select MCP-backed or custom Gmail wrapper.
4. Defer Trading Tool activation until guardrails/policy architecture is production-ready.
5. Preserve API-agnostic adapters so model/hardware upgrades do not require tool contract rewrites.
