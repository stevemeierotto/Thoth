You are a senior C++ systems engineer working on the Thoth project.

Before making any changes, read and strictly follow the architectural rules in AGENTS.md.

Critical constraints:

Do NOT modify GUI files.

Do NOT change the public interface of libbasic_agent.so.

Do NOT introduce new external dependencies beyond SQLite.

Continue using nlohmann/json.

Do NOT modify decision_trace.jsonl.

Follow naming conventions (snake_case files, PascalCase classes).

All new files must go in external/basic_agent/src/ and include/.

Your task:
Migrate episodic memory persistence from JSON files to SQLite in a phased, low-risk way.

Current files:

agent_workspace/memory.json

agent_workspace/chat_sessions.json

agent_workspace/decision_trace.jsonl (append-only, keep as-is)

Goal:
Replace JSON-based mutable memory storage with SQLite while preserving all existing semantics and behavior.

IMPORTANT: PHASED EXECUTION MODEL

You MUST follow this structure exactly.
Do NOT jump ahead.
Do NOT output full implementation immediately.

PHASE 1 — System Analysis (NO CODE)

Confirm ownership boundaries between GUI and core.

Determine whether migrating chat_sessions.json is feasible under current constraints.

If not feasible, redefine migration scope to:

Migrate memory.json only

Defer chat_sessions.json migration

Provide a revised phased plan aligned with architectural constraints.

Analyze how memory.json and chat_sessions.json are currently used.

Identify:

Data structures in use

Write paths

Read paths

Coupling points

Identify risks in migrating storage.

Propose a minimal-impact insertion point for a repository abstraction.

Output:

Structured analysis

Clear migration risk list

Proposed interface boundary

Wait for confirmation before proceeding.

The analysis has revealed that chat_sessions.json is directly managed by the GUI (MainFrame), and we are forbidden from modifying GUI files.

Revise Phase 1 conclusions:


Do not proceed to Phase 2 until scope is redefined.


PHASE 2 — Repository Abstraction Design (INTERFACE ONLY)

Design a storage abstraction layer:

Create:

MemoryRepository (pure virtual interface)

JsonMemoryRepository (legacy implementation stub)

SQLiteMemoryRepository (stub only)

Define:

Required methods

Transaction expectations

Error handling strategy

Thread safety assumptions

Do NOT implement logic yet.

Output:

Header file definitions only

Explanation of design decisions

Wait for confirmation.

PHASE 3 — SQLite Schema Design

Design normalized schema:

Tables:

sessions

messages

summaries

memory_meta

Include:

Exact CREATE TABLE statements

Indexes

Foreign keys

Rationale for column types

Timestamps in ms

Ensure:

Efficient session lookup

Efficient message retrieval by session

Minimal write contention

No C++ yet — schema only.

Wait for confirmation.

PHASE 4 — Minimal SQLite Integration

Implement:

SQLiteMemoryRepository skeleton

Schema creation on initialization

Open/close database

Transaction wrapper

Basic insert + select for sessions

Keep functionality minimal:

Only enough to validate schema works

No full migration yet

Output:

.h and .cpp

Clear explanation of integration point

How to switch between JSON and SQLite

Wait for confirmation.

PHASE 5 — Full Feature Parity

Expand SQLiteMemoryRepository to fully match:

Session creation

Message append

Summary storage

Metadata storage

Retrieval paths

Ensure:

All writes use transactions

Prepared statements are used

No SQL injection risk

Indexes are used properly

Provide unit test suggestions.

Wait for confirmation.

PHASE 6 — Migration Tool

Create a one-time migration tool:

Requirements:

Reads memory.json

Reads chat_sessions.json

Writes to SQLite

Runs inside single transaction

Idempotent (safe to run twice)

Detects if migration already completed

Output:

Standalone migration function or executable

Clear migration flow

Rollback behavior on failure

Wait for confirmation.

PHASE 7 — Compatibility Mode

Implement temporary compatibility mode:

Behavior:

If SQLite DB exists → use SQLite

If not → fall back to JSON

Optionally auto-trigger migration

Ensure:

No silent data loss

Clear logging

No behavior change from user perspective

Wait for confirmation.

Engineering Standards

Use RAII for DB connections.

Use prepared statements.

Use explicit transactions.

Use exceptions or error codes consistently with existing codebase.

Preserve existing data semantics exactly.

No behavioral changes in agent pipeline.

No GUI changes.

No prompt logic changes.

Deliverable Expectations

At each phase:

Provide only what is requested.

Explain reasoning clearly.

Keep implementation minimal and testable.

Avoid speculative overengineering.

Do NOT:

Dump the entire final system in one response.

Skip phases.

Change unrelated systems.

Modify RAG pipeline.

Modify ToolRegistry.
