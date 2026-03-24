# Completed Improvements Log

Last updated: 2026-03-22
Source: previous `docs/improvements.md` and `docs/next_steps.md` plan entries marked completed
### 2026-03-22 (Research Console & Stability Hardening)

- **Cognitive Spine Thread-Safety (ExecutiveController)**:
    - **Goal**: Resolve the "Thread-Safety Vacuum" causing UI crashes and state corruption due to concurrent access between UI (main) and Agent (background) threads.
    - **Issues Resolved**:
        - Unprotected public methods: `transition_to`, `update_goal_embedding`, `update_trajectory_embedding`, `dispatch_step`, `set_workflow_engine`.
        - Reference leakage: `get_current_plan()` returning `const Plan&`.
        - Inconsistent internal locking leading to race conditions and deadlocks.
        - Data races on GRAG embeddings (Goal, Current, Trajectory).
    - **Implementation Details**:
        - **Mutex Protection**: Applied `std::lock_guard<std::mutex>` across all shared state access in `ExecutiveController`.
        - **Snapshot Inspection**: Switched `get_current_plan()` to return by value (snapshot), ensuring UI thread memory safety.
        - **Deadlock-Free Internal Transitions**: Implemented `_unlocked` method variants for safe internal calls within the state machine's lock.
        - **Execution Reliability**: Fixed `execute_goal` execution loop start logic and joined stale threads to prevent leaks.
    - **Verification**: Successfully recompiled and passed full suite of unit tests including parallel tool batching and reflection cycles.
- **UI Restoration & Observability Hardening**:
    - **Menu Bar Implementation**: Restored the missing `SetupMenuBar` and implemented full handlers for File, Agent, Tools, Benchmarks, View, and Help menus.
    - **Plan Execution Panel**: Implemented a new vertical `PlanExecutionPanel` that exposes the real-time state of the `ExecutiveController`, showing current goal and ordered step statuses.
    - **GRAG Diagnostic Fixes**: Resolved a bug where GRAG diagnostics showed zero values by ensuring the `Config` constructor properly initializes retrieval weights ($w_q, w_d, w_t, w_k, w_g$) and the `grag_directional` toggle.
    - **Executive Control Integration**: Exposed `pause`, `resume`, `abort`, and `executeGoal` methods in `AgentInterface`, allowing direct control of the agent's cognitive loop from the UI.
    - **Contextual Explanation Buttons**: Replaced the generic `?` button with explicit `Explain Retrieval` and `Explain Plan` actions, and added a `Revise` button to the goal banner for dynamic goal steering.
- **RAG Indexer Stability & Parallelization**:
    - **Goal**: Decouple the indexing lifecycle from the cognitive loop to prevent system-wide deadlocks and multi-minute blocking during large file batch processing.
    - **Implementation Details**:
        - **Asynchronous Architecture**: Refactored `IndexManager` with a background worker thread and a thread-safe task queue. Added `indexFileAsync` and `indexProjectAsync`.
        - **CURL Handle Pool**: Upgraded `EmbeddingEngine` from a single handle to a thread-safe pool, allowing concurrent embeddings for indexing and real-time chat queries.
        - **Stateless Retrieval**: Hardened `RAGPipeline::retrieveRelevant` to support passing embeddings as arguments, removing reliance on shared internal state and reducing mutex contention.
        - **Granular Locking**: Optimized `IndexManager` to release `chunksMutex` during heavy embedding phases, allowing concurrent reads during long-running write operations.
    - **Verification**: Confirmed fix with successful project-wide compilation and verified background worker lifecycle.
- **Benchmark Integrity & Reliability**:
    - **Mock Data Isolation**: Modified `testBenchmarkReporter` in `tests/unit_tests.cpp` to prevent unit tests from polluting the production `benchmark_results.md` with hardcoded mock data.
    - **Benchmark Sampling**: Added a `--sample` flag to the `run_grag_benchmark` tool, allowing for rapid verification of real retrieval performance on a subset of test cases without environment timeouts.
    - **Log Sanitization**: Cleaned `docs/benchmark_results.md` to remove duplicate mock entries, restoring the document as a reliable source of actual research performance history.
- **Research-Oriented UI Transformation**:
...
    - **3-Column Main Workspace**: Implemented a professional layout with Knowledge Base (Left), Central Chat/Plan (Center), and Diagnostics/Strategy (Right) using `wxAuiManager`.
    - **System State Tray**: Added a tabbed bottom notebook providing dedicated views for RAG context management, Trajectory history, Experiment Lab, Knowledge Graph statistics, and live decision logs.
    - **Structured Tool Rendering**: Upgraded chat bubbles to detect and render JSON-encoded tool results (e.g., diff viewers for `code_modify`, web previews for `web_scrape`).
