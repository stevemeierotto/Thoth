# Completed Improvements Log

Last updated: 2026-03-10
Source: previous `docs/improvements.md` and `docs/next_steps.md` plan entries marked completed

## Summary

- **Tool Confirmation System**: Implemented `requires_confirmation()` across the `ITool` interface and all 9 tools, enforcing user approval for risky operations.
- **Global Security Enforcement**: Integrated `ConstraintChecker` into the main chat loop (`CommandProcessor`), ensuring security policies apply to both standard interaction and goal execution.
- Completed project rebranding (Thoth), SQLite episodic memory migration, and full Tool System integration.
- Implemented Dynamic Planning (Phase 9) with LLM-generated, variable-length plans and robust JSON parsing.
- Verified Resume Completeness (Phase 10), ensuring the system can fully reconstruct and resume plans from trace logs.
- Implemented Dynamic Plan Revision (Phase 11), allowing the LLM to automatically repair plans after step failures.
- Implemented Extended Agent & Cognitive Spine (Phase 12), re-enabling all tools and verifying the full goal-execution lifecycle with an end-to-end integration test.
- Implemented Self-Building Capability (Phase 3 of improvements.md), giving the agent tools to analyze, test, and modify its own codebase.
- **Semantic Transition & Retrieval Hardening (embedding_fix.md Phase 1-4)**:
    - **Dense Semantic Embeddings**: Integrated Ollama REST API (`/api/embed`) with `nomic-embed-text` support.
    - **Metric Hardening**: Implemented nDCG@5 and Directional Rank Lift to detect subtle ranking improvements.
    - **Hybrid Reranking**: Developed a dynamic blend of semantic and keyword-based TF-IDF scoring.
    - **Engineering Safeguards**: Added model versioning, automated re-indexing on metadata mismatch, and robust batching.
    - **Weight Optimization**: Conducted an automated weight sweep to identify optimal hybrid parameters ($w_q=0.4, w_d=0.4, w_k=0.3$).
- **Retrieval Stabilization & Observability (embedding_fix.md Phase 5-7)**:
    - **Config Locking**: Moved optimized weights ($w_q=0.4, w_d=0.4, w_k=0.3$) into `agent_workspace/retrieval_config.json`.
    - **Auto-Archiving**: Updated `BenchmarkReporter` to automatically record all benchmark runs in `docs/benchmark_results.md`.
    - **Glass Box Retrieval**: Refactored `GragScorer` to provide a detailed `ScoreBreakdown` (Query, Goal, Trajectory, Keyword, Graph) for every retrieved chunk.
    - **Reranking Optimization**: Expanded the initial retrieval pool to 40 candidates to give GRAG more semantic headroom before narrowing to the final top 5.
- Implemented Advanced Tool Orchestration (Phase 5 of improvements.md), including parallel step execution (Tool Batching), a production-grade `web_scrape` tool, and expanded Gmail integration (`gmail_read_messages`).
- Implemented Reasoning & Self-Correction (Phase 6 of improvements.md), featuring a `self_correct` reasoning tool, a global `ConstraintChecker` for security policy, and an autonomous `Reflection Loop` for goal recovery.
- **UI Enhancement**: Added individual "X" delete buttons to the RAG file slots, allowing for granular context management.
- Tool System now supports runtime dispatch, automatic prompt injection, and structured trace logging.
- Implemented and stabilized ExecutiveController with thread-safety and GRAG wiring.
- **GRAG Cognitive Retrieval (Phases 1–8)**:
    - **Foundation (Phases 1–2)**: Implemented `GragScorer`, adaptive $\alpha$ blending, and directional $G - C$ embeddings.
    - **Intelligent Routing (Phase 3)**: Added mode-based routing (PLAN_AWARE, GOAL_ONLY, CONVERSATIONAL) with automated fallback.
    - **Structured State (Phase 4)**: Migrated to JSON-structured state embeddings (v2) to prevent semantic drift.
    - **Experience Reuse (Phase 5)**: Implemented Plan History Reuse, automatically injecting past successful approaches into the planner.
    - **Index Lifecycle (Phase 6)**: Added selective re-indexing with file fingerprinting and metadata tracking.
    - **Empirical Evaluation (Phase 7)**: Integrated structured benchmarking (`grag_benchmark.jsonl`) and high-level success metrics.
    - **Relational Context (Phase 8)**: Deployed a prototype Graph Memory layer in SQLite for causal link retrieval.
