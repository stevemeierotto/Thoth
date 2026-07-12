# C6 Phase 3 — Longitudinal Cognitive Metrics Protocol

**Protocol version:** C6.3 v0.2.1  
**Status:** 🔒 **Locked for implementation** — v0.2 approved 2026-07-10; **v0.2.1 errata locked** 2026-07-10 (C6.3-01 Step 0)  
**Supersedes:** C6.3 v0.2, C6.3 v0.1  
**Depends on:** C6 Phase 1–2 (✅ shipped 2026-06-27 / 2026-06-29), [`benchmark_environment.md`](benchmark_environment.md) (E1 ✅), [`E2_PROTOCOL.md`](E2_PROTOCOL.md) v1.2 (scope firewall only — C6.3 does not modify E2)  
**Checkpoint tracking:** `cursor_list.md` § **C6** (Phase 3 row)  
**Parallel tracks:** **E3** (SCR per-run harness) — complementary, not substitutable

---

## Document structure

| Class | Sections | Binding? |
|-------|----------|----------|
| **Normative** | Invariants, definitions, constants, inputs/outputs, reporting, gates, forbidden rules, checkpoints | **Yes** — SHALL/MUST |
| **Informative** | Rationale, evaluation hierarchy overview, dependency stack, relationships, future work, doc map | **No** — context only |

If informative text conflicts with a normative rule, the **normative** rule governs.

---

## Informative — Evaluation hierarchy

C6 Phase 3 has a distinct identity in the benchmark suite:

| Layer | Question |
|-------|----------|
| **C3 / C5** | Did the mechanisms work? |
| **E2** | Did the agent improve within an episode? |
| **E3** | Did strategy consolidation improve future performance? |
| **B1** | Is retrieval quality improving? |
| **C6.3** | Across weeks or months, is the system measurably improving? |

Each layer answers a **different question**. C6.3 is the longitudinal evidence tier — not a substitute for any other benchmark.

### Phase narrative (C6 only)

| Phase | Question | Tier |
|-------|----------|------|
| **1** | Can we **record** per-goal cognition? | Instrumentation |
| **2** | Can we **summarize** a batch of goals? | Per-run analysis |
| **3** | Can we **detect improvement across sessions**? | Longitudinal eval |

### One sentence

C6 Phase 3 turns immutable per-goal benchmark artifacts into **time-series evidence** — with preregistered windows, confidence reporting, and cohort rules — that the operational cognitive loop is improving, stable, or regressing.

### Dependency stack

```
C6 Phase 1 — GOAL_COGNITIVE_METRICS emit
    ↓
C6 Phase 2 — summarize / plot / GUI export
    ↓
E1 — benchmark_run_id + env_hash attribution
    ↓
C6 Phase 3 — longitudinal analysis (read-only)
    ↓
Consumers — thesis appendix, F-series gate (implementation out of scope until C6.3-02+)
```

---

## Informative — Scope firewall (what C6 is **not**)

| C6.3 is **not** | Answered by |
|-----------------|-------------|
| Controlled episodic lift under frozen declared episodes | **E2-STRICT** |
| Organic consolidation → warm → retrieval → lift (diagnostic) | **E2-INTEGRATION** |
| Per-run strategy conformance (SCR) | **E3** |
| Fixed-corpus retrieval nDCG / hit@k | **B1** |
| Single-run mechanism correctness | **C3**, **C5** |
| Machinery trust / evaluator authority | E2 Phases A–D, Phase E |
| Causal proof that a code change caused a trend | **Forbidden** — see No Causal Claims |
| A license to change runtime behavior based on trends | **Forbidden** — see Primary Invariant |

### Three testing tiers (do not conflate)

| Tier | Examples | Valid claims |
|------|----------|--------------|
| **Fast mock CI** | `--dev`, C3/C5 | Mechanism fires |
| **Slow Ollama truth** | nightly `--full` | Retrieval quality; real synthesis |
| **Accumulated multi-session** | **C6 Phase 3** over weeks/months | Measurable system improvement — **learning claim** |

Official external statements about **learning over time** require Tier 3 evidence from this protocol (or a successor version), not Tier 1 alone.

### Benchmark comparison (informative)

| Benchmark | Measures | Primary artifacts |
|-----------|----------|-------------------|
| **C3** | Mechanism correctness | `logs/reflection_ab.jsonl` |
| **C5** | Internal cognition robustness | `logs/robustness_suite.jsonl` |
| **E2** | Episodic improvement (declared episodes) | E2 harness JSONL |
| **E3** | Strategy consolidation impact (SCR) | E3 harness JSONL (when implemented) |
| **B1** | Retrieval quality | `grag_benchmark.jsonl` |
| **C6.3** | Longitudinal trends | `cognitive_metrics.jsonl` + joins → longitudinal outputs |

Cross-benchmark synthesis requires an **explicit crosswalk** in the human report.

---

# Normative

---

## Primary Invariant

> **C6 Phase 3 SHALL NOT influence planning, retrieval, memory, strategy selection, prompting, scoring, or benchmark outcomes.**

> **Its sole responsibility is retrospective statistical analysis of immutable benchmark artifacts.**

| Allowed | Forbidden |
|---------|-----------|
| Read authoritative JSONL and sidecar artifacts | Modify `ExecutiveController` behavior based on trends |
| Join artifacts by stable relational keys | Feed trend signals into planner or strategy engine |
| Emit analysis JSONL, JSON summary, markdown, plots | Change E2 `official_scoring` or STRICT semantics |
| Filter cohorts by `benchmark_run_id`, `env_hash`, tier | Silently drop failed goals to inflate trends |
| Flag `benchmark_regression` when parallel gates fail | Promote F-series while official benchmarks are red |

---

## Artifact Immutability

Benchmark artifacts consumed by C6.3 SHALL be treated as **immutable historical records**.

| Rule | Requirement |
|------|-------------|
| **No overwrite** | Analysis SHALL NOT rewrite, truncate, or replace rows in authoritative input logs |
| **Correction policy** | If an artifact is corrected or regenerated, it SHALL receive a **new** `benchmark_run_id` and SHALL NOT overwrite prior official results |
| **Append-only outputs** | C6.3 outputs (`cognitive_longitudinal.jsonl`) are append-only; `cognitive_longitudinal_summary.json` is a snapshot of the latest run only |
| **Audit trail** | Each analysis run SHALL record which input paths and `benchmark_run_id` values were consumed |

---

## No Causal Claims

C6.3 identifies **statistical trends only**.

> **C6 Phase 3 SHALL NOT be interpreted as establishing causal relationships between code changes, configuration changes, and observed performance differences without supporting experimental evidence from controlled benchmarks (E2, E3, B1, C3/C5).**

Longitudinal metrics are **observational**. Correlation in a rolling window is not proof of causation.

---

## Definitions (normative)

| Term | Definition |
|------|------------|
| **Official benchmark execution** | A benchmark run produced by an **approved full-tier harness** (`tier=full`, Ollama truth path) under the project's release procedures, with a valid E1 `benchmark_run_id`, terminal harness success event, and pinned environment recorded in the benchmark sidecar. Mock/`--dev` runs are **never** official. |
| **Per-goal metrics** | Single `GOAL_COGNITIVE_METRICS` row — C6 Phase 1–2 scope |
| **Longitudinal analysis** | Time-ordered aggregation over preregistered windows with cohort segmentation — C6 Phase 3 |
| **Learning claim** | Statement that the system measurably improves across weeks or months |
| **Exploratory analysis** | Report where sample, confidence, completeness, or cohort requirements are not met — `evidence_scope: exploratory_only` |
| **Report completeness** | `complete` \| `incomplete` — see Missing Data Behavior |

**Naming:** C6 **Phase 3** ≠ improvements.md **Phase 3** (memory M1–M4). Always qualify as **C6 Phase 3** or **C6.3**.

---

## Versioned metrics (frozen for v0.2)

As metric definitions evolve, longitudinal comparisons MUST remain schema-aware.

| Field | Value (v0.2) | Rule |
|-------|--------------|------|
| `protocol_version` | `C6.3 v0.2.1` | Longitudinal protocol identity |
| `metric_schema_version` | **`1.0`** | Computation definitions for all Trend Category metrics |

Every longitudinal report (JSONL, JSON summary, markdown) SHALL record **both** `protocol_version` and `metric_schema_version`.

Comparing metric values computed under different `metric_schema_version` values SHALL require an explicit protocol note or cohort split. Silent cross-version comparison is **forbidden**.

---

## Frozen metric set (v0.2)

The Trend Categories section defines the **complete** v0.2 metric set. Adding a metric, removing a metric, or redefining how an existing metric is computed (e.g. changing success-rate eligibility rules) requires a **protocol revision** (`C6.3 v0.3+`) and a `metric_schema_version` bump.

Silent drift in what "success rate," "efficiency," or any category axis means is **forbidden**.

---

## Authoritative data sources

C6.3 analysis SHALL read **only** authoritative artifacts for official rollups.

### Authoritative inputs

