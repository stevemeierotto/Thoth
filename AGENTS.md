# AGENTS.md — AI Coding Agent Guide

**Read this document before making any changes to Thoth.**

This document describes the architecture, conventions, and critical rules for this project. Following these guidelines ensures changes integrate cleanly and preserve the system's design integrity.

**Last Updated**: 2026-03-10  
**Status**: Current — reflects completed work through Phase 12 and Memory Stability improvements

---

## Project Overview

**Thoth** is a C++ GUI application that provides an interface to an AI agent powered by a hybrid memory system and **GRAG (Goal-Relative Adaptive Graph Retrieval)** pipeline. The system implements a full cognitive architecture with dynamic planning, execution control, experience-guided reasoning, and self-correction capabilities.

- **Language**: C++
- **Build System**: CMake with presets (see `CMakeLists.txt`, `CMakePresets.json`)
- **License**: MIT — preserve license headers in all source files
- **Current Status**: Fully operational autonomous cognitive orchestrator with empirically validated retrieval, crash-resilient execution, and dynamic self-correction

---

## Architecture

### System Overview Diagram

```
┌─────────────────────────────────────────────────────────────────────────┐
│                           USER INTERFACE                                 │
│                    (wxWidgets GUI - MainFrame.cpp)                       │
└───────────────────────────────┬─────────────────────────────────────────┘
                                │
                                │ AgentInterface (Bridge)
                                │
┌───────────────────────────────▼─────────────────────────────────────────┐
│                      COMMAND PROCESSOR                                  │
│              (Security + Request Routing + Tool Dispatch)               │
│  ┌──────────────────────────────────────────────────────────────────┐   │
│  │  ConstraintChecker  │  ToolRegistry  │  PromptFactory            │   │
│  └──────────────────────────────────────────────────────────────────┘   │
└───────────────────────────────┬─────────────────────────────────────────┘
                                │
                ┌───────────────┴───────────────┐
                │                               │
┌───────────────▼───────────────┐  ┌───────────▼──────────────┐
│     EXECUTIVE CONTROLLER      │  │   STANDARD INTERACTION   │
│   (Cognitive Spine / State    │  │   (Direct LLM Response)  │
│        Machine)               │  │                          │
│                               │  │                          │
│  ┌─────────────────────────┐  │  │                          │
│  │  IPlanner (LLMPlanner)  │  │  │                          │
│  │  - Dynamic Planning     │  │  │                          │
│  │  - Plan Revision        │  │  │                          │
│  └───────────┬─────────────┘  │  │                          │
│              │                │  │                          │
│  ┌───────────▼─────────────┐  │  │                          │
│  │  Execution Modes        │  │  │                          │
│  │  - Standard             │  │  │                          │
│  │  - Scientific           │  │  │                          │
│  └───────────┬─────────────┘  │  │                          │
│              │                │  │                          │
│  ┌───────────▼─────────────┐  │  │                          │
│  │  WorkflowEngine         │  │  │                          │
│  │  - Step Dispatch        │  │  │                          │
│  │  - Tool Execution       │  │  │                          │
│  └───────────┬─────────────┘  │  │                          │
└──────────────┼────────────────┘  └──────────────────────────┘
               │
               │
    ┌──────────┴──────────┐
    │                     │
┌───▼──────────┐  ┌───────▼────────┐
│  ToolRegistry│  │  GRAG Pipeline │
│  (9 Tools)   │  │  (Retrieval)   │
└───┬──────────┘  └───────┬────────┘
    │                     │
    │                     │
┌───▼─────────────────────▼──────────────┐
│         HYBRID MEMORY SYSTEM            │
│                                         │
│  ┌───────────────────────────────────┐ │
│  │  SQLiteMemoryRepository           │ │
│  │  - Episodic Memory (trajectories) │ │
│  │  - Plan History                  │ │
│  │  - Step Metrics                  │ │
│  │  - Graph Memory (nodes/edges)    │ │
│  └───────────────────────────────────┘ │
│                                         │
│  ┌───────────────────────────────────┐ │
│  │  FactStore                         │ │
│  │  - Structured World Knowledge     │ │
│  └───────────────────────────────────┘ │
│                                         │
│  ┌───────────────────────────────────┐ │
│  │  IVectorStore (FlatVectorStore)   │ │
│  │  - Vector Embeddings (rag_index)  │ │
│  └───────────────────────────────────┘ │
└─────────────────────────────────────────┘
           │
           │
┌──────────▼──────────┐
│   External Services  │
│  - Ollama (LLM)     │
│  - Ollama (Embed)   │
│  - (Optional) OpenAI│
└─────────────────────┘
```

