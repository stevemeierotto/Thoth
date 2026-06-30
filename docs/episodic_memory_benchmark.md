# Episodic Memory Benchmark (M1.5 Verification)

**Purpose:** Prove M1 consolidation + warm retrieval works end-to-end before declaring **M1 verified**.  
**Architecture:** [`memory_architecture.md`](memory_architecture.md)  
**Implementation status:** M1 **verified** (2026-06-26); M1.5 **complete**

---

## Gate criteria

M1 may be marked **verified** only when all M1.5 items pass:

| ID | Deliverable | Verification |
|----|-------------|--------------|
| **M1.5a** | End-to-end episodic retrieval | Fact in pruned turns → warm → GRAG merge → retrievable by query |
| **M1.5b** | Consolidation failure injection | Embed / DB failure → hot unchanged, no partial warm/archive |
| **M1.5c** | Latency instrumentation | `summary_ms`, `embed_ms`, `transaction_ms`, `consolidation_ms` in consolidation trace |
| **M1.5d** | Episodic memory benchmark | Positive + **negative** retrieval cases in unit tests |

**After M1.5 passes:** update `cursor_list.md` M1 → **verified**; proceed to **M2**. ✅ Done 2026-06-26.

---

## M1.5a — End-to-end flow (Apollo case)

```
Turn 1: "My dog's name is Apollo."
… +58 filler turns (hot cap 50, batch 10)
→ consolidation
→ warm_memory + embedding
→ hot tier no longer contains Apollo turn
Query: "What is my dog's name?"
→ searchWarmMemory / GRAG retrieveRelevant
→ warm_memory chunk contains "Apollo"
```

Automated: `testEpisodicRetrievalEndToEnd` in `unit_tests.cpp` (`THOTH_MOCK_EPISODIC=1`).

---

## M1.5b — Failure injection

| Env | Effect | Expected |
|-----|--------|----------|
| `THOTH_MOCK_EMBED_EMPTY=1` | Embed returns empty | Hot count unchanged; no warm/archive |
| `THOTH_INJECT_CONSOLIDATION_FAIL=commit` | Rollback before COMMIT | Hot unchanged; no warm rows |

Automated: `testConsolidationFailureEmbed`, `testConsolidationFailureTransaction`.

---

## M1.5c — Latency fields

Logged on `memory_consolidation` trace stage `consolidation_committed`:

- `summary_ms` — SummaryGenerator (LLM or mock)
- `embed_ms` — canonical embed text → vector
- `transaction_ms` — `consolidateSessionBatch`
- `consolidation_ms` — total wall time for one batch

Automated: `testConsolidationLatencyRecorded`.

---

## M1.5d — Benchmark cases

| Case | Type | Setup | Query | Pass if |
|------|------|-------|-------|---------|
| **EPI-01** | Positive | Apollo in turn 1, consolidate | "What is my dog's name?" | Top warm / GRAG chunk mentions Apollo |
| **EPI-02** | Negative | Apollo session consolidated | "What is the capital of France?" | Warm rows contain no unrelated facts (e.g. Paris); under-cap session creates no warm |
| **EPI-03** | Positive | User preference in pruned batch | Preference-specific query | Warm `user_preferences` or render contains fact |
| **EPI-04** | Negative | Empty / unrelated session | Any query | No warm rows or scores near zero |

Automated: `testEpisodicMemoryBenchmarkNegative` (EPI-01–EPI-02 minimum); extend as mock/LLM harness matures.

---

## Running

```bash
cmake --build build/debug --target thoth-unit-tests
THOTH_MOCK_EPISODIC=1 ./build/debug/tests/thoth-unit-tests
```

Look for episodic / consolidation test failures in output.

---

*Last updated: 2026-06-26 (M1.5 complete; M1 verified)*
