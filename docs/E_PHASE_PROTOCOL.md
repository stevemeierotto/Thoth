# E — Empirical Validation Protocol

**Protocol version:** E v0.1  
**Status:** 🔒 **LOCKED** (2026-07-09) — E1 complete (`E_ANALYSIS_PLAN.md` E-AP v1.1); await explicit Step 2 approval  
**Supersedes:** Informal “Phase E scientific defense” label (deprecated in favor of **empirical validation**)  
**Depends on:** [`PHASE_D_COMPLETE.md`](phases/PHASE_D_COMPLETE.md) (Phase D sealed 2026-07-08), [`D5_PROTOCOL.md`](D5_PROTOCOL.md) v0.1 🔒, [`E2_PROTOCOL.md`](E2_PROTOCOL.md) v1.2 🔒, [`benchmark_environment.md`](benchmark_environment.md) (benchmark E1 ✅)  
**Checkpoint tracking:** `cursor_list.md` § **E.0.0** (E1 ✅ · EP-01 ✅ · **Step 2 locked**)

> **Scope:** Phase E is the **empirical validation layer**. Phase D proved the evaluation **machinery** preserved trust. Phase E proves **empirical claims made using that machinery** are specification-complete, reproducible, methodologically defensible, evidence-mapped, and publication-ready.

---

## Terminology (locked intent)

| Term | Meaning |
|------|---------|
| **Empirical validation** | Standard name for Phase E — validate benchmark claims and evidence chain |
| ~~Scientific defense~~ | **Deprecated** — sounds adversarial; do not use in new docs |
| **Engineering confidence** | Trust in machinery (authority, determinism, equivalence) — **Phase D** |
| **Empirical confidence** | Trust in published benchmark claims — **Phase E** |

---

## Current maturity (honest assessment)

| Area | Assessment |
|------|------------|
| Engineering rigor | **Strong** |
| Reproducibility infrastructure | **Strong** |
| Benchmark methodology | **Good but limited** (n=3 official STRICT trio; B1 not yet run) |
| Statistical rigor | **Early** — inference intentionally deferred |
| Publication readiness | **Not yet** |

Phase E does not overstate what has been demonstrated. It **closes the gap** between engineering proof and external empirical claims.

---

## Core separation (locked)

| Layer | Question | Answered by |
|-------|----------|-------------|
| **Machinery trust** | Did we build the evaluator correctly and preserve authority? | Phases A–D + D5 |
| **Empirical claims** | Are published benchmark results defensible? | **Phase E** |

> **Phase D establishes trust in the evaluation machinery. Phase E establishes trust in the empirical claims made using that machinery.**

---

## Five questions Phase E must answer (locked)

Phase E is complete when all five questions are answered **yes** with sealed evidence.

### E-Q1 — Is the evaluation protocol completely specified?

| Must specify | Source / artifact |
|--------------|-------------------|
| Definitions | `E2_PROTOCOL.md` v1.2+ |
| Metrics (definition) | `LIFT_MARGIN`, `evaluation_resolution`, `mean_episodic_lift`, retrieval hit, `GTE`, `ABS_LT`, … |
| Corpus scope | Declared case set (v1.2 trio; B1 expansion if in publication scope) |
| Exclusions | INTEGRATION non-scoring; organic consolidation path; v1.1 retraction |
| Authority | STRICT vs INTEGRATION firewall; who may emit `official_scoring` |

**Phase E validates protocol completeness — not universal optimality of engineering constants.**

### E-Q2 — Can an independent lab reproduce every published benchmark?

| Reproducibility artifact | Status target |
|--------------------------|---------------|
| Environment | E1 `run_id`, `env_hash`, `index_hash`, sidecar |
| Hashes | Evaluation fingerprint, corpus snapshot id |
| Seeds / pins | Version pins in `E2EvalConfig` |
| Run manifests | JSONL + summary artifacts per run |
| Artifacts | Frozen baselines, verification docs |

See **Reproducibility levels** below.

### E-Q3 — Are reported benchmark results methodologically defensible?

| Requirement | Mechanism |
|-------------|-----------|
| Protocol adherence | STRICT tier only for official numbers |
| Reporting rules | All STRICT outcomes reported, including failures |
| Honest failure handling | `E2_PROTOCOL.md` reporting policy |
| Threats to validity | Documented (see below) |

**Statistical inference is intentionally deferred.** The project proves **engineering behavior** under preregistered rules; it does not (in Phase E v0.1) estimate population parameters or claim formal significance.