### Data Flow

**Goal Execution Flow:**
```
User Input
  ↓
CommandProcessor (security check)
  ↓
ExecutiveController::execute_goal()
  ↓
IPlanner::createInitialPlan() → Plan
  ↓
ExecutiveController::run_loop()
  ├─→ GRAG Retrieval (for context)
  ├─→ WorkflowEngine::dispatch_step()
  │   └─→ ToolRegistry::executeTool()
  ├─→ Memory Update (episodic, graph)
  └─→ StrategyEngine (pattern detection)
  ↓
Plan Completion → Memory Persistence
```

**Retrieval Flow (GRAG):**
```
Query/Goal
  ↓
RAGPipeline::retrieveRelevant()
  ├─→ EmbeddingEngine (generate Q, G, C embeddings)
  ├─→ GragScorer::rescore() (directional scoring)
  │   ├─→ IVectorStore (semantic search)
  │   ├─→ TF-IDF (keyword search)
  │   └─→ Graph Memory (relational context)
  └─→ Top-K Results → PromptFactory
```

### High-Level Structure

```
Thoth/
├── src/                          # GUI layer (wxWidgets-based)
│   ├── MainFrame.cpp
│   ├── AgentInterface.cpp        # Bridge between GUI and core
│   └── VisualizationFrame.cpp
├── external/basic_agent/         # Core agent library
│   ├── src/                      # Agent logic implementation
│   ├── include/                  # Public headers
│   └── CMakeLists.txt
├── agent_workspace/              # Runtime state and memory files
│   ├── memory.db                 # DO NOT EDIT MANUALLY — SQLite episodic/semantic memory
│   ├── chat_sessions.json        # DO NOT EDIT MANUALLY — Conversation history
│   ├── rag/                      # Sandboxed RAG corpus (bootstrap source)
│   ├── rag_index.bin             # DO NOT EDIT MANUALLY — Vector embeddings index
│   ├── retrieval_config.json     # GRAG weight configuration (locked)
│   └── decision_trace.jsonl      # Pipeline execution trace (managed by DecisionTraceLogger)
├── logs/                         # Runtime logs
│   ├── decision_trace.jsonl      # Agent behavior & state transition log
│   └── grag_benchmark.jsonl     # Retrieval math performance log
├── tests/                        # Unit tests
│   ├── unit_tests.cpp
│   └── CMakeLists.txt
└── docs/                         # Documentation and improvement tracking
    ├── improvements.md
    └── completed_improvements_log.md
```

### Component Relationship

- **GUI Layer** (`src/`): User-facing interface. Links against `libbasic_agent.so`.
- **Core Agent Library** (`external/basic_agent/`): All agent logic. Compiles to `libbasic_agent.so`.
- **AgentInterface**: Bridge between GUI and core. Changes to the public interface require coordinated updates.

**⚠️ Do not modify GUI files without understanding the AgentInterface bridge.**

---

## Key Components

### Memory System

**Type**: Hybrid episodic/semantic/working memory  
**Location**: `agent_workspace/`

- `memory.db` — SQLite database storing:
  - Episodic memory (full execution trajectories: goal → plan → step → result → revision → outcome)
  - Semantic memory (structured facts via `FactStore`)
  - Graph memory (prototype SQLite-backed knowledge graph for relational context)
  - Plan history (for experience reuse)
  - Step metrics (for strategy learning)
