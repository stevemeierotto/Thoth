# EGAR-LX R3 Exploratory-Freeze Checklist v1.2.1

Date: 2026-11-17  
Scope: Exploratory freeze of the eight-case diagnostic set (LX-03,08,09,10,13,16,18,20).  
Independent human validation: **not claimed / not performed**.

## A. Sealed-answer isolation (required before any Thoth-facing use)

- [ ] Sealed workbook stays **outside** `agent_workspace/`, attachments, and any path used as `THOTH_WORKSPACE_PATH`.
- [ ] Do **not** copy `EGAR_R3_Sealed_Adjudication_Key_v1.2.1.xlsx` (or provenance extracts) into `agent_workspace/rag/`.
- [ ] Do **not** attach sealed sheets via the GUI knowledge-base / ALP attachment path.
- [ ] Only sanitized diagnostic packets (or exports derived solely from `EGAR_R3_Diagnostic_Visible_v1.2.1.xlsx`) may enter fixed-evidence / evaluation inputs.
- [ ] Confirm IndexManager sandbox still rejects paths outside `agent_workspace` (existing hard reject); treat workbook folder separation as **insufficient alone**.
- [ ] After staging, spot-check that no sealed phrases (fix commits, DNS priority −50, CLONE_NEWNS, etc.) appear in workspace RAG listings.

## B. Source-rights limitations (unresolved limitation; not an exploratory-run code blocker)

- [ ] Acknowledge private-research paraphrase status; redistribution/licensing clearance **pending** for all cases.
- [ ] Do not publish, redistribute, or index sealed/provenance URLs into a shared retrieval corpus.

## C. Conditional case findings (documented; scoring unchanged)

| Case | Status | Open item |
|------|--------|-----------|
| LX-03 | **PASS** (chronology verified 2026-11-17) | Corpus rights/review only |
| LX-16 | **PASS CONDITIONAL** | Exact packaged release / reporter retest unset |
| LX-20 | **PASS CONDITIONAL** | Original-reporter patched retest not located |
| LX-19 | **HOLD** | Excluded from diagnostic workbook |

## D. Systemd concentration

- [ ] Record that **3 of 8** visible cases are systemd regressions: LX-13, LX-18, LX-20.
- [ ] Accept this for exploratory freeze; do not treat the set as family-diversity complete.

## E. Independent-review limitations

- [ ] No external Linux-qualified fairness/admission sign-off has been performed or claimed.
- [ ] Formal admission and acquisition/evaluation arm assignment remain **out of scope** for this freeze.

## F. Artifact integrity

- [ ] Verify SHA-256 via `EGAR_R3_SHA256SUMS_v1.2.1.txt`.
- [ ] Keep v1.2 originals / `PRESERVED_*` copies unmodified as lineage.

## Exploratory-run blockers vs limitations

**Actual blockers (must clear before running Thoth on these cases):**
1. Isolation checklist A incomplete (risk of sealed ingest).
2. Using sealed workbook or provenance URLs as agent-visible input.

**Unresolved limitations (do not by themselves NO-GO an exploratory run if A is satisfied):**
1. Source-rights clearance pending.
2. LX-16 / LX-20 conditional verification gaps.
3. Systemd concentration 3/8.
4. No independent human validation.
5. Not formally admitted; no arm assignment.
