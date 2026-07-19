# G1e — Trajectory Polarity Probe Protocol

**Protocol version:** G1e v1.2  
**Status:** 🔒 **Phase 4 KEEP @ −0.05 applied** 2026-07-19 — production `trajectory: -0.05`; magnitude tuning **paused (not dropped)**  
**Execution status:** Phase 0–4 complete for KEEP@−0.05. Future probes (e.g. `−0.40`) remain allowed under separate approval — fork not closed as DROP  
**Prerequisite:** G1d ✅ closed for **positive** `w_t`; G1e supersedes production weight with polarity KEEP  
**Methodology reuse (immutable science source):** [`trajectory_ablation_benchmark.md`](trajectory_ablation_benchmark.md) v1.0 — case filter, arms A/B/C structure, EPSILON, winner rules  
**G1d close-out (immutable):** [`G1D_CLOSEOUT_PROTOCOL.md`](G1D_CLOSEOUT_PROTOCOL.md) G1d-CO v1.2 — **not** reopened by this fork  
**Checkpoint tracking:** [`cursor_list.md`](cursor_list.md) § G1e  

---

## Document structure

| Class | Sections | Binding? |
|-------|----------|----------|
| **Normative** | Framing, schedule, no-scalar-rescue, **Phase 3b**, decision criteria, sequencing, Phase 0–1, Phase 2 preflight, Phase 3–4, non-goals | **Yes** — SHALL/MUST |
| **Informative** | Rationale, doc map | **No** |

If informative text conflicts with a normative rule, the **normative** rule governs.

**Protocol Lock Rule:** This document is locked. Do not silently revise it during implementation. If implementation reveals this protocol must change, stop and request owner approval before editing this file or dependent code.

**Methodology firewall:** Arms A/B/C structure, case filter, EPSILON, and winner rules remain defined **only** in [`trajectory_ablation_benchmark.md`](trajectory_ablation_benchmark.md) v1.0. This protocol **SHALL NOT** edit those methodology sections. G1e adds polarity schedule, no-scalar-rescue, harness allowlist extension, and Phase 2 operational preflight only.

---

## Informative — One sentence

Test whether the **current** trajectory term improves `TRAJECTORY_DISAMBIGUATES` retrieval under **negative** `w_t` (polarity / wrong-sign hypothesis), without reopening G1d or changing production config until a KEEP is approved.

---

## Informative — Prepared vs executed

| State | Meaning |
|-------|---------|
| **Prepared** | Protocol locked; harness accepts G1e `--wt`; Phase 2 checklist locked |
| **Scientifically executed** | Phase 3 authoritative External polarity runs completed and logged |

Phase 3 (2026-07-18) completed the scientific runs. Phase 4 owns terminal KEEP confirmation and any production config write.

---

## Normative — Framing

| Item | Rule |
|------|------|
| Fork id | **G1e** |
| Relation to G1d | Post-G1d research fork. G1d remains ✅ **DROP**; production `retrieval_weights.trajectory` remains **0.0** unless a later KEEP is explicitly approved |
| Production config | **SHALL NOT** change in Phases 0–3 |
| Positive scalar sweeps | **Forbidden** under G1e (already covered by G1d Phase B + TUNE) |

---

## Normative — Research question

On `TRAJECTORY_DISAMBIGUATES` cases only: does Arm B with scheduled **negative** `w_t` improve nDCG@5 vs Arm A (`w_t=0`)?

---

## Normative — Locked weight schedule (negatives-first)

| Order | `w_t` | Notes |
|-------|-------|--------|
| 1 | **−0.05** | First polarity probe — **KEEP** (Phase 3) |
| 2 | **−0.10** | Not KEEP |
| 3 | **−0.20** | Not KEEP; win rate baseline for Phase 3b early-stop |
| 3b | **−0.30** | Magnitude extension (v1.2); early-stop if win rate worsens |

**Excluded:** `+0.25` and any other positive `w_t` under this fork. **`−0.40` is not authorized** unless Phase 3b early-stop does not fire **and** owner separately approves.

---

## Normative — Phase 3b (magnitude probe + win-rate early-stop) — locked v1.2