### E-Q4 — Do all published claims map to benchmark evidence?

| Rule | Enforcement |
|------|-------------|
| Every paper sentence → evidence tier | Claims audit (Step E4) |
| **n=3 corpus tier explicit** | Any claim backed only by v1.2 trio MUST carry `evidence_scope: n=3_strict_trio` in audit — never presented as general |
| No INTEGRATION-only claims as benchmark results | STRICT firewall + audit |
| No v1.1 / mock-tier sole evidence | Benchmark env-pinning (E1 track) tier discipline |
| **No machinery proof as effect evidence** | D5 / C5 equivalence gates ≠ empirical lift claims (see Forbidden) |

**E4 audit discipline (locked):** Claims audit must be performed **cold** — re-read each in-scope sentence without drafting context, **or** after a **≥24 h gap** from last edit to the sentence under audit, **or** by a **second reviewer** who did not author the claim text. Self-audit in the same session as writing is **not** sufficient for E-Q4 green.

### E-Q5 — Is the project ready for external publication?

| Gate | Artifact |
|------|----------|
| Reproducibility package complete | Run manifests + env compare tooling |
| Evidence chain sealed | `PHASE_E_COMPLETE.md` |
| Claims audit complete | Claim → gate → commit → run_id mapping |
| Final publication seal issued | Phase E closure gate (TBD at implementation) |

---

## Metric definition vs metric validation (locked distinction)

### Metric definition (well specified today)

These are **precisely defined** in `E2_PROTOCOL.md` v1.2:

| Metric / rule | Role |
|---------------|------|
| `LIFT_MARGIN` (0.10) | Pass threshold for positive lift cases |
| `evaluation_resolution` | Canonical case/summary outcome |
| `mean_episodic_lift` | Aggregate over declared inclusion set |
| Retrieval hit | Boolean expectation per case table |
| `GTE` / `ABS_LT` | Lift constraint types |
| Cold/warm arms | Isolated A/B under sealed log |

### Metric validation (open — not Phase E’s default claim)

Separate question: **do the constants represent meaningful or optimal improvement?**

| Open question | Current status |
|---------------|----------------|
| Is `LIFT_MARGIN = 0.10` meaningful? | Engineering constant; not theoretically optimal |
| Is episode inclusion `0.25` correct? | **Provisional** (v1.2 trio only) |
| Stability across larger corpora? | **Not demonstrated** until B1 authoritative runs |

> **Phase E validates protocol correctness and evidentiary discipline — not universal optimality of engineering constants.**

Constants may be revisited at **B1** or **E2 v1.3+** with explicit protocol revision — not silently mid-run.

---

## Reproducibility levels (locked progression)

| Level | Meaning | Phase |
|-------|---------|-------|
| **L1 — Source reproducible** | Auditor can build harness + run gates from git | A–D ✅ |
| **L2 — Environment reproducible** | `env_hash`, `index_hash`, attribution on runs; **evaluation backend** (model/provider) captured in sidecar — backend-agnostic (Ollama, llama.cpp, OpenAI, …) | E1 ✅ |
| **L3 — Benchmark reproducible** | Two-run E2-28 equivalence; fingerprint stable on declared corpus | B ✅ (trio) |
| **L4 — Publication package reproducible** | Independent lab can reproduce **every cited published run** | **Phase E** |

Phase E targets **L4** for claims in external publications.

### Trust chain (existing — unusually complete)

- Environment pinning (E1)
- Hashes + evaluation fingerprint
- Immutable sealed episode log
- STRICT / INTEGRATION benchmark firewall
- Authority preservation (D4/D5)
- Deterministic evaluation kernel (`e2_eval_kernel`)

> **Who has scoring authority?** Phase D answered this explicitly. Production retrieval must not influence STRICT benchmark scoring. Many cognitive architectures fail this audit silently.

---

## Publication targets (locked — two distinct goals)

### Architecture publication

**Questions:** Did you build it correctly? Preserve authority? Keep benchmarks deterministic?

| Question | Answer today |
|----------|--------------|
| Correct construction? | Phases A–C ✅ |
| Authority preserved after evolution? | Phase D + D5 ✅ |
| Deterministic official path? | Phase B + E2-28 ✅ |

**Phase D almost completely answers architecture publication.**

### Empirical publication

**Questions:** Does episodic memory improve learning? How much? Across what distribution? With what uncertainty?

