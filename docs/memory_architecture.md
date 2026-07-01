# Thoth Memory Architecture

**Status:** Active — M1 (memory consolidation) in progress  
**See also:** `improvements.md` Step 3.2, `memory_summary.md` (thesis synthesis)

---

## Cognitive layers

Thoth separates memory by cognitive role. Implementation follows this stack:

```
Working (hot)     →  recent raw turns in active use
        ↓ consolidation
Episodic (warm)   →  extracted experience (structured EpisodicMemory)
        ↓ (future: merge / promotion)
Semantic          →  stable knowledge (FactStore, RAG documents)
Procedural        →  reusable know-how (plans, trajectories, strategies)
Archival (cold)   →  raw transcript audit trail (archived_turns)
```

**Memory consolidation** (not “pruning”) transforms working memory into episodic + archival form. Humans consolidate; they do not garbage-collect conversation.

```
Hot (working)
  → Extract knowledge (SummaryGenerator → EpisodicMemory)
  → Store episodic (warm_memory + embeddings)
  → Archive raw transcript (archived_turns)
  → Remove redundant hot messages
```

All database steps run in a **single transaction** after the LLM call. Hot messages are deleted only when warm + archive + embedding writes succeed.

---

## MemoryScope

Retrieval and storage scope is policy-driven:

| Scope | Meaning | M1 default |
|-------|---------|------------|
| `SESSION` | Active chat session | ✅ |
| `PROJECT` | Thoth workspace / project | Reserved |
| `USER` | Cross-session user profile | Reserved |
| `GLOBAL` | System-wide | Reserved |

M1 enforces `SESSION` only; schema carries `scope` for later policy.

---

## EpisodicMemory (source of truth)

The in-memory `EpisodicMemory` object is authoritative. JSON in SQLite is **serialization for persistence**, not the source of truth.

Fields include goals, plans attempted, decisions, failures, tool results, open tasks, facts learned, user preferences, outstanding questions, plus importance / novelty / confidence scores.

- **Rendered summary** — prose for humans and prompt injection (`EpisodicMemoryRenderer`).
- **Canonical embed text** — deterministic structured text for embeddings (immune to renderer style changes).
- **Embeddings** — stored in `warm_memory_embeddings`, separate from content (re-embed on model upgrade).

### Scoring (M1)

| Field | M1 rule |
|-------|---------|
| `importance` | Deterministic heuristic (+decision, +tool, +preference, +task complete) |
| `novelty` | Fixed `0.5` until enough warm history exists |
| `confidence` | `1.0` unless malformed JSON, inferred fields, or truncation |

---

## SQLite tables (M1)

**`warm_memory`** — content and metadata (no embedding blob).

**`warm_memory_embeddings`** — `(memory_id, embedding_version)` → BLOB; mirrors `plan_embeddings` separation.

**`archived_turns`** — cold audit; `metadata_json` links to warm id, `derived_from_hash`, source range.

**`derived_from_hash`** — hash of canonical batch content; verifies summary ↔ archive integrity.

---

## GRAG integration

Retrieval merges sources **before** scoring:

```
documents (IndexManager)
     +
warm_memory (session-scoped cosine search)
     ↓
merge candidates
     ↓
GragScorer::rescore
     ↓
top-K → prompt injection
```

Prompt assembly also injects recent warm `rendered_summary` text under `[Memory Context]` (budget-limited).

---

## Failure policy

| Failure | Behavior |
|---------|----------|
| LLM summarize fails | Archive raw; warm row with `summary_missing=1`; transaction still commits |
| Embedding fails | **No transaction** — hot messages unchanged |
| SQLite commit fails | **ROLLBACK** — hot messages unchanged |

Background repair of `summary_missing` rows is deferred (post-M1).

---

## Future direction

**Knowledge Artifact model:** Unify documents, episodic memories, plans, strategies, reflections, and tool outputs as artifact types consumed by one GRAG retrieval interface. M1 introduces episodic memory as the **first non-document artifact type** without building the full abstraction yet.

**Warm lifecycle (M4+):** Merge similar warm summaries → promote to semantic memory → archive; prevents unbounded tiny summary accumulation.

**Rename:** `MemoryPruner` → `MemoryConsolidator` when churn allows; design docs use “consolidation” terminology now.

---

**M1 verified** (2026-06-26). **M2 complete** (2026-06-29). **M3 complete** (2026-06-30).

---

## Architectural invariant (M3)

Consolidation is deterministic with respect to the current hot memory state. Manual consolidation changes only **when** consolidation occurs, not **how** it behaves. Whether triggered by hot count, age, startup, session switch, or `/prune`, the same pipeline executes: `SummaryGenerator` → warm row + embedding → cold archive → hot delete (single transaction after LLM/embed).

`--ignore-thresholds` bypasses **automatic trigger conditions** only. Safety policies still apply: goal-active guard (unless `--unsafe`), embed failure abort, SQLite rollback.

---

## M3 — operational interface (`/prune`) ✅

| Component | Detail |
|-----------|--------|
| API | `Memory::getConsolidationStatus()`, `Memory::runConsolidation()` |
| Source vs Reason | `ConsolidationSource` (AUTOMATIC/MANUAL) separate from `ConsolidationReason` (policy triggers) |
| Trace dimensions | `requested_by` + `source` + `decision.reasons` |
| CLI | `/prune` → status; `explain`, `batch`, `run`; `--ignore-thresholds`, `--unsafe` |
| Goal guard | Block manual consolidation while goal executing unless `--unsafe` |
| Tests | M3-01 – M3-09 (`THOTH_MOCK_EPISODIC=1`) |
| Future | `/memory` namespace — parser stub only in M3 |

**Public types (frozen after M3):** `ConsolidationStatus`, `ConsolidationRequest`, `ConsolidationResult`, `ConsolidationDecision`.

---

## M2 — age-based consolidation policy ✅

| Component | Detail |
|-----------|--------|
| Policy | `evaluatePolicy()` → `ConsolidationDecision` with reason bitmask |
| Triggers | `HOT_COUNT` \| `SESSION_INACTIVE` \| `OLDEST_MESSAGE` (OR logic) |
| Config | `config.json` → `memory.max_hot_messages`, `max_hot_age_days`, `prune_batch_size` |
| Clock | `Clock` / `FakeClock` injected into `MemoryPruner` |
| Loop | Up to 5 batches; stop on policy clear, no progress, or cap (`consolidation_deferred` trace) |
| Startup | Discover stale sessions (no LLM); consolidate active only; defer others until switch |
| GUI | Timestamp-preserving load; consolidate-before-trim |

Reserved (not evaluated): `TOKEN_LIMIT`, `MEMORY_PRESSURE`.

---

## M1.5 verification ✅ complete

All gate criteria passed (`episodic_memory_benchmark.md`):

| ID | Test | Status |
|----|------|--------|
| M1.5a | `testEpisodicRetrievalEndToEnd` | ✅ |
| M1.5b | `testConsolidationFailureEmbed`, `testConsolidationFailureTransaction` | ✅ |
| M1.5c | `testConsolidationLatencyRecorded` | ✅ |
| M1.5d | `testEpisodicMemoryBenchmarkNegative` | ✅ |

Run: `THOTH_MOCK_EPISODIC=1 ./build/debug/tests/thoth-unit-tests`

---

*Last updated: 2026-06-30 (M3 `/prune` operational interface complete)*