### Goal

Test whether more negative `w_t=−0.30` improves (or at least does not worsen) B-vs-A win rate vs the Phase 3 `−0.20` result, given mean Δ improved with magnitude while win rate stalled.

### Preconditions

1. Phase 3 complete  
2. Same backend / `env_hash` lineage as Phase 2/3  
3. Production `trajectory` remains **0.0**  
4. No TfIdf; full ~30-case; no `--sample`

### Schedule

| Order | `w_t` |
|-------|-------|
| 1 | **−0.30** |

### Early-stop rule (normative)

| Metric | Baseline (Phase 3 `−0.20`) | Stop if |
|--------|----------------------------|---------|
| Win % (B vs A, ties excluded) | **57.1%** (8 wins / 6 losses) | new win % **&lt; 57.1%** |

If early-stop fires:

1. **Stop** further scalar magnitude tuning under G1e (including `−0.40`).  
2. Log **TUNING_STOPPED_WIN_RATE** with `−0.30` provenance.  
3. KEEP@`−0.05` remains the Phase 4 candidate unless owner revises selection.

If win rate does **not** worsen vs baseline:

1. Log `−0.30` results (KEEP or not).  
2. **STOP for owner review** before any `−0.40` run (separate approval required).

Δ alone **SHALL NOT** authorize continuing to `−0.40`.

### Harness

`isTrajectoryAblationG1eWt` / CLI `--wt` **SHALL** accept `−0.30` (and `-0.3` via float epsilon) in addition to `{−0.05, −0.10, −0.20}`.

---

## Normative — No-scalar-rescue rule

If **none** of `{−0.05, −0.10, −0.20}` produces a **KEEP** candidate under the decision criteria below:

1. **Stop** all further scalar `w_t` tuning under G1e.
2. Log terminal outcome: **NO_SCALAR_RESCUE → hand off to F5** (construction / semantic-T analysis).
3. **SHALL NOT** invent additional weights (e.g. ±0.01, −0.25) under this protocol.

---

## Normative — Ablation structure (reuse)

| Arm | `w_q` / `w_d` / `w_k` | `w_t` | Trajectory `T` |
|-----|------------------------|-------|----------------|
| **A** | 0.4 / 0.4 / 0.3 | **0.0** | Built via `TrajectoryBuilder` |
| **B** | 0.4 / 0.4 / 0.3 | Scheduled G1e weight | Built via `TrajectoryBuilder` |
| **C** | 0.4 / 0.4 / 0.3 | Scheduled G1e weight | **Forced zero vector** |

Case filter, EPSILON (`0.001`), and per-case winner rules: methodology v1.0.

---

## Normative — Decision criteria

Evaluate **Arm B vs Arm A** only for KEEP (same dual criteria as G1d methodology):

| Outcome | Criteria |
|---------|----------|
| **KEEP** | (1) B wins ≥ **60%** of cases (ties excluded from denominator) **AND** (2) mean nDCG@5 delta **(B − A) > 0** |
| **Not KEEP** | Dual criteria unmet for that `w_t` |

**G1e differences from G1d Phase C:**

- **No TUNE branch** — a failed KEEP does **not** authorize additional scalar weights beyond the locked schedule.
- After the schedule completes (or owner early-stops): if zero KEEP candidates → apply **no-scalar-rescue** → **F5** handoff.
- Arm C remains mechanism evidence only (construction vs weight); it does not authorize further scalar rescue.

---

## Normative — Sequencing

```
Phase 0  Protocol lock + tracking docs                         ✅
  ↓
Phase 1  Harness allowlist + provenance + unit tests           ✅
  ↓
STOP — owner review before experiments
  ↓
Phase 2  Operational preflight                                 ✅ executed 2026-07-18
  ↓
STOP — owner review before Phase 3
  ↓
Phase 3  Authoritative runs: −0.05 → −0.10 → −0.20              ✅ executed 2026-07-18
  ↓
Phase 4  Decision log / optional config                         ✅ KEEP@−0.05 production 2026-07-19
  ↓
OPEN     Magnitude tuning paused (not dropped) — e.g. −0.40 later [separate approval]
```

