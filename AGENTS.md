# AGENTS.md — AI Coding Agent Guide

**Read this document before making any changes to Thoth.**

This document describes the architecture, conventions, and critical rules for this project. Following these guidelines ensures changes integrate cleanly and preserve the system's design integrity.

---

## Project Overview

**Thoth** is a C++ GUI application that provides an interface to an AI agent powered by a hybrid memory system and RAG (Retrieval-Augmented Generation) pipeline.

- **Language**: C++
- **Build System**: CMake with presets (see `CMakeLists.txt`, `CMakePresets.json`)
- **License**: MIT — preserve license headers in all source files

---

## Architecture

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
│   ├── memory.db               # DO NOT EDIT MANUALLY
│   ├── chat_sessions.json        # DO NOT EDIT MANUALLY
│   ├── rag_index.bin             # DO NOT EDIT MANUALLY
│   └── decision_trace.jsonl      # Pipeline execution trace
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

- `memory.db` — Stores episodic and semantic memories
- `chat_sessions.json` — Tracks conversation history
- `rag_index.bin` — Vector embeddings for retrieval

**⚠️ NEVER manually edit memory files. They are managed by the memory system.**

### RAG Pipeline

**Core Files**:
- `rag.cpp` — Orchestrates retrieval-augmented generation
- `vector_store.cpp` — Manages vector embeddings
- `index_manager.cpp` — Handles index lifecycle
- `embedding_engine.cpp` — Generates embeddings

**⚠️ Changes here affect retrieval quality. Tread carefully and verify with tests.**

### Tool System

**Architecture**: Registry-based pattern

**Key Files**:
- `tools.cpp` / `tools.h` — Tool implementations
- `ToolDefinition` struct — Defines tool schema
- `ToolRegistry` class — Manages tool registration and lookup

**Critical Rules**:
- All tools must be registered through `ToolRegistry`
- **Never** add hardcoded virtual methods to a base class
- Tools use JSON in, JSON out interfaces

**Adding a New Tool**:
1. Define a `ToolDefinition` struct
2. Implement the tool function
3. Register it with `ToolRegistry`
4. Tool descriptions from `ToolRegistry::getAvailableTools()` are injected into prompts via `prompt_factory.cpp`

### Request Pipeline

**Orchestration**: `command_processor.cpp` manages the full request lifecycle

**Pipeline Stages** (see `agent_workspace/decision_trace.jsonl`):
1. Input processing
2. Memory retrieval (RAG)
3. Prompt assembly
4. LLM invocation
5. Response parsing
6. Tool execution (if needed)
7. Memory update

### Prompt Building

**File**: `prompt_factory.cpp`

Assembles the final prompt sent to the model, including:
- System instructions
- Retrieved context from RAG
- Available tools (from `ToolRegistry::getAvailableTools()`)
- User input

### LLM Interface

**File**: `llm_interface.cpp`

Abstracts the model backend. Currently supports Ollama local models.

**Current Model**: Small Qwen model (hardware-constrained)  
**Future**: Hardware upgrade and model upgrade planned

**⚠️ Do not hardcode model-specific behavior. The interface is designed to support model upgrades without changing agent logic.**

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

1. Read this document
2. Review `docs/improvements.md` for planned work
3. Check `agent_workspace/decision_trace.jsonl` to understand the request pipeline
4. Review existing implementations for patterns and conventions

### Making Changes

1. Locate the correct component (see Architecture section)
2. Follow coding conventions
3. If adding a tool, use `ToolRegistry`
4. If modifying the RAG pipeline, verify retrieval quality
5. Update tests in `tests/unit_tests.cpp` if interfaces change

### After Making Changes

1. Build with CMake: `cmake --build --preset <preset>`
2. Run unit tests: `./tests/unit_tests` (or via CTest)
3. Test the GUI application to verify integration
4. Update `docs/completed_improvements_log.md` if completing a planned improvement
5. Preserve MIT license headers in all modified files

---

## Reference Files

- **Build Configuration**: `CMakeLists.txt`, `CMakePresets.json`
- **Improvement Tracking**: `docs/improvements.md`, `docs/completed_improvements_log.md`
- **Dependencies**: `DEPENDENCIES.md`
- **Pipeline Trace**: `agent_workspace/decision_trace.jsonl`
- **Core Agent Library**: `external/basic_agent/src/`, `external/basic_agent/include/`

---

## Questions or Clarifications?

If you're unsure about any architectural decision or coding convention, refer to existing implementations in `external/basic_agent/src/` for patterns and precedents. The codebase is the source of truth.

**When in doubt, ask before making breaking changes.**