| Artifact | Path / event |
|----------|--------------|
| Per-goal cognitive metrics | `logs/cognitive_metrics.jsonl` — `GOAL_COGNITIVE_METRICS` |
| Official benchmark JSONL | Approved full-tier harness terminals + case rows |
| Benchmark environment sidecar | `logs/benchmark_env.latest.json`, `logs/benchmark_env.jsonl` |
| Strategy injection | `agent_workspace/app_log.jsonl` — `STRATEGY_INJECTION`, `PLANNER_CONTEXT_ASSEMBLY` |
| Consolidation summaries | `agent_workspace/decision_trace.jsonl` — `memory_consolidation` / `consolidation_committed` |
| Episodic benchmark outputs | E2 official JSONL (crosswalk only) |
| SCR benchmark outputs | E3 JSONL (when implemented) |

Override paths are permitted only when explicitly passed to the analyzer and recorded in reproducibility metadata.

### Non-authoritative inputs (forbidden for official rollup)

Console stdout/stderr, debug traces, IDE logs, GUI clipboard exports, ad-hoc unstructured grep.

---

## Immutable units of observation

Each unit SHALL carry a stable ID where the upstream artifact provides one. Analysis MUST NOT invent synthetic IDs.

```
Session          session_id
    ↓
Goal             plan_id  (+ goal_started_at_ms)
    ↓
Episode          episode_id  (when present)
    ↓
Strategy event   strategy_id
    ↓
Reflection       plan_id + reflection_count
    ↓
Consolidation    consolidation_id  (or session_id + emitted_at_ms)
```

| Unit | Required ID | Source |
|------|-------------|--------|
| Session | `session_id` | Metrics, decision trace |
| Goal | `plan_id` | Metrics |
| Benchmark run | `benchmark_run_id` | E1 `run_id` |
| Episode | `episode_id` | E2 / trace (when present) |
| Strategy event | `strategy_id` | StrategyEngine / trace |
| Reflection | `plan_id` + `reflection_count` | Metrics |
| Consolidation | `consolidation_id` or (`session_id`, `emitted_at_ms`) | Trace |

`trajectory_id` in older docs maps to **`plan_id`** unless `episode_id` is required.

---

## Missing data behavior

Long-running systems encounter gaps. The analyzer SHALL follow these rules — **omit, warn, mark incomplete**; never infer.

| Condition | Required behavior |
|-----------|-------------------|
| **Incomplete benchmark window** | Include available goals; set `report_completeness: incomplete` and `flags: ["incomplete_window"]`; downgrade to `exploratory_only` if n < `MIN_GOALS_LONGITUDINAL` |
| **Missing authoritative artifact** | Omit goals/segments that depend on it; emit warning; set `flags: ["missing_artifact:<name>"]`; `report_completeness: incomplete` |
| **IDs don't join** | Segment `join_status: UNRESOLVED` (per [`C6_phase3_join_contract.md`](C6_phase3_join_contract.md)); values `null`; `flags: ["join_unresolved"]` — **never impute** |
| **Fingerprint differs** (`env_hash`, `model_hash`, `corpus_fingerprint`) | Segment `join_status: FINGERPRINT_MISMATCH`; values `null`; exclude from rollup; disclose in `threats_disclosed`; if mixed into one rollup, force `exploratory_only` |
| **Selection bias risk** | Dropping failed goals without flagging is **forbidden**; if detected, invalidate report |

---

## Stable join schema (C6.3-01)

Joins SHALL use relational keys only. Ad-hoc field matching is forbidden.

| Key | Use |
|-----|-----|
| `session_id` | Session-scoped segments |
| `plan_id` | Goal-level join |
| `benchmark_run_id` | Official run grouping (`run_id` in JSON) |
| `timestamp_ms` | Ordering; consolidation before/after |
| `env_hash` | Environment cohort lock |
| `model_hash` | Model-version cohort |
| `corpus_fingerprint` | Corpus drift detection |
| `strategy_id` | Planning segments |
| `episode_id` | Episodic crosswalk only |

**Algorithms, `JoinStatus` enum, tie-breaks, and artifact paths:** [`C6_phase3_join_contract.md`](C6_phase3_join_contract.md) JC v1.0 🔒 (C6.3-01 Step 0).

**Analyzer algorithms, terminal allowlist, JSONL payload, and tests:** [`C6_phase3_analyzer_contract.md`](C6_phase3_analyzer_contract.md) AC v1.0 🔒 (C6.3-02 Step 0).

**Reporting, plots, gate interpretation, and markdown:** [`C6_phase3_reporting_contract.md`](C6_phase3_reporting_contract.md) RC v1.0 🔒 (C6.3-03 Step 03-0).

**Default implementation:** join-only (Option A) — no C++ schema change unless C6.3-01 explicitly approves Option B.

---

## Window definition

| Property | Definition |
|----------|------------|
| **Type** | Rolling — not calendar month |
| **Duration** | `DEFAULT_WINDOW_DAYS` = **28** |
| **End anchor** | Timestamp of the **most recent official benchmark execution** in cohort |
| **Start** | `window_end_ms − (28 × 86_400_000)` |
| **Bounds** | Inclusive: `goal_started_at_ms` ∈ **[start, end]** |
| **Timezone** | **UTC** (Unix epoch ms) |
| **Prior window** | `[end − 56d, end − 28d]` |
| **No official anchor** | Use `generated_at_ms` as end; `flags: ["no_official_anchor"]`; `exploratory_only` |

---

## Trend categories and metric definitions (metric_schema_version 1.0)

All reported trends MUST map to one of five categories.

### Performance

| Metric | Definition | Axis |
|--------|------------|------|
| Success rate | `completed / eligible` where eligible = `outcome` ∈ {`completed`, `failed`} | Wilson CI at `CONFIDENCE_LEVEL` |

### Efficiency

| Metric | Field(s) | Axis |
|--------|----------|------|
| Wall-clock latency | `total_wall_clock_ms` | p50, p95 |
| Reflection burden | `reflection_count` | mean |
| Revisions | `revisions_count` | mean |
| Token cost | `total_tokens`, `planning_tokens`, `synthesis_tokens` | p50 |

### Memory

| Metric | Field(s) | Axis |
|--------|----------|------|
| Retrieval volume | `retrieved_chunk_count` | mean |
| Directional blend | `grag_alpha` | mean |
| Post-consolidation segment | join: consolidation before `goal_started_at_ms` | success rate delta |

### Planning

| Metric | Field(s) | Axis |
|--------|----------|------|
| Plan reuse | `plan_reused` | segment success rate |
| Strategy injection | join: `strategy_id` / hint events | segment success rate |
| SCR (when E3 exists) | E3 per-run SCR | rolling mean |

### Safety

| Metric | Source | Axis |
|--------|--------|------|
| Benchmark regression | Latest official gate suite | pass/fail |
| Reproducibility | E1 hash mismatches on pinned reruns | flag |
| Tier contamination | `mock` rows in `full` cohort | flag |

**Excluded (v0.2):** raw `goal` text clustering; `trajectory_score` as sole success proxy (G1d).

---

## Preregistered constants

| Constant | Value |
|----------|-------|
| `MIN_GOALS_LONGITUDINAL` | **30** |
| `MIN_GOALS_PER_COHORT` | **10** |
| `DEFAULT_WINDOW_DAYS` | **28** |
| `TREND_MIN_ABS_DELTA` | **0.05** |
| `CONFIDENCE_LEVEL` | **0.95** |
| `LATENCY_REGRESSION_PCT` | **15** |
| `TOKEN_REGRESSION_PCT` | **15** |
| `OFFICIAL_TIER` | `full` |
| `METRIC_SCHEMA_VERSION` | **`1.0`** |
| `MAX_STRATEGY_JOIN_GAP_MS` | **300_000** (5 min) — strategy join time window; see join contract |

Change only in **C6.3 v0.3+** with protocol revision.

---

## Statistical reporting requirements

Directional trend labels (`improving` / `declining` / `stable`) require **all** of:

| Requirement | Rule |
|-------------|------|
| Minimum sample size | `goal_count ≥ MIN_GOALS_LONGITUDINAL` (global) or `≥ MIN_GOALS_PER_COHORT` (segment) |
| Point estimate | Observed rate or p50 |
| Confidence interval | Wilson (proportions); bootstrap or log-normal p50 CI for latency/tokens (document in analyzer) |
| Variance | `success_rate_variance` or `latency_variance` as applicable |
| Confidence label | `high` (n ≥ 30, CI width ≤ 0.20), `medium` (n ≥ 10, width ≤ 0.35), `low` (otherwise) |
| Directional label | Delta ≥ `TREND_MIN_ABS_DELTA` **and** prior CI does not fully contain current point **and** `confidence_label ≠ low` |

| Condition | `evidence_scope` |
|-----------|------------------|
| n below minimum, `confidence_label = low`, incomplete report, tier mixed, no official anchor, fingerprint mismatch | `exploratory_only` |

Formal p-values and Bayesian posteriors are **out of scope** for v0.2.

---

## Input contract — `GOAL_COGNITIVE_METRICS`

**Required:** `event`, `plan_id`, `session_id`, `goal_started_at_ms`, `outcome`.

