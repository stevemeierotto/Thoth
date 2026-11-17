# EGAR-LX R3 Corpus Repair Report v1.1

Date: 2026-11-16  
Action: Focused fairness repair of eight-case review corpus for seven-case exploratory pilot prep.  
Independent human validation: **not claimed / not performed**.

## Preserved originals
- Downloads originals left unmodified:
  - `EGAR_20R3_20-_20Blind_20Visible_20Review.xlsx`
  - `EGAR R3 - Sealed Adjudication Key.xlsx`
- Workspace preservation copies:
  - `egar_lx_corpus_r3_v1.1/ORIGINAL_EGAR_R3_Blind_Visible_Review.xlsx`
  - `egar_lx_corpus_r3_v1.1/ORIGINAL_EGAR_R3_Sealed_Adjudication_Key.xlsx`

## Deliverables
- Diagnostic (Thoth-facing): `EGAR_R3_Diagnostic_Visible_v1.1.xlsx`
- Sealed/provenance (reviewers only): `EGAR_R3_Sealed_Adjudication_Key_v1.1.xlsx`
- This report: `EGAR_R3_Corpus_Repair_Report_v1.1.md`

Location: `/home/steve/Thoth/docs/egar_lx_corpus_r3_v1.1`

## Case decisions

| Case | Decision | Notes |
|------|----------|-------|
| LX-03 | PASS CONDITIONAL | URLs/IDs removed. Correction version/date linkage **UNVERIFIED** this pass (live BTS fetch not completed). Mechanism-level scoring retained. |
| LX-08 | PASS | Sanitized. Merged-incident cluster + partman linkage remain pre-admission checks. |
| LX-09 | PASS | Sanitized. Upstream commit not required. |
| LX-10 | PASS | Sanitized. Family-level GRUB/LVM metadata scoring OK. |
| LX-13 | PASS | Sanitized. Qualified emulation/systemd hypotheses accepted per sealed rubric. |
| LX-16 | PASS CONDITIONAL | Misleading “no historical answer” header repaired. Closure noted; exact release/retest still limited. |
| LX-18 | PASS | Sanitized. |
| LX-19 | HOLD | Excluded from diagnostic workbook. Historical sealed evidence not modified. |

## Remaining blockers
1. Source-rights clearance pending (all cases).
2. LX-03 packaged correction chronology not re-verified against primary sources in this pass.
3. LX-16 exact corrective release / reporter retest still unset.
4. No independent Linux-qualified human validation.
5. Formal admission and arm assignment intentionally not performed.
6. If Downloads is not writable from the agent environment, copy the v1.1 deliverables there manually.

## Diagnostic workbook leakage verification
Forbidden URL/tracker/sealed-term hits: **0**

No live URLs, tracker hosts/IDs, or checked sealed-answer phrases found in Diagnostic Visible v1.1.

## Sheets in diagnostic workbook
['START HERE', 'Corpus Status', 'LX-03 Visible', 'LX-08 Visible', 'LX-09 Visible', 'LX-10 Visible', 'LX-13 Visible', 'LX-16 Visible', 'LX-18 Visible', 'Reviewer Form']

## Constraints honored
- Originals preserved
- Thoth source/protocol untouched
- Nothing ingested into Thoth
- No acquisition/evaluation arm assignment
- LX-19 historical evidence unmodified; HOLD only
