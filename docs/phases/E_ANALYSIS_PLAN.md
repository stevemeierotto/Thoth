# E — Evaluation Protocol and Analysis Plan

**Document revision:** E-AP v1.1  
**Locked:** 2026-07-09 (v1.0); amended 2026-07-09 (v1.1)  
**Phase E Step:** E1 (analysis plan lock)  
**Status:** 🔒 **LOCKED** — preregistered before Phase E Step 2 authoritative runs

> **Amendment v1.1:** Clarifies LLM **backend-agnostic** architecture vs **authoritative evaluation backend** designation. No change to metrics, corpus, hypotheses, exclusions, or pass/fail rules. Supersedes E-AP v1.0 (`48e7511`) on wording only.

| Governing protocol | Version |
|--------------------|---------|
| [`E_PHASE_PROTOCOL.md`](../E_PHASE_PROTOCOL.md) | v0.1 🔒 (amended 2026-07-09) |
| [`E2_PROTOCOL.md`](../E2_PROTOCOL.md) | v1.2 🔒 (2026-07-01) |
| Phase D seal | [`PHASE_D_COMPLETE.md`](PHASE_D_COMPLETE.md) (`e0a9ba5`, 2026-07-08) |

> **Naming:** This file is Phase **E** Step **E1** — not benchmark environment pinning ([`benchmark_environment.md`](../benchmark_environment.md) “E1” track).

---

# Preamble

## Publication scope

Phase E closes the gap between **engineering trust** (Phases A–D) and **empirical claim defensibility** (external publications).

| Publication target | Question | Primary evidence | Phase E role |
|--------------------|----------|------------------|--------------|
| **Architecture** | Was the evaluator built correctly? Authority preserved? Deterministic? | Phase D seal + machinery gates | Reference only — largely answered by Phase D |
| **Empirical (scoped)** | Does declared-episode retrieval produce measurable lift under STRICT lab conditions? | STRICT runs on declared corpus | **Primary Phase E target** |

**In-scope external documents for E4 claims audit:**

| Document | Role |
|----------|------|
| [`MYPAPER.md`](../MYPAPER.md) | GRAG architecture paper — retrieval claims map to GRAG tier; episodic lift claims map to STRICT tier |
| [`E_PHASE_PROTOCOL.md`](../E_PHASE_PROTOCOL.md) | Phase E contract and five questions (E-Q1..E-Q5) |
| Thesis / grant materials (when cited) | Must map to claim taxonomy below |

**Out of scope for official episodic benchmark claims in v1.0:** organic consolidation path (M1.5), INTEGRATION diagnostics, Phase D machinery proofs cited as learning lift, v1.1 retracted runs, mock/TfIdf CI as sole evidence (see **Authoritative evaluation backend** below).

## Normative document precedence

**This document** governs Phase E empirical evaluation for in-scope claims. Where referenced documents appear to disagree, resolve in this order:

| Priority | Source |
|----------|--------|
| 1 | **This document** — locked `E_ANALYSIS_PLAN.md` (Part I + Part II) |
| 2 | **Current protocol revision** — [`E2_PROTOCOL.md`](../E2_PROTOCOL.md) v1.2 (as cited in header) |
| 3 | **Phase D authority definitions** — [`PHASE_D_COMPLETE.md`](PHASE_D_COMPLETE.md), [`D_PHASE_PROTOCOL.md`](../D_PHASE_PROTOCOL.md), [`D5_PROTOCOL.md`](../D5_PROTOCOL.md) |
| 4 | **E2 benchmark protocol** — harness rules, case tables, and constants in cited `E2_PROTOCOL.md` revision |
| 5 | **Supporting specs** — [`benchmark_environment.md`](../benchmark_environment.md), [`GRAG.md`](../GRAG.md), phase seals (consistent with 2–4) |
| 6 | **Historical documents** — audit history only; never normative for current claims |

---

# Part I — Evaluation Protocol

*What is allowed to count as evidence, on what corpus, under what rules?*

## Definitions

All definitions below reference [`E2_PROTOCOL.md`](../E2_PROTOCOL.md) v1.2. This plan does **not** redefine pass/fail semantics.

