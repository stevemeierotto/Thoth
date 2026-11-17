# EGAR-LX R3 — LX-20 Candidate Report (v1.2)

Date: 2026-11-17  
Action: Source a strong historical Linux failure to replace HOLD case LX-19; produce versioned v1.2 corpus deliverables.  
Independent human validation: **not claimed / not performed**.

## Selected candidate
**LX-20 — systemd 255 PrivateDevices=true + BindPaths=/dev/net/tun mount-namespace regression**

- Primary: https://github.com/systemd/systemd/issues/30372 (2023-12-07)
- Fix: https://github.com/systemd/systemd/pull/30375 merged 2023-12-08 (ae7482b); present in systemd-stable v255.2 (7b818b3)
- Family: service sandboxing / private device namespace / VPN daemon startup
- Overlap: none with LX-03/08/09/10/13/16/18 causal mechanisms (distinct from LX-13 nspawn/qemu generators and LX-18 hibernation checks)

## Why this case
1. Authentic timestamped symptom-only first report precedes same-day maintainer diagnosis.
2. Sufficient evidence for scoring: version regression, unit directives, mount-rootfs errors, 226/NAMESPACE, BindPaths-removed follow-up.
3. Documented upstream correction with test and stable-line presence.
4. Cleaner pre-diagnosis cutoff than several alternatives with stronger retest language but answer-contaminated first posts.

## Verdict: PASS CONDITIONAL
**Supporting evidence:** cutoff integrity PASS; sufficiency PASS; leakage controls PASS in diagnostic export; causal support STRONG at maintainer/code level.  
**Condition:** original-reporter post-fix of a patched/packaged build was not located (reporter requested stable backport only). Rights and independent review remain open.

## Alternatives considered (not selected)
| Candidate | Why not chosen |
|-----------|----------------|
| Debian samba #1014453 | Initial BTS text already states the patch/answer — no clean symptom-only cutoff. |
| Launchpad snapd #2044335 | Has reporter fix confirmation, but specialized snap-update-ns/AppArmor surface; diagnosis arrives much later; narrower scoring generality than LX-20. |
| Debian systemd #1007018 | Thematic resolver overlap with LX-03; initial report already proposes packaging removal as the likely fix. |
| Fedora podman/rpm-ostree #2252000 | Strong bisect/fix path, but Atomic Desktop / newuidmap filecaps niche; maintainer verification rather than original-reporter retest. |

## Deliverables (v1.1 preserved)
Location: `/home/steve/Thoth/docs/egar_lx_corpus_r3_v1.2`

- `EGAR_R3_Diagnostic_Visible_v1.2.xlsx` — Thoth-facing diagnostic (7 retained + LX-20; no LX-19)
- `EGAR_R3_Sealed_Adjudication_Key_v1.2.xlsx` — sealed/provenance including LX-19 HOLD + LX-20
- `EGAR_R3_LX20_Candidate_Report_v1.2.md` — this report

## Leakage verification (diagnostic v1.2)
Forbidden URL/tracker/sealed-term hits: **0**

No live URLs, tracker IDs, or checked sealed phrases found in Diagnostic Visible v1.2.

## Remaining blockers
1. Source-rights clearance pending.
2. No independent Linux-qualified human validation.
3. LX-20 missing original-reporter patched retest.
4. Formal admission / arm assignment intentionally not performed.
5. Prior v1.1 conditional blockers for LX-03 chronology and LX-16 release/retest remain.

## Constraints honored
- v1.1 files unmodified
- LX-19 HOLD preserved
- Thoth code/protocol untouched; nothing ingested; no arm assignment
