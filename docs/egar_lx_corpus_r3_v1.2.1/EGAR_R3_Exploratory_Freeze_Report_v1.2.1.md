# EGAR-LX R3 Exploratory-Freeze Report v1.2.1

Date: 2026-11-17  
Recommendation: **CONDITIONAL GO** for exploratory freeze  
Independent human validation: **not claimed / not performed**

## What changed in v1.2.1 (documentation/provenance only)

1. **LX-03 chronology re-verified** against primary Debian sources and upgraded from CONDITIONAL→PASS for that issue.
2. **LX-19 provenance** wording updated from “Diagnostic Visible v1.1” to “v1.2 / v1.2.1”.
3. Exploratory-freeze checklist and checksums added.
4. **No changes** to visible diagnostic evidence fields, sealed root-cause mechanisms, or scoring criteria beyond chronology/status documentation.

## LX-03 primary-source verification

| Fact | Verified value | Citation |
|------|----------------|----------|
| Report date | 2020-10-22 | Debian BTS #972688 |
| Found in | network-manager 1.27.91-1 | BTS header / control |
| Fixed in | network-manager **1.27.91-2** | BTS “Fixed in version” |
| Changelog date | **Wed, 25 Nov 2020 14:59:00 +0100** | Attached 1.27.91-2 changelog on BTS close |
| Close mail | Wed, 25 Nov 2020 14:44:28 +0000 | ftpmaster close to 972688-close |
| Change | Revert "dns: change default DNS priority of VPNs to -50" (Closes: #972688) | Same changelog |
| Primary URL | https://bugs.debian.org/cgi-bin/bugreport.cgi?bug=972688 | inspected 2026-11-17 |
| Upstream forward | https://gitlab.freedesktop.org/NetworkManager/NetworkManager/-/issues/563 | BTS forwarded-to |

## Case board (v1.2.1)

| Case | Decision |
|------|----------|
| LX-03 | PASS |
| LX-08 | PASS |
| LX-09 | PASS |
| LX-10 | PASS |
| LX-13 | PASS |
| LX-16 | PASS CONDITIONAL |
| LX-18 | PASS |
| LX-19 | HOLD (excluded from diagnostic) |
| LX-20 | PASS CONDITIONAL |

## Checksums (SHA-256)

- Diagnostic: `28fa7c18dd3e8f6f3cc39bcc89a84a767846a28e2a89431885e56b6c81651dd2`
- Sealed: `e282a70eba231cc2e09e4c4e07b62ae35029af592e5329b2e72f3a01cce76d0b`

See `EGAR_R3_SHA256SUMS_v1.2.1.txt`.

## GO / CONDITIONAL GO / NO-GO

**CONDITIONAL GO** for an exploratory freeze.

- **Not a NO-GO:** LX-16/LX-20 conditionals, systemd concentration, pending rights, and lack of independent validation are **limitations**, not hard blockers to a carefully isolated exploratory run.
- **Hard blockers if present:** sealed material in Thoth workspace/RAG/attachments; using provenance URLs as agent-visible oracles; treating the set as formally admitted or arm-assigned.

## Constraints honored

- v1.2 files preserved (`PRESERVED_*` copies; original `docs/egar_lx_corpus_r3_v1.2/` left in place)
- No Thoth code/protocol changes
- No ingest, Thoth run, arm assignment, or git commit
