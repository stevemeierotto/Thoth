# M4 — Memory Range Restore Protocol

**Protocol version:** M4 v1.0  
**Status:** 🔒 **Locked** · ✅ **Implemented** 2026-07-18  
**Depends on:** M1–M3 consolidation stack (✅), [`memory_architecture.md`](memory_architecture.md)  
**Checkpoint tracking:** [`cursor_list.md`](cursor_list.md) §2 (M4), [`improvements.md`](improvements.md) Phase 3 Step 3.2  
**Supersedes:** Draft M4 protocol refinements in agent review (2026-07-18)

---

## Document structure

| Class | Sections | Binding? |
|-------|----------|----------|
| **Normative** | Scope, modes, range, ordering, duplicates, transactions, API, guards, consolidation interaction, DecisionTrace, CLI, tests, forbidden | **Yes** — SHALL/MUST |
| **Informative** | Rationale, dependency notes, doc map | **No** |

If informative text conflicts with a normative rule, the **normative** rule governs.

**Protocol Lock Rule:** This document is locked. Do not silently revise it during implementation. If implementation reveals the protocol must change, stop and request owner approval before editing this file or code that depends on it.

---

## Informative — One sentence

M4 gives operators **timestamp-ranged** access to cold `archived_turns`: **replay** (read-only inspect) and **rehydrate** (transactional copy into hot `messages`), without changing M1–M3 consolidation policy.

---

## Normative — Scope

### In scope (M4)

- Filter cold archives by `original_timestamp_ms` range only
- **Replay** — strictly read-only inspect of matched turns
- **Rehydrate** — all-or-nothing transactional copy of matched non-duplicate turns into hot `messages`
- Legacy `MemoryPruner::restore(sessionId)` as full-session **replay** alias
- CLI under `/prune restore`
- Distinct DecisionTrace events for replay vs rehydrate
- Unit tests M4-01–M4-10 under `THOTH_MOCK_EPISODIC=1`

### Out of scope (deferred — not M4)

- Warm-memory-id / batch-id / semantic restore modes
- Warm-summary merge / promotion lifecycle (see `memory_architecture.md` “Warm lifecycle” — **post-M4**, not this protocol)
- Archive deletion or move-from-cold
- Background repair, async queue, consolidation queue
- GUI panels
- F-series functionality
- Changes to M3 frozen consolidation types or `PruningPolicy` behavior

---

## Normative — Modes

### Replay

Replay **SHALL** be a strictly read-only memory operation:

| Allowed | Forbidden |
|---------|-----------|
| Read `archived_turns` | Insert/update/delete any memory-table row (`messages`, `archived_turns`, `warm_memory`, `warm_memory_embeddings`, …) |
| Return the matched ordered set | Call consolidation (`runConsolidation` / `consolidateIfNeeded` / `evaluatePolicy`) |
| Emit DecisionTrace `memory_restore_replay` when invoked via `Memory::runRestore` / CLI | Clear stale-session marks or change consolidation backoff |
| | Require `--unsafe` or block on goal-active |

**Memory-store side effects:** none. After replay, SQLite memory-table state is unchanged.

**Observability:** DecisionTrace append on the `runRestore`/CLI path is permitted and required; it is **not** a memory-store side effect.

### Rehydrate

Rehydrate **SHALL** copy matched non-duplicate cold turns into hot `messages` inside a single SQLite transaction.

| Required | Forbidden |
|----------|-----------|
| All-or-nothing commit or full rollback | Delete or mutate cold `archived_turns` |
| Preserve cold as audit trail (copy, not move) | Mutate warm tables |
| | Invoke consolidation as part of restore |
| | Partial commits |

---

## Normative — Range (sole M4 filter)

```text
RestoreRange:
  start_ms: optional int64   // inclusive lower bound on original_timestamp_ms
  end_ms:   optional int64   // inclusive upper bound on original_timestamp_ms
```

| Bounds | Meaning |
|--------|---------|
| both omitted | Full session (all archived turns for `session_id`) |
| only `start_ms` | `original_timestamp_ms >= start_ms` |
| only `end_ms` | `original_timestamp_ms <= end_ms` |
| both set | `start_ms <= original_timestamp_ms <= end_ms` |
| both set and `start_ms > end_ms` | **Blocked** — no DB writes; `block_reason` indicates invalid range |

No other filter dimensions are part of M4.

---

## Normative — Stable ordering

Every replay result and every rehydrate matched set **SHALL** be ordered:

1. **Primary:** `original_timestamp_ms` ascending  
2. **Secondary:** `archive_id` ascending  

Legacy full-session replay **SHALL** use the same order.

---

## Normative — Duplicate prevention

Hot `messages` has no UNIQUE constraint on `(session_id, timestamp_ms)`. Duplicate prevention is a **protocol invariant**, not optional advice.

**Definition:** Archived turn *A* is a **duplicate** of hot message *H* in the same session iff:

```text
H.timestamp_ms == A.original_timestamp_ms
AND H.role == A.role
AND H.content == A.content
```