| Term | Definition (summary) |
|------|----------------------|
| **E2-EVAL-STRICT** | Official scoring mode: deterministic retrieval from sealed episode log; `official_scoring: true`; cross-session heuristics OFF |
| **E2-INTEGRATION** | Non-scoring diagnostic mode: `official_scoring: false`; `e2_outcome` MUST NOT be emitted |
| **Cold arm** | Baseline execution without warm episodic injection for the case |
| **Warm arm** | Execution with declared episodic injection from case table |
| **Lift** | Difference in trajectory score (warm − cold) under pinned `calculate_trajectory_score()` |
| **`evaluation_resolution`** | **Canonical truth** for case/summary outcome (`SCORED_*`, `NOT_SCORABLE`, etc.) — sole authority at export |
| **`e2_outcome`** | Derived at export only from `evaluation_resolution` — never a persisted source-of-truth field |
| **Retrieval hit** | Boolean per case table: observed episodic retrieval matches `expect_retrieval_hit` |
| **Fail closed** | Retrieval failure, provenance violation, or sealed-log mutation → arm failure; no partial scoring |
| **Declared episode** | Episode content from preregistered case table, sealed before arm execution — not organic consolidation |

**Research construct (lab):** episodic lift measures whether **declared frozen episodic content** available to **deterministic STRICT retrieval** changes goal trajectory scores under preregistered cases — not full “learning” or organic memory consolidation.

## Corpus declaration

### Required: v1.2 STRICT trio (`n=3_strict_trio`)

| Case ID | Injection (warm) | Goal | Expected retrieval hit | Lift constraint |
|---------|------------------|------|------------------------|-----------------|
| **E2-01** | `"My dog's name is Apollo."` | `"What is my dog's name?"` | `true` | GTE 0.10 |
| **E2-02** | `"My assistant's codename is Zephyrx7."` | `"What is my assistant's codename?"` | `true` | GTE 0.10 |
| **E2-03** | `"My dog's name is Apollo."` (both arms) | `"What is the capital of France?"` | `false` | ABS_LT 0.10 |

Source: [`E2_PROTOCOL.md`](../E2_PROTOCOL.md) § Cases (v1 — fixed set). Expectations live in `episodic_learning_cases.*`; evaluator is table-driven.

**Evidence scope label:** All official episodic lift claims from this corpus MUST carry `evidence_scope: n=3_strict_trio` until a protocol amendment expands corpus scope.

### B1 (30-case hardened corpus): **OUT of scope for Phase E v0.1**

| Decision | **Deferred** |
|----------|--------------|
| **Rationale** | B1 case registry is not yet preregistered in git; [`E_PHASE_PROTOCOL.md`](../E_PHASE_PROTOCOL.md) records B1 as planned, not authoritative. Phase E v0.1 closes empirical validation on the declared lab trio first without blocking on B1 delivery. |
| **Consequence** | Phase E Step 2 executes **trio re-run only**. Generalization claims beyond the trio are **forbidden** in E4/E5 for v0.1. |
| **Future path** | B1 inclusion requires protocol amendment (E-AP v1.1+ or `E2_PROTOCOL.md` v1.3+) with preregistered B1 case table before additional authoritative runs. |

## Authority

**Who may emit `official_scoring: true`?**

Only the STRICT benchmark path with `wiring_stage=B` (Phase B harness contract). See [`PHASE_D_COMPLETE.md`](PHASE_D_COMPLETE.md) and [`D5_PROTOCOL.md`](../D5_PROTOCOL.md).

| Rule | Enforcement |
|------|-------------|
| STRICT kernel owns retrieval truth at evaluation boundary | `e2StrictRetrieve()` + sealed log; Executive/RAG outputs non-authoritative for STRICT scoring |
| INTEGRATION MUST NOT influence official outputs | Phase D4 wiring + D5 authority meta-proof |
| Subscribers observe only | D-phase Constitutional Rule — no subscriber acquires scoring authority |
| Phase D gates prove machinery trust | **Not** empirical lift — see evidence tiers |

Phase D answered: *production retrieval must not influence STRICT benchmark scoring.*

## Exclusions

The following are **excluded** from official episodic benchmark evidence in Phase E v0.1:

