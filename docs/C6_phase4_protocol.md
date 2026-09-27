# C6 Phase 4 — Prospective Longitudinal Protocol

**Protocol version:** C6.4 v1.0  
**Status:** 🔒 **Locked** 2026-09-27. Owner accepted this text, including `official_baseline`, `trend=not_evaluable` with `reason=no_prior_window`, and promotion on two official windows. This lock does not authorize Phases 1–8.  
**Supersedes for prospective work:** nothing inside C6.3. C6.3 v0.2.1 stays sealed. This document is the successor contract for new collection only.  
**Depends on:** C6.3 v0.2.1 (statistical rules and sealed machinery, unchanged), E1 environment records ([`benchmark_environment.md`](benchmark_environment.md)), E2 v1.2 and Phase E v0.1 (sealed; not modified), C3 and C5 completion records (sealed mechanism tests)  
**Does not modify:** `C6_phase3_protocol.md`, `C6_phase3_analyzer_contract.md`, `C6_phase3_reporting_contract.md`, `C6_phase3_join_contract.md`, their fixtures, or their goldens

If informative text conflicts with a normative rule in this document, the normative rule governs. If this document conflicts with C6.3 v0.2.1, C6.3 still governs historical C6.3 artifacts. This document governs evidence collected under `protocol_version: C6.4 v1.0` after an explicit window open.

**Protocol Lock Rule:** This document is locked. Do not silently revise it during implementation. If implementation reveals the protocol must change, stop and request owner approval before editing this file or code that depends on it.

---

## Document structure

| Class | Sections | Binding? |
|-------|----------|----------|
| **Normative** | Question, population, windows, outcomes, schema, fingerprints, metrics, scopes, segments, safety, promotion, instrumentation contract, forbidden rules | **Yes** — SHALL/MUST |
| **Informative** | Rationale, paper boundary, engineering phase map | **No** — context only |

Owner decisions recorded here were made on 2026-09-27 before this draft was written. They are inputs to the contract, not analyzer output.

---

## One sentence

C6.4 asks whether, across prospective windows of ordinary Thoth goal execution on a pinned deployment, observed goal performance is improving, stable, or declining under the measurement rules carried forward from C6.3.

The answer is observational. A negative, stable, not-yet-comparable baseline (`trend=not_evaluable`), insufficient, or incomplete result is a valid result.

---

## Informative — What this protocol is for

C6 Phase 1 already records one `GOAL_COGNITIVE_METRICS` row per terminal goal. C6.3 built the longitudinal reader and then defined “official” as a `tier=full` Ollama-era harness terminal. That definition cannot be satisfied by the deployed `InferenceClient` stack, and no official C6.3 window was collected.

C6.4 keeps the measurement rules and changes the population and the environment identity. The observations are ordinary GUI Run and API goal executions. The deployment under test is whatever `InferenceClient` the pinned build is configured to use. The current default provider is `llama_cpp`. Ollama remains a supported provider. C6.4 does not require Ollama.

C6.4 does not replace:

| Layer | Question | Status under C6.4 |
|-------|----------|-------------------|
| **C3** | Does the reflection policy fire on deterministic faults? | Sealed mock mechanism test. Not an authoritative live study |
| **C5** | Do injected failures stay bounded and structurally well-behaved? | Sealed mock fault-injection test. Not a live-model suite |
| **E2 / Phase E** | Did declared-episode lift appear on the sealed Ollama run? | Sealed. `mean_episodic_lift = 0.0`, scope `n=3_strict_trio` |
| **B1** | Is retrieval quality improving on a fixed corpus? | Separate. Not this protocol |
| **C6.4** | Over prospective operational windows, what happens to goal outcomes? | This document |

---

# Normative

---

## Primary invariant

C6.4 analysis SHALL NOT influence planning, retrieval, memory, strategy selection, prompting, scoring, or the outcome of a goal.

Recording provenance on a goal is allowed. Choosing or rewriting goals, prompts, retrieval weights, or strategy selection in order to change a C6.4 result is forbidden.

---

## No causal claims

C6.4 reports statistical descriptions of recorded goals.

It SHALL NOT be stated, in a C6.4 report or in text that cites a C6.4 report as proof, that experience, memory, EGAR, GRAG, strategy injection, consolidation, or the model caused a change in outcomes, or that the model learned, or that task-completion efficiency improved.

Experience-present splits (plan reuse, strategy injection, post-consolidation) are associations. They are not causes.