**Rules:**

- Rehydrate **SHALL** skip duplicates and increment `skipped_dup`.
- Rehydrate **SHALL NOT** insert a duplicate.
- Duplicate skips are success, not failure, and **SHALL NOT** trigger rollback by themselves.
- Duplicate checks **SHALL** run inside the rehydrate transaction against the hot tier as of that transaction.
- Replay returns cold rows regardless of hot content.

---

## Normative — Transaction / rollback (rehydrate)

1. Call `beginTransaction()` before any hot insert.
2. For each matched archived turn in stable order: skip if duplicate; else insert into `messages` with `role`, `content`, and `timestamp_ms = original_timestamp_ms`.
3. On any insert failure, statement-prepare failure, or unexpected error: call **`rollback()`**. Hot tier **SHALL** be unchanged. The attempt reports failure; no partial inserts survive.
4. On full success: call **`commit()`**.
5. **All-or-nothing:** either the entire non-duplicate insert set commits, or none of it does. Partial commits are forbidden.
6. When `matched == 0`: success no-op; hot unchanged (begin+commit with zero inserts is compliant).
7. Failure-injection tests **SHALL** prove rollback leaves hot unchanged (same discipline as M1.5/M3 consolidation failure injection).

Replay **SHALL NOT** open a write transaction.

---

## Normative — Public types

Illustrative names; implementation may place them in `restore_api.h` or extend consolidation headers without changing M3 frozen consolidation types.

```cpp
enum class RestoreMode { REPLAY, REHYDRATE };

struct RestoreRange {
    std::optional<int64_t> start_ms;
    std::optional<int64_t> end_ms;
};

struct RestoreRequest {
    RestoreMode mode = RestoreMode::REPLAY;
    RestoreRange range;                 // both empty => full session
    bool allow_during_goal = false;     // rehydrate only
    std::string requested_by;           // CLI | TEST | ...
};

struct RestoreResult {
    RestoreMode mode = RestoreMode::REPLAY;
    int matched = 0;       // cold turns in range
    int restored = 0;      // newly inserted into hot (rehydrate only; else 0)
    int skipped_dup = 0;   // rehydrate only; else 0
    bool blocked = false;
    std::string block_reason;
    std::vector<MemoryRepository::ArchivedTurnRecord> turns;
    // REPLAY: always the matched ordered set
    // REHYDRATE: empty (counts only)
};
```

---

## Normative — API surface

| API | Contract |
|-----|----------|
| `MemoryRepository` ranged cold query | Filter by `session_id` + `RestoreRange`; stable order |
| `MemoryRepository` rehydrate | Single transaction; duplicate invariant; rollback on failure; cold untouched |
| `MemoryPruner::restore(sessionId, RestoreRequest)` | Orchestrates mode, validation, guards, DecisionTrace |
| `Memory::runRestore(sessionId, RestoreRequest)` | Facade; empty session → active session; goal-active guard for rehydrate (same pattern as `runConsolidation`) |
| **Legacy** `MemoryPruner::restore(sessionId)` | Full-session **REPLAY** alias ≡ `RestoreRequest{REPLAY, empty range}`; returns `std::vector<ArchivedTurnRecord>`; **SHALL NOT** emit DecisionTrace |
| **Legacy** `Memory::getArchivedTurns()` | Calls legacy `restore(activeSessionId)`; silent full-session replay |

Legacy APIs **SHALL** preserve return type and **SHALL NOT** become rehydrate.

---

## Normative — Guards

| Condition | Replay | Rehydrate |
|-----------|--------|-----------|
| Invalid range (`start_ms > end_ms`) | blocked | blocked |
| Goal active and `allow_during_goal == false` | allowed | blocked (M3-style reason; `--unsafe` sets allow) |
| Goal active and allow / `--unsafe` | allowed | allowed |
| Pruner/repo unavailable | empty or blocked per facade | blocked |

---

## Normative — Consolidation interaction

- M4 **SHALL NOT** alter consolidation policy, thresholds, batch size, backoff, or M3 frozen types (`ConsolidationStatus`, `ConsolidationRequest`, `ConsolidationResult`, `ConsolidationDecision`).
- M4 **SHALL NOT** call `runConsolidation`, `consolidateIfNeeded`, or `evaluatePolicy` as part of restore.
- M4 **SHALL NOT** clear stale-session marks or reset consolidation backoff.
- After rehydrate, hot count may exceed `max_hot_messages`. Subsequent automatic or manual consolidation **SHALL** use existing M1–M3 policy unchanged. That interaction is expected; M4 does not suppress it.
- CLI **SHALL NOT** require a “may trigger consolidation” warning line.

---

## Normative — DecisionTrace

| Invocation | Top-level event name |
|------------|----------------------|
| `runRestore` / CLI **replay** | `memory_restore_replay` |
| `runRestore` / CLI **rehydrate** | `memory_restore_rehydrate` |
| Legacy `restore(sessionId)` / `getArchivedTurns()` | *(none)* |

Replay and rehydrate **SHALL NOT** share a single `memory_restore` top-level event name.

