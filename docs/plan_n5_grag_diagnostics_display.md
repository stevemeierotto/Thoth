# Plan N5 — GRAG Diagnostics Display Honesty

**Status:** ✅ **Implemented** — 2026-07-18  
**Prerequisites:** Plan N (N1–N6) ✅ Complete · Investigation: unbounded `final_score` mislabeled as `%`; α / ‖G−C‖ = 0 expected without goal embeddings  
**Roadmap:** GUI presentation honesty only  
**Out of Plan N core:** This was deferred from Plan N L11; it is a **separate** locked plan

---

## Purpose

Make the GRAG Diagnostics panel honest about what values mean:

1. Chunk **`final_score`** is an unbounded hybrid **rank**, not a probability — never display it as `%`.
2. **Alpha and directional metrics** should clearly indicate when conversational retrieval does **not** use a goal embedding (so zeros are not read as “broken”).

---

## Locked decisions

| ID | Decision |
|----|----------|
| **D1** | Chunk list column title: **`Final Score`** (matches internal `final_score`; do not invent “Rank” terminology). |
| **D2** | Format values as plain floats (e.g. `1.18`, `0.42`, `2.03`) — **no `%` suffix**, no ×100 scaling for display. |
| **D3** | When conversational retrieval does not use a goal embedding: show **Alpha** as `N/A` or `N/A (chat)` — **not** `0.00` as if a live directional blend failed. |
| **D4** | Keep **Magnitude ‖G−C‖** numeric (e.g. `0.000`). Interpretation comes from a separate **Mode** (or equivalent) label such as `Conversational` — do not hide the magnitude value or append long hint prose to the number. |
| **D5** | **Presentation only.** No engine / `libbasic_agent` scoring changes. **No engine payload / JSON schema changes** unless separately approved. |
| **D6** | Do **not** add: color coding, bars, new gauges, normalization, confidence, probability, or score scaling. (Existing alpha gauge may be left inert / zeroed when Alpha is N/A; do not expand gauge UX.) |

---

## In scope

| Item | File(s) |
|------|---------|
| `FormatScoreLabel` → float, no `%` | `src/GragDiagnosticsPanel.cpp` |
| Column rename `Score` → `Final Score` | same |
| Alpha N/A when no goal direction; Mode (e.g. Conversational) for interpretation | same |
| Magnitude value unchanged | same |
| Tests N5-T1–T5 | `tests/` (prefer pure format helpers if extracted) |
| Docs closeout | this file + `docs/completed_improvements_log.md` |

Follow `docs/architectural_facts.md` §8 — in-place panel edits only; do not break sidebar `AddCollapsiblePane` patterns.

---

## Out of scope

| Item | Why |
|------|-----|
| GRAG math, boosts, grounding floor | Plan M / GRAG |
| Chat generation / sanitize / stops | Plan N N1–N6 done |
| Engine event payload fields | D5 — separately approved only |
| History scrub / N1b wording | Other deferred items |
| Color / bars / gauges / normalization / confidence / probability | Explicitly excluded (D6) |

---

## Behavior matrix (locked)

| Context | Final Score column | Alpha | Magnitude | Mode (or equiv.) |
|---------|--------------------|-------|-----------|------------------|
| Conversational / no goal embedding | `1.18`-style float, no `%` | `N/A` or `N/A (chat)` | `0.000` (keep number) | `Conversational` (or from existing `scoring_type`) |
| Goal / directional retrieval with real α | same float format | `Alpha: 0.xx` (existing style OK) | numeric as today | Existing scoring type / goal mode |

Exact Alpha string (`N/A` vs `N/A (chat)`) may be chosen at implement time as long as it does **not** present a bare `0.00` that implies failure.

---

## Tests (locked)

| ID | Case | Expect |
|----|------|--------|
| **N5-T1** | Input score `1.18` | Output contains `"1.18"`; does **not** contain `%` |
| **N5-T2** | Input score `0.42` | Output contains `"0.42"`; does **not** contain `%` |
| **N5-T3** | Non-finite / missing | `N/A` |
| **N5-T4** | Conversational / no goal direction presentation | Alpha shown as N/A (not bare `0.00` failure); magnitude still numeric; mode indicates conversational / no goal embedding |
| **N5-T5** | Unknown scoring mode | Fallback formatting still works (Final Score float rules; panel does not crash / invent `%`) |

---

## Verify

1. Build GUI + tests green (N5-T1–T5).  
2. Grep: no `* 100` / `"%"` path left in `FormatScoreLabel` (or successor).  
3. No edits under `external/basic_agent/` for N5.  
4. Live smoke (host GUI): boosted chunk shows `>1` without `%`; plain chat shows Alpha N/A + Mode conversational with magnitude still visible.

---

## Explicit non-goals

- N5 does not recalibrate retrieval relevance.
- N5 does not make α non-zero in plain chat.
- N5 does not claim `final_score` is a probability.

---

## Protocol lock

This document is **locked**. Do not silently revise contracts during implementation. If implementation forces a contract change, stop and request approval (AGENTS.md Protocol Lock Rule).

STATUS: N5 COMPLETE — IMPLEMENTED 2026-07-18