A claim about task-completion efficiency requires a later protocol that preregisters the metric and the decision rule before data collection. It SHALL NOT be added to C6.4 after results are seen.

---

## Historical compatibility

C6.4 begins at a prospective boundary. It SHALL NOT rewrite, relabel, rehash, or re-scope any of the following:

| Artifact | Rule |
|----------|------|
| C6.3 v0.2.1 protocol, contracts, fixtures, goldens, and the 2026-07-12 exploratory summary | Sealed. No official C6.3 longitudinal window was collected |
| C3 records and mock reflection runs | Remain mock mechanism evidence |
| C5 records and mock robustness runs | Remain mock fault-injection evidence |
| E1 environment rows and their `environment_hash` | Unchanged. Not recomputed |
| E2 protocol and Phase E v0.1 | Sealed. `mean_episodic_lift = 0.0` on `n=3_strict_trio` stands |
| Historical `BenchmarkTier` strings `MOCK`, `DEV`, `FULL`, `OLLAMA` | Remain those strings. `OLLAMA` is not redefined as `FULL` or as `authoritative` |
| Historical reports that cite the above | Unchanged |

Rows written before the C6.4 schema boundary have no C6.4 `evaluation_tier` and no `window_id`. Absence of those fields SHALL NOT be backfilled. Such rows SHALL NOT enter a C6.4 official cohort.

Cross-walking a historical `OLLAMA` or `FULL` row into `evaluation_tier=authoritative` is forbidden. A future llama.cpp run SHALL NOT be described as a continuation of the sealed Ollama E2 result, and the two SHALL NOT be pooled into one mean.

---

## Version identity

| Field | Value under this draft | Rule |
|-------|------------------------|------|
| `protocol_version` | `C6.4 v1.0` | Identity of this locked contract |
| `metric_schema_version` | `1.0` | Same computation definitions as C6.3 metric schema 1.0 for the metrics this protocol still infers. Not a license to add trend labels |
| `environment_schema_version` | `c64-env-1` | Present only on prospective C6.4 environment pins and goal rows. Historical E1 rows omit it |

Comparing numbers computed under different `protocol_version` or `metric_schema_version` values in one official cohort is forbidden.

---

## Research question

Across prospective operational windows on a pinned deployment, is the success rate of eligible Thoth goals improving, stable, or declining, relative to the preceding comparison window, under the rules in this document?

Secondary fields describe cost, reflection, retrieval, and experience-present segments. They do not receive an improving or declining label under this protocol.

---

## Population

### What counts as a goal observation

A C6.4 goal observation is one terminal execution of one plan produced by ordinary Thoth goal execution:

| Source | Eligible as a goal observation |
|--------|--------------------------------|
| GUI goal-banner **Run** that starts or restarts executive execution of the displayed session goal | Yes, when the other eligibility fields are present |
| API goal execution that calls the same executive goal path | Yes, when the other eligibility fields are present |
| Plain chat that never creates or executes a plan | **No** |

There is no artificial 30-item benchmark in this protocol. The E2 trio SHALL NOT be repeated to manufacture n. A goal SHALL NOT be copied to increase the sample. If fewer than 30 eligible goals occur, the report states insufficient evidence.

### Identity of one observation

One observation is one `plan_id`. A retry that executes another attempt is a new `plan_id` and a new row. The earlier row stays. Restarts of the process SHALL NOT delete rows and SHALL NOT append a second outcome for an existing `plan_id`. Two rows with the same `plan_id` and conflicting outcomes are recognized malformed evidence.

### Field classes

Names below are existing record names unless marked **new**.