**Required for official cohort:** `benchmark_run_id`, `env_hash`.

---

## Output contract

### Artifacts

| Output | Path | Role |
|--------|------|------|
| Append-only stream | `logs/cognitive_longitudinal.jsonl` | `COGNITIVE_LONGITUDINAL_SUMMARY` per run — **full payload per row** (see [`C6_phase3_analyzer_contract.md`](C6_phase3_analyzer_contract.md) AC v1.0) |
| Machine-readable snapshot | `logs/cognitive_longitudinal_summary.json` | Latest run only — downstream tooling |
| Human report | `logs/cognitive_longitudinal_report.md` | Operators / thesis appendix |
| Plots | `logs/plots/longitudinal/` | Optional (C6.3-03) |

### Reproducibility metadata (required in every report)

Every JSONL row, JSON summary, and markdown header SHALL include:

| Field | Source |
|-------|--------|
| `protocol_version` | `C6.3 v0.2.1` |
| `metric_schema_version` | `1.0` |
| `analyzer_version` | Script version string / git describe |
| `analyzer_commit_hash` | Git SHA at analysis time (or `unknown` with flag) |
| `generated_at_ms` | Analysis wall time (UTC) |
| `input_log_path` | Metrics JSONL path |
| `input_artifact_paths` | List of all authoritative inputs read |
| `window_start_ms`, `window_end_ms`, `window_anchor` | Window definition |
| `report_completeness` | `complete` \| `incomplete` |

### JSON summary minimum shape

```json
{
  "protocol_version": "C6.3 v0.2.1",
  "metric_schema_version": "1.0",
  "analyzer_version": "0.2.0",
  "analyzer_commit_hash": "abc123",
  "generated_at_ms": 0,
  "report_completeness": "complete",
  "window": { "start_ms": 0, "end_ms": 0, "days": 28, "anchor": "official_benchmark_execution" },
  "evidence_scope": "official_longitudinal",
  "confidence_label": "high",
  "reproducibility": {
    "input_log_path": "logs/cognitive_metrics.jsonl",
    "input_artifact_paths": [],
    "benchmark_run_ids_consumed": []
  },
  "categories": {
    "performance": { "success_rate": 0.0, "ci_low": 0.0, "ci_high": 0.0, "variance": 0.0, "trend": "stable" },
    "efficiency": {},
    "memory": {},
    "planning": {},
    "safety": { "benchmark_regression": false, "reproducibility_ok": true, "official_gates_green": true }
  },
  "threats_disclosed": [],
  "segments": {},
  "flags": [],
  "warnings": []
}
```

---

## Threats to validity (required disclosure)

Every report MUST include `threats_disclosed` (JSON + markdown). Check all that apply:

| Threat | Detection |
|--------|-----------|
| Changing models | `model_hash` drift |
| Corpus growth | `corpus_fingerprint` drift |
| Hardware changes | `env_hash` drift without model/corpus change — canonical ID: `runtime_environment_drift` (RC v1.0) |
| Prompt evolution | Git SHA / config pin change |
| Protocol revision | `protocol_version` or `metric_schema_version` change mid-window |
| Small sample | n < `MIN_GOALS_LONGITUDINAL` |
| Tier mixing | `mock` + `full` in same rollup |
| Incomplete data | Missing artifacts or joins |
| Selection bias | Undocumented goal exclusion |

---

## F-series promotion gate (C6.3-04)

> **C6.3-04 lock (2026-07-11):** Operational policy — mandatory vs supporting gates, window definition, approval governance, and promotion record — is specified in § **C6.3-04 implementation lock**. **Normative prose:** `docs/improvements.md` (sole owner). `cursor_list.md` is informational only and shall reference, not duplicate, those criteria.

Promotion requires **all mandatory gates** (see § C6.3-04 implementation lock). Constitutional floor:

| Gate | Requirement |
|------|-------------|
| Longitudinal evidence | ≥ 2 consecutive official evaluation windows; `evidence_scope: official_longitudinal`; `confidence_label` ≥ `medium`; `report_completeness: complete` |
| Segment signal | Segment trend with adequate n and CI — **supporting evidence** under C6.3-04 lock |
| Mechanism alignment | E3 or E2 diagnostic agrees on bottleneck — **supporting evidence** under C6.3-04 lock |
| No benchmark regression | `safety.benchmark_regression = false` |
| Reproducibility | No unresolved E1 hash mismatches on pinned reruns |
| Official gates green | Required harnesses green in review period — see RC § Safety gate interpretation |
| Owner approval | Explicit owner approval recorded in `improvements.md` |

Promotion while any Safety gate is red is **forbidden**.

---

## Forbidden

| Forbidden | Why |
|-----------|-----|
| Influence execution | Primary Invariant |
| Rewrite authoritative input artifacts | Artifact Immutability |
| Causal claims without controlled evidence | No Causal Claims |
| Cross-version metric comparison without disclosure | Versioned metrics |
| Add/redefine metrics without protocol revision | Frozen metric set |
| Parse non-authoritative sources for official rollup | Authoritative sources |
| Impute missing joins or artifacts | Missing data behavior |
| Merge `mock` and `full` in `official_longitudinal` | Tier conflation |
| Directional trend without CI + sample gates | Statistical reporting |
| Promote F-series with benchmark regression | Promotion gate |

---

## Implementation checkpoints

| ID | Stop criteria |
|----|---------------|
| **C6.3-0** | Protocol locked ✅ |
| **C6.3-01 Step 0** | Join contract + v0.2.1 errata locked ✅ |
| **C6.3-01 Step 1** | Synthetic fixtures + `golden_join_results.json` ✅ |
| **C6.3-01 Step 2** | Join library `c6_longitudinal_join.py` ✅ |
| **C6.3-01 Step 3** | Golden regression `test_c6_longitudinal_join.py` — T1–T6 ✅ |
| **C6.3-01 Step 4** | Production smoke `--smoke` on `c6_longitudinal_join.py` ✅ |
| **C6.3-02 Step 0** | Analyzer contract AC v1.0 locked ✅ |
| **C6.3-02** | Analyzer `analyze_cognitive_longitudinal.py` + `test_c6_longitudinal_analyzer.py` ✅ |
| **C6.3-03 Step 03-0** | Reporting contract RC v1.0 locked ✅ |
| **C6.3-03 Step 03-2** | Threat detection umbrella 03-2a–d ✅ |
| **C6.3-03 Step 03-3** | Safety gate wiring ✅ (§ Step 03-3) |
| **C6.3-03 Step 03-4** | Plot module ✅ (§ Step 03-4) |
| **C6.3-03 Step 03-5** | CLI orchestration ✅ (§ Step 03-5) |
| **C6.3-03 Step 03-6** | Reporting regression R3–R9 + CTest ✅ (§ Step 03-6) |
| **C6.3-03 Step 03-7** | Version bump + golden seal + log close-out ✅ (§ Step 03-7) |
| **C6.3-03** | Reporting layer sealed ✅ (03-0–03-7) |
| **C6.3-04** | Promotion gate in `improvements.md` ✅ (§ C6.3-04) |
| **C6.3-05** | Operator invocation guide ✅ (§ `cognitive_longitudinal_ops.md`) |
| **C6.3-06** | Regression fixtures (§ C6.3-06 — sub-checkpoints 06-1–06-5) ✅ |
| **C6.3-06-1** | Fixture catalog `tests/fixtures/cognitive_longitudinal/README.md` ✅ |
| **C6.3-06-2** | Intentionally absent fixture (`missing_decision_trace.jsonl` — README only) ✅ |
| **C6.3-06-3** | Official longitudinal companion corpus ✅ |
| **C6.3-06-4** | Official analyzer golden + F1 regression ✅ |
| **C6.3-06-5** | Documentation seal (C6.3-06 close-out) ✅ |

Pause between checkpoints: build green → tests green → confirm before next step.

---

## C6.3-06 implementation lock 🔒 (2026-07-11)

**Status:** ✅ **Implemented** 2026-07-11 · parent checkpoint **C6.3-06** decomposed into sub-checkpoints **06-1–06-5** (execution order only — each sub-checkpoint seals independently)

### Parent implementation invariant (normative)

> **C6.3-06 is fixture and regression infrastructure work only.** No analyzer, reporting, join, threat, provenance, or plotting **behavior** changes are permitted during fixture authoring unless a separate repair checkpoint is opened.

> **Fixture authoring shall preserve the observable behavior of all existing regression suites.** Any proposal that requires production code changes to preserve fixture behavior must be implemented in a separate checkpoint.

> **Fixture semantics are part of the regression contract.** If a fixture's meaning changes because of file existence, path layout, naming, or metadata, treat that as a semantic change requiring its own checkpoint.

> **If implementation requires modification of AC, RC, or JC semantics to make a fixture pass, stop implementation and open a new checkpoint.** Fixture authoring shall not redefine protocol behavior.

> **No new fixture-metadata infrastructure with a single consumer.** Catalog authority is `tests/fixtures/cognitive_longitudinal/README.md` only — no manifest JSON, placeholder files, broken symlinks, or sidecar metadata formats unless a broader fixture-metadata strategy is separately approved.