- **Cognitive Reliability & Persistence**:
    - **Stateful Resumption**: Fixed `checkResumablePlan` to fully restore the agent's internal state (goal, plan, embeddings) upon session activation, ensuring continuity across window reloads.
    - **Session-Aware Event Routing**: Added `session_id` to the `ControllerEvent` system, allowing the UI to accurately save goals and plans to background sessions even if the user switches chats.
    - **Cold-Start GRAG**: Optimized `GragScorer` to activate directional retrieval from Step 1 by treating empty current state as a zero vector, preventing zeroed diagnostics at plan initiation.
- **Stability & Memory Hardening**:
    - **Thread Pool Architecture**: Refactored `AgentInterface` from a leaking thread-per-operation model to a single persistent worker thread with a task queue.
    - **Micro-Batch Indexing**: Implemented batching (size 10) in `IndexManager` and `EmbeddingEngine` to eliminate memory spikes and `std::bad_alloc` crashes during large file indexing.
    - **SQLite Safety**: Deployed a `safe_col_text` helper across all repository methods to prevent crashes when encountering NULL database columns.
    - **UI Resource Guards**: Implemented character limits for chat messages (50k), generic tool outputs (5k), and log tailing (10k) to ensure smooth rendering of massive agent outputs.
    - **Window Lifetime Checks**: Added validity guards in all asynchronous `CallAfter` callbacks to prevent segmentation faults during application shutdown.

### 2026-03-16 (UI & Diagnostics Enhancements)

- **Menu Bar Integration**: Added the requested File/Agent/Tools/Benchmarks/View/Help menu structure with placeholder bindings so the UI now exposes the agent controls, benchmarking hooks, and visibility toggles while keeping the core window responsive. citeturn0exec0
- **Goal Continuity**: Persisted the active goal inside each `Thoth::ChatSession`, saved/loaded it with `chat_sessions.json`, and refreshed the banner on activation or after plan events so the goal stays visible across restarts. citeturn0exec1
- **GRAG Diagnostics Fixes**: Normalized the diagnostics payload (nested `result` & `diagnostics` blobs), restored the retrieved-chunks score column, and added percentage formatting so alpha/magnitude/scoring type reflect the telemetry instead of defaulting to zero. citeturn0exec1

### 2026-03-12 (Adaptive Graph Learning Final Implementation)

- **Adaptive Graph Learning (Phase 5.6)**:
    - **Stable Chunk Identity**: Implemented content-based hashing (SHA256) for chunks to ensure graph associations survive project re-indexing.
    - **Schema Hardening**: Updated SQLite schema to track edge weights, success/failure counts, and last-used timestamps for dormancy detection.
    - **Causal Linking**: Modified `ExecutiveController` to capture the "Active Set" of chunks (Top 5 with 0.8 relative threshold) and link them across successful step transitions.
    - **Logistic Reinforcement**: Deployed `GraphRefiner` with a logistic learning rule ($W_{new} = W + lr \times (1-W)$) to saturate weights and prevent numerical explosion.
    - **One-Hop Activation**: Upgraded `GragScorer` to perform 1-hop neighbor activation from high-confidence query hits ($Q_{sim} \geq 0.7$), ensuring experience-guided retrieval without noise.
    - **Multi-Tier Forgetting**: Integrated a global decay mechanism ($0.995$) with an additional dormancy penalty ($0.97$) for edges unused for > 30 days.
    - **Observability**: Expanded retrieval diagnostics to include `graph_source_node`, `graph_activations`, and contribution metrics in `grag_benchmark.jsonl`.

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
- **External Corpus Integration (Research Papers)**:
    - **New Corpus**: Replaced project documentation with 5 high-density AI research papers (ReAct, RAG, MemGPT, Generative Agents, CoT) as the primary retrieval benchmark.
    - **Test Suite Expansion**: Designed 30 new test cases (10 per type) specifically targeting semantic overlaps between papers (e.g., "memory" in MemGPT vs. Generative Agents).
    - **Retrieval Hardening**: 
        - **Chunk Optimization**: Reduced target chunk size to 2048 chars and added a hard 8000-char truncation guard to prevent Ollama context window errors.
        - **Reliability**: Forced `127.0.0.1` for Ollama connections to prevent IPv6 resolution failures.
        - **Cold Start Fix**: Updated `IndexManager` to train the local TF-IDF engine's vocabulary before chunking, ensuring valid keyword scores even when semantic embeddings are delayed.
    - **Empirical Validation**:
        - **Goal Disambiguation**: Confirmed a massive **+0.216 nDCG@5 lift**, proving that GRAG's directional steering is highly effective at selecting the correct paper when terminology overlaps.
        - **Overall Mean RR**: Improved from 0.587 to 0.647.
        - **Stability**: Verified that the sandbox boundary is strictly enforced across the research corpus.

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