| Field | Class | Name |
|-------|-------|------|
| `plan_id` | Required for eligibility | existing metrics field |
| `session_id` (non-empty) | Required for eligibility | existing |
| `goal_started_at_ms` | Required for eligibility and window membership | existing |
| `outcome` | Required for eligibility when terminal | existing. `completed`, `failed`, or `aborted` |
| `window_id` | Required for official scope | **new**. Copied from the open window. Absent means the goal is not C6.4 official evidence |
| `protocol_version` | Required for official scope | `C6.4 v1.0` |
| `environment_schema_version` | Required for official scope | `c64-env-1` |
| `evaluation_tier` | Required for official scope | **new**. Official cohort requires `authoritative` |
| `inference.backend_name` | Required for official scope | existing E1 inference field. This is the provider identity (`llama_cpp`, `ollama`, or a later registered name). Not a rename of `BenchmarkTier` |
| `model.llm_model` | Required for official scope | existing |
| `model.embedding_model`, `model.embedding_method` | Required for official scope | existing |
| `prov.thoth_git_sha`, `prov.basic_agent_git_sha` | Required for official scope | existing code identity |
| `environment_hash` | Required for official scope, recorded as emitted | existing E1 hash. Not rewritten for old rows. Not the sole C6.4 cohort key |
| `c64_cohort_fingerprint` | Required for official scope | **new**. Defined below. Cohort equality uses this value |
| `goal_finished_at_ms` | Required once the goal is terminal | existing |
| `run_id` | Optional | existing benchmark attribution. Operational GUI/API goals may omit a harness `run_id` when `window_id` is set |
| `plan_reused` | Segment | existing bool on the metrics row |
| Strategy join (`STRATEGY_INJECTION`, `PLANNER_CONTEXT_ASSEMBLY`) | Segment | existing app-log events. `session_id` on those events is required for a resolved join |
| Consolidation join | Segment | existing `memory_consolidation` / `consolidation_committed` traces |
| `reflection_count`, `revisions_count` | Descriptive | existing |
| `total_tokens` (and `planning_tokens`, `synthesis_tokens` when present) | Descriptive | existing |
| `retrieved_chunk_count`, `grag_alpha` | Descriptive | existing |
| `total_wall_clock_ms` | Descriptive | existing |
| `goal` text | Not a metric | existing. SHALL NOT be clustered or used as a success proxy |

---

## Outcomes

| Situation | Treatment |
|-----------|-----------|
| `outcome=completed` | Eligible. Counts as success |
| `outcome=failed` | Eligible. Stays in the denominator. SHALL NOT be dropped |
| `outcome=aborted` | Recorded. Excluded from the success-rate denominator |
| Goal still open at analysis time (no terminal `outcome`) | Not eligible. Disclosed as `open_at_analysis`. SHALL NOT be imputed as failed or completed |
| Retry | New `plan_id`, new observation. Prior row unchanged |
| Chat with no plan execution | Not an observation |
| Same `plan_id` emitted twice with the same terminal payload | One observation. Exact duplicate lines SHALL NOT be counted twice |
| Same `plan_id` emitted twice with different outcomes | Recognized malformed evidence. The report is incomplete |

Success rate is `completed / (completed + failed)` among eligible goals in the window. This is the C6.3 definition.

---

## Window membership time

Membership uses the C6.3 clock rule. Do not invent a different one.

| Rule | Definition |
|------|------------|
| Clock | UTC Unix epoch milliseconds |
| Membership | `goal_started_at_ms` inclusive in `[window_start_ms, window_end_ms]` |
| Finish after end | The goal is in the window if its start is in range. Its terminal outcome is used when present at analysis time |
| Start before the window | Not in the window, even if it finishes inside |
| Prior comparison span | `[window_start_ms − 28 days, window_start_ms)` using the same millisecond length as C6.3, `28 × 86_400_000` |

A goal is not moved into a window after the fact because its result was favorable.

---

## Prospective window

An official C6.4 window exists only if an operator open record was written **before** any goal that is intended to count toward it.

### Open record

The open record SHALL contain:

| Field | Content |
|-------|---------|
| `window_id` | New identifier. SHALL NOT be reused |
| `protocol_version` | `C6.4 v1.0` |
| `metric_schema_version` | `1.0` |
| `environment_schema_version` | `c64-env-1` |
| `window_start_ms` | Open time, UTC epoch ms |
| `planned_end_ms` | `window_start_ms + 28 × 86_400_000` |
| `evaluation_tier` | `authoritative` |
| `inference.backend_name` | Provider actually configured |
| Model, embedding, git SHAs | As in the field table |
| `c64_cohort_fingerprint` | Pin at open |
| Evidence paths | Metrics log, app log, decision trace, environment sidecar used for this window |
| Operator note | Who opened it, and that collection is prospective |

While that window is the open official window, qualifying GUI Run and API goal executions SHALL receive its `window_id` and pin automatically. The operator SHALL NOT choose, after the outcome, which goals keep the id.

Goals executed when no official window is open SHALL still log normal `GOAL_COGNITIVE_METRICS` when the product already does so. They SHALL NOT receive a `window_id` later.

Closing the window writes `window_end_ms` (the planned end, or the earlier termination time in § Fingerprint changes) and then the analyzer may run. The analyzer SHALL NOT open or extend a window.