### Design principles (frozen)

| Principle | Rule |
|-----------|------|
| **Companion fixtures** | New scenarios use named companion files; **companion fixtures shall never replace exploratory fixtures** |
| **Default goldens** | Default exploratory regression outputs shall remain **identical after normalization** using existing golden normalization rules; differences beyond approved version fields constitute a build failure |
| **Official goldens** | Official companion goldens are maintained **independently** (`analyzer_golden_official_longitudinal.json`) |
| **Regression scope** | No existing regression suite requires modification except to add **explicit companion-fixture cases** |
| **Generation** | Large repetitive synthetic datasets should be produced by a **deterministic generator** or **documented transformation process** rather than extensive manual editing; generation scripts are **maintainer tools only** and shall **not** execute during normal regression |
| **Provenance** | Official companion corpus provenance recorded **in README only** (corpus name, generator path/version, last regenerated, protocol version, analyzer version, metric schema version) |

### Reserved namespaces (frozen)

| Namespace | Reserved for |
|-----------|----------------|
| `plan-c6xx` | Default exploratory C6 corpus (current) |
| `plan-olxx` | Official longitudinal companion corpus |
| `plan-negxx` | Future negative fixtures (deferred) |

> Reserved namespaces **shall not** be reused for unrelated scenarios. New fixture families must allocate a distinct namespace to avoid golden collisions and join ambiguity.

### Sub-checkpoint map

```
06-1  Fixture catalog (README only) ✅
06-2  Intentionally absent fixture (README catalog update only) ✅
06-3  Official longitudinal companion corpus ✅
06-4  Official analyzer golden + companion regression case (F1) ✅
06-5  Documentation seal (C6.3-06 close-out) ✅
```

Pause between sub-checkpoints: build green → tests green → confirm before next step.

---

## C6.3-06-1 implementation lock 🔒 (2026-07-11)

**Status:** ✅ **Implemented** 2026-07-11 · catalog at `tests/fixtures/cognitive_longitudinal/README.md`

### Implementation invariant (normative)

> **C6.3-06-1 introduces no code changes, no fixture file changes, and no golden changes.** One new file only: `tests/fixtures/cognitive_longitudinal/README.md`.

If catalog authoring reveals a test/fixture mismatch, **document it in the README** (e.g. referenced but not yet committed) and defer fixes to the appropriate sub-checkpoint.

### Authority (frozen)

> This catalog documents fixture intent only. Behavioral semantics are defined exclusively by the Analyzer Contract (AC), Reporting Contract (RC), and Join Contract (JC). In case of conflict, those contracts take precedence.

### Purpose

Provide a maintainer-facing fixture catalog so each committed C6 longitudinal fixture's scenario, consumers, expected outcome, and golden linkage are discoverable without reading all seven test harnesses.

### README structure (frozen)

1. **Overview** — synthetic-only philosophy; default vs companion fixtures; maintainer rules (diff-gated goldens; `--write-goldens` opt-in only)
2. **Reserved namespaces** — `plan-c6xx`, `plan-olxx`, `plan-negxx` + reservation rule (see parent lock)
3. **Master fixture table** — columns: **Fixture · Scenario · Purpose · Consumed by · Expected outcome · Golden · Maintainer**
4. **Join case map** — cross-reference `golden_join_results.json` `case_map`; note **C6-05** exercised in join T6 via in-memory empty trace, not as a golden row
5. **Companion / variant matrix** — `gate_status_golden.json` cases → env/metrics overrides
6. **Planned fixtures** — forward pointers for 06-3+ (official companion corpus, official golden); `missing_decision_trace.jsonl` → **06-2** (intentionally absent)
7. **Golden normalization note** — pointer to existing test normalization rules; no new normalization logic in 06-1

### Catalog scope (frozen)

All **14 committed** files under `tests/fixtures/cognitive_longitudinal/` shall be cataloged:

`metrics.jsonl`, `app_log.jsonl`, `decision_trace.jsonl`, `benchmark_env.jsonl`, `benchmark_env_gates_aborted.jsonl`, `benchmark_env_gates_green.jsonl`, `metrics_gates_green.jsonl`, `prior_longitudinal.jsonl`, `golden_join_results.json`, `analyzer_golden_summary.json`, `analyzer_trend_golden.json`, `gate_status_golden.json`, `report_header_golden.md`, `report_header_incomplete_golden.md`.

**Intentionally absent fixture (formalized in 06-2):** `missing_decision_trace.jsonl` — consumed by Analyzer A6, O6; Reporting R1b, R2, R8; must not exist on disk under current JC.

**Out-of-dir note:** Plot suite P1–P8 uses inline synthetic payloads, not this directory — state explicitly to avoid confusion.

### Files (C6.3-06-1)

| File | Change |
|------|--------|
| `tests/fixtures/cognitive_longitudinal/README.md` | **New** — required |

**Optional (do not block 06-1):** `docs/INDEX.md` one-line link; protocol footnote only.

**Not touched in 06-1:** `scripts/*`, `tests/*`, goldens, fixture JSONL/JSON/MD except new README, `completed_improvements_log.md` (seals in 06-5).

### Explicitly deferred (not C6.3-06-1)

| Item | Sub-checkpoint |
|------|----------------|
| `missing_decision_trace.jsonl` (intentionally absent) | **06-2** ✅ locked |
| Official longitudinal companion corpus | **06-3** |
| Official analyzer golden + F1 case | **06-4** |
| Protocol / completed-log seal | **06-5** |
| Negative fixture slices (`plan-negxx`) | Deferred beyond C6.3-06 |

### C6.3-06-1 stop criteria

1. `tests/fixtures/cognitive_longitudinal/README.md` exists with all seven README sections  
2. All 14 committed fixture files cataloged with required columns  
3. `missing_decision_trace.jsonl` documented as planned **06-2**, not silently omitted  
4. Reserved namespace rules documented  
5. Authority statement (AC/RC/JC precedence) present  
6. **No** changes outside the README file  
7. Existing seven-suite + `ctest -R c6-longitudinal` regression unchanged and green  

**Next:** **C6.3-06-2** intentionally absent fixture → **06-3** official corpus → **06-4** official golden → **06-5** seal.

---

## C6.3-06-2 implementation lock 🔒 (2026-07-11)

**Status:** ✅ **Implemented** 2026-07-11 · intentionally absent fixture documented in README §3

### Semantic discovery (frozen — precludes empty-file commit)

Under current JC semantics (`load_jsonl` → `build_join_context`):

| Path state | `trace_exists` | `decision_trace` in `missing_artifacts` | `report_completeness` |
|------------|----------------|----------------------------------------|------------------------|
| **File absent** (intended) | `false` | **yes** | **`incomplete`** ✅ |
| **Empty file committed** | `true` | **no** | **`complete`** ❌ |

> **An empty committed `missing_decision_trace.jsonl` is incorrect under the current contract** and would break Analyzer A6, O6 and Reporting R1b, R2, R8.

**Rejected approaches (frozen):**

| Approach | Verdict |
|----------|---------|
| Empty file + join errata | **Rejected** — production semantics must not change to satisfy a fixture preference |
| Broken symlink | **Rejected** — portability / maintainability |
| Separate `missing_decision_trace.manifest.json` | **Rejected** — one-off metadata format; README is sole catalog authority (06-1) |

### Implementation invariant (normative)

> **C6.3-06-2 introduces no production code changes, no new fixture files on disk, and no golden changes.** README catalog update only.

> **Fixture authoring shall preserve the observable behavior of all existing regression suites.** Any proposal that requires production code changes to preserve fixture behavior must be implemented in a separate checkpoint.

> **If implementation requires modification of AC, RC, or JC semantics to make a fixture pass, stop and open a new checkpoint.**

### Purpose

Formalize `missing_decision_trace.jsonl` as an **intentionally absent** fixture in the catalog. Its **absence on disk** is the fixture input; tests already pass the path and rely on `load_jsonl` returning `file_exists=false`.

### Fixture state taxonomy (frozen — add to README)

| Fixture state | Meaning |
|---------------|---------|
| **Present** | File exists; analyzer/join consumes it as a normal input artifact |
| **Empty** | File exists with zero valid JSONL records (`trace_exists=true`; not a missing-artifact scenario) |
| **Intentionally absent** | Path referenced by tests but **must not exist on disk**; absence is the fixture |

`missing_decision_trace.jsonl` → **intentionally absent**.

### README updates (frozen)

1. **Promote** implicit fixture row to full catalog entry with columns: Fixture · Fixture state · Scenario · Purpose · Consumed by · Expected outcome · Golden · Maintainer  
2. **Required maintainer prose:**  
   > This fixture is intentionally represented by the **absence** of the file. Creating the file changes the analyzer input semantics and invalidates the missing-artifact scenario.  
3. **Catalog entry (frozen fields):**