| Exclusion | Reason |
|-----------|--------|
| **E2-INTEGRATION** outputs | Non-scoring diagnostic mode only |
| **Protocol v1.1** runs | Retracted — `official_scoring: false`, superseded by v1.2 |
| **Mock / TfIdf CI tier** as sole external evidence | Not a **live evaluation-backend** run with L2 env pinning (see below) |
| **Organic consolidation path** | M1.5 scope — E2-STRICT uses frozen injection log, not live consolidation |
| **Phase D machinery proofs** (D5 authority, C5 equivalence, determinism gates) | Engineering trust ≠ effect evidence |
| **GRAG bucket diagnostics** | Retrieval characterization — not STRICT official episodic lift |
| **Heuristic / cross-session retrieval** in STRICT claims | Forbidden in E2-EVAL-STRICT path |

## Evidence tiers

| Tier | Definition |
|------|------------|
| **STRICT** | Only source of **official episodic benchmark claims**. |
| **INTEGRATION** | Engineering diagnostics only — never official benchmark evidence. |
| **Phase D** | Machinery trust and authority preservation only — not empirical lift evidence. |
| **GRAG** | Retrieval quality characterization; bucket citation discipline applies when cited. |
| **Historical** | Superseded results retained only for audit history (e.g. v1.1 retraction). |
| **`n=3_strict_trio`** | Scope label — claims backed only by v1.2 trio; not generalization evidence. |

## Metrics (definition)

Referenced from [`E2_PROTOCOL.md`](../E2_PROTOCOL.md) v1.2 — **not redefined** in this plan.

| Metric / constant | Value / rule | Role |
|-------------------|--------------|------|
| `LIFT_MARGIN` | **0.10** | Pass threshold for positive lift cases (GTE) |
| `lift_constraint` | `GTE` or `ABS_LT` | Per-case table-driven constraint |
| `mean_episodic_lift` | Mean over cases with `include_in_mean_episodic_lift == true` | Aggregate (E2-01, E2-02 only) |
| `expect_retrieval_hit` | Boolean per case | Retrieval expectation |
| `evaluation_resolution` | Canonical case/summary outcome | Single authority at export |
| `kStrictEpisodeInclusionMinScore` | **0.25** (implementation) | Episode enters ranked pool when overlap ≥ threshold |
| **E2 SUCCESS (STRICT)** | All cases pass + no fail-closed arms + `mean_episodic_lift > 0` | Summary outcome |
| **E2 FAILURE (STRICT)** | Any case fail, fail-closed arm, or `mean_episodic_lift <= 0` | Valid experimental outcome when logged |

## Constants posture

| Constant | Status |
|----------|--------|
| `LIFT_MARGIN = 0.10` | Engineering constant — not claimed theoretically optimal |
| `kStrictEpisodeInclusionMinScore = 0.25` | **Provisional** — calibrated on v1.2 trio only; not from B1 or cross-validation |
| Case membership (E2-01..03) | Fixed v1.2 preregistered set |

Phase E validates **protocol correctness and evidentiary discipline** — not universal optimality of engineering constants. Constant changes require protocol amendment.

## Protocol freeze

Once this document is locked, no metric definitions, corpus membership, reporting rules, authority tiers, hypotheses, exclusions, or evaluation constants may change without a **protocol amendment** (see below). Step 2 cannot quietly redefine success.

## Protocol amendments

Any modification after protocol lock requires a **new protocol revision identifier** and **explicit supersession** of the previous revision before additional benchmark execution.

| Amendment type | Identifier pattern | Requirement |
|----------------|-------------------|-------------|
| Analysis plan revision | `E-AP v1.x` + commit SHA | Supersedes prior `E_ANALYSIS_PLAN.md`; justification required |
| E2 protocol revision | `E2_PROTOCOL.md` v1.3+ | New lock date; may not change mid-run |
| Corpus expansion (e.g. B1) | Both above + preregistered case table | No authoritative runs until amendment locked |

Amendments must be committed, cite what they supersede, and state justification per [`E_PHASE_PROTOCOL.md`](../E_PHASE_PROTOCOL.md) falsification clause. No further authoritative STRICT runs for external claims until the superseding revision is locked.