- `chat_sessions.json` — Tracks conversation history
- `rag_index.bin` — Vector embeddings for retrieval (managed via `IVectorStore` abstraction)

**⚠️ NEVER manually edit memory files. They are managed by the memory system.**

**Key Features**:
- **Crash Recovery & Resumability**: Full execution state persisted in real-time. Agent can resume any goal after crash via `resume_from_plan()`
- **FactStore**: Persistent store for structured world knowledge (separate from episodic memory)
- **Plan History Reuse**: `MemoryRepository` retrieves past successful plans by directional similarity

### GRAG (Goal-Relative Adaptive Graph Retrieval)

**Status**: ✅ Fully operational — Core scoring, multi-index routing, graph memory prototype  
**Empirical Validation**: +0.200 nDCG@5 improvement over baseline RAG (verified 2026-03-10)

**Core Files**:
- `rag.cpp` / `rag.h` — Orchestrates GRAG retrieval pipeline
- `grag_scorer.cpp` — Implements directional scoring (`D = G - C`)
- `vector_store.cpp` / `i_vector_store.h` — Vector store abstraction (enables future migration to production databases)
- `index_manager.cpp` — Handles index lifecycle with selective re-indexing
- `embedding_engine.cpp` — Generates embeddings via Ollama REST API (`nomic-embed-text`)

**Key Features**:
- **Directional Retrieval**: Scores documents by alignment with `D = G - C` (Goal minus Current State)
- **Adaptive Blending**: Dynamically adjusts balance between semantic similarity and goal-directed retrieval
- **Multi-Index Routing**: Three modes (`PLAN_AWARE`, `GOAL_ONLY`, `CONVERSATIONAL`) with automated fallback
- **Graph Memory**: Prototype SQLite-backed knowledge graph augments vector scores with relational context
- **Explainable Retrieval**: Every chunk carries `ScoreBreakdown` (Query, Goal, Trajectory, Keyword, Graph signals)
- **Config Locking**: Optimized weights stored in `agent_workspace/retrieval_config.json` ($w_q=0.4, w_d=0.4, w_k=0.3, w_g=0.3$)

**⚠️ Changes here affect retrieval quality. Tread carefully and verify with tests.**

**Planned Upgrades** (not yet implemented):
- Trajectory Awareness activation (`w_t = 0.2` currently stubbed to 0.0)
- Hierarchical Subgoal Trees (active subgoal embedding per subgoal)

**Completed Upgrades**:
- Dynamic Graph Edge Learning: Edge weights are now dynamically adjusted via `GraphRefiner` based on execution success/failure. Graph density metrics are logged in retrieval diagnostics.

### Tool System

**Architecture**: Registry-based pattern  
**Status**: ✅ Fully operational — 9 tools registered, confirmation system enforced  
**Specification**: See `docs/TOOLS.md v1.0` for complete contract

**Key Files**:
- `tools.cpp` / `tools.h` — `ToolRegistry` implementation
- `itool.h` — Base interface (`ITool`) with `requires_confirmation()` support
- Individual tool implementations in `*_tool.h` / `*_tool.cpp`

**Registered Tools** (9 total):
1. `summarize_text` — Text summarization
2. `gmail_read_labels` — Gmail label reading
3. `gmail_read_messages` — Gmail message reading
4. `project_analyze` — Project structure analysis
5. `run_tests` — Unit test execution
6. `code_modify` — Code reading and diff application (⚠️ `apply_diff` is stub)
7. `web_scrape` — Web scraping with RAG ingestion
8. `store_fact` — Structured fact storage (requires `FactStore` initialization)
9. `self_correct` — In-step self-correction reasoning (requires `LLMInterface` initialization)