| Field | Value |
|-------|-------|
| Fixture | `missing_decision_trace.jsonl` |
| Fixture state | Intentionally absent |
| Scenario | Missing decision trace |
| Purpose | Incomplete-report regression (`decision_trace` artifact missing) |
| Consumed by | Analyzer A6, O6; Reporting R1b, R2, R8 |
| Expected outcome | `report_completeness: incomplete`; `missing_artifact:decision_trace` |
| Golden | `report_header_incomplete_golden.md` |
| Maintainer | **Do not create this file** unless absent-vs-empty trace semantics are formally revised in AC/JC |

4. **§6 Planned fixtures** — remove `missing_decision_trace.jsonl` (06-2 complete after README update)  
5. **Cross-reference** — C6-05 join T6 (in-memory `trace_exists=false`) is **distinct** from this intentionally absent analyzer/reporting path

### Files (C6.3-06-2)

| File | Change |
|------|--------|
| `tests/fixtures/cognitive_longitudinal/README.md` | **Update** — required |

**Explicitly not created:** `missing_decision_trace.jsonl`, `missing_decision_trace.manifest.json`, or any other new file.

**Not touched:** `scripts/*`, `tests/*`, goldens, `completed_improvements_log.md` (seals in 06-5).

### C6.3-06-2 stop criteria

1. README documents `missing_decision_trace.jsonl` as **intentionally absent** with full catalog columns  
2. Fixture state taxonomy present in README  
3. Maintainer rule: creating the file invalidates the scenario unless AC/JC revised  
4. §6 planned fixtures no longer lists 06-2 as pending  
5. **No new files** on disk except README edit  
6. Existing incomplete-path tests pass unchanged (A6, O6, R1b, R2, R8)  
7. Seven-suite + `ctest -R c6-longitudinal` green  
8. No production code or golden changes  

**Next:** **C6.3-06-3** official corpus → **06-4** official golden → **06-5** seal.

---

## C6.3-06-3 implementation lock 🔒 (2026-07-11)

**Status:** ✅ **Implemented** 2026-07-11 · generator `scripts/generate_c6_official_longitudinal_fixtures.py` + four `*_official_longitudinal.jsonl` companions (`plan-ol01`–`plan-ol32`)

### Checkpoint boundary (frozen)

| Checkpoint | Job |
|------------|-----|
| **06-3** (this lock) | Official companion **corpus files** + README catalog/provenance + maintainer generator |
| **06-4** | `analyzer_golden_official_longitudinal.json` + explicit regression case (F1) — **not** 06-3 |
| **06-5** | Protocol / `completed_improvements_log.md` seal — **not** 06-3 |

06-3 does **not** add regression tests, commit goldens, or introduce fixture-metadata systems beyond README documentation.

### Fixture semantics invariant (normative)

> **Fixture semantics are part of the regression contract.** If a fixture's meaning changes because of file existence, path layout, naming, or metadata, treat that as a semantic change requiring its own checkpoint.

> **Fixture authoring shall preserve the observable behavior of all existing regression suites.** Any proposal that requires production code changes to preserve fixture behavior must be implemented in a separate checkpoint.

> **C6.3-06-3 introduces no production analyzer/join/reporting/threat/provenance changes and no golden or test-harness changes.**

> **If implementation requires AC, RC, or JC semantic changes to make the corpus pass, stop and open a new checkpoint.**

### Purpose

Create the canonical synthetic **official-longitudinal** companion dataset (`plan-olxx` namespace). When analyzed with companion inputs, the corpus shall satisfy the analyzer requirements for `official_longitudinal` evidence under the **current** Analyzer Contract. Implementation method (generator vs manual) is an implementation detail, provided the corpus is **deterministic and auditable**.

Golden pinning and automated regression assertion → **06-4 only**.

### Outcome-oriented success criterion (frozen)

Maintainer validation (not CTest until 06-4) must confirm under pinned `AS_OF_MS = 1_800_002_000_000`:

| Field | Required (pointer — AC v1.0) |
|-------|------------------------------|
| `report_completeness` | `complete` |
| `evidence_scope` | `official_longitudinal` |
| `confidence_label` | not `low` |
| `categories.safety.reproducibility_ok` | `true` |
| `categories.safety.tier_contamination` | `false` |
| `categories.safety.benchmark_regression` | `false` |
| `categories.safety.official_gates_green` | `true` |
| `flags` | no `fingerprint_mismatch`; no `gate_evidence_missing:*` for required harnesses |
| `metric_schema_version` | `1.0` |

Thresholds (`MIN_GOALS_LONGITUDINAL`, CI rules, etc.) — **reference AC only**; do not duplicate in fixtures or README beyond pointers.

### Locked decisions (frozen — no alternatives)

| Decision | Rule |
|----------|------|
| **Namespace** | `plan-olxx` / `run-olxx` / `session-olxx` — disjoint from `plan-c6xx` |
| **Companion files** | Four `*_official_longitudinal.jsonl` files; **never** modify default exploratory fixtures or goldens |
| **Join golden** | Do **not** add `plan-olxx` to `golden_join_results.json` |
| **Provenance** | **README only** — no `*.manifest.json` or sidecar metadata |
| **Generator** | `scripts/generate_c6_official_longitudinal_fixtures.py` — maintainer-only; deterministic; **not** run in CTest or seven-suite regression |
| **Rejected** | Manifest JSON, placeholder/empty/symlink trace files, join/analyzer changes to force corpus pass |

### Deliverables (frozen)

**1. Four companion JSONL files** under `tests/fixtures/cognitive_longitudinal/`:

| File | Role |
|------|------|
| `metrics_official_longitudinal.jsonl` | Official-cohort goal metrics (`plan-ol01` …) |
| `benchmark_env_official_longitudinal.jsonl` | Per-`run_id` env + official anchor + in-window gate terminals (extend `benchmark_env_gates_green.jsonl` pattern) |
| `app_log_official_longitudinal.jsonl` | Minimal app-log rows (sparse OK) |
| `decision_trace_official_longitudinal.jsonl` | Minimal trace rows (sparse OK) |

All four: fixture state **Present**; joins clean (matched `env_hash`, FULL tier, no fingerprint mismatch).

**2. Maintainer generator** — `scripts/generate_c6_official_longitudinal_fixtures.py`

- Writes all four files deterministically from fixed parameters
- Optional `--validate` in **same script**: analyzer smoke asserting `evidence_scope` / `report_completeness` — maintainer-only, not CTest until 06-4
- Not invoked by normal regression

**3. README update** — `tests/fixtures/cognitive_longitudinal/README.md`

- Four companion rows in master table
- Provenance subsection (text only): corpus name, generator path/version, last regenerated, protocol version, analyzer version, metric schema version, plan namespace range
- Remove official corpus from §6 Planned fixtures
- Note automated regression deferred to **06-4**

### Internal phases (execution order — single 06-3 lock)

```
06-3a  Implement deterministic generator
06-3b  Generate and commit four companion JSONL files
06-3c  README catalog + provenance (README only)
06-3d  Maintainer validation (generator --validate or documented smoke)
```

### Files (C6.3-06-3)

| File | Phase |
|------|-------|
| `scripts/generate_c6_official_longitudinal_fixtures.py` | 06-3a |
| `tests/fixtures/cognitive_longitudinal/metrics_official_longitudinal.jsonl` | 06-3b |
| `tests/fixtures/cognitive_longitudinal/benchmark_env_official_longitudinal.jsonl` | 06-3b |
| `tests/fixtures/cognitive_longitudinal/app_log_official_longitudinal.jsonl` | 06-3b |
| `tests/fixtures/cognitive_longitudinal/decision_trace_official_longitudinal.jsonl` | 06-3b |
| `tests/fixtures/cognitive_longitudinal/README.md` | 06-3c |

**Not touched:** `analyze_cognitive_longitudinal.py`, join/reporting/threat/provenance modules, test harnesses, goldens, `golden_join_results.json`, `completed_improvements_log.md` (06-5).

### Explicitly deferred (not C6.3-06-3)

| Item | Sub-checkpoint |
|------|----------------|
| `analyzer_golden_official_longitudinal.json` | **06-4** |
| F1 companion test | **06-4** |
| Protocol / completed-log seal | **06-5** |
| `plan-negxx` negative slices | Deferred beyond C6.3-06 |

### C6.3-06-3 stop criteria

1. Four companion JSONL files committed (`plan-olxx`)  
2. Generator committed; deterministic regeneration documented in README  
3. Provenance recorded **in README only**  
4. Maintainer validation confirms `official_longitudinal` under current AC  
5. Default exploratory fixtures and goldens unchanged  
6. Existing seven-suite + `ctest -R c6-longitudinal` green — **no new regression tests**  
7. No production code changes  
8. No new metadata file formats introduced  

**Next:** **C6.3-06-4** official golden + F1 → **06-5** seal.

---

## C6.3-06-4 implementation lock 🔒 (2026-07-11)

**Status:** ✅ **Implemented** 2026-07-11 · F1 + `analyzer_golden_official_longitudinal.json` + `--write-official-golden`

### Checkpoint boundary (frozen)

| Checkpoint | Job |
|------------|-----|
| **06-4** (this lock) | `analyzer_golden_official_longitudinal.json` + F1 regression + `--write-official-golden` |
| **06-5** | Protocol / `completed_improvements_log.md` seal — **not** 06-4 |

