# E2 — Episodic Memory Learning Eval (Protocol)

**Protocol version:** 1.2  
**Locked:** 2026-07-01  
**Supersedes:** v1.0, v1.1  
**Status:** 🔒 Preregistered — evaluation kernel hardened; harness retrieval migration pending re-baseline  
**Distinct from:** `episodic_memory_benchmark.md` (M1.5 pipeline verification — retrieval only)

> **Immutability:** Constants and pass/fail rules in this document may not change during an in-flight E2 **STRICT** run. Revisions require **Protocol v1.3** with a new lock date and commit SHA.

---

## Core objective

E2 is a **deterministic evaluation function**:

```
E2(query, corpus_snapshot, frozen_episode_log, model_version) → reproducible score + artifacts
```

**Forbidden in E2-EVAL-STRICT:** hidden state, cross-session memory effects, implicit heuristics, partial retrieval on error, untraced chunks.

---

## Retraction — v1.1 results

All benchmark runs executed under **Protocol v1.1** (including mock-tier SUCCESS) are:

| Field | Value |
|-------|-------|
| `provisional` | `true` |
| `superseded_by` | `1.2` |
| `official_scoring` | `false` |

**v1.1 results MUST NOT be used as baseline, promotion evidence, or comparison target.** Only **E2-EVAL-STRICT** runs under v1.2 produce official numbers.

---

## Reporting policy

All **STRICT** benchmark outcomes will be reported, including results that **fail to support the hypothesis**. E2 FAILURE under STRICT is a successful experiment when logged with preregistered criteria not met.

**INTEGRATION** outputs are diagnostic only and MUST NOT be cited as benchmark results without the label **“non-scoring diagnostic mode.”**

---

## Research question

**Does declared episodic content available to deterministic retrieval improve future goal behavior under lab conditions?**

Organic consolidation pipeline correctness is **M1.5** (separate). E2-STRICT tests the evaluation function with a **frozen episode injection log**, not the consolidation pipeline itself.

| Question | Answered by |
|----------|-------------|
| Can consolidated memory be retrieved? | M1.5 ✅ |
| Does retrieval from **declared frozen episodes** change later **goal outcomes** (STRICT)? | **E2-EVAL-STRICT** |
| Does **organic** consolidation → warm tier → retrieval → lift? | **E2-INTEGRATION** (non-scoring diagnostic) |
| Does cross-session / heuristic retrieval behave as expected? | **E2-INTEGRATION** (non-scoring) |

---

## Evaluation tiers (enforcement-level)

### E2-EVAL-STRICT — **only official scoring mode**

| Property | Requirement |
|----------|-------------|
| Cross-session retrieval | **OFF** — hard boundary |
| Heuristics (token-overlap, suppression, adaptive ranking) | **OFF** — module not active in STRICT path |
| Retrieval function | Deterministic: `f(query, corpus_snapshot, frozen_episode_log, version_pins)` |
| Retrieval failure / timeout | **Fail closed** — entire arm fails; no partial results scored |
| Episode injection log | **Sealed** before arm execution; immutable thereafter |
| Provenance | Every retrieved chunk fully traced; untraced → arm failure |
| Output | `official_scoring: true`, `scoring_tier: "STRICT"`, `e2_outcome` permitted |

### E2-INTEGRATION — **non-scoring diagnostic mode only**

| Property | Requirement |
|----------|-------------|
| Cross-session retrieval | **ON** (per service config) |
| Heuristics | **ON** (per service config) |
| Output | `official_scoring: false`, `scoring_tier: "INTEGRATION"` |
| `e2_outcome` | **MUST NOT** be emitted |
| Comparison to STRICT | **FORBIDDEN** — not “better E2,” not alternate baseline |

**Rule:** INTEGRATION results MUST NOT influence official benchmark outputs, promotion triggers, or external claims.

---

## Episode injection log — strict immutability

The episode injection log is **input-only** for retrieval. It is **not** runtime memory and **must not** be mutated during evaluation execution.

### Semantics

```
retrieval = f(query, corpus_snapshot_id, frozen_episode_log, version_pins)
```

### Enforcement mechanism (required implementation)

`SealedEpisodeInjectionLog` (see `episodic_learning_eval.h`):