**Critical Rules**:
- All tools must be registered through `ToolRegistry`
- **Never** add hardcoded virtual methods to a base class
- Tools use JSON in, JSON out interfaces
- **Security**: All risky operations must implement `requires_confirmation()` and enforce user approval
- Tools must never access memory/database directly — all access goes through runtime

**Adding a New Tool**:
1. Implement `ITool` interface (see `itool.h`)
2. Create tool class inheriting from `ITool`
3. Register in `ToolRegistry::ToolRegistry()` constructor or `initialize()` method
4. Tool descriptions from `ToolRegistry::getAvailableTools()` are injected into prompts via `prompt_factory.cpp`
5. Follow `docs/TOOLS.md v1.0` specification

### ExecutiveController — The Cognitive Spine

**Status**: ✅ Fully operational — Thread-safe, resumable, observable state machine

**Core Files**:
- `executive_controller.cpp` / `executive_controller.h` — Main orchestration engine
- `plan.h` / `plan.cpp` — Plan data structures with JSON serialization
- `iplanner.h` — Planner interface (implemented by `LLMPlanner`)
- `iexecution_mode.h` — Execution mode strategy pattern
- `standard_execution_mode.cpp` — Standard sequential execution
- `scientific_execution_mode.cpp` — Scientific hypothesis testing mode

**Key Features**:
- **Full Lifecycle Management**: `PLANNING → EXECUTING_STEP → OBSERVING_RESULT → REVISING_PLAN → COMPLETED`
- **Thread-Safe**: Protected via `std::mutex` with `std::lock_guard` across all shared state
- **Structured Event Emission**: All events logged to `decision_trace.jsonl` via `DecisionTraceLogger` with schema versioning
- **Dynamic Planning**: LLM-driven variable-length plans (no hardcoded scaffolds)
- **Dynamic Plan Revision**: Automatic plan repair after step failures using trajectory data
- **Crash Recovery**: Full state persistence enables resuming from any point via `resume_from_plan()`
- **Strategy Pattern**: Pluggable execution modes (Standard, Scientific) without nested state machines
- **GRAG Integration**: Updates goal/current/trajectory embeddings for directional retrieval

**State Machine**:
```
IDLE → PLANNING → EXECUTING_STEP → OBSERVING_RESULT → [REVISING_PLAN] → COMPLETED
                                                      ↓
                                                   ABORTED/FAILED
```

### Request Pipeline

**Orchestration**: `command_processor.cpp` manages the full request lifecycle  
**Security**: `ConstraintChecker` enforces global security policies

**Pipeline Stages** (see `logs/decision_trace.jsonl`):
1. Input processing
2. Memory retrieval (GRAG with directional scoring)
3. Prompt assembly (includes available tools if enabled)
4. LLM invocation
5. Response parsing
6. Tool execution (if needed, with confirmation gates)
7. Memory update (episodic, semantic, graph)
8. Trajectory recording (for strategy learning)

### Prompt Building

**File**: `prompt_factory.cpp`

Assembles the final prompt sent to the model, including:
- System instructions
- Retrieved context from GRAG (with explainable `ScoreBreakdown`)
- Available tools (from `ToolRegistry::getAvailableTools()` when `Config::enable_tools` is true)
- User input
- Plan history hints (from `StrategyEngine` for experience reuse)

### LLM Interface

**File**: `llm_interface.cpp`

Abstracts the model backend. Currently supports Ollama local models.

**Current Model**: Small Qwen model (hardware-constrained)  
**Embedding Model**: `nomic-embed-text` (768 dimensions) via Ollama REST API  
**Future**: Hardware upgrade and model upgrade planned

**⚠️ Do not hardcode model-specific behavior. The interface is designed to support model upgrades without changing agent logic.**

### Strategy Engine & Trajectory Learning

**Status**: ✅ Implemented — Analyzes execution trajectories to surface reusable patterns

**Core Files**:
- `strategy_engine.cpp` / `strategy_engine.h` — Pattern detection and strategy promotion
- `trajectory_builder.cpp` / `trajectory_builder.h` — Builds semantic trajectory embeddings
- `step_metrics_repository.cpp` — Tracks tool success rates and latency

