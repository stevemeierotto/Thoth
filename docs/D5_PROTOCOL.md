# D5 — Evolution Trust Proof Protocol

**Protocol version:** D5 v0.1  
**Status:** 🔒 **LOCKED** (2026-07-08) — implementation plan § D.5.0 may proceed; await explicit implementation approval  
**Supersedes:** None (first D5 protocol)  
**Depends on:** [`D_PHASE_PROTOCOL.md`](D_PHASE_PROTOCOL.md) D0–D4 (D4 complete 2026-07-08), [`C_PHASE_PROTOCOL.md`](C_PHASE_PROTOCOL.md) v1.1, [`E2_PROTOCOL.md`](E2_PROTOCOL.md) v1.2, Phase B v1 baseline ([`phases/PHASE_B_COMPLETE.md`](phases/PHASE_B_COMPLETE.md))  
**Checkpoint tracking:** `cursor_list.md` § **D.5.0** (implementation plan — locked only after this protocol locks)

> **Scope:** D5 is the **final Phase-D trust boundary**. It is a **meta-proof**, not a feature phase. D4 built the trust mechanisms; D5 proves they **survived evolution**. D5 does **not** introduce new runtime behavior.

---

## Purpose

D5 proves that cumulative system evolution from D1–D4 preserved:

- **authority** boundaries,
- **determinism** of authoritative evaluation,
- **behavioral equivalence** on trusted fixtures, and
- **observer isolation** (passive consumer law).

D5 verifies the **composed proof surface** — it does **not** re-prove every lower-level invariant in isolation.

### What D5 is

| Role | Description |
|------|-------------|
| **Final proof gate** | Five constitutional invariants + phase closure evidence |
| **Bounded regression aggregator** | Calls existing helpers at fixed depth — no recursive proof tree |
| **Phase seal** | Written contract + evidence artifact closing Phase D |

### What D5 is not

| Anti-pattern | Why forbidden |
|--------------|---------------|
| “Run everything again” | Becomes maintenance debt; duplicates D4 composition |
| Second D4 umbrella | D4 already proved composition (`THOTH_E2_D4=1`) |
| Feature / wiring phase | No new subscribers, flags, or production paths |
| Protocol semantics change | Requires E2 v1.3+ and separate approval |

---

## Phase narrative (D4 → D5)

| Checkpoint | Proof type | Gate |
|------------|------------|------|
| D4 Step 1 | Structural seam proof | `THOTH_E2_D4_STEP1=1` |
| D4 Step 2 | Live INTEGRATION behavior proof | `THOTH_E2_D4_01=1` |
| D4 Step 3 | STRICT authority preservation proof | `THOTH_E2_D4_02=1` |
| D4 Step 4 | Backward compatibility proof | `THOTH_E2_D4_STEP4=1` |
| D4 Step 5 | Composition proof | `THOTH_E2_D4=1` |
| **D5** | **Evolution trust proof** | `THOTH_E2_D5=1` |

**D4** answers: “Do the D4 obligations compose?”  
**D5** answers: “After all D-phase evolution, did trust-critical behavior survive?”

---

## Core question (locked)

> After all D-phase changes (D1–D4), does the system still preserve trust-critical behavior with no hidden authority drift?

## Tightened objective (locked)

> D5 proves that accumulated D-phase evolution preserves previously established authority boundaries, deterministic behavior, and benchmark equivalence. D5 does **not** re-prove every lower-level invariant; it verifies the **composed proof surface**.

---

## Evidence composition rule (locked)

> **D5 is evidence composition, not proof regeneration.**

D5 **consumes** previously accepted proof evidence from D1–D4 where those invariants have already been established. D5 does **not** regenerate lower-phase proofs. D5 only verifies that the accumulated proof surface remains coherent after evolution.

This is the single most important idea in D5. Future maintainers must not need to infer it.

| Composition (D5 does) | Regeneration (D5 does not) |
|-----------------------|---------------------------|
| Reference D1–D4 close-out gate evidence | Re-run `runE2D4Tests()`, `runE2D3Tests()`, etc. by default |
| Run D5-specific meta-proofs (authority audit bundle, C5, determinism) | Re-prove every checkpoint behavioral suite |
| Verify composed surface coherence | Build recursive proof tree D5 → D4 → D3 → D2 → D1 |