### Early inspection

Health counts (goal count, missing provenance, malformed rows, fingerprint equality) MAY be inspected during the window. Success rate SHALL NOT be used to decide to stop early, drop goals, change thresholds, or change the pin in order to obtain a direction. Thresholds in this document are fixed before the first goal of the window.

---

## Evaluation tier and provider

These are different facts.

| Concept | Field | Values | Meaning |
|---------|-------|--------|---------|
| Evidence class | `evaluation_tier` (**new**) | `mock`, `dev`, `authoritative` | How the run was produced |
| Provider | `inference.backend_name` (existing) | `llama_cpp`, `ollama`, or a later name registered by an accepted protocol revision | Which `InferenceClient` backend actually ran |
| Historical E1 class/provider mixture | `runtime.tier` | `MOCK`, `DEV`, `FULL`, `OLLAMA`, `GUI`, `UNKNOWN` | Left as written. Not the C6.4 cohort key |

`evaluation_tier=authoritative` means the goal or harness used the configured `InferenceClient` on the pinned deployment. It does not mean Ollama, and it does not mean the legacy token `FULL`.

`evaluation_tier=mock` means scripted or fault-injected execution (C3, C5, episodic mock). `evaluation_tier=dev` means the dev test-suite tier.

A C6.4 official goal cohort contains only `evaluation_tier=authoritative` rows that share one `c64_cohort_fingerprint`. Mixing `mock` or `dev` rows into that cohort forces exploratory scope. Those rows are still disclosed. They are not deleted.

`environment_schema_version: c64-env-1` is required on prospective pins. Writers SHALL NOT add `evaluation_tier` onto historical files. New environment hashes for new runs may change because new runs include new identity fields. Stored historical `environment_hash` values SHALL NOT be recalculated.

### Cohort fingerprint

`c64_cohort_fingerprint` is the SHA-256 hex of a canonical JSON object containing, and only containing:

- `environment_schema_version`
- `protocol_version`
- `metric_schema_version`
- `evaluation_tier`
- `inference.backend_name`
- `prov.thoth_git_sha`
- `prov.basic_agent_git_sha`
- `model.llm_model`
- `model.embedding_model`
- `model.embedding_method`
- `model.embedding_dimension`
- `corpus.fingerprint`
- digest of the loaded retrieval-weight configuration
- digest of runtime-editable cognitive settings that are not already fixed by `basic_agent_git_sha` (consolidation thresholds, strategy-promotion thresholds, and plan-reuse thresholds, when those values are configuration rather than code)

The exact byte-canonicalization is an engineering requirement of Phase 3. Two pins are the same cohort only when this fingerprint matches. `environment_hash` is stored beside it and is not a substitute.

---

## Fingerprint changes

A material change is any change to a component of `c64_cohort_fingerprint`.

| Change | Material |
|--------|----------|
| Product git SHA or Engine git SHA | Yes |
| `inference.backend_name` | Yes |
| LLM model identity | Yes |
| Embedding model, method, or dimension | Yes |
| Corpus fingerprint | Yes |
| Retrieval-weight configuration | Yes |
| Runtime memory, EGAR, or plan-reuse thresholds that enter the pin | Yes |
| `protocol_version`, `metric_schema_version`, or `environment_schema_version` | Yes |
| Hostname, log path, or window id itself | No |

**Rule:** a material change terminates the open official window at the change time. `window_end_ms` becomes that time if it is earlier than the planned end. Every observation already recorded stays in that window. Post-change observations SHALL NOT be appended to it.

The terminated window is analyzed as it is. If it no longer meets official scope, the result is insufficient, exploratory, or incomplete. It is not repaired by borrowing later goals.

A successor window requires a new open record, a new `window_id`, and the new pin, written before its goals count. The software SHALL NOT silently keep the old `window_id` across the change, and it SHALL NOT auto-promote the new pin into an official window without that open record.

Mixed fingerprints inside one `window_id` are a provenance failure. The report is exploratory and discloses `cohort_split`.

---

## Shared logs and validation

`agent_workspace/app_log.jsonl` and `agent_workspace/decision_trace.jsonl` are shared streams. C6.4 consumes candidate rows from them. It does not require those files to contain only C6 rows.