## Reproducibility prerequisites

| Level | Requirement | Status for Step 2 |
|-------|-------------|-------------------|
| **L1 — Source reproducible** | Build harness + gates from git | ✅ Phases A–D |
| **L2 — Environment reproducible** | `run_id`, `env_hash`, `index_hash`, sidecar per [`benchmark_environment.md`](../benchmark_environment.md) | **Required** for Step 2 authoritative runs |
| **L3 — Benchmark reproducible** | E2-28 two-run equivalence on declared corpus | ✅ Demonstrated on trio (Phase B) |
| **L4 — Publication package** | Independent lab reproduces every cited run | Target Step 3 (Phase E) |

Step 2 runs MUST use L2 env pinning. Phase B fingerprint `1ce31c6aa3f6987841c1a0ddecae6f9171e5ef86fc9c88601b1a017e25f669b4` is historical reference — Step 2 produces fresh pinned-env artifacts for Phase E L4 scope.

## Authoritative evaluation backend (LLM-agnostic)

Thoth’s cognitive stack is **LLM-backend agnostic**. The runtime may use **Ollama**, **llama.cpp**, **OpenAI**, or other providers via the existing abstraction — none is architecturally required for STRICT evaluation.

Phase E distinguishes:

| Term | Meaning |
|------|---------|
| **Authoritative evaluation backend** | The **live** LLM/embedding provider designated for a pinned STRICT run — model id, API endpoint, and version pins captured in the L2 environment sidecar (`env_hash`, `run_id`) |
| **`--full` / live runtime tier** | Harness tier that invokes a real backend — contrast with mock/TfIdf CI stubs used for fast regression |
| **Backend used in this project (to date)** | **Ollama** — Phase B baseline and planned Step 2 runs use Ollama as the operational evaluation backend; this is a **recorded operational choice**, not a dependency of the evaluation protocol |

**Rules:**

1. Official episodic benchmark claims require STRICT runs on the **declared authoritative evaluation backend** with L2 pinning — not mock/TfIdf CI as sole evidence.  
2. The sidecar and run manifest MUST record **which backend** was used so L4 reproduction targets that backend (or an explicitly declared equivalent under amendment).  
3. Wording like “Ollama-tier runs” is **deprecated** in Phase E docs — use **authoritative evaluation-backend runs** (live `--full` tier, env-pinned).  
4. Switching backends (e.g. Ollama → llama.cpp) for new authoritative runs requires declaring the new backend in the run manifest; it is not a protocol violation if env pins and STRICT rules are unchanged.

## B1 / E2 timing

Per [`E_PHASE_PROTOCOL.md`](../E_PHASE_PROTOCOL.md) § B1/E2 timing — fork locked in this document:

| E1 decision (this document) | E2 runs | L4 target (Phase E) |
|-----------------------------|---------|---------------------|
| B1 **deferred** ✅ | Authoritative STRICT **re-run of v1.2 trio only** | L4 on trio runs cited in E4 |
| B1 in scope (future amendment) | Authoritative STRICT on B1 registry | L4 on every cited B1 run + trio if still cited |

**Locked fork:** B1 **deferred**. E2 does not wait on B1.

---

# Part II — Analysis Plan

*What will we report, how will we interpret it, and what claims are in scope?*

## Research questions

### Primary (maps to E2 protocol)

**Does declared episodic content available to deterministic retrieval improve future goal behavior under lab conditions?**

Operationalized under E2-EVAL-STRICT as: retrieval hit expectations met and positive `mean_episodic_lift` on preregistered positive cases (E2-01, E2-02), with E2-03 negative control satisfying ABS_LT constraint.

### Secondary

| ID | Question | Evidence tier |
|----|----------|---------------|
| **RQ-2** | Does STRICT isolate the episodic variable from heuristic/cross-session leakage? | Phase D + STRICT artifact audit |
| **RQ-3** | Are results reproducible under pinned environment (L2/L3)? | Step 2 manifests + E2-28 |
| **RQ-4** | Do GRAG retrieval claims in `MYPAPER.md` remain supportable independently of episodic lift? | GRAG tier (separate from STRICT) |

## Hypotheses