1. Harness builds log entries from case table **before** arm clock starts.
2. Harness calls `seal()` — sets internal `sealed_ = true`.
3. After `seal()`:
   - Any `append()` / `clear()` / mutating API → **`std::logic_error`** (runtime hard failure).
   - Retrieval receives **`const SealedEpisodeInjectionLog&`** plus internal sealed check (const-reference alone is insufficient).
4. Deep copy at seal: entries stored in immutable snapshot; no shared mutable backing store with Memory/RAG.

**Prohibited:** mid-run injection, adaptive seeding, planner/retrieval writing to the log, consolidation side effects substituting for declared injections in STRICT mode.

---

## Kernel ownership (STRICT)

The **evaluation kernel** owns retrieval truth. The **Executive** owns runtime behavior. The harness may compare them for diagnostics but **must never substitute** executive/RAG diagnostics for kernel provenance at the STRICT evaluation boundary.

After checkpoint **A3**, every `RetrievedChunkRecord` scored in STRICT mode must originate from `e2StrictRetrieve()` — not from `RAGPipeline` step results or GRAG diagnostics.

---

## STRICT retrieval boundary

Conceptual heart of E2-STRICT — sealed input, deterministic kernel, traced output:

```
  SealedEpisodeInjectionLog (frozen case-table episodes)
              │
              │  const&  — no mutation after seal()
              ▼
       e2StrictRetrieve(query, corpus index, config)
              │
              │  pure function: no SQLite, Executive, RAG, or side effects
              ▼
       RetrievedChunkRecord[]  (chunk_id, source, source_id, validation_status)
              │
              │  evaluation boundary
              ▼
  provenanceFromStrictRetrievalResult()  →  strictProvenanceValid()
              │
              ▼
       (Phase B+) table-driven evaluator / lift
```

**Executive / RAGPipeline** may still run during Phase A migration for harness shape continuity; their RETRIEVAL outputs were **non-authoritative** at the A3 evaluation boundary. **Checkpoint A4 retires that pattern** by wiring Executive RETRIEVAL directly to `e2StrictRetrieve()` at the single dispatch point in `WorkflowEngine::executeRetrieval`.

**Vacuous retrieval guard:** if kernel `retrieval_status == SUCCESS`, expectations require episodic retrieval, and `chunk_count == 0` → `FAILED_RETRIEVAL` (not a vacuous provenance pass on empty `chunks[]`).

### STRICT kernel scoring (A3 — intentional design, not incidental plumbing)

**STRICT retrieval is not production GRAG.** The evaluation kernel (`e2StrictRetrieve` in `e2_strict_retrieval.cpp`) uses a **deliberate, purity-preserving scoring split** — distinct from `RAGPipeline` / directional GRAG scoring in the product path.

| Candidate source | Scoring mechanism | Rationale |
|------------------|-------------------|-----------|
| **Corpus chunks** (index) | `IndexManager::retrieveChunks` — existing index/TfIdf path on **pre-built, immutable index** | Harness/test builds index before the kernel call; kernel does not mutate index or engine vocab during the call |
| **Sealed evaluation episodes** | **Deterministic token overlap** — shared-token count normalized by `sqrt(|query_tokens| × |episode_tokens|)` | **Purity-driven:** calling `EmbeddingEngine::embed()` for episode gating would mutate TfIdf vocabulary state (`updateVocabulary` / document frequency) and violate the A3 formal purity criterion. Token overlap is side-effect-free on engine state |

**Not used on the STRICT kernel path:** GRAG directional blend, graph memory, warm-tier merge, heuristic suppression, or episodic embedding similarity for inclusion gating.

**Implementation note (A3):** E2-10 initially failed with episode embedding similarity because cold TfIdf engines produce zero vectors (IDF requires prior `documentFreq`). The switch to token overlap for episode inclusion was **required for purity and determinism**, not a ranking polish — see A3.5 “document, do not expand scope unless demonstration is impossible.”

#### Episode inclusion threshold — **provisional (not load-bearing)**

Episodes enter the ranked candidate pool only when overlap score **≥ `kStrictEpisodeInclusionMinScore` (0.25)** in `e2_strict_retrieval.cpp`.