| Row | Treatment |
|-----|-----------|
| **Unrelated** | Not a C6 candidate. Ignored for `total_invalid`. Examples: app-log `event_name` other than `STRATEGY_INJECTION` and `PLANNER_CONTEXT_ASSEMBLY`; decision-trace rows that are not `trace_type=memory_consolidation` |
| **Recognized and valid** | Consumed. Strategy events need a non-empty `session_id` and `timestamp_ms`. Consolidation traces need `finished_at_ms` and a `stages` array, including `consolidation_committed` when the join looks for commitment |
| **Recognized and malformed** | `total_invalid` increments. The report is incomplete. Examples: a `PLANNER_CONTEXT_ASSEMBLY` or `STRATEGY_INJECTION` row with an empty `session_id`; a `memory_consolidation` row missing `finished_at_ms` or `stages`; two conflicting outcomes for one `plan_id` |

Malformed recognized rows SHALL NOT be ignored to make a report complete. Unrelated rows SHALL NOT be counted as malformed.

Metrics rows used for an official cohort are recognized evidence. A metrics row that claims a `window_id` but lacks `plan_id`, a non-empty `session_id`, or `goal_started_at_ms` is malformed.

Historical logs SHALL NOT be edited to satisfy this section.

Empty candidate sets are not malformed. With a present, readable file and zero invalid recognized rows, a missing strategy or consolidation candidate follows the C6.3 join outcomes: resolved `strategy_injected=false` or `post_consolidation=false`. A missing file is `MISSING_ARTIFACT` and the report is incomplete. Missing evidence SHALL NOT be treated as experience-absent.

---

## Experience-related segments

These are observational splits of the eligible official goals. They use the C6.3 join rules (`MAX_STRATEGY_JOIN_GAP_MS` = 300000, strategy tie-break, consolidation-before-start).

| Segment | Source |
|---------|--------|
| Plan reuse | Metrics bool `plan_reused` |
| Strategy injection | `STRATEGY_INJECTION`, or `PLANNER_CONTEXT_ASSEMBLY` with `strategy_injection` true, joined on `session_id` |
| Not injected | Resolved `PLANNER_CONTEXT_ASSEMBLY` with `strategy_injection` false, or no candidate in a complete file |
| Post-consolidation | Committed consolidation for that `session_id` with `finished_at_ms` before `goal_started_at_ms` |

Segment success rates MAY receive the same directional label rule as the primary rate when the segment has at least 10 eligible goals. A segment label is still not a causal claim. `MIN_GOALS_PER_COHORT` remains 10.

---

## Metrics

### Primary outcome

| Metric | Definition | Directional label |
|--------|------------|-------------------|
| Success rate | `completed / (completed + failed)` | Yes, under the trend rule below |

Wilson score interval at confidence level 0.95. Variance is the binomial variance of that rate, as in C6.3.

### Directional label

A **qualifying prior comparison window** is an earlier opened window whose report scope was `official_baseline` or `official_longitudinal`, with the same `protocol_version`, `metric_schema_version`, `environment_schema_version`, and `c64_cohort_fingerprint` as the current window, and whose span is the prior comparison span defined above. An insufficient, exploratory, or incomplete earlier window is not a qualifying prior.

When no qualifying prior comparison window exists:

| Field | Value |
|-------|-------|
| `trend` | `not_evaluable` |
| `reason` | `no_prior_window` |

`not_evaluable` is not `stable`. `stable` is used only when a qualifying prior exists and the directional test below does not fire. `not_evaluable` SHALL NOT be read as evidence of no change.

When a qualifying prior exists, `improving` or `declining` only when all of the following hold:

1. Absolute difference between the current point and the prior comparison window’s point is at least `0.05`.
2. The prior window’s Wilson interval does not fully contain the current point.
3. The current confidence label is not `low`.

Otherwise the label is `stable`. Ties and smaller differences are `stable`.

The same rule applies to a segment trend. A segment on a window with no qualifying prior is `not_evaluable` with `reason=no_prior_window`, not `stable`.

### Confidence label

| Label | Rule |
|-------|------|
| `high` | n ≥ 30 and interval width ≤ 0.20 |
| `medium` | n ≥ 10 and interval width ≤ 0.35 |
| `low` | otherwise |

n is the eligible count (`completed` + `failed`) in the current window. Both official scopes require the label not be `low`.

### Descriptive fields

Reported as aggregates only. They SHALL NOT receive `improving` or `declining` under C6.4.