06-4 is **regression wiring only** — not corpus authoring (06-3), not protocol close-out (06-5).

F1 provides **automated regression evidence** for Phase 3 exit criterion #3 by validating the official companion corpus against its dedicated golden. It does **not** redefine protocol exit criteria.

### Implementation invariant (normative)

> **Fixture semantics are part of the regression contract.** If a fixture's meaning changes because of file existence, path layout, naming, or metadata, treat that as a semantic change requiring its own checkpoint.

> **Fixture authoring shall preserve the observable behavior of all existing regression suites.**

> **The official companion regression shall exercise the same public analyzer entry point as production analysis.** Test-only wrappers may select alternate fixture paths but **shall not** bypass or replace analyzer execution (no fabricated payloads, no test-only analyzer branches).

> **C6.3-06-4 introduces no production analyzer, join, reporting, plotting, threat, or provenance code changes.** No modifications to 06-3 companion JSONL unless golden refresh reveals a corpus bug (fix via generator, not analyzer).

> **Default exploratory regression outputs shall remain identical after normalization** (`normalize_payload`, pinned `AS_OF_MS`, stripped `analyzer_commit_hash`). Differences beyond approved version fields constitute a build failure.

> **Official companion golden is maintained independently** of `analyzer_golden_summary.json`.

### Locked decisions (frozen — no alternatives)

| Decision | Rule |
|----------|------|
| **Test ID** | **F1** — `test_f1_official_golden_summary` in `run_all_tests` |
| **Companion paths** | Four `*_official_longitudinal.jsonl` from 06-3 |
| **Pinned time** | `AS_OF_MS = 1_800_002_000_000` |
| **Normalization** | Reuse `normalize_payload()` |
| **Golden** | `analyzer_golden_official_longitudinal.json` |
| **Default golden** | `analyzer_golden_summary.json` **unchanged** |
| **Golden refresh** | `--write-official-golden` only — **not** `--write-goldens` for unrelated assets |
| **Path API** | `fixture_paths()` + `run_analysis()` unchanged; official via `official_fixture_paths()` + `run_official_analysis()` — **no boolean switches** on default paths |

### F1 assertions (frozen — two layers)

| Layer | Check |
|-------|-------|
| **Architectural invariants** | `report_completeness == "complete"`; `evidence_scope == "official_longitudinal"` |
| **Golden** | `normalize_payload(run_official_analysis(...))` deep-equals committed golden |

**Plans specify behavior; goldens specify values.** Do **not** hard-code golden-covered fields (e.g. `eligible_count`, `confidence_label`) as separate test asserts.

`run_official_analysis` **must** call `analyzer.analyze_longitudinal` — same entry point as production.

### Golden failure rule (frozen)

If F1 differs from the committed golden, **first** determine whether the change originated from:

1. Intended fixture modification (06-3 / generator),  
2. Intended analyzer change (outside 06-4 — separate checkpoint), or  
3. Unintended regression  

**Do not regenerate the golden until the source of the difference is identified.**

### Deliverables (frozen)

1. **`official_fixture_paths()` + `run_official_analysis()`** in `scripts/test_c6_longitudinal_analyzer.py`  
2. **F1** — architectural invariants + golden compare  
3. **`analyzer_golden_official_longitudinal.json`** — committed normalized payload; numeric fields recorded in golden, not in plan  
4. **`--write-official-golden`** — writes **only** official golden  
5. **README** — F1 consumer, golden linkage, `--write-official-golden` documented  

### Internal phases (execution order — single 06-4 lock)

```
06-4a  Official companion wrapper + F1 skeleton
06-4b  Generate and commit official golden (pinned, normalized)
06-4c  Wire F1 + --write-official-golden
06-4d  README update; full regression gate green
```

### Files (C6.3-06-4)

| File | Phase |
|------|-------|
| `scripts/test_c6_longitudinal_analyzer.py` | 06-4a, 06-4c |
| `tests/fixtures/cognitive_longitudinal/analyzer_golden_official_longitudinal.json` | 06-4b |
| `tests/fixtures/cognitive_longitudinal/README.md` | 06-4d |

**Not touched:** `analyze_cognitive_longitudinal.py`, join/reporting/plot/threat/provenance modules, `analyzer_golden_summary.json`, 06-3 companion JSONL (unless intentional corpus fix), `completed_improvements_log.md` (06-5).

### Explicitly deferred (not C6.3-06-4)

| Item | Sub-checkpoint |
|------|----------------|
| Protocol / completed-log seal | **06-5** |
| Reporting official golden | Out of scope |
| New CTest entry | Unnecessary — `c6-longitudinal-analyzer` runs F1 |

### C6.3-06-4 stop criteria

1. `analyzer_golden_official_longitudinal.json` committed  
2. F1 passes: architectural invariants + normalized golden match  
3. `--write-official-golden` documented and functional  
4. H10 / default exploratory golden unchanged after normalization  
5. Seven-suite + `ctest -R c6-longitudinal` green  
6. README reflects F1 + official golden  
7. No production analyzer, join, reporting, plotting, threat, or provenance code changes  
8. F1 provides automated regression evidence for Phase 3 exit criterion #3  

**Next:** **C6.3-06-5** documentation seal.

---

## C6.3-06-5 implementation lock 🔒 (2026-07-11)

**Status:** ✅ **Implemented** 2026-07-11 · documentation seal — closes parent **C6.3-06**

### Checkpoint boundary (frozen)

| Checkpoint | Job |
|------------|-----|
| **06-5** (this lock) | Append historical seal + status-pointer sync — **no** executable changes |
| **06-1–06-4** | Already implemented — **not** reopened by 06-5 |

06-5 is a **maintenance checkpoint**, not a feature proposal. It records implementation status and evidence pointers; it does **not** redefine protocol requirements or duplicate normative content from AC/RC/JC.

### Implementation invariant (normative)

> **C6.3-06-5 introduces no code, fixture, golden, or test changes.**

#### Allowed

| Category | Permitted |
|----------|-----------|
| **Historical record** | Append C6.3-06 parent seal entry to `completed_improvements_log.md` |
| **Status sync** | Mark C6.3-06 and C6.3-06-5 ✅ in protocol checkpoint table |
| **Forward-reference repair** | Correct stale `Next:` / `pending` lines that still point at C6.3-06 |
| **Deferred-table pointers** | Flip AC/RC deferred rows from pending → ✅ with log/protocol pointers only |
| **Optional navigation** | One-line links in `cursor_list.md` / `INDEX.md` to fixture README |

#### Forbidden

| Category | Prohibited |
|----------|------------|
| **Code & fixtures** | Changes to `scripts/*`, `tests/fixtures/*`, goldens, or C++ tests |
| **Normative duplication** | Restating AC/RC/JC thresholds, gate definitions, or promotion policy in seal entries |
| **New infrastructure** | Manifests, metadata schemas, unified regression runners, or helper abstractions |
| **Golden writes** | `--write-official-golden`, `--write-goldens`, or generator runs during seal |
| **Protocol redefinition** | Edits to exit-criteria wording, parent invariants, or v0.3 bump |
| **Historical rewrite** | Editing prior seal entry bodies — append only |

#### Semantic change rule

Any change to fixture meaning, golden values, test assertions, or analyzer behavior is **outside** 06-5 and requires its own checkpoint — even if discovered while drafting the seal entry.

#### Stop rule

If implementation discovers a failing regression suite, a golden mismatch, a missing 06-1–06-4 deliverable, or a need to change normative contract text: **stop 06-5**, diagnose, and open the appropriate repair or sub-checkpoint. Do not patch code or goldens under a documentation seal.

### Locked decisions (frozen — no alternatives)

| Decision | Rule |
|----------|------|
| **Seal shape** | One parent entry in `completed_improvements_log.md` — sub-checkpoints summarized in table rows, not separate log entries |
| **Regression commands** | Seal entry **references** fixture README § Regression gate — does **not** duplicate command blocks |
| **Exit criteria** | Pointer table to protocol § Exit criteria — cite prior seals for criteria 2–5; 06-5 completes criterion 1 |
| **Golden discipline** | Carry-forward pointers only — fixture README + § C6.3-06-4; no restated procedures |
| **Production parity** | Carry-forward from § C6.3-06-4 — 06-5 adds no tests |
| **Verification** | Documentation review is primary stop criterion; full regression re-run is **optional** sanity only (06-5 introduces no executable changes) |

### Deliverables (frozen)

#### Required

1. **`completed_improvements_log.md`** — append parent seal entry (`C6 Phase 3 — C6.3-06 regression fixtures ✅`); repair stale `Next:` / `pending` forward references only  
2. **`docs/C6_phase3_protocol.md`** — checkpoint table C6.3-06 + C6.3-06-5 ✅; stale `Next:` cleanup (e.g. § C6.3-04 block)  
3. **`docs/improvements.md`** — remove C6.3-06 from active forks; status marker + log pointer  
4. **AC/RC deferred pointers** — one-line ✅ where rows still list C6.3-06 pending (`C6_phase3_reporting_contract.md`, `C6_phase3_analyzer_contract.md` if applicable)

