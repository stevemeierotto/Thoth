# EGAR-LX R3 Exploratory Pilot Preparation Report v1.2.1

Date: 2026-11-17  
Corpus freeze: **approved for exploratory use** (LX-16 and LX-20 remain conditional)  
Independent human validation: **not claimed / not performed**  
This report does **not** finalize arm assignments, ingest cases, run Thoth, amend the protocol, or modify frozen workbooks.

---

## 1. Checksum verification

Command: `sha256sum -c EGAR_R3_SHA256SUMS_v1.2.1.txt` in `docs/egar_lx_corpus_r3_v1.2.1/`

| Artifact | Manifest SHA-256 | Result |
|----------|------------------|--------|
| `EGAR_R3_Diagnostic_Visible_v1.2.1.xlsx` | `28fa7c18dd3e8f6f3cc39bcc89a84a767846a28e2a89431885e56b6c81651dd2` | **OK** |
| `EGAR_R3_Sealed_Adjudication_Key_v1.2.1.xlsx` | `e282a70eba231cc2e09e4c4e07b62ae35029af592e5329b2e72f3a01cce76d0b` | **OK** |
| `PRESERVED_EGAR_R3_Diagnostic_Visible_v1.2.xlsx` | `085f014bea739df7de4ddb97c69a3e1d1c9223e66e27f167c4de725967697fa9` | **OK** |
| `PRESERVED_EGAR_R3_Sealed_Adjudication_Key_v1.2.xlsx` | `104c28244a5781e983dcacd32683a0d0ab4cf40edb4d95a015f83046fc7710db` | **OK** |

Live re-hash of the two final workbooks matched the manifest byte-for-byte.

**Checksum status: PASS**

---

## 2. Isolation inspection — PASS

### Paths examined

| Path | Finding |
|------|---------|
| `agent_workspace/` | No EGAR R3 workbook filenames |
| `agent_workspace/rag/` | Product docs only (`EGAR.md`, Cognate, architectural facts, etc.). **Not** R3 sealed/diagnostic xlsx |
| `agent_workspace/rag/attachments/` | Empty |
| `agent_workspace/rag/revisions/` | Empty |
| `agent_workspace/rag_attachment_registry.json` | Only product/test_suite markdown owners; no corpus xlsx |
| `agent_workspace/docs/` | Benchmark paper txts only |
| `THOTH_WORKSPACE_PATH` / related env | Unset in this shell |
| Sealed phrase scan under `agent_workspace` (excluding binaries) | No hits for checked sealed terms |
| Corpus location | `docs/egar_lx_corpus_r3_v1.2.1/` (outside `agent_workspace`) |

### Existing protections reused (not extended)

- `IndexManager` hard-rejects index paths outside `agent_workspace` (`isUnderAgentWorkspace`).
- Protocol rule: sealed keys must not be indexed, placed in goals, or stored as experience; acquisition must not quote evaluation sealed diagnoses (`EGAR_LINUX_DIAGNOSTIC_AB_PROTOCOL.md` §4 Answer separation).
- Fixed-evidence containment / synthetic A/B harness already expect disposable work roots rather than the development sealed store.

### Residual operational risks (not current exposure)

Workbook folder separation alone is still insufficient if an operator later copies sealed material into `agent_workspace/rag/`, attaches it in the GUI, or points `THOTH_WORKSPACE_PATH` at a tree that contains the sealed key. Current tree state shows **no such exposure**.

**Isolation status: PASS** (present workspace clean; checklist discipline still required at run time)

---

## 3. Frozen corpus record

| Field | Value |
|-------|-------|
| Version | EGAR-LX R3 **v1.2.1** exploratory freeze |
| Visible cases | LX-03, LX-08, LX-09, LX-10, LX-13, LX-16, LX-18, LX-20 |
| HOLD (sealed only) | LX-19 |
| Diagnostic SHA-256 | `28fa7c18dd3e8f6f3cc39bcc89a84a767846a28e2a89431885e56b6c81651dd2` |
| Sealed SHA-256 | `e282a70eba231cc2e09e4c4e07b62ae35029af592e5329b2e72f3a01cce76d0b` |

### Unresolved limitations (carry forward)

1. Source-rights clearance pending for all cases.  
2. LX-16 CONDITIONAL — exact packaged release / reporter retest unset.  
3. LX-20 CONDITIONAL — original-reporter patched retest not located.  
4. Systemd concentration **3 of 8** visible cases (LX-13, LX-18, LX-20).  
5. No independent Linux-qualified human validation claimed.  
6. Protocol text still records that formal experimental execution authorization and final sample-size freeze are separate decisions; this preparation proposes a **pilot split only**.

---

## 4. Proposed 4-acquisition / 4-evaluation assignment

Protocol reference: approved pilot shape remains four acquisition + four paired evaluation cases (`EGAR_LX_FAMILY_DIVERSITY_FEASIBILITY_REPORT.md`); partitions must be distinct issues; families may overlap; sealed answers stay evaluation-only (`EGAR_LINUX_DIAGNOSTIC_AB_PROTOCOL.md` §4).