| Field | Aggregate |
|-------|-----------|
| `total_wall_clock_ms` | p50 and p95 |
| `reflection_count` | mean |
| `revisions_count` | mean |
| `total_tokens` | p50 |
| `retrieved_chunk_count` | mean |
| `grag_alpha` | mean |

`LATENCY_REGRESSION_PCT` and `TOKEN_REGRESSION_PCT` from C6.3 are not trend rules in the shipped analyzer and are not trend rules here. Adding them later requires a new protocol version written before the window that would use them.

Post-consolidation success-rate delta is a segment description, not a causal effect.

---

## Report scopes

| Scope | When |
|-------|------|
| `official_baseline` | All qualifying conditions below, and no qualifying prior comparison window. `trend` is `not_evaluable` with `reason=no_prior_window` |
| `official_longitudinal` | All qualifying conditions below, and a qualifying prior comparison window exists. `trend` is `improving`, `declining`, or `stable` under the directional rule |
| `insufficient` | The window was prospective and the evidence is well formed, but eligible n < 30 or the confidence label is `low` |
| `exploratory` | Any of: fingerprint mixture, tier or provider mixture, missing required provenance, retrospective `window_id`, protocol or schema mismatch, failed mechanism prerequisite, missing episodic prerequisite for this stratum |
| `incomplete` | Missing required artifact, or any recognized malformed row (`total_invalid > 0`) |

`incomplete` outranks the other scopes. A report SHALL NOT be `official_baseline` or `official_longitudinal` if it is incomplete.

Absence of a qualifying prior does not make the report exploratory or insufficient when the qualifying conditions hold. That report is `official_baseline`.

Qualifying conditions for either official scope:

1. The window open record exists and predates the goals.
2. Eligible n ≥ 30 and confidence is not `low`.
3. Every official goal shares one `c64_cohort_fingerprint` and `evaluation_tier=authoritative`.
4. Required provenance fields are present.
5. `protocol_version` is `C6.4 v1.0` and `environment_schema_version` is `c64-env-1`.
6. No recognized malformed evidence.
7. Mock C3 and mock C5 prerequisites for this code fingerprint passed (§ Mechanism prerequisites).
8. The episodic authoritative prerequisite for this provider and model stratum is on record (§ Episodic prerequisite).
9. No disqualifying in-window failure of those prerequisites.

Either official scope is evidence. Neither scope is promotion.

---

## Mechanism prerequisites (C3 and C5)

C3 and C5 stay deterministic. Their pass criteria are unchanged (C3: the sealed reflection A/B expectations; C5: the sealed robustness case expectations). C6.4 SHALL NOT require those harnesses to call a live model or to emit `evaluation_tier=authoritative`.

For each `c64_cohort_fingerprint` code identity (`thoth_git_sha` and `basic_agent_git_sha`) used by an official window, the record SHALL include one passing mock C3 run and one passing mock C5 run built from that same pair of SHAs. Those runs are disclosed as `evaluation_tier=mock`.

A pass on an older SHA does not cover a newer SHA. The historical 2026-06-26 C3 result and the 2026-06-28 C5 result remain historical. They cover only the builds that produced them.

Failure, abort, or absence of either mock run blocks both official scopes. It does not delete operational goals. It does not relabel the mock runs.

Re-running C3 or C5 because the first run on that SHA failed, in order to obtain a pass, is forbidden. An infrastructure failure that never produced a terminal result may be repeated; the failed attempt stays in the record with the reason it was invalid.

These runs are cheap mechanism checks. They are not the 30 goals.

---

## Episodic prerequisite

Phase E stays sealed. A later provider-independent episodic evaluation is a **new evaluation version**. Suggested identity, to be confirmed when that harness is specified: `episodic_authoritative_v2`. It SHALL use `InferenceClient` and the configured provider, store `inference.backend_name` separately from `evaluation_tier=authoritative`, and SHALL NOT edit E2 artifacts or average its lift with `mean_episodic_lift = 0.0`.

llama.cpp results and Ollama results SHALL NOT be pooled unless a later protocol preregisters an explicit cross-provider rule before either run used in that pool.

**Role in C6.4:** one passing `episodic_authoritative_v2` execution per provider and model stratum is a prerequisite certification artifact, recorded before the first official report on that stratum (`official_baseline` or `official_longitudinal`). It is not required again inside each 28-day window when the stratum is unchanged. C6’s question is operational goal performance over time, not a repeated three-case lift experiment. Repeating that live run every window adds inference cost and no longitudinal observations.