CLI **MAY** also wrap the command in `admin_command` stages (as `/prune` does today) **in addition to** the mode-specific memory event.

Minimum stage payload fields: `session_id`, `requested_by`, range bounds (`start_ms` / `end_ms` or open), `matched`; rehydrate also `restored`, `skipped_dup`, `blocked`.

---

## Normative — CLI

```text
/prune restore [--rehydrate] [--unsafe]
       [--start <ms>] [--end <ms>] [session]
```

| Flag / arg | Meaning |
|------------|---------|
| (default) | Replay |
| `--rehydrate` | Rehydrate mode |
| `--unsafe` | `allow_during_goal = true` (rehydrate); ignored for replay semantics |
| `--start` / `--end` | Inclusive timestamp bounds (milliseconds) |
| `[session]` | Target session; omitted → active session |

Help text **SHALL** list `restore` alongside `status`, `explain`, `batch`, `run`.

`/memory restore` is **not** part of M4 (M3 `/memory` stub remains deferred).

### Output constants

| Constant | Value |
|----------|-------|
| Replay preview max lines | 20 |
| Replay content max chars (single line) | 120 |
| Truncation suffix | `…` |

### Output formats

**Replay success:**
```text
[Restore replay] session=<id> matched=<n> start=<ms|*> end=<ms|*>
  <original_timestamp_ms> <role> <content_truncated>
```
If `matched > 20`, trailing line: `… and <k> more`.

**Rehydrate success:**
```text
[Restore rehydrate] session=<id> matched=<n> restored=<n> skipped_dup=<n> start=<ms|*> end=<ms|*>
```
No turn body dump.

**Blocked:**
```text
[Restore blocked] <block_reason>
```

**Rehydrate failure (rolled back):**
```text
[Restore rehydrate failed] session=<id> reason=<short> (hot unchanged)
```

---

## Normative — Tests

All under `THOTH_MOCK_EPISODIC=1` unless otherwise noted.

| ID | Requirement |
|----|-------------|
| M4-01 | Legacy `restore(sessionId)` ≡ full-session empty-range replay (order + content) |
| M4-02 | Ranged replay returns only in-bounds turns; stable order |
| M4-03 | Open-ended start-only and end-only ranges |
| M4-04 | Replay leaves hot/cold/warm row counts unchanged |
| M4-05 | Rehydrate inserts non-dups; cold count unchanged; hot increases by `restored` |
| M4-06 | Second rehydrate same range: `restored == 0`, `skipped_dup == matched` |
| M4-07 | Goal-active blocks rehydrate; `allow_during_goal` / `--unsafe` allows |
| M4-08 | Invalid range blocked; no writes |
| M4-09 | Injected mid-transaction failure → rollback; hot unchanged |
| M4-10 | Replay and rehydrate emit distinct DecisionTrace top-level names |

---

## Normative — Forbidden

- Expanding M4 to warm-id, semantic, or non-timestamp restore modes without a new protocol version
- Deleting cold archives as part of rehydrate
- Altering M3 consolidation contracts or policy evaluation inside restore
- Emitting DecisionTrace from legacy `restore(sessionId)` / `getArchivedTurns()`
- GUI, async restore, F-series work under this checkpoint
- Silent protocol edits after lock

---

## Informative — Owner lock decisions (2026-07-18)

Resolved at lock from refined draft defaults:

1. Duplicate identity = `(timestamp_ms, role, content)`  
2. CLI preview caps = 20 lines / 120 chars  
3. CLI home = `/prune restore`  
4. Legacy restore remains silent (no DecisionTrace)  
5. Rehydrate `RestoreResult::turns` empty on success  

---

## Informative — Implementation sequence

1. Repository ranged query (stable order) + rehydrate transaction / duplicate / rollback  
2. `RestoreRequest` / `RestoreResult` + `MemoryPruner::restore(session, request)`; preserve legacy alias  
3. `Memory::runRestore` + rehydrate goal-active guard  
4. Distinct DecisionTrace events  
5. `/prune restore` CLI + normative output; help text  
6. Unit tests M4-01–M4-10  
7. Build + test gate  
8. Update `memory_architecture.md`, `improvements.md`, `cursor_list.md`, `completed_improvements_log.md` for **implementation** complete (separate from this lock)

**Expected touch set:** `memory_repository.h`, `sqlite_memory_repository.*`, `memory_pruner.*`, `memory.*`, `command_processor.*`, new or extended restore API header, `tests/unit_tests.cpp`, docs listed above.

---

## Informative — Doc map

| Need | File |
|------|------|
| **This protocol (normative)** | `M4_PROTOCOL.md` |
| Memory stack context | `memory_architecture.md` |
| Roadmap status | `improvements.md`, `cursor_list.md` |
| M3 operational precedent | `memory_architecture.md` § M3; `completed_improvements_log.md` 2026-06-30 |

---

*Locked 2026-07-18 — M4 v1.0. Implemented 2026-07-18. Normative rules remain locked; do not amend without owner approval.*