Descriptive expectations under STRICT — **not causal inference claims**.

| ID | Hypothesis (descriptive) | Falsified if |
|----|--------------------------|--------------|
| **H1** | On the v1.2 trio, warm arms with declared episodic injection produce higher trajectory scores than cold arms on E2-01 and E2-02 | `mean_episodic_lift <= 0` or case expectation fail |
| **H2** | E2-03 negative control shows no spurious lift above margin | `abs(lift) >= LIFT_MARGIN` on E2-03 |
| **H3** | Retrieval hits match case table on all three cases | Any `expect_retrieval_hit` mismatch |
| **H4** | STRICT arms complete without fail-closed retrieval/provenance errors | Any fail-closed arm status |

STRICT FAILURE under preregistered criteria is a **valid outcome** — not a protocol violation.

## Reporting rules

Per [`E2_PROTOCOL.md`](../E2_PROTOCOL.md) § Reporting policy:

1. **All STRICT outcomes reported** — including failures to support H1–H4  
2. Case-level: `evaluation_resolution`, lift, retrieval hit, arm statuses  
3. Summary-level: `e2_outcome`, `mean_episodic_lift`, evaluation fingerprint  
4. INTEGRATION outputs labeled **“non-scoring diagnostic mode”** if mentioned at all  
5. GRAG claims: cite bucket and corpus per [`GRAG.md`](../GRAG.md) — not conflated with STRICT episodic lift  
6. Every official episodic claim carries `evidence_scope: n=3_strict_trio` in E4 audit  

## Statistical posture

1. **Statistical inference intentionally deferred.**
2. Phase E reports **descriptive benchmark outcomes and protocol compliance**; inferential statistical claims remain **outside the scope of Protocol v1.2**.

**Explicitly excluded from Phase E v0.1:** confidence intervals, bootstrap resampling, hypothesis tests, p-values, population parameter estimation. These belong, if anywhere, in a future revision after B1 corpus exists.

## Threats to validity and mitigations

### Internal validity

| | |
|--|--|
| **Threat** | Heuristic retrieval, cross-session memory, or Executive path contaminates STRICT isolation of the episodic variable. |
| **Mitigation** | Sealed episode log; `e2StrictRetrieve()` kernel; runtime heuristic guard (A5); Phase D authority + D5 meta-proof; `official_scoring` only on `wiring_stage=B`. |

### Construct validity

| | |
|--|--|
| **Threat** | Episodic lift may not measure “learning” in a general cognitive sense — only lab declared-episode retrieval effect. |
| **Mitigation** | Explicit construct declaration in Part I; organic consolidation path excluded; INTEGRATION labeled diagnostic; claims scoped to declared-episode STRICT construct. |

### External validity

| | |
|--|--|
| **Threat** | Results may not generalize beyond the three-case lab corpus. |
| **Mitigation** | Mandatory `n=3_strict_trio` label on all trio-backed claims; B1 deferred with generalization claims forbidden; B1 amendment required before broader claims. |

### Conclusion validity

| | |
|--|--|
| **Threat** | Readers may expect formal statistical conclusions or significance from n=3 descriptive results. |
| **Mitigation** | Statistical inference intentionally deferred; descriptive-only posture stated in Part II; E4 audit rejects unqualified generalization wording. |

## Falsification posture

Per [`E_PHASE_PROTOCOL.md`](../E_PHASE_PROTOCOL.md) § Falsification and negative results:

| Event | Required response |
|-------|-------------------|
| **L3 reproduction fails** (E2-28 mismatch on trio) | Stop; diagnose; do not publish prior numbers as current; amend plan before re-run |
| **L4 reproduction fails** (Step 3) | E-Q2 = no until resolved; claim unpublished or retracted |
| **STRICT FAILURE** on preregistered case | Report descriptively — valid outcome |
| **H1–H4 falsified** | Report per reporting rules; no quiet claim-softening |
| **B1 contradiction** (future) | N/A for v0.1 — B1 deferred |
| **E4 claim fails audit** | Remove, narrow to `n=3_strict_trio`, or defer publication |

Negative results logged honestly are **successful experiments**.

## Claim taxonomy