Each **STOP** requires explicit owner go-ahead before the next phase (AGENTS.md Planning / Implementation Gate).

**Current status:** Production `trajectory: -0.05` (KEEP). G1e magnitude search is **paused, not abandoned**. `−0.40` and further probes require separate approval.

---

## Normative — Phase 0 (Protocol + tracking)

### Goal

Lock this document and align tracking docs. No harness experiments.

### Required updates

| File | Change |
|------|--------|
| This file | Locked G1e v1.0 (base) |
| [`cursor_list.md`](cursor_list.md) | Register **G1e**; G1d remains ✅ DROP |
| [`GRAG.md`](GRAG.md) | Point polarity future work at G1e |
| [`plan_reuse_tuning.md`](plan_reuse_tuning.md) | G1e pointer; production stays `0.0` |
| [`improvements.md`](improvements.md) | Status pointer only |

### Exit criteria

1. Protocol locked  
2. Tracking docs consistent  
3. **STOP** before Phase 1 if Phase 1 was not co-approved; otherwise proceed to Phase 1 in the same approved slice  

---

## Normative — Phase 1 (Harness)

### Goal

Accept G1e weights on `run_trajectory_ablation_benchmark` without breaking G1d TUNE allowlist `{0.05, 0.1}`.

### Required changes

| Item | Requirement |
|------|-------------|
| Keep | `isTrajectoryAblationTuneWt()` = `{0.05, 0.1}` only |
| Add | `isTrajectoryAblationG1eWt(float)` → `{−0.05, −0.10, −0.20}` (+ `−0.30` under v1.2 Phase 3b) |
| CLI `--wt` | Accept **either** G1d TUNE **or** G1e set; reject all else |
| Provenance | When G1e wt: stdout + JSONL include `fork=g1e` and `tune_wt=<value>`; **SHALL NOT** label as Phase C TUNE |
| Arms | Reuse `trajectoryAblationArmConfig(arm, wt_bc)` |
| Scoring | No scorer / TrajectoryBuilder / F5 changes unless a clamp-to-nonnegative bug blocks negatives — then **STOP** for repair approval |
| Tests | Unit tests: G1e accepted; G1d TUNE still accepted; reject `0.2`, `0.0`, `+0.25`, `−0.25` |

### Exit criteria

1. Build succeeds  
2. Relevant unit tests pass (including existing G1d ablation tests)  
3. Binary accepts `--wt -0.05` at CLI validation layer  
4. **STOP before experiments** (no authoritative bucket run in Phase 1)

---

## Normative — Phase 2 (Operational preflight) — locked v1.1

### Goal

Confirm the live environment can support authoritative External-inference G1e polarity runs. **No methodology or scoring changes. No KEEP/DROP claims.**

Phase 2 is **operational verification only**. Completing Phase 2 does **not** scientifically execute G1e.

### Preconditions

1. Phase 0–1 complete (protocol + harness)  
2. Phase 2 **execution** separately approved by owner (locking this checklist ≠ approval to run it)  
3. `THOTH_TRAJECTORY_ABLATION_TFIDF` **MUST NOT** be set for Phase 2 evidence  
4. Phase 2 uses the production inference path (`EmbeddingEngine::External` → instantiated `InferenceClient`)  
5. Production `retrieval_weights.trajectory` remains **0.0**

### Checklist