- **Memory Stability (Phase 4 of roadmap)**:
    - **Fact Store**: Implemented a persistent `FactStore` for structured world knowledge and a corresponding `store_fact` tool.
    - **Vector Abstraction**: Refactored RAG to use `IVectorStore`, enabling future migration to professional vector databases.
- **Advanced Reasoning (Phase 5 of roadmap)**:
    - **Scientific Mode**: Implemented `ScientificExecutionMode` using the Strategy Pattern for iterative hypothesis testing.
    - **Strategy Engine**: Added `StrategyEngine` to process execution trajectories and optimize future plan generation.
- Hardening, security, CI, and RAG index lifecycle improvements completed in previous sessions.

## Completed Work

### 2026-03-10

- **Strict Sandbox Boundaries**:
    - **Enforcement**: Modified `IndexManager` to hard-reject and log any indexing requests outside `/home/steve/Thoth/agent_workspace/`.
    - **Bootstrap Security**: Updated `CommandProcessor::ensureInitialized` to bootstrap RAG exclusively from the `agent_workspace/rag/` sandbox rather than the project root.
    - **Benchmark Hardening**: Rewrote `run_grag_benchmark.cpp` to use a strictly sandboxed corpus and added path-guard logic to prevent accidental directory traversal.
- **Trajectory-Aware Retrieval (Phase 5.5)**:
    - **TrajectoryBuilder**: Implemented a new component that summarizes the last 7 execution steps into a semantic vector ($T$).
    - **Infrastructure Wiring**: Updated `ExecutiveController` to persist episode steps in SQLite and update `RAGPipeline` after every step.
    - **Benchmark Validation**: Verified GRAG performance using a comprehensive 100-case sandboxed benchmark suite.
    - **Empirical Results**: Confirmed significant lift in ambiguous retrieval:
        - **Overall nDCG@5**: +0.022 lift.
        - **Mean RR**: +0.084 lift (0.528 → 0.612).
        - **Goal Disambiguation**: +0.101 nDCG lift, proving the effectiveness of the $G - C$ directional vector in selecting the correct context when queries are ambiguous.

### 2026-03-09

- **Reranking Optimization (Phase 7)**:
    - **Candidate Expansion**: Modified `RAGPipeline` to fetch 40 candidates in the initial recall phase, allowing more semantic signal to be processed during rescoring.
    - **Verification**: Verified the multi-stage pipeline via the benchmark runner.
- **Observability & Explainable AI (Phase 6)**:
    - **Explicit Score Data Structure**: Introduced `ScoreBreakdown` to capture all retrieval signals independently.
    - **Transparency Hardening**: Modified the rescoring loop to preserve and sort individual signal scores, enabling "Why this document?" explainability.
- **Retrieval Stabilization & Observability (Phase 5)**:
    - **Empirical Weight Locking**: Optimized weights from Phase 4 are now stored in a persistent JSON config and synchronized between the global `Config` and `RAGPipeline`.
    - **Automated Performance Tracking**: Every benchmark run now appends a human-readable Markdown summary to `docs/benchmark_results.md`, including Precision, MRR, and nDCG metrics.

### 2026-03-05

- **GRAG Phase 3–8 Implementation**:
    - **Multi-Index Routing (Phase 3)**: Implemented `GragRoutingMode` logic to automatically switch between PLAN_AWARE, GOAL_ONLY, and CONVERSATIONAL retrieval based on agent state.
    - **Structured State (Phase 4)**: Enforced a strict JSON schema for state embeddings to maintain semantic focus. Completed SQLite migration to Version 2.
    - **Plan History Reuse (Phase 5)**: Wired `MemoryRepository` to retrieve past successful plans by directional similarity, reducing planning latency for recurring tasks.
    - **Index Lifecycle (Phase 6)**: Implemented `shouldReindexFile` fingerprinting to skip re-indexing unmodified source files during startup.
    - **Metrics Harness (Phase 7)**: Enabled per-retrieval diagnostic logging and established the initial performance baseline.
    - **Graph Memory Prototype (Phase 8)**: Built the SQLite `graph_nodes` and `graph_edges` schema with hybrid scoring integration in `GragScorer`.
- **Advanced Core Architecture (Roadmap Phase 4-5)**:
    - **Memory Stability (Phase 4)**: Deployed the `FactStore` and `IVectorStore` abstractions, decoupling RAG from the file-based prototype index.
    - **Scientific Reasoning (Phase 5)**: Implemented `ScientificExecutionMode` and `ProblemState` for iterative hypothesis evaluation. Verified convergence halting in unit tests.

### 2026-03-08
...