Seal entry content rules: file paths and sub-checkpoint summary only — **no** counts, IDs, or expected totals (those live in fixtures and goldens).

#### Optional (non-blocking)

| File | Change |
|------|--------|
| `docs/cursor_list.md` | C6 Phase 3 row ✅; link fixture README |
| `docs/INDEX.md` | One-line link to fixture README |

### Internal phases (execution order — single 06-5 lock)

```
06-5a  Append completed_improvements_log.md C6.3-06 seal + exit-criteria pointer table
06-5b  Protocol checkpoint table + stale Next-line cleanup
06-5c  improvements.md status sync
06-5d  AC/RC deferred pointer cleanup; optional cursor_list / INDEX
06-5e  Documentation review (primary verification)
```

### Files (C6.3-06-5)

| File | Phase |
|------|-------|
| `docs/completed_improvements_log.md` | 06-5a |
| `docs/C6_phase3_protocol.md` | 06-5b |
| `docs/improvements.md` | 06-5c |
| `docs/C6_phase3_reporting_contract.md` | 06-5d |
| `docs/C6_phase3_analyzer_contract.md` | 06-5d (if pending) |
| `docs/cursor_list.md` | 06-5d (optional) |
| `docs/INDEX.md` | 06-5d (optional) |

**Not touched:** `scripts/*`, `tests/fixtures/*`, goldens, C++ tests, AC/RC/JC normative body text.

### Explicitly deferred (not C6.3-06-5)

| Item | Checkpoint |
|------|------------|
| Negative fixture slices (`plan-negxx`) | Beyond C6.3-06 |
| `run_c6_regression.py` | Beyond C6.3-06 |
| Production log samples | Beyond C6.3-06 |
| AC/RC/JC version bumps / C6.3 v0.3 | Separate protocol revision |

### C6.3-06-5 stop criteria

1. Parent seal entry appended to `completed_improvements_log.md`  
2. Stale `Next:` / `pending` forward references to C6.3-06 corrected (log, protocol, improvements, AC/RC deferred rows)  
3. Protocol checkpoint table: C6.3-06 and C6.3-06-5 ✅  
4. `improvements.md` marks C6.3-06 complete with log pointer  
5. **No** code, fixture, golden, or test changes  
6. Seal entry summarizes and points — does not duplicate normative contract text  
7. No new infrastructure introduced  

### C6.3-06 parent stop criteria (reference — satisfied by 06-1–06-5 together)

See § C6.3-06 parent lock. 06-5 completes item 1 (sub-checkpoint sealing in log). Items 2–7 were satisfied at 06-4 implementation; 06-5 documents pointers only.

**Explicitly deferred beyond C6.3-06:** negative slices (`plan-negxx`), `run_c6_regression.py`, production log samples, AC/RC/JC version bumps.

---

## C6.3-04 implementation lock 🔒 (2026-07-11)

**Status:** ✅ **Implemented** 2026-07-11 · single atomic checkpoint (internal phases 04-a–d are execution order only — **not** separate locks)

### Implementation invariant (normative)

> **C6.3-04 introduces no executable logic, no analyzer behavior changes, no schema revisions, and no automated promotion mechanism.**

Documentation and cross-links only.

### Purpose

Define when an F-series horizon item (F1–F8) may move from 📋 deferred to active roadmap work, using C6 longitudinal evidence as input to a **human** promotion decision. Close the `cursor_list.md` §9 open question (“no promotion criteria defined”).

### Authority (document ownership — frozen)

| Document | Role |
|----------|------|
| **`docs/improvements.md`** | **Normative** — sole owner of all F-series promotion criteria, procedure, record template, governance notes |
| **`docs/C6_phase3_protocol.md`** | Constitutional floor — § F-series promotion gate + this lock |
| **`docs/C6_phase3_analyzer_contract.md`** | **Normative for thresholds** — sample sizes, trend rules, `evidence_scope`; **shall not be duplicated** in `improvements.md` |
| **`docs/cursor_list.md`** | **Informational only** — may reference `improvements.md`; **shall not duplicate** promotion criteria |

Protocol exit criterion #5: `improvements.md` references C6.3-04.

### Mandatory promotion gates (all required — blocking)

Failure on **any** mandatory gate **blocks** promotion regardless of supporting evidence.

| Gate | Requirement | Verify via (pointer — do not duplicate thresholds) |
|------|-------------|-----------------------------------------------------|
| **Longitudinal evidence** | Two consecutive official evaluation windows (definition below); each summary: `evidence_scope: official_longitudinal`, `report_completeness: complete`, `confidence_label` not `low` | C6 summary JSON / report; AC § `evidence_scope` |
| **No benchmark regression** | `categories.safety.benchmark_regression == false` | RC § Safety gate interpretation |
| **Reproducibility** | `categories.safety.reproducibility_ok == true`; no unresolved E1 / fingerprint drift on pinned reruns | Summary `flags`, `reproducibility`; `benchmark_environment.md` |
| **Official gates green** | `categories.safety.official_gates_green == true`; no `gate_evidence_missing:*` for required harnesses | RC required harnesses: `reflection_ab`, `robustness`, `episodic_learning` |
| **Owner approval** | Explicit owner approval recorded in promotion record | See § Promotion record template |

**Owner approval governance (frozen):**

> Promotion requires explicit owner approval recorded in the promotion record. Analyzer output informs the decision but **never authorizes promotion automatically**.

**Forbidden:** Promotion while any Safety gate is red. Incomplete longitudinal reports (RC incomplete banner) are not valid promotion evidence.

#### Two consecutive official evaluation windows (frozen definition)

> **Two consecutive official evaluation windows** means two **non-overlapping** official longitudinal analyses produced under the **same** `protocol_version` and `metric_schema_version`, each with a valid official anchor and qualifying for `official_longitudinal` scope, with **no intervening failed official evaluation** between them.

| Term | Meaning |
|------|---------|
| **Non-overlapping** | Window `[start_ms, end_ms]` pairs do not share interior overlap (adjacent boundaries allowed) |
| **Same protocol version** | Both summaries share `protocol_version` and `metric_schema_version` |
| **Failed official evaluation** | An official-window run where any mandatory gate would have failed at close-out (regression, `reproducibility_ok: false`, `official_gates_green: false`, or scope below `official_longitudinal`) |
| **Not calendar months** | Consecutive means **evaluation windows**, not wall-clock months unless they coincide |

**Procedure:** Retain `cognitive_longitudinal.jsonl` history; compare the two most recent qualifying official windows.

### Supporting evidence (advisory — non-blocking)

| Evidence | Role | Source |
|----------|------|--------|
| **Segment signal** | Informs *which* F-item is most justified | `segments.*` — trend rules per AC (reference `TREND_MIN_ABS_DELTA`, `MIN_GOALS_PER_COHORT`; do not restate in `improvements.md`) |
| **Mechanism alignment** | Informs whether bottleneck matches proposed F-item | E2 diagnostics / E3 SCR when present — qualitative crosswalk |

**Frozen rule:**

> **Supporting evidence is advisory.** Only mandatory gates are blocking. Unclear mechanism alignment or weak segment signal may justify deferring owner approval, but they are **not** formal veto conditions unless the owner chooses not to sign.

### Promotion procedure (frozen)

1. Run longitudinal analysis for the current official evaluation window.
2. Confirm all **five mandatory gates** against latest summary JSON and markdown report.
3. Confirm **two consecutive official evaluation windows** via JSONL history or archived summaries.
4. Review **supporting evidence** to select target F-id and rationale.
5. Owner records approval in promotion record template.
6. Update target F-item status in `improvements.md` from 📋 to active (date + pointer to record).

**Reversibility (frozen):**

> **Promotion is not permanent.** Later evidence may return an F-series item to deferred status if mandatory promotion criteria are no longer satisfied. Reversal uses the same evidence sources; owner records the change in `improvements.md`.

### Promotion record template (frozen — copy into `improvements.md`)

```markdown
### F-series promotion record — C6.3-04

- **Item:** F_ — _title_
- **Date:** YYYY-MM-DD
- **Approved by:** _owner name_ (explicit owner approval required)
- **Windows reviewed:**
  - W1: `generated_at_ms` / window `[start_ms, end_ms]` — summary path or commit
  - W2: `generated_at_ms` / window `[start_ms, end_ms]` — summary path or commit
- **Mandatory gates (all required):**
  - [ ] Longitudinal evidence (two consecutive official windows, `official_longitudinal`, complete, confidence not low)
  - [ ] `benchmark_regression: false`
  - [ ] `reproducibility_ok: true`
  - [ ] `official_gates_green: true`
  - [ ] Owner approval (this record)
- **Supporting evidence (advisory):**
  - Segment signal: _brief note_
  - Mechanism alignment (E2/E3): _brief note or N/A_
- **Rationale:** _why this F-item now_
```

### Governance notes (frozen)