**Status:** Provisional calibration on the **preregistered v1.2 trio only** (E2-01–E2-03). It is **not** derived from B1 hardened corpus, cross-validation, or a preregistered constant in protocol v1.2. Treat as **engineering gap-finding on three points**, not a locked eval parameter, until revisited at **B1** or **protocol v1.3**.

Measured overlap scores on v1.2 case text (implementation tokenizer, 2026-07-02):

| Case / arm | Query (abbrev.) | Plant message | Overlap score | Included at 0.25? |
|------------|-----------------|---------------|---------------|-------------------|
| E2-01 warm | dog's name | Apollo fact | **0.833** | yes |
| E2-02 warm | assistant codename | Zephyrx7 fact | **0.833** | yes |
| E2-03 warm/cold | capital of France | Apollo fact | **0.167** | no |

Threshold **0.25** sits in the observed gap (~0.17 vs ~0.83) for these three cases. **A4 equivalence tests and B1 expansion may require retuning or protocol promotion** — do not extrapolate STRICT retrieval quality claims from this cutoff alone.

### Episode entry schema (frozen at seal)

| Field | Required |
|-------|----------|
| `episode_id` | yes |
| `source` | `synthetic` \| `user` \| `evaluation` \| `system` |
| `content` | yes |
| `content_hash` | yes |
| `injected_at_ms` | yes (before arm start) |

---

## Version pinning (full reproducibility)

`E2EvalConfig` MUST include and artifacts MUST log:

| Field | Required | Notes |
|-------|----------|-------|
| `corpus_snapshot_id` | **yes** | E1 index / corpus fingerprint |
| `model_version_or_weights_hash` | **yes** | LLM or mock model identity |
| `embedding_model_version` | if applicable | e.g. TfIdf internal version |
| `retrieval_engine_version` | if applicable | deterministic retrieval code/config hash |

Missing required pin in STRICT → **harness init failure** (`validateStrictConfigForOfficialRun`) or **case/summary failure** (evaluator rejects config). **No implicit defaults.**

---

## Evaluation fingerprint (single reproducibility unit)

Individual pins (`corpus_snapshot_id`, `model_version_or_weights_hash`, etc.) are necessary but not sufficient for drift detection.

STRICT runs MUST compute and log **`evaluation_fingerprint`**:

```
evaluation_fingerprint = SHA-256(canonical_json)
```

Where `canonical_json` includes (stable key order):

| Field | Value |
|-------|-------|
| `protocol_version` | e.g. `"1.2"` |
| `scoring_function` | `kEpisodicLearningScoringFunction` |
| `strict_retrieval_engine` | `kE2StrictRetrievalEngineVersion` |
| `tier` | `"STRICT"` |
| `versions` | full `E2VersionPin` object |

Implementation: `computeEvaluationFingerprint()` in `e2_strict_enforcement.*`. Logged on every case row and summary in official harness output.

---

## Provenance enforcement

Every retrieved chunk in STRICT MUST include:

| Field | Requirement |
|-------|-------------|
| `source` | `corpus` \| `synthetic` \| `user` \| `evaluation` \| `system` |
| `source_id` | non-empty identifier (chunk id, episode id, file id) |
| `validation_status` | `valid` \| `invalid` \| `untraced` |

### STRICT rule (single consistent policy)

**At the evaluation boundary** (where retrieved chunks are presented to the evaluator / `strictProvenanceValid()`):

**If any chunk has `validation_status == untraced` OR missing `source` / `source_id`:**

→ Mark **evaluation arm failure** (`arm_scoring_status: "FAILED_PROVENANCE"`), fail closed, do not compute lift for that arm.

Internal retrieval stages may enrich or repair metadata **before** this boundary. Partial metadata inside the pipeline is permitted; **incomplete provenance at the boundary is not**. There is no degraded-score mode and no partial-provenance threshold.

Partial provenance is not scored in STRICT.

---

## Heuristics boundary (evaluation leakage prevention)

Token-overlap ranking, suppression filters, score-based warm demotion, cross-session warm merge, and adaptive retrieval tuning:

