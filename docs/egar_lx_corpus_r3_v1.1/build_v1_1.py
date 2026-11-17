#!/usr/bin/env python3
"""Build EGAR-LX R3 corpus repair deliverables v1.1 (local workbook repair only)."""
from __future__ import annotations

import re
import shutil
from pathlib import Path

from openpyxl import Workbook, load_workbook

OUT_DIR = Path(__file__).resolve().parent
DOWNLOADS = Path("/home/steve/Downloads")
BLIND = DOWNLOADS / "EGAR_20R3_20-_20Blind_20Visible_20Review.xlsx"
SEALED = DOWNLOADS / "EGAR R3 - Sealed Adjudication Key.xlsx"
# Prefer workspace originals if Downloads unreadable later
if not BLIND.exists():
    BLIND = OUT_DIR / "ORIGINAL_EGAR_R3_Blind_Visible_Review.xlsx"
if not SEALED.exists():
    SEALED = OUT_DIR / "ORIGINAL_EGAR_R3_Sealed_Adjudication_Key.xlsx"

OUT_DIAG = OUT_DIR / "EGAR_R3_Diagnostic_Visible_v1.1.xlsx"
OUT_SEAL = OUT_DIR / "EGAR_R3_Sealed_Adjudication_Key_v1.1.xlsx"
REPORT = OUT_DIR / "EGAR_R3_Corpus_Repair_Report_v1.1.md"

PILOT_CASES = ["LX-03", "LX-08", "LX-09", "LX-10", "LX-13", "LX-16", "LX-18"]
HOLD_CASES = ["LX-19"]

URL_RE = re.compile(r"https?://\S+", re.I)
BUG_URLISH = re.compile(
    r"(?:bugs\.debian\.org|gitlab\.freedesktop\.org|github\.com|"
    r"savannah\.gnu\.org|lists\.debian\.org)\S*",
    re.I,
)
BUG_ID_RE = re.compile(
    r"(?:#\d{3,}|bug(?:report)?\s*#?\s*\d{3,}|"
    r"(?:Debian|BTS|Launchpad)\s*(?:bug\s*)?#?\d{3,}|"
    r"(?:PR|pull request)\s*#?\d{3,})",
    re.I,
)
SLASH_BUG = re.compile(r"(?:bugreport\.cgi\?bug=|/issues/|/pull/|/bugs/\?)\d+", re.I)

SEALED_TERMS = [
    "DNS priority of -50",
    "CLONE_NEWNS",
    "partman-efi version 108",
    "zombie PID",
    "_pid_exists",
    "PR #30074",
    "commit 2a7903b",
    "negative VPN DNS",
    "grub-core/disk/lvm.c",
    "1.27.91-2",
    "acca526",
    "500ec22",
]


def sanitize_text(text: str) -> str:
    if not text:
        return text
    t = URL_RE.sub("[source reference removed]", text)
    t = BUG_URLISH.sub("[source reference removed]", t)
    t = SLASH_BUG.sub("[source id removed]", t)
    t = BUG_ID_RE.sub("[source id removed]", t)
    t = re.sub(r"bugs\.debian\.org/\d+", "[source reference removed]", t, flags=re.I)
    t = re.sub(r"(\[source reference removed\]\s*)+", "[source reference removed]", t)
    t = re.sub(r"(\[source id removed\]\s*)+", "[source id removed]", t)
    return t.strip()


def sheet_pairs(ws):
    rows = []
    for row in ws.iter_rows(values_only=True):
        a = "" if row[0] is None else str(row[0])
        b = "" if len(row) < 2 or row[1] is None else str(row[1])
        extras = [("" if c is None else str(c)) for c in row[2:]] if len(row) > 2 else []
        rows.append((a, b, extras))
    return rows


def write_kv_sheet(wb, title, pairs, header_note=None):
    ws = wb.create_sheet(title)
    ws.column_dimensions["A"].width = 28
    ws.column_dimensions["B"].width = 100
    r = 1
    if header_note:
        ws.cell(r, 1, "Packet note")
        ws.cell(r, 2, header_note)
        r += 1
    for a, b, extras in pairs:
        if not a and not b and not any(extras):
            continue
        ws.cell(r, 1, a)
        ws.cell(r, 2, b)
        for i, e in enumerate(extras):
            ws.cell(r, 3 + i, e)
        r += 1
    return ws