- **No causal claims:** Promotion authorizes roadmap work, not proof that a change caused a trend.
- **F-series registry:** F1–F8 horizon descriptions remain in `cursor_list.md` §8; **promotion rules live only in `improvements.md`**.
- **Thresholds:** Minimum sample and trend thresholds are defined by **AC v1.0** and shall not be duplicated in `improvements.md`.
- **Automation:** No CI gate, script, or analyzer flag may auto-promote an F-item.

### `improvements.md` section structure (frozen)

Implement as a policy section with these headings only:

1. Purpose  
2. Authority  
3. Mandatory Promotion Gates  
4. Supporting Evidence  
5. Promotion Procedure  
6. Promotion Record Template  
7. Governance Notes  

### Internal implementation phases (single 04 lock — not separate protocol checkpoints)

```
04-a  Add policy section to improvements.md (structure above)
04-b  cursor_list.md — pointers only; resolve §9 open question
04-c  Light status sync (protocol checkpoint table, RC deferred table)
04-d  Seal completed_improvements_log.md
```

### Files (C6.3-04)

| File | Phase |
|------|-------|
| `docs/improvements.md` | 04-a |
| `docs/cursor_list.md` | 04-b |
| `docs/C6_phase3_protocol.md` | 04-c |
| `docs/C6_phase3_reporting_contract.md` | 04-c (deferred table) |
| `docs/completed_improvements_log.md` | 04-d |

**Not touched:** `scripts/*`, `tests/*`, goldens, analyzer/reporting/join code, CTest.

### Explicitly deferred (not C6.3-04)

| Item | Checkpoint |
|------|------------|
| Ops / nightly invocation docs | **C6.3-05** |
| Fixture corpus expansion | **C6.3-06** ✅ |
| Observational harness markdown block | RC v1.1+ |
| `scripts/check_promotion_readiness.py` or auto-promotion | Out of scope |
| Protocol version bump | Stays **C6.3 v0.2.1** |

### C6.3-04 stop criteria

1. `improvements.md` contains the seven-part policy structure with mandatory vs supporting split  
2. Owner-approval governance explicit (no auto-promotion)  
3. Two consecutive official evaluation windows definition present  
4. Reversibility documented  
5. Thresholds referenced via AC only — not duplicated  
6. `cursor_list.md` references but does not duplicate criteria; §9 open question resolved  
7. Implementation invariant honored  
8. Protocol exit criterion #5 satisfied  

**Regression sanity (optional):** Seven-suite + CTest unchanged green — no code expected to change.

**Next:** **C6.3-05** — operator guide ✅ · **C6.3-06** fixture expansion ✅ (§ C6.3-06) · C6 Phase 3 exit criteria satisfied — see `completed_improvements_log.md` § C6.3-06.

---

## Exit criteria (C6 Phase 3 implementation complete)

1. C6.3-01 through C6.3-06 sealed in `completed_improvements_log.md`
2. Analyzer produces all four output artifacts with reproducibility metadata
3. At least one report with correct `evidence_scope`, `threats_disclosed`, and `metric_schema_version`
4. `cursor_list.md` C6 Phase 3 row ✅
5. `improvements.md` references C6.3-04

---

# Informative (continued)

---

## Relationship to parallel tracks

| Track | How C6.3 relates |
|-------|------------------|
| **C3** | May trend `reflection_count`; does not replace A/B |
| **C5** | Safety gate input |
| **E2** | Optional crosswalk; never primary success metric |
| **E3** | Planning segment when JSONL exists |
| **B1** | Corpus fingerprint in join keys |
| **G1d** | Informs v0.3+ trajectory metrics |
| **Phase E** | Does not extend `n=3_strict_trio` claims |

---

## Future extensions (non-normative)

Out of scope for v0.2 — require C6.3 v0.3+ to normativize:

- Seasonal analysis
- Multi-cohort A/B comparison
- Model-version learning curves
- Curriculum analysis (F8)
- Per-domain learning
- Anomaly detection / change-point automation
- Bayesian trend posteriors

---

## Files (expected touch)

| File | Checkpoint |
|------|------------|
| `docs/C6_phase3_protocol.md` | C6.3-0 ✅ |
| `docs/C6_phase3_join_contract.md` | C6.3-01 ✅ |
| `docs/C6_phase3_analyzer_contract.md` | C6.3-02 Step 0 ✅ |
| `docs/C6_phase3_reporting_contract.md` | C6.3-03 Step 03-0 ✅ |
| `tests/fixtures/cognitive_longitudinal/*` | C6.3-01 Step 1 ✅ · C6.3-02 ✅ · C6.3-06 (§ 06-1–06-5) |
| `tests/fixtures/cognitive_longitudinal/README.md` | C6.3-06-1 ✅ · C6.3-06-2 ✅ · C6.3-06-3 ✅ · C6.3-06-4 ✅ · C6.3-06-5 ✅ |
| `scripts/generate_c6_official_longitudinal_fixtures.py` | C6.3-06-3 |
| `tests/fixtures/cognitive_longitudinal/*_official_longitudinal.jsonl` | C6.3-06-3 |
| `scripts/c6_longitudinal_join.py` | C6.3-01 ✅ |
| `scripts/test_c6_longitudinal_join.py` | C6.3-01 Step 3 ✅ |
| `scripts/analyze_cognitive_longitudinal.py` | C6.3-02 ✅ · C6.3-03 Step 03-2c ✅ · 03-3 ✅ · 03-5 ✅ |
| `scripts/test_c6_longitudinal_analyzer.py` | C6.3-02 ✅ · C6.3-03 I/G/O + 03-3 G1–G5 ✅ · C6.3-06-4 ✅ (F1) |
| `tests/fixtures/cognitive_longitudinal/analyzer_golden_official_longitudinal.json` | C6.3-06-4 |
| `scripts/c6_longitudinal_provenance.py` | C6.3-03 Step 03-2a ✅ |
| `scripts/test_c6_longitudinal_provenance.py` | C6.3-03 Step 03-2a ✅ |
| `scripts/c6_longitudinal_threats.py` | C6.3-03 Step 03-2b ✅ · 03-3c ✅ |
| `scripts/test_c6_longitudinal_threats.py` | C6.3-03 Step 03-2b ✅ · 03-3c H10–H11 ✅ |
| `tests/fixtures/cognitive_longitudinal/analyzer_golden_summary.json` | C6.3-03 Step 03-2d ✅ · 03-3d ✅ |
| `scripts/c6_longitudinal_report.py` | C6.3-03 Step 03-1 ✅ |
| `scripts/test_c6_longitudinal_reporting.py` | C6.3-03 Step 03-1 ✅ (R1–R2); R3–R9 ✅ (03-6) |
| `scripts/plot_cognitive_longitudinal.py` | C6.3-03 Step 03-4 ✅ |
| `scripts/test_c6_longitudinal_plots.py` | C6.3-03 Step 03-4 ✅ (P1–P8) |
| `docs/cognitive_longitudinal_ops.md` | C6.3-05 ✅ |
| `docs/improvements.md` | C6.3-04 ✅ (§ C6.3-04 — normative promotion policy) |
| `docs/completed_improvements_log.md` | C6.3-06 ✅ (§ C6.3-06 parent seal) |

---

## Doc map

| Need | File |
|------|------|
| Per-goal schema | `cursor_list.md` § C6 |
| Join algorithms + golden regression | **`C6_phase3_join_contract.md`** § Steps 2–3 |
| Analyzer algorithms + tests | **`C6_phase3_analyzer_contract.md`** AC v1.0 |
| Reporting + plots + gates | **`C6_phase3_reporting_contract.md`** RC v1.0 |
| Operator invocation (manual / scheduling) | **`cognitive_longitudinal_ops.md`** |
| Fixture catalog + regression gate | **`tests/fixtures/cognitive_longitudinal/README.md`** |
| Env pinning | `benchmark_environment.md` |
| E2 episodic eval | `E2_PROTOCOL.md` |
| SCR definition | `COGNATE_V2.md` § 6.1 |
| Active backlog | `cursor_list.md` |

---

**Protocol lock:** C6.3 v0.2.1 is **locked** (2026-07-10). v0.2.1 errata: `app_log.jsonl` authoritative for strategy injection; `MAX_STRATEGY_JOIN_GAP_MS`; join contract JC v1.0; **C6.3-04 lock** (2026-07-11) — F-series promotion policy ownership, mandatory vs supporting gates, window definition; **C6.3-06 lock** (2026-07-11) — fixture expansion 06-1–06-5, fixture semantics + regression preservation; **06-2** — intentionally absent fixture; **06-3** — official corpus + generator; **06-4 lock** — official golden + F1, `run_official_analysis` wrapper, `--write-official-golden`, golden failure rule, no production code changes; **06-5 lock** (2026-07-11) — documentation seal only, append historical record + status-pointer sync, no executable changes; **06-5 implemented** 2026-07-11. Treat Primary Invariant, Artifact Immutability, No Causal Claims, authoritative sources, versioned metrics, frozen metric set, join keys, join contract, window definition, missing-data rules, statistical reporting, reproducibility metadata, threats disclosure, and promotion gate as immutable until **C6.3 v0.3** under Plan → Review → Refine → Lock → Implement → Verify.