| Location | STRICT | INTEGRATION |
|----------|--------|-------------|
| `e2_eval_kernel` (eval + scorer + strict retrieval) | **FORBIDDEN** — not compiled in | **FORBIDDEN** |
| `run_episodic_learning_benchmark` harness | MUST NOT score heuristics path as official | diagnostic only |
| `rag.cpp` / runtime retrieval | **not in `e2_eval_kernel`** | allowed in product binary |

### Mechanical enforcement (required)

**Primary — link isolation (best):**

| CMake target | Sources | Linked by |
|--------------|---------|-----------|
| `e2_eval_kernel` | `episodic_learning_eval`, `episodic_learning_cases`, `e2_strict_enforcement`, `e2_strict_retrieval` | `basic_agent` (PUBLIC), official harness |
| `basic_agent` | full runtime including `rag.cpp` | GUI, integration tests |

`e2_eval_kernel` defines `THOTH_E2_STRICT_KERNEL=1`. **`rag.cpp` is excluded.** Heuristic symbols are not available to eval-layer translation units.

**Secondary — harness compile marker:**

`run_episodic_learning_benchmark` defines `THOTH_E2_OFFICIAL_HARNESS=1` and MUST call `validateStrictConfigForOfficialRun()` before any scored arm.

**Tertiary — runtime hard boundary (backup only):**

Heuristic entry points MAY `abort()` / throw if invoked with `E2EvalConfig::tier == STRICT`. **Runtime flag alone is insufficient** as sole enforcement. Required in implementation checkpoint **A5** (see `cursor_list.md` § E2).

The runtime guard is **diagnostic only** — it detects violations; it **never redirects, repairs, or substitutes** kernel retrieval.

### Enforcement philosophy

Compile-time exclusion prevents accidental linkage. Static audits verify intended dispatch. The runtime guard detects architectural regressions during execution. **No single mechanism is considered sufficient on its own.**

| Layer | Role |
|-------|------|
| **Compile-time** (tier 1) | Prevents accidental linkage of heuristics into the eval kernel |
| **Static audit** (tier 2) | Verifies STRICT dispatch references the kernel, not RAG |
| **Runtime guard** (tier 3) | Detects miswired STRICT execution reaching heuristic entry points |

**No silent fallback (architectural invariant):** Under STRICT, retrieval either uses `e2StrictRetrieve()` or **fails immediately**. STRICT **never degrades** into heuristic retrieval.

**Authoritative STRICT signal:** Execution/evaluation context (`E2EvalConfig` propagated with the plan or arm) — **not** environment variables alone.

**Signal precedence:**

| Layer | Determines |
|-------|------------|
| **Compile-time** (`e2_eval_kernel`, `THOTH_E2_STRICT_KERNEL`) | Which retrieval capabilities exist in a given binary/target — heuristic code excluded from kernel TUs |
| **Runtime context** (`E2EvalConfig::tier` on execution/evaluation context) | Which retrieval **mode is requested** for this arm/plan/step |
| **Impossible/contradictory combinations** | Architectural configuration errors — **fail closed** (e.g. STRICT mode reaching heuristic entry → guard hard fail, not silent fallback) |

Environment variables (`THOTH_*`) are harness convenience only — **never authoritative** for STRICT mode selection or guard arming.

**Failure domains (intentional distinction):**

| Domain | Source | Meaning |
|--------|--------|---------|
| **Operational retrieval failure** | A3+ kernel / boundary (`FAILED_RETRIEVAL`, `FAILED_PROVENANCE`, …) | Expected typed retrieval outcome within the STRICT lab function |
| **Architectural invariant violation** | A5 runtime guard (`LINK:RUNTIME_HEURISTIC`) | STRICT execution reached heuristic retrieval — **broken wiring**, not a retrieval result |

**A3 → A4 → A5 transition:** A3 temporarily allowed Executive execution for harness continuity while STRICT evaluation ignored Executive retrieval. **A4 retires that pattern** by wiring Executive directly to the kernel. **After A4, any heuristic retrieval under STRICT is an architectural regression** — A5 enforces this. No runtime carve-out for pre-A4 continuity behavior.

### Gate priority (enforcement stack)

| Tier | Mechanism | Role |
|------|-----------|------|
| **1** | `e2_eval_kernel` compile-time exclusion | **Source of truth** — pre-build guarantee |
| **2** | Linker/symbol audit on scored-path object files | **Verification** — does not substitute for tier 1 |
| **3** | Runtime guard, unit tests, wiring-state gate | Behavioral backup |