Absence of the prerequisite blocks official scope for that stratum. Collection of operational goals MAY proceed before the prerequisite exists; those goals remain stored and the report stays exploratory until the prerequisite is on record. The prerequisite SHALL NOT be satisfied by the sealed Phase E Ollama result when the stratum is `llama_cpp`, or the reverse.

This protocol does not specify the episodic harness. It only states the interface C6.4 will require.

---

## Promotion

Measurement is not promotion. Analyzer output SHALL NOT move F1–F8.

Promotion eligibility requires two official windows in total, not three.

The pair is:

1. The earlier window is `official_baseline` or `official_longitudinal`.
2. The later window is `official_longitudinal`, and its qualifying prior comparison window is that earlier window.

On the first collection, that pair is one `official_baseline` followed by one `official_longitudinal`. Those are the two windows. A third window is not required. `official_baseline` counts as the first member of the pair and does not count as the later member, because the later member must have a trend against that prior (`improving`, `declining`, or `stable`), not `not_evaluable`.

The two windows are non-overlapping, share `protocol_version`, `metric_schema_version`, `environment_schema_version`, and `c64_cohort_fingerprint`, and do not share interior overlap. Adjacent boundaries are allowed.

No failed official evaluation lies between them. A failed official evaluation is a window that was opened for official collection and closed with any qualifying condition false, or with a scope other than `official_baseline` or `official_longitudinal`.

The safety prerequisites in this document held for both windows.

An owner-signed promotion record in [`improvements.md`](improvements.md) remains a separate act, the same governance act as C6.3-04. The C6.3-04 template remains the record format, with C6.4 paths and versions filled in. One window, including a lone `official_baseline`, is not promotion. Analyzer output does not sign the record.

Supporting segment notes may inform which F-item is discussed. They do not authorize promotion.

Until that record exists, F1–F8 stay deferred.

---

## Instrumentation contract

Later engineering MUST satisfy this section. This document does not implement it.

| ID | Contract |
|----|----------|
| **I1 Shared-stream validation** | Unrelated app-log and decision-trace rows do not increment `total_invalid`. Recognized malformed C6 rows do, and they force `incomplete` |
| **I2 Planner session** | `STRATEGY_INJECTION` and `PLANNER_CONTEXT_ASSEMBLY` carry the executing goal’s non-empty `session_id` |
| **I3 Window attribution** | While an official window is open, GUI Run and API goal execution copy `window_id`, `protocol_version`, `evaluation_tier`, provider, and `c64_cohort_fingerprint` onto the metrics row. No open window means those official fields are omitted, not backfilled |
| **I4 Environment identity** | Prospective pins include `environment_schema_version=c64-env-1`, `evaluation_tier`, and the existing `inference.backend_name`. Historical `runtime.tier` and historical `environment_hash` values are not rewritten |
| **I5 Append-only** | Restarts do not truncate C6.4 inputs. Conflicting duplicate `plan_id` rows are detectable as malformed |
| **I6 Analyzer scope** | A C6.4 reader emits the scopes in this document. The C6.3 reader and C6.3 fixtures keep their current behavior when given C6.3 inputs |

C6.3 `OFFICIAL_TIER=full` remains the C6.3 rule. C6.4 readers SHALL NOT apply that string test to C6.4 official scope.

---

## Forbidden

| Forbidden | Why |
|-----------|-----|
| Influence execution from a C6.4 trend | Primary invariant |
| Rewrite C6.3, E1, E2, C3, C5, historical tiers, or historical hashes | Historical compatibility |
| Call mock C3 or C5 authoritative | Mechanism prerequisites |
| Pool Phase E with a later provider run | Episodic prerequisite |
| Require Ollama in order to call llama.cpp authoritative | Provider independence |
| Drop failed goals, or replace them with retries | Outcomes |
| Assign official `window_id` after the outcome is known | Prospective window |
| Add a directional label for latency, tokens, reflection count, or revisions | Metrics |
| State that a segment difference is a cause | No causal claims |
| Promote an F-item from a script or from one window | Promotion |
| Repeat the E2 trio or clone goals to reach n = 30 | Population |
| Edit historical app logs or decision traces to reduce `total_invalid` | Shared logs |

---

## Operator procedure

This is the procedure the software and the operator follow after the engineering phases exist. It is not an instruction to open a window now.