| Claim type | Minimum evidence required | Phase E v0.1 scope |
|------------|---------------------------|-------------------|
| Architecture correctness | Phase D seal + machinery gates | ✅ Reference |
| Benchmark machinery trust | Phase D (authority, determinism, equivalence) | ✅ Reference — not lift |
| Official episodic lift (scoped) | STRICT + `n=3_strict_trio` label | ✅ Step 2 target |
| Generalization beyond trio | STRICT on B1 registry | ❌ Forbidden (B1 deferred) |
| Retrieval quality (GRAG) | GRAG diagnostics + bucket discipline | ✅ Separate from STRICT |
| Engineering diagnostic | INTEGRATION (non-scoring) | Reference only |
| Historical comparison | Historical tier only | Audit only |

## E4 handoff

Claims audit (Phase E Step 4) MUST follow cold-read discipline per [`E_PHASE_PROTOCOL.md`](../E_PHASE_PROTOCOL.md) E-Q4:

- Re-read each in-scope sentence **without** drafting context, **or**
- Audit after **≥24 h gap** from last edit to the sentence, **or**
- **Second reviewer** who did not author the claim text

Self-audit in the same session as writing is **not** sufficient for E-Q4 green.

E4 maps every paper sentence → evidence tier + run_id + commit from this plan’s claim taxonomy.

## Step 2 handoff

Exact runs Phase E Step 2 will execute (frozen by this document):

| Field | Value |
|-------|-------|
| **Harness** | `run_episodic_learning_benchmark` |
| **Tier** | `E2-EVAL-STRICT` (`--tier strict` or default STRICT) |
| **Wiring** | `wiring_stage=B` (authoritative scoring path) |
| **Corpus** | v1.2 trio only — E2-01, E2-02, E2-03 |
| **B1 fork** | **Deferred** — no B1 cases in Step 2 |
| **Environment** | L2 — `BenchmarkContext::create()`; `run_id`, `env_hash`, `index_hash`; sidecar `logs/benchmark_env.latest.json` |
| **Runtime tier** | **`--full` / live evaluation backend** — not mock/TfIdf CI stubs |
| **Authoritative evaluation backend** | **Ollama** (project default to date); backend-agnostic stack — llama.cpp, OpenAI, or others permitted when declared and L2-pinned |
| **Protocol** | `E2_PROTOCOL.md` v1.2 constants — no mid-run changes |
| **Artifacts** | `logs/episodic_learning_benchmark.jsonl`; evaluation fingerprint; sealed episode snapshots |
| **Validation** | E2-28 scoped equivalence across two consecutive identical STRICT builds on trio |
| **Out of scope for Step 2** | INTEGRATION runs as official evidence; B1 expansion; inferential statistics |

Step 2 delivers partial **E-Q2** (L2/L3 on trio) and **E-Q3** (methodological adherence). Full L4 is Step 3.

---

## E-Q1 close-out (Step 1)

| Criterion | Status |
|-----------|--------|
| Definitions specified | ✅ Part I |
| Metrics referenced (not redefined) | ✅ Part I |
| Corpus declared (trio + B1 fork) | ✅ B1 **deferred** |
| Exclusions documented | ✅ Part I |
| Authority tiers documented | ✅ Part I |
| Evidence tiers defined (one sentence each) | ✅ Part I |
| Protocol freeze + amendments | ✅ Part I |
| Normative document precedence | ✅ Preamble |
| Claim taxonomy | ✅ Part II |
| Threats + mitigations | ✅ Part II |
| Statistical posture (descriptive only) | ✅ Part II |
| Step 2 handoff explicit | ✅ Part II |

**E-Q1 answer:** **Yes** — evaluation protocol completely specified for declared empirical scope (`n=3_strict_trio`).

**Maximum evidence scope (v1.0):** `n=3_strict_trio` only.

**Deferred:** Step 2 authoritative runs · Step 3 L4 package · Step 4 claims audit · Step 5 `PHASE_E_COMPLETE.md`

**Paused before Step 2** — await explicit implementation approval per AGENTS.md gate.

---

**Supersedes:** E-AP v1.0 (`48e7511`) — v1.1 wording clarification only  
**Commit:** (recorded on v1.1 amendment commit)