Artifacts SHOULD include `scoring_block_reason` when scoring is blocked (e.g. `WIRING:A2`, `PROVENANCE:UNTRACED`, `LINK:RUNTIME_HEURISTIC`).

### Pending wiring (not blocking protocol)

Phase A migration checkpoints wire the harness incrementally. **Official benchmark authority begins only at Phase B re-baseline** (`official_scoring: true`, `e2_outcome` permitted) — **after A4 proves harness–executive retrieval equivalence**.

| Checkpoint | Kernel retrieval @ boundary | Official scoring |
|------------|----------------------------|------------------|
| A1–A2 | No | No |
| A3 | Yes (`e2StrictRetrieve`) | No — kernel verification only |
| A4–A5 | Yes (+ executive / runtime guard) | No |
| **Phase B** | Yes | **Yes** — authoritative SUCCESS/FAILURE |

Until Phase B, treat all harness output as **non-authoritative** regardless of exit code.

During checkpoints **A1–A2**, harness runs use **evaluation-disabled mode** (`scoring_enabled: false`, no `e2_outcome`, `official_scoring: false`). During **A3–A5**, harness runs use **kernel-verified mode** (`retrieval_enabled: true`, `evaluation_boundary_verified: true`, still `official_scoring: false`, no `e2_outcome`). See **`cursor_list.md` § E2** for full checkpoint plan.

### Observation layer — `StepResult.run_block_reason` (Phase B2)

Causal separation: **event (A5 throw) → observation (B2 record) → evaluation (Phase B3 interpret)**.

| Rule | Meaning |
|------|---------|
| **Semantic freeze** | `StepResult.run_block_reason` meaning is locked after B2 — later phases must not normalize or repair transport |
| **B3 consume-only** | Evaluation **must not modify** `StepResult`; read and copy mechanically only |
| **`NONE`** | Absence of block signal — **not** implicit success; B3 must not treat as `SCORED_SUCCESS` without arm confirmation |
| **Non-`NONE`** | Raw fact that guard event was captured at workflow boundary |

### Export layer — JSONL projection (Phase B4)

Causal separation continues: **evaluation (B3 compute) → export (B4 serialize)**.

| Field | Role | When emitted |
|-------|------|--------------|
| **`evaluation_resolution`** | **Canonical truth** | Always when B3 resolution is set |
| `scoring_block_reason` | Metadata view of `NOT_SCORABLE` (protocol string) | Case/summary when `evaluation_resolution == NOT_SCORABLE` |
| `e2_outcome` | **Derived at export time only** — never a persisted source-of-truth field | Only when `evaluation_resolution` is `SCORED_*` |
| `e2_outcome_detail` | Optional debug companion (non-authoritative) | When resolution is set |
| `not_scorable_by_reason` | Rollup breakdown by `run_block_reason` | Summary when resolution rollup is active |
| `success_rate` | Pass rate over **scorable cases only** | Summary when resolution rollup is active |

**Single-authority rule:** Export reads `evaluation_resolution` first; **never infer resolution from `e2_outcome`**. All JSONL construction for case and summary payloads goes through `caseEvaluationToJson()` / `episodicLearningSummaryToJson()` — no inline duplicate serializers in the harness.

### Official harness (Phase B5)

**Measurement theory boundary:** B5 establishes the deterministic evaluation kernel with reproducibility gate and failure taxonomy — not merely a checkpoint flag.

| Rule | Meaning |
|------|---------|
| **Authoritative stage** | `THOTH_E2_WIRING_STAGE=B` only — no `OFFICIAL` alias |
| **Official Harness Invariant** | Only `B` may emit `official_scoring: true` |
| **Scored loop** | Single `runScoredEvaluationLoop()` — zero `wiring_stage` branches inside |
| **`SCORING`** | Configuration of the same loop — not an alternative loop |
| **Retrieval canonicalization** | All retrieval outputs sorted/normalized before scored loop (STRICT kernel: score + `chunk_id` tie-break) |
| **Protocol freeze** | From B5, `evaluation_resolution` and `e2_outcome` meaning are immutable; later phases extend metadata only |
| **Reproducibility (E2-28)** | Two `B` runs equivalent iff scoped fields + diagnosis bucket match — not byte-identical JSONL |