**This is a proposal only. It is not a finalized arm assignment.**

### Proposed Acquisition (n=4) — build the hashed experience snapshot

| Case | Family (descriptive) | Difficulty (qualitative) | Status | Why acquisition |
|------|----------------------|--------------------------|--------|-----------------|
| **LX-03** | Networking / DNS (NetworkManager) | Medium | PASS | Clear before/after resolv.conf evidence; strong learning surface without sealed-priority wording |
| **LX-08** | Boot / installer EFI | Medium–clear | PASS | Distinct boot-install mechanism; strong authentic logs |
| **LX-09** | SSH multiplexing | Medium–clear | PASS | Non-boot, non-systemd diversity; crisp first-vs-reuse contrast |
| **LX-18** | systemd hibernation / on-demand swap | Medium–hard | PASS | One systemd case in acquisition for experience neighborhood; fully PASS (reporter retest recorded in sealed) |

### Proposed Evaluation (n=4) — held-out paired A/B

| Case | Family (descriptive) | Difficulty (qualitative) | Status | Why evaluation |
|------|----------------------|--------------------------|--------|----------------|
| **LX-10** | Boot / GRUB LVM metadata | Hard | PASS | Same broad “boot” family as LX-08 but different causal mechanism; stress-tests generalization |
| **LX-13** | Containers / nspawn+qemu / systemd generators | Hard | PASS | Cross-architecture ambiguity; held out from acquisition |
| **LX-16** | HAProxy reload / UNIX socket handoff | Medium | **CONDITIONAL** | Conditional scoring limits belong on evaluation, not in the acquisition snapshot basis |
| **LX-20** | systemd PrivateDevices / BindPaths | Medium | **CONDITIONAL** | Second systemd case held out; keeps both conditionals off the acquisition set |

### Balance rationale

1. **Causal diversity (acquisition):** NM DNS, installer EFI, OpenSSH mux, systemd hibernate — four distinct mechanisms.  
2. **Causal diversity (evaluation):** GRUB LVM, nspawn/qemu generators, HAProxy sockets, PrivateDevices bind ordering — four distinct mechanisms; no duplicate incident with acquisition.  
3. **Difficulty:** Acquisition mixes clearer packets (08/09) with medium (03/18). Evaluation carries the harder intermittent/ambiguous cases (10/13) plus medium conditionals (16/20).  
4. **Conditionals:** Both LX-16 and LX-20 are evaluation-only so the frozen acquisition snapshot is built only from PASS cases.  
5. **Systemd split:** 1 of 3 in acquisition (LX-18); 2 of 3 in evaluation (LX-13, LX-20). Avoids putting the entire systemd cluster on one side.  
6. **Near-duplicate screen:** LX-08 vs LX-10 share a boot *family label* only; sealed causes differ (wrong ESP vs GRUB LVM metadata). LX-13 vs LX-20 are both systemd but different subsystems (generators/CLONE path vs PrivateDevices bind timing).

### Alternate considered (not preferred)

Putting LX-20 in acquisition and LX-18 in evaluation would still be legal, but would place a CONDITIONAL case into the experience-generating set. Prefer PASS-only acquisition for the first exploratory snapshot.

---

## 5. Remaining prerequisites before starting the pilot

| # | Prerequisite | Blocks start? |
|---|--------------|---------------|
| 1 | Keep sealed workbook outside workspace/RAG/attachments for the run | **Yes**, if violated |
| 2 | Feed Thoth **visible diagnostic exports only** (from Diagnostic Visible v1.2.1), never sealed/provenance | **Yes**, if violated |
| 3 | Explicit owner approval to **finalize** this proposed 4/4 split (this report does not finalize) | **Yes** for execution |
| 4 | Disposable experiment workspace (not development `agent_workspace/` as the EGAR snapshot store; not C6-LIVE; not sealed store) | **Yes** for protocol-safe run |
| 5 | Seeded arm-order schedule for evaluation pairs (existing harness capability; seed must be chosen and recorded) | Recommended before evaluation |
| 6 | Source-rights acknowledgment for private exploratory use | Limitation; process acknowledgment |
| 7 | LX-16 / LX-20 conditional scoring limits briefed to reviewers | Limitation; not a start blocker |
| 8 | Formal protocol “authorized for experimental execution” status | Protocol text still draft/unfrozen for full experiment; exploratory pilot needs an explicit owner go-ahead beyond corpus freeze |

---

## 6. Summary board

| Check | Result |
|-------|--------|
| Checksums vs manifest | **PASS** |
| Isolation of sealed answers in current Thoth paths | **PASS** |
| Proposed 4/4 assignment | Recorded above (**proposal only**) |
| Ready to start pilot immediately? | **Not yet** — finalize assignment + disposable workspace + owner execution approval still required |

**Overall preparation status: READY FOR ASSIGNMENT FINALIZATION**  
Isolation and checksum gates are clear. The next human step is to accept or revise the proposed 4/4 split, then authorize a disposable-workspace exploratory run under the existing protocol rules without ingesting sealed materials.