See also: § Intentionally not re-proven, § Coverage-gap rule.

---

## Constitutional invariants

D5 proves exactly **five** invariants. Invariants 1–4 map to sub-gates and preregistered test IDs. Invariant 5 is an interpretive constraint on what a D5 pass means.

### Invariant 1 — Authority preservation

**Question:** Did any D-phase change accidentally alter who has authority?

After D-phase evolution, the following must remain true:

| Property | Requirement |
|----------|-------------|
| Benchmark authority | Unchanged — `wiring_stage=B` remains sole scoring authority |
| Scoring authority | Unchanged — Phase B contract frozen |
| Subscribers | Passive — Constitutional Rule: Observe / Record / Replay / Present only |
| Observers | Cannot mutate execution or publisher state |
| Evaluation wiring | Cannot influence authoritative outcomes on STRICT path |
| Production live path | Cannot emit `official_scoring: true` or benchmark authority fields |

**Sub-gate:** `THOTH_E2_D5_AUTHORITY=1`  
**Preregistered ID:** **E2-D5-03** — Passive Consumer Law / constitutional structural audit

**Evidence rule:** Consume D4 composition evidence (E2-D4-01 containment, E2-D4-02 scoped equivalence, D4-I1..I7 chain). Run only the **narrow structural audit bundle** not covered by Invariants 2–3. **Do not** re-run `runE2D4Tests()` or full D1–D3 behavioral suites by default.

---

### Invariant 2 — Behavioral preservation

**Question:** Did evolution preserve existing trusted outputs?

| Property | Requirement |
|----------|-------------|
| C5 equivalence | MATCH on mapping-safe golden fixtures (E2-01..03) |
| Pinned fixtures | Stable under `makeE2StrictTestConfig()` |
| Production vs benchmark | Normalized snapshots equivalent under pinned config |
| Path fingerprints | Stable post-normalization on pinned config |

**Sub-gate:** `THOTH_E2_D5_C5=1`  
**Preregistered ID:** **E2-D5-01** — C5 equivalence matrix re-pass

**Evidence rule:** `runE2C5RegressionGate()` — call, do not duplicate. **Do not** re-run full C1–C4 unless coverage-gap rule fires.

---

### Invariant 3 — Determinism preservation

**Question:** Did the system remain reproducible?