| Question | Answer today |
|----------|--------------|
| Learning lift under STRICT? | Demonstrated on **3-case lab corpus** only |
| Distribution / uncertainty? | **Descriptive** — inference deferred |
| Expanded corpus? | B1 planned, not authoritative yet |

**That is Phase E.** These are **separate publication targets** in many research programs.

---

## Threats to validity (locked — reviewer-facing)

| Threat | Question | Current answer |
|--------|----------|----------------|
| **Internal validity** | Does STRICT isolate the intended variable? | **Yes** — sealed log, kernel isolation, no heuristic leakage |
| **Construct validity** | Does episodic lift measure “learning”? | **Partial** — measures declared-episode retrieval effect under lab conditions, not organic consolidation |
| **External validity** | Generalizes beyond declared corpus? | **Not yet** — n=3 official; B1 required for broader claims |
| **Conclusion validity** | Can statistical conclusions be drawn? | **Descriptive only** — inference intentionally deferred |

### n=3 corpus consequence (locked)

The v1.2 STRICT trio (E2-01..03) is a **lab demonstration corpus**, not a generalization sample.

| Rule | Meaning |
|------|---------|
| **Tier label** | Claims supported only by trio runs MUST be tagged `n=3_strict_trio` in E4 audit and external text |
| **No silent generalization** | Wording like “episodic memory improves learning” without scope qualifier is **forbidden** unless B1-scale (or larger declared) evidence is in scope |
| **E4 gate** | Claims audit MUST list corpus tier per claim; trio-only claims cannot pass as unqualified empirical publication claims |
| **E5 gate** | `PHASE_E_COMPLETE.md` MUST state maximum evidence scope achieved (trio only vs B1-inclusive) |

> B1 expansion is a **hard dependency** for any external claim that implies generalization beyond the trio. E4/E5 may still green **scoped** trio claims — they must be **labeled**, not upgraded.

Phase E must **document** threats to validity explicitly in `PHASE_E_COMPLETE.md` — not hide behind engineering gate pass.

---

## Statistical posture (locked wording)

**Do not say:** “statistically underpowered” (reviewers may read “invalid”).

**Do say:** **Statistical inference intentionally deferred.**

| Objective | Phase |
|-----------|-------|
| Prove engineering behavior under preregistered rules | A–D, E-Q3 |
| Estimate population parameters / significance | Future (optional post-E; not required for E v0.1 close) |

---

## Falsification and negative results (locked)

Phase E must specify what happens when evidence **does not** support a claim — not only when it does.

| Event | Required response |
|-------|-------------------|
| **L3 reproduction fails** (E2-28 mismatch on declared corpus) | Stop; diagnose per Build/Test Failure Rule; **do not** publish prior numbers as current; amend analysis plan before re-run |
| **L4 reproduction fails** (independent package cannot reproduce cited run) | E-Q2 = **no** until resolved; claim remains **unpublished** or explicitly retracted |
| **B1 expansion contradicts trio pattern** (lift absent or inverted on expanded corpus) | Report descriptively per `E2_PROTOCOL.md` reporting policy; **do not** quiet-soften claims under time pressure; update E4 audit with negative tier |
| **STRICT FAILURE on preregistered case** | Valid outcome — log with preregistered criteria not met; not a protocol violation |
| **Claim fails claims audit (E4)** | Remove, narrow scope (e.g. to `n=3_strict_trio`), or defer publication — no rubber-stamp |

> Negative results are **successful experiments** when logged honestly. Phase E forbids retroactive claim narrowing without updating `E_ANALYSIS_PLAN.md` (amendment + justification) and E4 audit.

---

## Relationship to adjacent tracks

| Track | Relationship to Phase E |
|-------|-------------------------|
| **B1** | **Corpus scope fork** — locked in E1 `E_ANALYSIS_PLAN.md`. If B1 is **in** publication scope, E2 authoritative runs use B1 case registry; if **out**, E2 re-runs trio only and all external claims remain `n=3_strict_trio` tier |
| **V3 Zenodo** | Deliverable **after** E-Q4/Q5 green — not Phase E core |
| **E3** (improvements.md) | SCR harness — **separate eval ID**, not E2 “Phase E” |
| **C6 Phase 3** | Longitudinal metrics — parallel thesis honesty track |
| **G1d** | Trajectory diagnostic — not STRICT official evidence |

**B1 / E2 timing (locked):**