---

## Retrieval failure behavior (STRICT)

| Event | STRICT behavior |
|-------|-----------------|
| Retrieval service error | Arm **FAIL**; `arm_scoring_status: "FAILED_RETRIEVAL"` |
| Retrieval timeout | Arm **FAIL** |
| `SUCCESS` but zero chunks when episodic retrieval required | **`FAILED_RETRIEVAL`** (vacuous guard — not a provenance pass) |
| Empty result when case expects no episodic hit | Scored via table expectations (not a service error) |
| Partial chunk set after error | **Not allowed** — fail closed, discard partial |

---

## Design: cold vs warm A/B (STRICT)

Both arms share the same `corpus_snapshot_id` and `E2EvalConfig` version pins.

| Arm | STRICT setup |
|-----|--------------|
| **Cold** | `SealedEpisodeInjectionLog` empty unless `cold_arm_pre_consolidated` (E2-03); deterministic retrieval |
| **Warm** | Same snapshot + **pre-sealed** injection log from case table (explicit episodes only); new controller session id |

**Isolation invariant:** No artifacts from the cold arm DB or execution path are reused by the warm arm except episodes **declared in the case table injection spec** and frozen before warm arm start.

Cross-session SQLite warm reads are **not** the warm-arm mechanism in STRICT.

**Scope limit — STRICT vs organic consolidation:** E2-STRICT warm arms use **pre-sealed case-table episodes** (`buildStrictInjectionLogFromCaseTable`), not the product consolidation pipeline (`plantAndConsolidate` → episodic → warm tier). M1.5 validates organic consolidation produces retrievable memory; E2-STRICT validates deterministic retrieval from **declared frozen input**. Organic consolidation → warm → retrieval → lift is **E2-INTEGRATION** (diagnostic, non-scoring) only.

**One sealed log per arm:** exactly one `buildStrictInjectionLogFromCaseTable` call per arm invocation; passed by const reference thereafter. STRICT arm execution must not mutate episodic storage during A2.

**Checkpoint A1 (implementation):** No retrieval, scoring, executive, or RAG behavior changes during A1. That checkpoint exists solely to prove deterministic construction of STRICT sealed injection logs from the frozen case table (`buildStrictInjectionLogFromCaseTable`). Cold arm: empty sealed log unless `cold_arm_pre_consolidated` declares a pre-existing episode (E2-03).

**Checkpoint A2 (implementation):** Remove runtime episodic creation (`plantAndConsolidate`) from STRICT arm execution. One sealed log per arm via `buildStrictInjectionLogFromCaseTable`; harness runs `runCaseArm` plumbing smoke with evaluation disabled. No kernel retrieval, scoring, or `e2_outcome`.

**Checkpoint A3 (implementation):** Wire `e2StrictRetrieve()` at the evaluation boundary; map results via `provenanceFromStrictRetrievalResult()`. **`official_scoring` remains false** — A3 proves kernel retrieval correctness, not benchmark outcome. Executive may run for harness continuity; its outputs are non-authoritative for STRICT evaluation. **This continuity pattern is temporary and retired at A4.** Harness default wiring stage after A3: `A3`. **STRICT kernel scoring:** corpus via index retrieval; sealed episodes via deterministic token overlap (not GRAG/embed gating) — see **§ STRICT kernel scoring (A3)**. Episode inclusion threshold **0.25 is provisional** (v1.2 trio only). See **§ STRICT retrieval boundary** and **`cursor_list.md` § Checkpoint A3**.

**Checkpoint A4 (implementation):** **Single dispatch decision point** — `WorkflowEngine::executeRetrieval` branches on `E2EvalTier::STRICT` to call `e2StrictRetrieve()` directly; NON-STRICT continues `RAGPipeline::retrieveRelevant`. Harness injects one sealed log per arm into step context before `execute_goal`; Executive does not build sealed logs. Harness retains a direct boundary call for **equivalence proof** (success and failure paths); provenance for STRICT evaluation reads Executive RETRIEVAL output. **`official_scoring` remains false.** Default wiring stage after A4: `A4`. See **`cursor_list.md` § Checkpoint A4**.