1. **Verify the pin.** Record git SHAs, `inference.backend_name`, model, embedding, corpus fingerprint, and retrieval-weight digest. Confirm `evaluation_tier` will be `authoritative` for goals and `mock` for C3 and C5.
2. **Mechanism check on this build.** Run mock C3 and mock C5 once. Keep the terminal records with the SHAs. Do not relaunch a completed failure to seek a pass.
3. **Episodic stratum check.** If this provider and model have no `episodic_authoritative_v2` pass on record, that prerequisite is still open. Operational collection may start; official scope cannot be claimed until the prerequisite exists. Do not re-run it every month once it exists for this stratum.
4. **Open the window.** Write the open record. From this moment, qualifying goal execution is attributed automatically.
5. **Use Thoth normally.** GUI Run and API goals count. Chat without a plan does not. Do not select successes. Do not delete failures.
6. **Health only, if inspected mid-window.** Counts, provenance gaps, fingerprint equality. Not a decision to tune.
7. **On a material pin change.** The window terminates. Keep the rows. Open a new window only by a new open record if collection should continue under the new pin.
8. **Close at 28 days,** or at the earlier termination time. Freeze the input paths named in the open record. Do not trim them.
9. **Analyze once** under `protocol_version` C6.4. Publish the scope the rules produce, including `official_baseline` with `trend=not_evaluable`, `official_longitudinal` with `improving`, `declining`, or `stable`, or insufficient, exploratory, or incomplete.
10. **Leave F-series unchanged** unless a later owner record meets the promotion section.

---

## Engineering phases implied by this contract

Implementation is not authorized by this lock.

| Phase | What this protocol requires | Depends on |
|-------|-----------------------------|------------|
| **0** | Owner acceptance of this document | Done 2026-09-27. Not an implementation approval |
| **1** | I1 and I2. Independent of official windows. Does not make C6.3 windows official | Separate implementation approval |
| **2** | This document | 🔒 Locked 2026-09-27 |
| **3** | I4 and `c64_cohort_fingerprint` canonicalization. New records only | Separate implementation approval |
| **4** | `episodic_authoritative_v2` harness via `InferenceClient`. Prerequisite artifact, not a per-window rerun | Phase 3 for the pin fields |
| **5** | I3 and I5. Automatic attribution during an open window | Phase 3 and a separate implementation approval |
| **6** | I6. New fixtures in a new namespace. C6.3 goldens untouched | Phase 3 definitions; can use synthetic pins before Phase 4 and 5 |
| **7** | One short live smoke on the configured provider, expected `llama_cpp`, proving a single goal or a single episodic case writes the new fields. Not a 28-day window | Phases 3–6 as applicable to the smoke path |
| **8** | Operator open of the first real window | Phases 1, 3, 5, 6, and a conscious choice about whether the episodic prerequisite must already be green (it must be green before an official claim, not before open) |

Phase 4 need not block Phase 5 or Phase 6. It blocks an official claim on a stratum, not the writing of synthetic tests. The earlier plan’s suggestion to build live reflection and live robustness suites before C6.4 collection is withdrawn. This protocol does not ask for those suites.

---

## Informative — Paper boundary

`MYPAPER.md` §6 lists agent-level task-completion efficiency, as distinct from retrieval accuracy, as future work. This protocol does not edit that paper.

C6.4 can later support a statement about the proportion of operational goals that completed, with a Wilson interval and a directional label only when the trend rule fires. That proportion is a success outcome. It is not an efficiency measure, not a retrieval nDCG, and not evidence that GRAG, EGAR, or experience produced the proportion.

| Distinction | C6.4 |
|-------------|------|
| Goal success versus task-completion efficiency | Success rate only. No efficiency trend label |
| Association versus causation | Segments are associations |
| Operational change versus proof of GRAG | Out of scope |
| Retrieval quality versus goal success | Retrieval quality stays B1 |

---

## Lock record

| Field | Value |
|-------|-------|
| Protocol | C6.4 v1.0 |
| Locked | 2026-09-27 |
| Owner act | Acceptance of this text, including the revised `official_baseline` rule |
| Baseline identity | `protocol_version` `C6.4 v1.0`; `metric_schema_version` `1.0`; `environment_schema_version` `c64-env-1` |
| Content hash convention | None. This repository does not assign a separate protocol-file hash at lock. Git commit identity is the seal |
| Not included | Phase 1 or any later phase. No official window has been opened |

New records use `protocol_version: C6.4 v1.0`. Acceptance is not implementation approval for Phases 1–8.