| # | Step | Requirement | Pass |
|---|------|-------------|------|
| **1** | Backend gate | Call `probeInferenceBackend()`. Pass **SHALL** mean `reachable == true` on the **instantiated** client. Backend identity **SHALL** come from `backendName()`, not config alone. | ✅ |
| **2** | Record provenance | Persist probe snapshot **before** any ablation work: `backend_name`, `base_url`, `embed_base_url`, diagnostics → sidecar `environment.inference.*` and participating `env_hash` inputs; stdout `backend=<name>`; short preflight record (timestamp, reachable, backend name, endpoints). | ✅ |
| **3** | Corpus / index | Same research corpus contract as methodology v1.0 / `run_grag_benchmark` (`agent_workspace/docs/`). Required files readable; index builds; chunk count non-zero and plausible. | ✅ |
| **4** | Path confirmation | Confirm External path (not TfIdf). Optional cheap probe: `--sample N --wt -0.05` **without** TfIdf — **path evidence only**. Sample output is **not** KEEP / Phase 3 decision evidence. | ✅ |
| **5** | Provenance spot-check | Stdout / sidecar / JSONL show: `backend=<exercised name>`; `fork=g1e` and `tune_wt` when `--wt` used; **not** labeled Phase C TUNE; one `run_id` and one `env_hash` for the preflight lineage; inference fields present when External path is used. | ✅ |
| **6** | Evidence packet | Packet for owner review: pass/fail per step; backend name; endpoints; `run_id` / `env_hash` if probe/sample made; corpus/chunk notes. | ✅ |

### Apply on backend failure

If Step 1 unreachable → **stop Phase 2** immediately. Do **not** sample. Do **not** start Phase 3. Log **“prepared, authoritative polarity pending.”**

If reachable → the recorded snapshot is the **only** backend identity allowed for Checkpoint Phase 2 evidence and for Phase 3 (same backend identity / `env_hash` lineage).

### Out of scope (Phase 2)

- Full `TRAJECTORY_DISAMBIGUATES` bucket runs (Phase **3**)  
- KEEP / NO_SCALAR_RESCUE / F5 handoff (Phase **4**)  
- Production `retrieval_config.json` writes  
- Arm / EPSILON / decision-matrix methodology edits  
- Positive `w_t` sweeps  
- New infrastructure unless a blocking Phase 1 gap is found and owner-approved  

### Phase 2 exit criteria (Checkpoint Phase 2)

1. Steps 1–6 passed (or Step 1 failed closed with pending log — then Phase 3 blocked)  
2. Evidence packet available for owner review  
3. **STOP for owner review** before Phase 3  

Fail any step → do **not** start Phase 3.

**Non-claim:** Phase 2 pass **SHALL NOT** be reported as scientific polarity results or KEEP/DROP.

---

## Normative — Phases 3–4 (deferred; separate approval)

| Phase | Work |
|-------|------|
| **3** | Full ~30-case External runs at −0.05, then −0.10, then −0.20 (no TfIdf for decisions); same `env_hash` lineage as approved Phase 2 |
| **3b** | Optional magnitude probe `−0.30` with win-rate early-stop vs `−0.20` (v1.2); `−0.40` only with separate approval if early-stop does not fire |
| **4** | Log KEEP @ −0.05; set production `trajectory: -0.05`; **do not drop** open magnitude-tuning lane |

Scientific execution of G1e **begins at Phase 3**.

---

## Normative — Non-goals (all G1e phases unless separately approved)

- Reopening or editing G1d methodology science sections  
- Reopening G1d-CO Phase D as unfinished work  
- Production `retrieval_config.json` writes in Phases 0–3  
- Positive `w_t` sweeps under G1e  
- Scorer / TrajectoryBuilder / F5 implementation  
- Silent protocol edits post-lock  
- Treating Phase 2 preflight as scientific polarity evidence  

---

## Informative — Rationale

G1d positive weights (`0.20`, `0.10`, `0.05`) all yielded mean Δ(B−A) ≤ 0; closer-to-zero was less harmful. DROP zeros production influence of the **current** positive-weight formulation. G1e tests the polarity hypothesis only; failure hands off to F5 rather than endless scalar rescue.

---

## Informative — Doc map

| Need | File |
|------|------|
| **This protocol** | `G1E_POLARITY_PROTOCOL.md` |
| G1d methodology (immutable) | `trajectory_ablation_benchmark.md` v1.0 |
| G1d close-out (immutable) | `G1D_CLOSEOUT_PROTOCOL.md` |
| Production weight notes | `plan_reuse_tuning.md`, `GRAG.md` |
| Tracking | `cursor_list.md`, `improvements.md` |

---

*G1e v1.0–v1.2. Phase 4 KEEP@−0.05 applied to production 2026-07-19. Magnitude tuning paused (not dropped); further probes (e.g. −0.40) need separate approval.*