**Checkpoint A5 (implementation):** Runtime guard at `RAGPipeline::retrieveRelevant` — diagnostic fuse only; inspects authoritative `E2EvalConfig::tier` propagated from execution context (not `THOTH_*` env alone). On STRICT tier, throws `E2RuntimeHeuristicGuardViolation` (`LINK:RUNTIME_HEURISTIC`) — detect only, no redirect/repair. **`official_scoring` remains false.** Default wiring stage after A5: `A5`. See **`cursor_list.md` § Checkpoint A5**.

---

## Cases (v1 — fixed set)

| ID | Injection (warm arm) | Goal | Expected retrieval hit | Expected lift |
|----|----------------------|------|------------------------|---------------|
| **E2-01** | `"My dog's name is Apollo."` | `"What is my dog's name?"` | `true` | `>= 0.10` |
| **E2-02** | `"My assistant's codename is Zephyrx7."` | `"What is my assistant's codename?"` | `true` | `>= 0.10` |
| **E2-03** | `"My dog's name is Apollo."` (both arms) | `"What is the capital of France?"` | `false` | `abs(lift) < 0.10` |

Expectations live in `episodic_learning_cases.*`. Evaluator reads table only — no case-ID branching.

---

## Scoring freeze

STRICT evaluates using **`ExecutiveController::calculate_trajectory_score()`** as pinned at run start. `final_success_score` equals that value.

No changes to scoring function during an in-flight STRICT run.

---

## Locked pass / fail criteria (STRICT only)

### Constants

| Constant | Value |
|----------|-------|
| `LIFT_MARGIN` | **0.10** |
| `BINARY_PASS` | cold not COMPLETED AND warm COMPLETED satisfies GTE on positive cases |

### Table-driven per-case evaluation

| Column | Meaning |
|--------|---------|
| `expect_retrieval_hit` | Observed injection/corpus hit matches boolean |
| `lift_constraint` | `GTE` or `ABS_LT` |
| `forbidden_retrieval_tokens[]` | Any match in either arm → case fail |
| `include_in_mean_episodic_lift` | E2-01, E2-02 only |

### E2 SUCCESS (STRICT — all required)

1. All cases pass table expectations  
2. No arm failed closed (retrieval, provenance, sealed-log violation)  
3. `mean_episodic_lift > 0` over cases with `include_in_mean_episodic_lift == true`

### E2 FAILURE (STRICT — any triggers)

- Any case fails expectations  
- Any fail-closed arm status  
- `mean_episodic_lift <= 0`

---

## Harness contract

| Item | Value |
|------|-------|
| Binary | `run_episodic_learning_benchmark` |
| JSONL | `logs/episodic_learning_benchmark.jsonl` |
| Official mode | `--tier strict` or default STRICT |
| Diagnostic mode | `--tier integration` (non-scoring) |
| E1 | One `run_id`; attribution on all STRICT arms |

### STRICT artifact fields (required)

- `scoring_tier`: `"STRICT"`
- `official_scoring`: `true`
- `e2_eval_config`: full `E2EvalConfig` JSON
- `evaluation_fingerprint`: `{ fingerprint_hash, canonical_json }`
- `episode_injection_log`: sealed snapshot
- `retrieved_chunks[]`: with `source`, `source_id`, `validation_status`
- `e2_outcome`: `SUCCESS` \| `FAILURE` (summary only)

### INTEGRATION artifact fields (required)

- `scoring_tier`: `"INTEGRATION"`
- `official_scoring`: `false`
- **No** `e2_outcome` field
- Diagnostic trace fields only

---

## Enforcement summary

| Violation | When detected | Result |
|-----------|---------------|--------|
| Mutate sealed episode log | Runtime | `std::logic_error`; arm FAIL |
| Heuristic active in STRICT path | Build or runtime | Build fail or hard abort (`LINK:RUNTIME_HEURISTIC`) — **architectural violation**, not `FAILED_RETRIEVAL` |
| Cross-session read in STRICT | Runtime | Arm FAIL |
| Retrieval error/timeout STRICT | Runtime | Arm FAIL (fail closed) |
| Untraced chunk STRICT | Runtime | Arm FAIL (`FAILED_PROVENANCE`) |
| Missing version pin | Harness init / evaluator | Run abort or case FAIL (no defaults) |
| `strictDefaults()` used for official scoring | Evaluator | Case FAIL (`STRICT missing required version pins`) |
| Emit `e2_outcome` in INTEGRATION | Harness | Build fail or assert |
| Compare INTEGRATION to STRICT | Process / docs | Protocol violation |