def sanitize_visible_pairs(pairs):
    out = []
    for a, b, extras in pairs:
        field = a.strip().lower()
        if field in {
            "source",
            "source note",
            "sources",
            "original issue",
            "primary report",
        } or (
            "source" in field
            and ("http" in b.lower() or "bugs." in b.lower() or re.search(r"#\d{3,}", b))
        ):
            out.append(
                (
                    a,
                    "Historical public report retained in sealed provenance only. "
                    "Not supplied to the diagnostic agent.",
                    [],
                )
            )
            continue
        out.append((sanitize_text(a), sanitize_text(b), [sanitize_text(e) for e in extras]))
    return out


def finalize_agent_text(text: str) -> str:
    text = text.replace(
        "[source reference removed]",
        "Historical public report retained outside this packet.",
    )
    text = text.replace("[source id removed]", "historical identifier withheld")
    return text


def build():
    blind_wb = load_workbook(BLIND, data_only=True)
    sealed_wb = load_workbook(SEALED, data_only=True)

    diag = Workbook()
    diag.remove(diag.active)
    write_kv_sheet(
        diag,
        "START HERE",
        [
            (a, b, [])
            for a, b in [
                (
                    "Packet",
                    "EGAR-LX R3 Diagnostic Visible v1.1 — exploratory pilot candidates (seven cases).",
                ),
                (
                    "Audience",
                    "Thoth-facing diagnostic input only. Contains no sealed answers, "
                    "source URLs, or tracker identifiers.",
                ),
                (
                    "Derived from",
                    "EGAR_20R3 Blind Visible Review (preserved). Sanitized per fairness-review repair 2026-11-16.",
                ),
                ("Included cases", ", ".join(PILOT_CASES)),
                (
                    "Excluded",
                    "LX-19 retained on HOLD in sealed workbook only; historical evidence unmodified; not exported here.",
                ),
                ("Order", "Diagnose each case independently from its visible sheet only."),
                (
                    "Prohibitions",
                    "Do not browse for case solutions, retrieve historical patches, or request sealed materials.",
                ),
                (
                    "Reconstruction note",
                    "Some cases are researcher-reconstructed symptom-stage packets. "
                    "Do not assume every line was timestamped before the historical diagnosis.",
                ),
                (
                    "Admission",
                    "Researcher-prepared exploratory candidates. Not independently human-validated. "
                    "Not formally admitted. No acquisition/evaluation arm assignment.",
                ),
                ("Rights", "Source-rights clearance remains pending. Private research use only."),
            ]
        ],
    )
    write_kv_sheet(
        diag,
        "Corpus Status",
        [
            (a, b, [])
            for a, b in [
                ("Case", "Repair status after v1.1 sanitization / sealed updates"),
                (
                    "LX-03",
                    "PASS CONDITIONAL — URLs removed; chronology/correction linkage marked UNVERIFIED "
                    "in sealed provenance (live re-check not completed this pass).",
                ),
                (
                    "LX-08",
                    "PASS — sanitized; keep merged-incident cluster in sealed provenance; "
                    "partman package linkage still pre-admission check.",
                ),
                ("LX-09", "PASS — sanitized; upstream commit identity not required for scoring."),
                (
                    "LX-10",
                    "PASS — sanitized; family-level GRUB/LVM metadata scoring remains appropriate.",
                ),
                (
                    "LX-13",
                    "PASS — sanitized; accept qualified emulation/systemd hypotheses per sealed rubric.",
                ),
                (
                    "LX-16",
                    "PASS CONDITIONAL — visible header repaired; historical resolution/scoring limits "
                    "clarified in sealed sheets.",
                ),
                (
                    "LX-18",
                    "PASS — sanitized; keep systemd hibernation incident cluster together in provenance.",
                ),
                (
                    "LX-19",
                    "HOLD — excluded from this diagnostic workbook; sealed historical evidence unchanged.",
                ),
            ]
        ],
    )

    case_header = {
        c: "Sanitized visible diagnostic case. Draft exploratory candidate. NOT independently validated."
        for c in PILOT_CASES
    }
    case_header["LX-16"] = (
        "Sanitized visible diagnostic case with corrected packet header. "
        "Draft exploratory candidate. NOT independently validated."
    )

    for case in PILOT_CASES:
        pairs = sanitize_visible_pairs(sheet_pairs(blind_wb[f"{case} Visible"]))
        fixed = []
        for a, b, extras in pairs:
            al = a.strip().lower()
            if case == "LX-16":
                if al.startswith("egar-lx-16"):
                    a = "EGAR-LX-16 — VISIBLE"
                    b = (
                        "Symptom-stage diagnostic evidence for exploratory review. "
                        "A historical maintainer resolution exists in the sealed key; it is withheld here. "
                        "Researcher-qualified; not independently validated."
                    )
                if al == "status":
                    b = (
                        "RESEARCHER-QUALIFIED exploratory candidate — pending independent review; "
                        "not formally admitted. Visible packet is symptom-stage only."
                    )
                if al == "restrictions":
                    b = (
                        "Do not assume a unique source-code defect can be identified from these observations alone. "
                        "Maintainer diagnosis, commit identity, and exact corrective release are withheld. "
                        "Score reasoned failure-domain diagnosis and discriminating checks; do not require a patch hash."
                    )
                if "no historical answer" in b.lower():
                    b = re.sub(
                        r"no historical answer",
                        "historical resolution withheld in sealed key",
                        b,
                        flags=re.I,
                    )
            if case == "LX-03" and al == "status":
                b = (
                    "Exploratory candidate after URL sanitization; chronology/correction linkage "
                    "verification incomplete in sealed provenance. Not formally admitted."
                )
            if al == "disclosure" or "do not provide the sealed answer" in b.lower():
                b = (
                    "Research reconstruction of observed facts only. "
                    "Sealed materials are not part of this packet."
                )
            fixed.append((a, finalize_agent_text(b), [finalize_agent_text(e) for e in extras]))

        fixed2 = []
        for a, b, extras in fixed:
            if (
                URL_RE.search(b)
                or BUG_URLISH.search(b)
                or re.search(r"bugs\.debian\.org/\d+", b, re.I)
                or "http://" in b.lower()
                or "https://" in b.lower()
            ):
                b = finalize_agent_text(sanitize_text(b))
                if URL_RE.search(b) or "http" in b.lower():
                    b = "Historical public report retained in sealed provenance only."
            fixed2.append((a, b, extras))
        write_kv_sheet(diag, f"{case} Visible", fixed2, header_note=case_header[case])

    rf = sanitize_visible_pairs(sheet_pairs(blind_wb["Reviewer Form"]))
    rf2 = [(a, finalize_agent_text(b), [finalize_agent_text(e) for e in extras]) for a, b, extras in rf]
    write_kv_sheet(
        diag,
        "Reviewer Form",
        rf2,
        header_note="Human reviewer worksheet. Do not paste sealed answers here for Thoth.",
    )
    diag.save(OUT_DIAG)

    seal = Workbook()
    seal.remove(seal.active)
    write_kv_sheet(
        seal,
        "START HERE",
        [
            (a, b, [])
            for a, b in [
                (
                    "Packet",
                    "EGAR-LX R3 Sealed Adjudication Key v1.1 — reviewers/adjudicators only.",
                ),
                (
                    "Derived from",
                    "EGAR R3 - Sealed Adjudication Key (preserved original). Updated 2026-11-16 for fairness repair.",
                ),
                (
                    "Sequence",
                    "Collect locked visible-only assessments first; then use this key. "
                    "Never distribute with the diagnostic visible packet.",
                ),
                (
                    "Thoth",
                    "NEVER send this workbook, any sealed/provenance sheet, or source URLs from it "
                    "to Thoth or its retrieval index.",
                ),
                (
                    "LX-19",
                    "HOLD — historical sealed evidence preserved unchanged; not included in Diagnostic Visible v1.1.",
                ),
                (
                    "Admission",
                    "No formal admission. No acquisition/evaluation arm assignment. "
                    "No claim of independent human validation.",
                ),
                ("Rights", "Source-rights clearance remains pending."),
            ]
        ],
    )

    for case in PILOT_CASES + HOLD_CASES:
        for suffix in ("Sealed", "Provenance"):
            name = f"{case} {suffix}"
            pairs = sheet_pairs(sealed_wb[name])
            updated = []
            for a, b, extras in pairs:
                al = a.strip().lower()
                if case == "LX-03" and suffix == "Sealed":
                    if al == "historical correction":
                        b = (
                            "DRAFT CLAIM (UNVERIFIED THIS PASS): Debian network-manager 1.27.91-2 reportedly reverted "
                            "the VPN default negative DNS-priority change; recorded fix date previously stated as 2020-11-25. "
                            "Live BTS/changelog re-verification was not completed during the v1.1 repair. "
                            "Do not treat the version/date linkage as independently confirmed until verified against primary sources."
                        )
                    if al == "status":
                        b = (
                            "Sealed draft v1.1: root-cause narrative retained from prior research; "
                            "correction version/date linkage marked UNVERIFIED; licensing/chronology checks "
                            "still required before admission."
                        )
                    if al == "scoring status":
                        b = (
                            "Provisional guidance only; use protocol external diagnosis 0–2 and remedy 0–2 rubric. "
                            "Do not require naming the exact package version if linkage remains unverified; "
                            "credit mechanism-level DNS-priority exclusion reasoning."
                        )
                if case == "LX-03" and suffix == "Provenance":
                    if al == "recorded correction":
                        b = (
                            "PREVIOUSLY STATED: 2020-11-25, network-manager 1.27.91-2. "
                            "VERIFICATION STATUS (v1.1): INCONCLUSIVE — live primary-source re-check was not completed "
                            "in this repair pass. Verify against Debian BTS and package changelog before admission. "
                            "Do not invent confirmatory evidence."
                        )
                    if al == "source verification":
                        b = (
                            "Source-backed draft from prior R2/R3 research. Detailed upstream chronology and licensing "
                            "were not independently re-verified in v1.1. Limitation documented rather than fabricating confirmation."
                        )
                    if al == "status":
                        b = (
                            "User-approved recording lineage preserved; v1.1 documents UNVERIFIED correction linkage. "
                            "Not formally admitted. Keep sealed separate from diagnostic export."
                        )
                    if al == "leakage risk":
                        b = (
                            "Original report mentions downgrade; never import report text or this sealed sheet into Thoth. "
                            "Diagnostic Visible v1.1 removes live URLs and tracker IDs from the agent-facing packet."
                        )
                if case == "LX-16" and suffix == "Sealed":
                    if al.startswith("egar-lx-16"):
                        b = (
                            "REVIEWERS ONLY — do not provide to Thoth. Historical resolution exists and is summarized below; "
                            "exact release/retest limits apply."
                        )
                    if al == "important limit":
                        b = (
                            "Issue was marked fixed/closed in the upstream tracker (closure dated 2024-02-29 in prior research notes). "
                            "Exact final packaged release and independent post-fix reporter retest are NOT established in this verification. "
                            "Multiple related socket-inheritance/reload fixes must not be collapsed into a single patch identity. "
                            "Do not require a commit hash for full diagnosis credit."
                        )
                    if al == "status":
                        b = (
                            "SEALED v1.1 — researcher-qualified; header/scoring limits clarified; "
                            "independent review pending; not admitted."
                        )
                    if al == "scoring target":
                        b = (
                            "Strong: distinguishes expected address-in-use error from persistent missing old-process UNIX socket; "
                            "identifies failed socket inheritance/path restoration; suggests checking socket existence, -x usage, "
                            "old process health, version comparison, and controlled restart. "
                            "Do not require exact commit or release string from visible evidence."
                        )
                if case == "LX-16" and suffix == "Provenance":
                    if al == "fairness — sufficiency":
                        b = (
                            "PASS for ranked investigation and likely failure domain after v1.1 visible-header repair; "
                            "not sufficient for demanding a precise source patch or release from the visible packet."
                        )
                    if al == "disposition":
                        b = (
                            "RESEARCHER-QUALIFIED v1.1 (visible header corrected; scoring limits explicit); "
                            "not independently validated or formally admitted."
                        )
                    if al.startswith("lx-16"):
                        b = "Researcher desk review updated 2026-11-16 (v1.1 fairness repair)."
                if case == "LX-19" and suffix == "Provenance" and al == "disposition":
                    b = (
                        "HOLD for exploratory pilot export (v1.1). Historical sealed evidence preserved without modification. "
                        "Excluded from Diagnostic Visible v1.1 due to reconstructed intra-report cutoff and pending independent validation. "
                        "Not formally admitted. Researcher-conditional candidate lineage retained."
                    )
                if case == "LX-19" and suffix == "Provenance" and al == "independent validation":
                    b = (
                        "Not performed. External Linux-qualified reviewer must validate visible case and sealed scoring. "
                        "v1.1 does not claim such validation."
                    )
                updated.append((a, b, extras))

            if case == "LX-03" and suffix == "Provenance":
                updated.append(
                    (
                        "v1.1 verification attempt",
                        "Live Debian BTS / upstream issue fetch for chronology confirmation was not completed "
                        "in this repair environment. Limitation recorded. No confirmatory changelog excerpt invented.",
                        [],
                    )
                )
                updated.append(
                    (
                        "v1.1 diagnostic export",
                        "Agent-facing packet EGAR_R3_Diagnostic_Visible_v1.1.xlsx contains LX-03 without URLs or tracker IDs.",
                        [],
                    )
                )
            if case == "LX-16" and suffix == "Provenance":
                updated.append(
                    (
                        "v1.1 visible-header repair",
                        'Removed misleading “no historical answer” language from the diagnostic export. '
                        "Clarified that a historical maintainer resolution exists but remains sealed, with release/retest limits.",
                        [],
                    )
                )
            if case == "LX-19" and suffix == "Sealed":
                updated.insert(
                    1,
                    (
                        "Pilot disposition (v1.1)",
                        "HOLD — do not export to Thoth diagnostic workbook. "
                        "Historical answer fields below are unchanged from the prior sealed draft.",
                        [],
                    ),
                )
            if case == "LX-19" and suffix == "Provenance":
                updated.append(
                    (
                        "v1.1 hold action",
                        "No modification to historical diagnosis, component, fix identity, or original evidence boundary text "
                        "beyond disposition/hold annotations. Excluded from seven-case exploratory diagnostic export.",
                        [],
                    )
                )

            note = (
                "HOLD — historical evidence retained; not part of Diagnostic Visible v1.1."
                if case == "LX-19"
                else None
            )
            write_kv_sheet(seal, name, updated, header_note=note)

    write_kv_sheet(
        seal,
        "Adjudication Form",
        sheet_pairs(sealed_wb["Adjudication Form"]),
        header_note="Human adjudicator only. v1.1 — no independent validation claimed.",
    )
    write_kv_sheet(
        seal,
        "Corpus Decisions v1.1",
        [
            (a, b, [])
            for a, b in [
                ("Case", "v1.1 decision"),
                (
                    "LX-03",
                    "PASS CONDITIONAL — sanitized for pilot; correction chronology UNVERIFIED "
                    "(blocker for formal admission, not for exploratory use if mechanism-level scoring used).",
                ),
                (
                    "LX-08",
                    "PASS — sanitized; pre-admission checks remain (partman-efi linkage; merged-bug cluster).",
                ),
                ("LX-09", "PASS — sanitized."),
                ("LX-10", "PASS — sanitized."),
                ("LX-13", "PASS — sanitized."),
                (
                    "LX-16",
                    "PASS CONDITIONAL — header/scoring limits repaired; exact release/retest still open.",
                ),
                ("LX-18", "PASS — sanitized."),
                (
                    "LX-19",
                    "HOLD — excluded from diagnostic workbook; sealed evidence unmodified.",
                ),
                (
                    "Corpus blocker",
                    "Source-rights clearance pending for all cases. No independent human validation performed or claimed.",
                ),
                (
                    "Oracle blocker mitigated",
                    "Live URLs and tracker IDs removed from Diagnostic Visible v1.1.",
                ),
            ]
        ],
    )
    seal.save(OUT_SEAL)

    diag_check = load_workbook(OUT_DIAG, data_only=True)
    forbidden_hits = []
    for name in diag_check.sheetnames:
        for row in diag_check[name].iter_rows(values_only=True):
            for c in row:
                if c is None:
                    continue
                s = str(c)
                sl = s.lower()
                if URL_RE.search(s) or BUG_URLISH.search(s) or re.search(r"bugs\.debian\.org/\d+", s, re.I):
                    forbidden_hits.append((name, "url/tracker", s[:220]))
                if re.search(r"#\d{4,}", s):
                    forbidden_hits.append((name, "hash-id", s[:220]))
                for host in [
                    "bugs.debian.org",
                    "github.com",
                    "gitlab.freedesktop.org",
                    "savannah.gnu.org",
                    "lists.debian.org",
                ]:
                    if host in sl:
                        forbidden_hits.append((name, "hostname", s[:220]))
                for needle in SEALED_TERMS:
                    if needle.lower() in sl:
                        forbidden_hits.append((name, f"sealed:{needle}", s[:220]))

    present = [n for n in diag_check.sheetnames if n.endswith("Visible") and n.startswith("LX-")]
    assert "LX-19 Visible" not in diag_check.sheetnames
    assert set(present) == {f"{c} Visible" for c in PILOT_CASES}

    lx16_text = " ".join(
        str(c) for row in diag_check["LX-16 Visible"].iter_rows(values_only=True) for c in row if c
    )
    assert "no historical answer" not in lx16_text.lower()

    # Consistency spot-check: sealed scoring still present for pilot cases
    seal_check = load_workbook(OUT_SEAL, data_only=True)
    for case in PILOT_CASES:
        assert f"{case} Sealed" in seal_check.sheetnames
        assert f"{case} Provenance" in seal_check.sheetnames
    assert "LX-19 Sealed" in seal_check.sheetnames

    report = f"""# EGAR-LX R3 Corpus Repair Report v1.1

Date: 2026-11-16  
Action: Focused fairness repair of eight-case review corpus for seven-case exploratory pilot prep.  
Independent human validation: **not claimed / not performed**.

## Preserved originals
- Downloads originals left unmodified:
  - `EGAR_20R3_20-_20Blind_20Visible_20Review.xlsx`
  - `EGAR R3 - Sealed Adjudication Key.xlsx`
- Workspace preservation copies:
  - `{OUT_DIR.name}/ORIGINAL_EGAR_R3_Blind_Visible_Review.xlsx`
  - `{OUT_DIR.name}/ORIGINAL_EGAR_R3_Sealed_Adjudication_Key.xlsx`

## Deliverables
- Diagnostic (Thoth-facing): `{OUT_DIAG.name}`
- Sealed/provenance (reviewers only): `{OUT_SEAL.name}`
- This report: `{REPORT.name}`

Location: `{OUT_DIR}`

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
Forbidden URL/tracker/sealed-term hits: **{len(forbidden_hits)}**
"""
    if forbidden_hits:
        report += "\n### Hits\n"
        for h in forbidden_hits:
            report += f"- {h}\n"
    else:
        report += (
            "\nNo live URLs, tracker hosts/IDs, or checked sealed-answer phrases found in "
            "Diagnostic Visible v1.1.\n"
        )

    report += f"""
## Sheets in diagnostic workbook
{diag_check.sheetnames}

## Constraints honored
- Originals preserved
- Thoth source/protocol untouched
- Nothing ingested into Thoth
- No acquisition/evaluation arm assignment
- LX-19 historical evidence unmodified; HOLD only
"""
    REPORT.write_text(report)

    # Best-effort copy into Downloads
    copied = []
    for src in (OUT_DIAG, OUT_SEAL, REPORT):
        dest = DOWNLOADS / src.name
        try:
            shutil.copy2(src, dest)
            copied.append(str(dest))
        except OSError as exc:
            copied.append(f"FAILED {dest.name}: {exc}")

    print(report)
    print("COPIED_TO_DOWNLOADS:", copied)
    print("FORBIDDEN_HITS", len(forbidden_hits))
    for h in forbidden_hits:
        print(" ", h)
    return len(forbidden_hits)


if __name__ == "__main__":
    raise SystemExit(build())