| Property | Requirement |
|----------|-------------|
| Consecutive strict runs | Identical scoped-equivalence snapshots (E2-28) |
| Fingerprint | Identical across two consecutive `wiring_stage=B` builds |
| Diagnosis bucket | Stable on golden trio (bucket #0) |
| Hidden nondeterminism | No authority-relevant drift between identical runs |

**Sub-gate:** `THOTH_E2_D5_DETERMINISM=1`  
**Preregistered ID:** **E2-D5-02** — Phase B fingerprint two-run gate

**Evidence rule:** `testE2B5OfficialFingerprintDeterminism()` (existing E2-28 helper). **Do not** re-run full Phase B suite or D4-02 determinism tests — those were proven at earlier checkpoints.

---

### Invariant 4 — Evidence completeness

**Question:** Can we prove D-phase completion?

D5 must produce a final proof artifact showing:

| Checkpoint | Status source |
|------------|---------------|
| D1 complete | `THOTH_E2_D1=1` — committed at D1 close-out |
| D2 complete | `THOTH_E2_D2=1` — committed at D2 close-out |
| D3 complete | `THOTH_E2_D3=1` — committed at D3 close-out |
| D4 complete | `THOTH_E2_D4=1` — committed at D4 close-out |
| D5 complete | `THOTH_E2_D5=1` — this run |

**Closure gate:** `THOTH_E2_D5=1`  
**Documentation artifact:** `docs/phases/PHASE_D_COMPLETE.md` (phase seal — pointers to gate evidence, dates, commit refs)

---

### Invariant 5 — Preservation, not promotion

**Question:** Does passing D5 grant new authority or authorize architectural promotion?

**Answer:** No. D5 confirms **preservation** of existing trust boundaries only.

| D5 pass means | D5 pass does **not** mean |
|---------------|---------------------------|
| Trust boundaries preserved after D-phase evolution | New scoring or benchmark authority granted |
| Composed proof surface remains coherent | INTEGRATION ≡ STRICT equivalence established |
| Phase D trust seal recorded | Architectural promotion authorized |
| Safe to proceed to Phase E scientific defense | “Approved for everything” |

Passing D5 is a **final trust artifact** — it seals preservation, it does not promote separated execution modes or confer new authority. Interpreting D5 green as promotion suitability is **forbidden**.

---

## Frozen invariants (what D5 does not reopen)

D5 **consumes** these as immutable dependencies. Reopening any item requires a **new protocol version** (E2 v1.3+) and explicit human approval — not D5 work.

| Frozen surface | Authority |
|----------------|-----------|
| `evaluation_resolution` / `e2_outcome` semantics | Phase B / `E2_PROTOCOL.md` v1.2 |
| `resolveEvaluation()`, Phase B export contract | Phase B locked |
| STRICT harness / `wiring_stage=B` fingerprint derivation | Phase B locked |
| INTEGRATION tier definition (E2-06) | `E2_PROTOCOL.md` v1.2 |
| D1–D4 subscriber contracts | `D_PHASE_PROTOCOL.md` + checkpoint evidence |
| Scoped equivalence comparator (`episodicLearningScopedEquivalenceEqual`) | E2-28 / D4 Step 3 locked |
| Passive Consumer Law (five conditions) | `D_PHASE_PROTOCOL.md` |
| Phase D Constitutional Rule | `D_PHASE_PROTOCOL.md` |

---

## Intentionally not re-proven (locked)

D5 **must not** re-run these by default. They were proven at lower checkpoints and are **consumed by reference** in the D5 evidence chain.

| Not re-proven | Proven at | Consumed via |
|---------------|-----------|--------------|
| Full D4 composition (`runE2D4Tests()`) | D4 Step 5 | D4 close-out attestation |
| Full D3 proof suite | D3 Step 6 | D4 Step 4 backward-compat evidence |
| Full D2 / D1 behavioral gates | D1–D2 close-out | D4 Step 4 evidence |
| D4-02 full authority suite | D4 Step 3 | Invariant 1 attestation + narrow isolation check only |
| Full Phase B test battery | Phase B close-out | Invariant 3 two-run gate only |
| Full C1–C4 integration suite | Phase C close-out | Invariant 2 C5 gate only |
| Default full unit-test suite | — | Optional post-D5 hygiene (not core D5) |
| G2 `ctest` | — | Optional post-D5 hygiene (not core D5) |

### Coverage-gap rule (locked)

> **Only execute a lower-phase gate when D5 evidence coverage identifies a missing invariant.**

Default: **no** recursive proof tree (D5 → D4 → D3 → D2 → D1).

A lower gate may be invoked only if:

1. A D5 sub-gate fails with a diagnostic pointing to an uncovered invariant, **and**
2. The protocol or implementation plan is amended with explicit justification before re-run.

---

## Gate contract

### What `THOTH_E2_D5=1` means

> **`THOTH_E2_D5=1` means all D5 meta-proofs passed against their declared evidence sources. It does not mean that every historical test suite was rerun.**

Example: D5 does **not** re-run D2 (`THOTH_E2_D2=1`) by default — because it was not supposed to. D2 was proven at D2 close-out and consumed via the D4 backward-compatibility and D5 evidence-composition chain.

When `THOTH_E2_D5=1` is set, the test harness runs the **D5 evolution trust meta-proof** and exits:

1. **Attest** D4 composition evidence prerequisites (reference only — no `runE2D4Tests()` re-execution).
2. **Invariant 1** — `THOTH_E2_D5_AUTHORITY=1` equivalent: constitutional structural audit bundle.
3. **Invariant 2** — `THOTH_E2_D5_C5=1` equivalent: C5 regression gate.
4. **Invariant 3** — `THOTH_E2_D5_DETERMINISM=1` equivalent: Phase B two-run determinism gate.
5. **Invariant 4** — emit phase closure evidence (D1–D5 PASS artifact).
6. **Invariant 5** — preservation-not-promotion constraint applies to the recorded conclusion (no promotion claims in evidence output).

On success: print consolidated D5 evidence block and exit 0.  
On failure: exit 1 per AGENTS.md Build/Test Failure Rule — **stop; do not auto-repair.**

### Sub-gates (independently runnable for diagnosis)

| Env gate | Invariant | Preregistered ID |
|----------|-----------|----------------|
| `THOTH_E2_D5_AUTHORITY=1` | Authority preservation | E2-D5-03 |
| `THOTH_E2_D5_C5=1` | Behavioral preservation | E2-D5-01 |
| `THOTH_E2_D5_DETERMINISM=1` | Determinism preservation | E2-D5-02 |
| `THOTH_E2_D5=1` | Invariants 1–4 + phase closure | (orchestrator) |

**Interpretive constraint (no sub-gate):** Invariant 5 — preservation, not promotion — governs how D5 pass is recorded and interpreted.

### Gate priority in `main()`

`THOTH_E2_D5=1` must be checked **before** D5 sub-gates and **before** D4 gates (composition gate is a separate checkpoint already closed).

---

## Evidence that counts

### Per-sub-gate evidence

**Authority (`THOTH_E2_D5_AUTHORITY=1`):**

- D4 composition attestation recorded (gate ID + close-out date)
- Structural audit bundle green (D1-03 structural, D3-03 authority boundary, D4-02 isolation absence)
- No reverse-edge symbols in subscriber sources (narrow grep contract)
- Conclusion: authority boundaries preserved post-evolution

**C5 (`THOTH_E2_D5_C5=1`):**

- `runE2C5RegressionGate()` green
- Golden fixtures MATCH on mapping-safe cases
- Conclusion: behavioral equivalence preserved

**Determinism (`THOTH_E2_D5_DETERMINISM=1`):**

- `testE2B5OfficialFingerprintDeterminism()` green
- Scoped snapshots deep-equal across consecutive builds
- Conclusion: deterministic trust preserved

### Closure evidence (`THOTH_E2_D5=1`)

On green, record:

1. `THOTH_E2_D5_AUTHORITY=1` pass  
2. `THOTH_E2_D5_C5=1` pass  
3. `THOTH_E2_D5_DETERMINISM=1` pass  
4. D1–D4 PASS attestation (by reference to committed checkpoint gates)  
5. **Conclusion:** evolution trust proof green — Phase D trust boundary sealed (**preservation only — not promotion**)
6. **Deferred:** Phase E scientific defense  

---

## Forbidden changes (D5)

| Forbidden | Rationale |
|-----------|-----------|
| New runtime behavior, subscribers, or config flags | D5 is validation only |
| New preregistered test IDs beyond E2-D5-01..03 | Meta-proof surface is fixed |
| Re-run `runE2D4Tests()` inside D5 by default | Redundant with D4 composition |
| Recursive lower-phase orchestrators | Coverage-gap rule |
| INTEGRATION ≡ STRICT promotion or lift claims | Out of E2 v1.2 scope |
| Silent edits to `E2_PROTOCOL.md`, tier semantics, `resolveEvaluation()` | Protocol Lock Rule |
| Claiming Phase E complete | D5 closes Phase D only |

---

## Reopening the proof boundary

The D5 proof boundary must be **reopened** if any future change modifies:

- scoring authority
- benchmark authority
- fingerprint generation
- equivalence comparison rules
- subscriber ownership boundaries
- **evaluation artifact generation**
- **proof evidence formats**

The last two are explicit: a trust proof can be invalidated by changing how evidence is produced, even if runtime behavior appears unchanged.

The following changes **reopen** the D5 trust boundary and require re-proof (at minimum the affected invariant sub-gate; possibly full `THOTH_E2_D5=1`):

| Change class | Reopens |
|--------------|---------|
| Any Phase B semantic change (`evaluation_resolution`, fingerprint, buckets) | Invariants 1, 2, 3 — protocol bump required |
| New subscriber or channel event type | Invariant 1 — Passive Consumer Law re-audit |
| Executive branching on subscriber/eval state | Invariant 1 — Constitutional Rule violation |
| `resolveEvaluation()` or Phase B export change | Invariants 1, 2, 3 |
| INTEGRATION/STRICT tier definition change | Invariants 1, 2 — protocol bump |
| Production path emitting scoring authority | Invariant 1 — D4 containment breach |
| C5 fixture or pinned config change | Invariant 2 |
| STRICT harness or wiring_stage contract change | Invariants 1, 3 |
| D4 wiring seam change (plugin registration, eval publication) | Full D4 + D5 re-proof |
| Evaluation artifact generation change (JSONL schema, envelope fields, export rollups) | Invariants 1, 2, 4 — evidence invalidation |
| Proof evidence format change (gate output, attestation records, comparator snapshot fields) | Invariants 2, 3, 4 — evidence invalidation |

Changes that **do not** reopen D5 (examples):

- GUI presentation-only reads of existing JSONL
- Documentation typo fixes with no semantic change
- Optional post-D5 full-suite / G2 hygiene runs

---

## Optional post-D5 hygiene (out of core D5 scope)

These may run **after** `THOTH_E2_D5=1` green as a **separate explicit request** — they are **not** part of the D5 meta-proof contract:

| Activity | Gate |
|----------|------|
| Default full unit-test suite | (no env gate — full binary) |
| G2 `ctest -R thoth-unit-tests` | CTest |
| C5 matrix evidence printer | `THOTH_E2_C5_MATRIX=1` |

---

## Implementation order (mandatory)

Per AGENTS.md Planning/Implementation Gate:

| Order | Step |
|-------|------|
| 1 | Draft D5 protocol (this document) ✅ |
| 2 | Review / refine ✅ |
| 3 | **Lock** protocol (commit; status → 🔒) ✅ |
| 4 | Draft / refine implementation plan in `cursor_list.md` § D.5.0 **against locked protocol** |
| 5 | Explicit implementation approval |
| 6 | Implement + verify targeted gates |
| 7 | `PHASE_D_COMPLETE.md` + pause before Phase E |

**Do not implement D5 test orchestrators until step 5 completes.**

---

## Exit criteria (D5 complete)

1. This protocol locked (🔒) and committed  
2. `THOTH_E2_D5=1` green  
3. `THOTH_E2_D5_AUTHORITY=1`, `THOTH_E2_D5_C5=1`, `THOTH_E2_D5_DETERMINISM=1` green individually  
4. Phase closure artifact: `docs/phases/PHASE_D_COMPLETE.md`  
5. `cursor_list.md` and `D_PHASE_PROTOCOL.md` pointers updated  
6. **Pause for review** before Phase E  

---

## Key files

| File | Role |
|------|------|
| `docs/D5_PROTOCOL.md` | **This document** — D5 trust contract |
| `docs/D_PHASE_PROTOCOL.md` | Phase D umbrella; D5 summary pointer |
| `docs/cursor_list.md` | § D.5.0 implementation plan (post-lock) |
| `docs/phases/PHASE_D_COMPLETE.md` | Phase D close-out artifact |
| `tests/unit_tests.cpp` | D5 orchestrators + env gates |
| `docs/E2_PROTOCOL.md` | Frozen evaluation semantics (consume only) |

---

## D5 lock record

**Drafted:** 2026-07-08  
**Locked:** 2026-07-08 (v0.1)  
**Status:** 🔒 **LOCKED**  
**Review incorporated:** Meta-proof framing; evidence composition rule (not proof regeneration); five constitutional invariants (including preservation-not-promotion); anti-redundancy / coverage-gap rule; D4 evidence consumption; sub-gate model; `THOTH_E2_D5=1` semantics (meta-proofs only, not full historical re-run); evidence-generation reopening boundary; phase seal discipline.

**Next:** Step 1 implementation (`THOTH_E2_D5_AUTHORITY=1`) per `cursor_list.md` § D.5.0 Step 1 — explicit approval required.