---

## Test IDs

| ID | Tier | Asserts |
|----|------|---------|
| E2-01 | STRICT | Retrieval hit + lift ≥ margin |
| E2-02 | STRICT | Preference injection case |
| E2-03 | STRICT | No pollution; \|lift\| < margin |
| E2-04 | STRICT | JSONL + sidecar + version pins |
| E2-05 | STRICT | Cognitive metrics attribution |
| E2-06 | INTEGRATION | Diagnostic run; `official_scoring: false`; no `e2_outcome` |
| E2-07 | STRICT | `validateStrictConfigForOfficialRun` + evaluation fingerprint |

---

## Out of scope (v1.2)

- Full `runtime_retrieval_service` refactor (unless required to satisfy STRICT invariants during wiring review)
- M4 restore, C6 Phase 3, E3 SCR, GUI
- Using INTEGRATION results for promotion

---

## Implementation sequence

**Checkpoint detail:** `cursor_list.md` § E2 (pause between each sub-checkpoint; build + tests green).

1. **Protocol v1.2** — this document ✅  
2. **Schema** — `E2EvalConfig`, `SealedEpisodeInjectionLog` in `episodic_learning_eval.h` ✅  
3. **Evaluation kernel** — `e2_eval_kernel` CMake target, `e2_strict_enforcement`, `e2_strict_retrieval` ✅  
4. **No implicit defaults** — `validateStrictConfigForOfficialRun`, evaluator pin rejection ✅  
5. **Evaluation fingerprint** — `computeEvaluationFingerprint` logged by harness ✅  
5.0 **Wiring contract** — evaluation-disabled (A1/A2), kernel-verified (A3–A5), gate priority, scope limits in this doc ⏳  
5.5 **Embedding pin correctness** — fix `\u0002` int→char coercion; semantic pin validation ✅  
6a **A1** — `buildStrictInjectionLogFromCaseTable`; E2-08 + determinism; evaluation-disabled harness only ✅  
   *No retrieval, scoring, executive, or RAG changes during A1 — deterministic STRICT sealed log construction from the frozen case table only.*  
6b **A2** — decouple consolidation from STRICT path; E2-09 + E2-09b; scope-limits doc ✅  
6c **A3** — `e2StrictRetrieve()` @ evaluation boundary; `provenanceFromStrictRetrievalResult`; E2-10; `retrieval_enabled: true`; **not** official scoring ✅  
   *Kernel scoring documented § STRICT kernel scoring (A3): token overlap for episodes (purity); 0.25 inclusion threshold **provisional** on v1.2 trio only.*
   *Kernel consumes sealed log and produces boundary provenance independent of Executive/RAG. No ranking redesign unless E2-10 blocked. No `e2_outcome`.*
6d **A4** — executive RETRIEVAL → strict path via single dispatch in `executeRetrieval`; harness–executive equivalence; tier-2 static audit ✅  
6e **A5** — runtime guard in `RAGPipeline::retrieveRelevant`; `LINK:RUNTIME_HEURISTIC` on STRICT miswire; E2-11 ✅  
7. **STRICT re-baseline run** — official SUCCESS/FAILURE only after 5.5 + 6a–6e  
8. **Close-out** — `completed_improvements_log.md` (STRICT only); INTEGRATION tier harness (Phase C)

---

## System framing

E2 is a **constrained execution environment with explicit failure semantics** — closer to a test kernel or minimal evaluation VM than a benchmark pipeline. Logical separation (tiers, sealed logs, provenance, fail-closed) is in place; **link isolation** (`e2_eval_kernel` vs runtime `rag.cpp`) prevents architecture drift from bypassing eval rules in the eval layer itself.

---

*Preregistered v1.2: deterministic STRICT lab function, sealed episode log, version pins, fail-closed retrieval, provenance enforcement, INTEGRATION non-scoring, v1.1 superseded.*