| E1 decision | E2 runs | L4 target |
|-------------|---------|-----------|
| B1 **in scope** | Authoritative STRICT on **B1 registry** (after B1 case table lands) | L4 on every **cited** B1 run + trio if still cited |
| B1 **deferred** | Authoritative STRICT **re-run of v1.2 trio only** | L4 on trio runs only; generalization claims **forbidden** |

E2 does **not** silently wait on B1 — the wait/fork is **explicit in E1**, not discovered mid-Phase-E.

**Naming:** E2 track **Phase E** ≠ improvements.md **E3** (SCR).

---

## Checkpoint ladder (locked)

| Step | Work | Delivers | Primary question |
|------|------|----------|------------------|
| **E0** | Lock this protocol + § E.0.0 | Terminology, five questions, validity frame | — |
| **E1** | Protocol + analysis plan lock | [`phases/E_ANALYSIS_PLAN.md`](phases/E_ANALYSIS_PLAN.md) — E-AP v1.1 ✅ | **E-Q1** |
| **EP-01** | Episodic authoritative inference harness | Engineering prereq — § E.0.0 EP-01 🔒 | — |
| **E2** | Authoritative STRICT runs | Pinned-env artifacts on **corpus declared in E1** (trio; B1 deferred) — § E.0.0 Step 2 🔒 | **E-Q2** (partial), **E-Q3** |
| **E3** | Reproducibility package (L4) | Manifests + verification doc for **every cited run** in E4 scope | **E-Q2** |
| **E4** | Claims audit (cold-read discipline) | Paper ↔ evidence tier map + `n=3_strict_trio` labels | **E-Q4** |
| **E5** | Close-out | `PHASE_E_COMPLETE.md` + max evidence scope statement | **E-Q5** |

**Step dependency rule:** E2 requires **EP-01** complete. E2 corpus = E1 declaration. E3 L4 scope = E4 cited runs. E5 cannot claim generalization scope beyond E1/E4 tier labels.

---

## Forbidden (locked)

- Claiming Phase E complete without all five questions answered
- Using INTEGRATION results as official benchmark evidence
- **Citing Phase D machinery proofs (D5 meta-proof, C5 equivalence, authority/determinism gates) as empirical evidence of learning lift or generalization** — engineering trust ≠ effect evidence
- Presenting trio-only (`n=3_strict_trio`) evidence as unqualified general empirical claims
- Presenting engineering constants as theoretically optimal without protocol revision
- Mock/TfIdf CI as sole evidence for external empirical claims
- Silent protocol constant changes mid-run
- Conflating architecture publication readiness with empirical publication readiness
- Rubber-stamping E4 claims audit in the same session as claim authoring (see E-Q4 cold-read rule)
- Quiet claim-softening after failed L3/L4 reproduction or negative B1 results without plan amendment

---

## Close-out artifact

**File:** `docs/phases/PHASE_E_COMPLETE.md`

**Mandatory minimum** (mirror Phase D seal discipline):

- Date, protocol version
- Answers to E-Q1..E-Q5 (yes + evidence pointer each)
- D1–D5 + Phase E evidence references
- Commit hash(es)
- Threats to validity summary
- **Maximum evidence scope** (`n=3_strict_trio` only vs B1-inclusive)
- Reproducibility level achieved (L1–L4)
- Final conclusion: empirical validation green — **descriptive claims only unless inference protocol added later**
- **Paused before** next external action (e.g. Zenodo V3)

---

## Exit criteria (phase complete)

1. This protocol locked in git  
2. All five questions (E-Q1..E-Q5) answered with sealed evidence  
3. `PHASE_E_COMPLETE.md` recorded  
4. Claims audit complete for in-scope publications  
5. **Pause for review** before Zenodo V3 / grant submission using new numbers  

---

**Review incorporated:** Engineering vs empirical confidence; metric definition vs validation; reproducibility levels L1–L4; statistical inference deferred; architecture vs empirical publication; threats to validity; five core questions; empirical validation terminology. **Amendment 2026-07-09:** n=3 corpus tier labeling + E4/E5 gates; falsification/negative-result path; B1/E2 timing fork; E4 cold-read audit discipline; forbid citing D-phase machinery proofs as empirical evidence.

**Status:** 🔒 **v0.1 locked** (2026-07-09, amended 2026-07-09). E1 complete — EP-01 complete — **Step 2 plan locked** (2026-07-09); paused before Step 2 implementation.