**Key Features**:
- **TrajectoryBuilder**: Summarizes last N execution steps into semantic vector `T` (currently stubbed in scoring)
- **StrategyEngine**: Analyzes trajectories to surface repeating successful patterns (e.g., `search → parse → summarize`)
- **Strategy Hints**: Informs planner to prefer high-success tools when multiple options exist

### Security & Constraints

**Status**: ✅ Implemented — Global security enforcement with confirmation gates

**Core Files**:
- `constraint_checker.cpp` / `constraint_checker.h` — Global security policy enforcement
- Integrated into `CommandProcessor` for both standard interaction and goal execution

**Key Features**:
- **Tool Confirmation System**: `requires_confirmation()` enforced across all risky operations
- **Sandbox Boundaries**: `IndexManager` hard-rejects requests outside `agent_workspace/`
- **Constraint Checking**: Validates operations before execution

### JSON Library

**Library**: nlohmann/json  
**Location**: `external/basic_agent/include/json.hpp`

**⚠️ Always use this library. Do not introduce a second JSON library.**

---

## Coding Conventions

### File Organization

- All new source files: `external/basic_agent/src/`
- All new headers: `external/basic_agent/include/`
- Tests: `tests/unit_tests.cpp`

### Naming Conventions

- **Files**: `snake_case` (e.g., `command_processor.cpp`)
- **Classes**: `PascalCase` (e.g., `ToolRegistry`, `EmbeddingEngine`)
- **Functions/Variables**: Follow existing patterns in the codebase

### Tool Implementations

- Must go through `ToolRegistry` — never bypass it
- JSON in, JSON out for all tool interfaces
- Register tools with descriptive names and schemas

### Dependencies

- Do not introduce new external dependencies without updating `DEPENDENCIES.md`
- Check if functionality can be implemented with existing dependencies first

### Build Verification

- Build with CMake before considering any change complete
- Run tests to verify functionality
- Check that `libbasic_agent.so` builds without errors

---

## Critical Rules — What AI Agents Should NOT Do

### 🚫 DO NOT modify `agent_workspace/` files directly

The memory system manages these files. Manual edits will corrupt the agent's state.

### 🚫 DO NOT add virtual methods to the Tools base class

The tool system uses a registry pattern. Adding virtual methods bypasses the registry and breaks extensibility.

### 🚫 DO NOT introduce a second JSON library

Use `nlohmann/json` at `external/basic_agent/include/json.hpp`. Multiple JSON libraries cause linking conflicts and bloat.

### 🚫 DO NOT change the `libbasic_agent.so` public interface without updating `AgentInterface.h`

The GUI depends on this interface. Breaking changes require coordinated updates to `src/AgentInterface.cpp`.

### 🚫 DO NOT delete or overwrite files in `docs/`

- `docs/improvements.md` — Active improvement plans
- `docs/completed_improvements_log.md` — Historical record

These files track project progress. Append to them; do not overwrite.

### 🚫 DO NOT modify GUI files without understanding the bridge

`src/MainFrame.cpp`, `src/AgentInterface.cpp`, and `src/VisualizationFrame.cpp` interact with the core through `AgentInterface`. Changes require understanding this boundary.

---

## Development Workflow

### Before Making Changes

1. Read this document in full
2. Review `docs/completed_improvements_log.md` to understand what's already implemented
3. Review `docs/improvements.md` for planned work
4. Check `docs/architectural_facts.md` for implementation status and known issues
5. Review relevant architecture docs (`GRAG.md`, `TOOLS.md`, `PLAN.md`) if touching those systems
6. Check `logs/decision_trace.jsonl` to understand the request pipeline
7. Review existing implementations for patterns and conventions

### Making Changes

1. Locate the correct component (see Architecture section)
2. Follow coding conventions
3. If adding a tool:
   - Implement `ITool` interface
   - Register in `ToolRegistry`
   - Follow `docs/TOOLS.md v1.0` specification
   - Add `requires_confirmation()` if operation is risky
4. If modifying GRAG pipeline:
   - Verify retrieval quality with benchmarks
   - Update `retrieval_config.json` if weights change
   - Check `docs/GRAG.md` for scoring formula
5. If modifying ExecutiveController:
   - Maintain thread-safety (use `std::lock_guard`)
   - Emit events via `DecisionTraceLogger`
   - Preserve resumability (ensure state is persisted)
6. Update tests in `tests/unit_tests.cpp` if interfaces change

### After Making Changes

1. Build with CMake: `cmake --build --preset <preset>`
2. Run unit tests: `./tests/unit_tests` (or via CTest)
3. Test the GUI application to verify integration
4. Update `docs/completed_improvements_log.md` if completing a planned improvement
5. Preserve MIT license headers in all modified files

---

## Reference Files

### Architecture & Design
- **GRAG Specification**: `docs/GRAG.md` — Complete GRAG architecture and scoring formula
- **Tool System Spec**: `docs/TOOLS.md` — Tool contract and implementation requirements
- **Planning Architecture**: `docs/PLAN.md` — ExecutiveController design principles
- **Project Overview**: `docs/README.md` — High-level system description
- **Architectural Facts**: `docs/architectural_facts.md` — Implementation status and audit results

### Build & Configuration
- **Build Configuration**: `CMakeLists.txt`, `CMakePresets.json`
- **Dependencies**: `DEPENDENCIES.md`
- **Retrieval Config**: `agent_workspace/retrieval_config.json` — Locked GRAG weights

### Tracking & Logs
- **Active Roadmap**: `docs/improvements.md` — Current improvement phases
- **Completed Work**: `docs/completed_improvements_log.md` — **Authoritative source of truth for completed features**
- **Benchmark Results**: `docs/benchmark_results.md` — Auto-archived performance metrics
- **Pipeline Trace**: `logs/decision_trace.jsonl` — Agent behavior & state transitions (managed by `DecisionTraceLogger`)
- **GRAG Metrics**: `logs/grag_benchmark.jsonl` — Retrieval math performance monitoring

### Core Implementation
- **Core Agent Library**: `external/basic_agent/src/`, `external/basic_agent/include/`
- **GUI Bridge**: `src/AgentInterface.cpp` — Interface between GUI and core library

---

## Current System Status

### ✅ Fully Implemented
- GRAG retrieval with directional scoring and graph memory
- ExecutiveController with thread-safety and crash recovery
- Dynamic planning and plan revision
- 9-tool system with confirmation gates
- FactStore for structured knowledge
- Strategy Engine and trajectory learning infrastructure
- Scientific execution mode
- Self-building capabilities (project analysis, test execution, code modification)
- Security enforcement (ConstraintChecker, sandbox boundaries)

### 🔬 Prototype / Partial
- Trajectory Awareness (infrastructure exists, `w_t` stubbed to 0.0 in scoring)
- Code Modification (`apply_diff` is stub)

### 📋 Planned
- Hierarchical Subgoal Trees (active subgoal embedding per subgoal)
- Trajectory Awareness activation in scoring
- Memory Pruning and Archival policies

### 🚫 Stub / Not Implemented
- `code_modify` tool's `apply_diff` operation

---

## Questions or Clarifications?

If you're unsure about any architectural decision or coding convention:
1. Refer to `docs/completed_improvements_log.md` for what's actually implemented
2. Check `docs/architectural_facts.md` for implementation status and known issues
3. Review relevant architecture docs (`GRAG.md`, `TOOLS.md`, `PLAN.md`)
4. Refer to existing implementations in `external/basic_agent/src/` for patterns and precedents

The codebase is the source of truth, but `docs/completed_improvements_log.md` is the authoritative record of completed work.

**When in doubt, ask before making breaking changes.**
