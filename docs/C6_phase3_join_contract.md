# C6 Phase 3 — Join Contract (Normative)

**Contract version:** JC v1.0  
**Status:** 🔒 **Locked** — C6.3-01 Step 0 (2026-07-10)  
**Parent protocol:** [`C6_phase3_protocol.md`](C6_phase3_protocol.md) **C6.3 v0.2.1**  
**Checkpoint:** C6.3-01 (Step 0 ✅ · Step 1 ✅ · Step 2 ✅ · Step 3 ✅ · Step 4 ✅)

> **Governance:** If this contract and the parent protocol disagree, **the protocol wins**. If the join library and this contract disagree, **this contract wins** — update the library, never the reverse.

---

## Purpose

This document specifies **how** immutable benchmark artifacts are joined for C6 Phase 3 longitudinal analysis. The parent protocol defines *what* constitutes valid joins and authoritative inputs; this contract defines deterministic algorithms, enums, and artifact paths.

C6.3-01 answers only:

> *Can we deterministically reconstruct relationships between immutable benchmark artifacts?*

The C6.3-02 analyzer **consumes** join results from the library implementing this contract. It **must not** redefine join semantics. Analyzer contract: [`C6_phase3_analyzer_contract.md`](C6_phase3_analyzer_contract.md) AC v1.0 🔒.

---

## Layer stack

```
C6_phase3_protocol.md (v0.2.1)  →  validity, invariants, constants
C6_phase3_join_contract.md (JC v1.0)  →  algorithms, enums, paths
scripts/c6_longitudinal_join.py  →  implementation (C6.3-01 Steps 1–3)
tests/fixtures/cognitive_longitudinal/  →  synthetic proof (never production logs)
```

---

## Authoritative artifact paths (frozen)

| Artifact | Default path | Events / rows used |
|----------|--------------|-------------------|
| Per-goal metrics | `logs/cognitive_metrics.jsonl` | `GOAL_COGNITIVE_METRICS` |
| Strategy injection | `agent_workspace/app_log.jsonl` | `STRATEGY_INJECTION`, `PLANNER_CONTEXT_ASSEMBLY` |
| Consolidation | `agent_workspace/decision_trace.jsonl` | `trace_type: memory_consolidation`, stage `consolidation_committed` |
| Benchmark environment | `logs/benchmark_env.jsonl`, `logs/benchmark_env.latest.json` | `run_id`, `environment_hash`, model, corpus |
| Official harness JSONL | Harness-specific under `logs/` | Terminal events, tier, `run_id` |

Override paths are permitted only when explicitly passed to the join library and recorded in output metadata.

### Non-authoritative (forbidden for joins)

Console stdout/stderr, debug traces, IDE logs, GUI exports, unstructured grep.

---

## Field mapping (protocol key → JSON)

| Protocol key | JSON field |
|--------------|------------|
| `benchmark_run_id` | `run_id` on metrics and harness rows |
| `timestamp_ms` (goals) | `goal_started_at_ms` |
| `timestamp_ms` (traces) | `finished_at_ms` or `emitted_at_ms` |
| `timestamp_ms` (app_log) | `emitted_at_ms` if present; else `timestamp_ms` — loader MUST normalize to int64 UTC ms |
| `env_hash` | metrics `env_hash`; sidecar `environment_hash` |
| `model_hash` | sidecar `BenchmarkEnvironment` model hash |
| `corpus_fingerprint` | sidecar corpus fingerprint |
| `consolidation_id` (synthetic) | `{session_id}:{finished_at_ms}` when no explicit ID exists |

---

## Time normalization (required)

All join logic SHALL:

- Use **UTC only** — Unix epoch **integer milliseconds**
- Compare with integer ordering (monotonic)
- **Never** parse local timezones
- Reject non-integer or negative timestamps on required fields → `join_status: INVALID_INPUT`

---

## Frozen `JoinStatus` enum

**Do not invent values outside this set.**

| Value | Meaning |
|-------|---------|
| `RESOLVED` | Join completed; segment value fields are authoritative |
| `UNRESOLVED` | Required keys or candidates missing; segment values = `null` |
| `MISSING_ARTIFACT` | Entire input file absent |
| `FINGERPRINT_MISMATCH` | Cohort fingerprint incompatible — segment **invalidated** |
| `STALE_EVENT` | Candidate exists but outside `MAX_STRATEGY_JOIN_GAP_MS` |
| `INVALID_INPUT` | Metrics row fails schema minimum |

---

## Frozen `JoinConfidence` enum (optional on output)

| Value | When |
|-------|------|
| `exact` | Direct field or confirmed absence in authoritative data |
| `time_proximity` | Strategy join via session + timestamp window |
| `synthetic` | Derived ID (e.g. `consolidation_id`) |
| `inferred` | Reserved — **not used in JC v1.0** |

---

## Value vs certainty separation (required)

Segment outputs **must not** overload booleans with unknown state.

| `join_status` | Segment value fields |
|---------------|----------------------|
| `RESOLVED` | Populated (`true` / `false` / strings as applicable) |
| Any other status | **`null`** — including `strategy_injected`, `post_consolidation`, env fields |

Example — confirmed no strategy:

```json
{
  "strategy_injected": false,
  "strategy_id": null,
  "join_status": "RESOLVED",
  "join_confidence": "exact"
}
```

Example — unknown:

```json
{
  "strategy_injected": null,
  "strategy_id": null,
  "join_status": "UNRESOLVED",
  "join_confidence": null
}
```

---

## Preregistered constant (from parent protocol)

| Constant | Value | Source |
|----------|-------|--------|
| `MAX_STRATEGY_JOIN_GAP_MS` | **300_000** (5 minutes) | `C6_phase3_protocol.md` § Preregistered constants |

Implementation references the protocol constant — not a local magic number.

---

## Join algorithms

### A. Goal → environment

**Input:** metrics row (`run_id`, `env_hash` optional)

**Lookup:** `benchmark_env` row where `run_id` == metrics `run_id`

| Outcome | `join_status` | Values |
|---------|---------------|--------|
| Match found, cohort consistent | `RESOLVED` | `env_hash`, `model_hash`, `corpus_fingerprint`, `tier`; `join_confidence: exact` |
| No matching `run_id` | `UNRESOLVED` | all env fields `null` |
| Env file missing | `MISSING_ARTIFACT` | all `null` |
| Hash drift vs cohort lock | `FINGERPRINT_MISMATCH` | all `null` — **do not consume in rollup** |

### B. Goal → strategy injected

**Input:** metrics row (`session_id`, `goal_started_at_ms`, `plan_id`)

**Candidates:** `app_log.jsonl` rows where:

- `session_id` matches metrics row
- `event` ∈ {`STRATEGY_INJECTION`, `PLANNER_CONTEXT_ASSEMBLY`}
- `timestamp_ms` ≤ `goal_started_at_ms`
- `goal_started_at_ms − timestamp_ms` ≤ `MAX_STRATEGY_JOIN_GAP_MS`

**Deterministic tie-break** (total order — two implementations must agree):

1. Latest `timestamp_ms` (descending)
2. Event type: `STRATEGY_INJECTION` over `PLANNER_CONTEXT_ASSEMBLY`
3. Lexicographic `strategy_id` (descending); empty sorts last
4. Source file line number (ascending)

| Outcome | `join_status` | `strategy_injected` | `strategy_id` | `join_confidence` |
|---------|---------------|----------------------|-----------------|-------------------|
| Winner is `STRATEGY_INJECTION` | `RESOLVED` | `true` | from metadata | `time_proximity` |
| Winner is `PLANNER_CONTEXT_ASSEMBLY`, `strategy_injection: true` | `RESOLVED` | `true` | `null` | `time_proximity` |
| Winner is `PLANNER_CONTEXT_ASSEMBLY`, `strategy_injection: false` | `RESOLVED` | `false` | `null` | `exact` |
| No candidates in session | `RESOLVED` | `false` | `null` | `exact` |
| Candidates only outside gap | `STALE_EVENT` | `null` | `null` | `null` |
| `app_log.jsonl` missing | `MISSING_ARTIFACT` | `null` | `null` | `null` |

`plan_id` is **not** used for strategy join in JC v1.0 (not present on app_log events).

### C. Goal → post-consolidation

**Input:** metrics row (`session_id`, `goal_started_at_ms`)

**Candidates:** `decision_trace.jsonl` where:

- `trace_type` == `memory_consolidation`
- stage `name` == `consolidation_committed` and `success` == true
- `stage.metadata.session_id` == metrics `session_id`
- trace `finished_at_ms` < `goal_started_at_ms`

**Tie-break:** latest `finished_at_ms` (descending); then file line order (ascending).

| Outcome | `join_status` | `post_consolidation` | `consolidation_id` | `join_confidence` |
|---------|---------------|---------------------|-------------------|-------------------|
| Candidate found | `RESOLVED` | `true` | `{session_id}:{finished_at_ms}` | `synthetic` |
| No candidate | `RESOLVED` | `false` | `null` | `exact` |
| `decision_trace.jsonl` missing | `MISSING_ARTIFACT` | `null` | `null` | `null` |

### D. Plan reuse (denormalized)

**Input:** metrics row `plan_reused` (bool)

| Outcome | `join_status` |
|---------|---------------|
| Field present | `RESOLVED` — value from row; `join_confidence: exact` |
| Field absent | `INVALID_INPUT` for this segment |

No external artifact join required.

---

## Fingerprint mismatch (strict)

`FINGERPRINT_MISMATCH` is **not** `RESOLVED` with a warning. Segment values are `null`. Downstream longitudinal statistics **must not** include invalidated segments. Reports may duplicate `flags: ["fingerprint_mismatch"]` for human readers; `join_status` drives exclusion.

---

## Missing data summary

| Condition | Behavior |
|-----------|----------|
| Missing artifact file | `MISSING_ARTIFACT`; warn; omit dependent segments |
| Unresolved keys | `UNRESOLVED`; never impute |
| Invalid metrics row | `INVALID_INPUT`; skip goal or segment per analyzer policy |
| Incomplete window (analyzer) | Parent protocol `report_completeness: incomplete` |

---

## Output shape — `JoinedGoalRecord` (per `plan_id`)

Minimum fields for library output (consumed by C6.3-02):

```json
{
  "plan_id": "…",
  "session_id": "…",
  "goal_started_at_ms": 0,
  "benchmark_run_id": "…",
  "plan_reused": { "value": false, "join_status": "RESOLVED", "join_confidence": "exact" },
  "strategy": {
    "strategy_injected": false,
    "strategy_id": null,
    "join_status": "RESOLVED",
    "join_confidence": "exact"
  },
  "consolidation": {
    "post_consolidation": false,
    "consolidation_id": null,
    "join_status": "RESOLVED",
    "join_confidence": "exact"
  },
  "environment": {
    "env_hash": "…",
    "model_hash": "…",
    "corpus_fingerprint": "…",
    "tier": "full",
    "join_status": "RESOLVED",
    "join_confidence": "exact"
  }
}
```

When `join_status` ≠ `RESOLVED`, nested value fields are `null`.

---

## Fixture philosophy (required)

> Fixtures **SHALL** remain **synthetic**. **Never** commit production benchmark artifacts. This keeps the repository deterministic forever.

Golden regression file: `tests/fixtures/cognitive_longitudinal/golden_join_results.json`.

### Golden format lock (checkpoint contract)

**Authority hierarchy:**

```
C6_phase3_protocol.md  →  join contract  →  golden fixtures  →  implementation
```

The **join contract** is authoritative. **`golden_join_results.json`** is the canonical executable representation of that contract for regression testing.

Any change to golden semantics requires:

1. Join contract revision (if algorithms or public shape change)
2. Regenerated fixtures with explicit review
3. Explicit human approval

Silent golden drift is **forbidden**.

---

## C6.3-01 Step 4 — Production smoke (locked)

**Status:** ✅ **Complete** — 2026-07-10  
**Deliverable:** `--smoke` mode on `scripts/c6_longitudinal_join.py`  
**Depends on:** Steps 1–3 ✅, JC v1.0

> **Smoke summarizes the current state of production artifacts only. It SHALL NOT be interpreted as a benchmark.**

Step 3 proves join correctness on synthetic fixtures. Step 4 proves operational usability on live logs. Smoke failure does **not** block C6.3-01 checkpoint close — Step 3 is the correctness gate.

### Objective

Optional operator-facing smoke on **production artifacts** — reports **join-system health** (segment-level), validation drops, and report completeness. No golden assertions, no statistics, no trend analysis.

### CLI separation

| Mode | Command | Purpose |
|------|---------|---------|
| Fixture regression (gating) | `python3 scripts/test_c6_longitudinal_join.py` | Step 3 — correctness proof |
| Production smoke (non-gating) | `python3 scripts/c6_longitudinal_join.py --smoke` | Step 4 — operational health |

Non-smoke CLI (Step 2) unchanged — detailed dev summary + optional JSON for debugging.

### Default production paths (repo-root-relative)

| Artifact | Path |
|----------|------|
| Metrics | `logs/cognitive_metrics.jsonl` |
| App log | `agent_workspace/app_log.jsonl` |
| Decision trace | `agent_workspace/decision_trace.jsonl` |
| Benchmark env | `logs/benchmark_env.jsonl` |

All four overridable via `--metrics`, `--app-log`, `--decision-trace`, `--benchmark-env`.

**Fixtures forbidden:** `tests/fixtures/cognitive_longitudinal/*` is synthetic only — never used as smoke defaults.

### Aggregation rule (frozen)

**Segment-level** — Step 4 reports join-system health, not goal health.

Three independent algorithms (environment, strategy, consolidation) are accounted separately. Each joined goal contributes **three** segment join outcomes.

| Counter | Rule |
|---------|------|
| `goals_analyzed` | Valid metrics rows that entered join (one per goal) |
| `segment_joins_resolved` | Segment joins with `join_status: RESOLVED` |
| `segment_joins_unresolved` | `UNRESOLVED` |
| `segment_joins_missing_artifact` | `MISSING_ARTIFACT` |
| `segment_joins_fingerprint_mismatch` | `FINGERPRINT_MISMATCH` |
| `segment_joins_stale_event` | `STALE_EVENT` |

Goal-level rollup is **forbidden** in smoke output — it would hide which segment failed.

`INVALID_INPUT` segments (if any) reported under `--verbose` only.

### `report_completeness`

| Value | Condition |
|-------|-----------|
| `complete` | All required artifacts readable; `JoinContext.missing_artifacts` empty; `ValidationSummary.total_invalid == 0` |
| `incomplete` | Any of: required file unreadable (partial join still permitted where library allows), artifact flagged missing in context, validation dropped rows, or unreadable optional artifacts affecting join coverage |

Informational only — does **not** change exit code.

### `invalid_rows_skipped`

`ValidationSummary.total_invalid` — rows dropped by validator before indexing. Operators MUST see this; silent discard is forbidden in smoke reporting.

### Frozen smoke output fields

Adding or renaming smoke counters requires **JC v1.1+** contract revision (or parent protocol revision). Shell scripts and ops tooling may depend on this shape.

**Default output (required):**

```text
C6.3-01 smoke (non-gating)
  goals_analyzed:                     N
  segment_joins_resolved:             N
  segment_joins_unresolved:           N
  segment_joins_missing_artifact:     N
  segment_joins_fingerprint_mismatch: N
  segment_joins_stale_event:          N
  invalid_rows_skipped:               N
  report_completeness:                complete|incomplete
```

**`--verbose` additions (required when flag set):**

```text
  metrics_rows:           N
  app_log_rows:           N
  decision_trace_rows:    N
  benchmark_env_rows:     N
  earliest_goal_ms:       <epoch ms|(none)>
  latest_goal_ms:         <epoch ms|(none)>
```

`earliest_goal_ms` / `latest_goal_ms` from `goal_started_at_ms` on joined metrics rows.

### Exit codes

| Code | Meaning |
|------|---------|
| `0` | Smoke executed — **even if join counts show problems** |
| `1` | Invalid arguments |
| `2` | Cannot execute (e.g. metrics file missing/unreadable) |

Join problems do **not** fail the smoke run. Smoke fails only when it cannot execute.

### `--smoke` behavior

- Emit compact smoke summary only — no per-goal JSON unless `--verbose` extends output per implementation
- Use production path defaults when paths not overridden
- Call existing join library — **must not** redefine join semantics

### Step 4 success criteria

1. `python3 scripts/c6_longitudinal_join.py --smoke` runs against production defaults
2. Output matches frozen field list above (segment-level counters)
3. `python3 scripts/test_c6_longitudinal_join.py` still exits `0`
4. Manual operator run on machine with real logs documented in checkpoint close

### Step 4 explicitly out of scope

Analyzer (`analyze_cognitive_longitudinal.py`), statistics, trend analysis, promotion gates, runtime/C++ changes, CTest/CMake, golden assertions on production data, committing production log samples, failing CI on smoke counts.

### C6.3-01 checkpoint close (after Step 4 green)

- Mark Step 4 ✅ in protocol + join contract
- Update `completed_improvements_log.md` — C6.3-01 fully sealed (Steps 0–4)
- Full **C6 Phase 3** remains open until C6.3-02+

---

## C6.3-01 implementation scope (reminder)

| In scope | Out of scope |
|----------|--------------|
| Step 0 ✅ Join contract + protocol v0.2.1 errata | C++ / Option B |
| Step 1 ✅ Synthetic fixtures + `golden_join_results.json` | `analyze_cognitive_longitudinal.py` |
| Step 2 ✅ `scripts/c6_longitudinal_join.py` | Plots, statistics, CTest expansion |
| Step 3 ✅ `scripts/test_c6_longitudinal_join.py` | Runtime behavior changes |
| Step 4 ✅ `--smoke` on production artifacts | |

---

## C6.3-01 Step 2 — Join library implementation plan (locked)

**Status:** 🔒 **Locked for implementation** — 2026-07-10  
**Deliverable:** `scripts/c6_longitudinal_join.py`  
**Depends on:** Step 1 fixtures ✅, JC v1.0, C6.3 v0.2.1

### Objective

Read-only library: **load → validate → index → join** → `JoinedGoalRecord` list per JC v1.0 algorithms. C6.3-02 consumes output; **must not** redefine join semantics.

### Module split rule

Split by **responsibility**, not line count. Default: single file `scripts/c6_longitudinal_join.py`. Split only when unrelated concerns emerge (e.g. CLI + stats + plotting).

### Layer pipeline

```
loader     → raw rows (+ _source_line per row)
validator  → reject malformed rows before indexing
indexer    → JoinContext + ValidationSummary
resolvers  → segment joins (context + metrics_row)
orchestrator → join_goals_from_records()
cli        → summary, exit codes
```

Loading and indexing are **separate**.

### Frozen enums

`JoinStatus`: `RESOLVED`, `UNRESOLVED`, `MISSING_ARTIFACT`, `FINGERPRINT_MISMATCH`, `STALE_EVENT`, `INVALID_INPUT`

`JoinConfidence`: `exact`, `time_proximity`, `synthetic` (`inferred` reserved — not used)

**New enum values require protocol + contract revision (JC v1.1+).** Downstream code must treat unknown statuses as errors.

### Constants (from parent protocol)

| Constant | Value |
|----------|-------|
| `MAX_STRATEGY_JOIN_GAP_MS` | `300_000` |
| `CONTRACT_VERSION` | `JC v1.0` |
| `PROTOCOL_VERSION` | `C6.3 v0.2.1` |

### `normalize_timestamp_ms(value) -> int | None`

| Input | Result |
|-------|--------|
| `int` | as-is |
| `float` with zero fractional part | `int(value)` |
| numeric string (e.g. `"1721003000"`) | parsed `int` |
| ISO strings, bool, None, negative, non-integer float | `None` → `INVALID_INPUT` |

UTC epoch ms only; no timezone parsing.

### `ValidationSummary`

Returned alongside `JoinContext` from `prepare_join()` — **not** embedded in `JoinContext`.

| Field | Meaning |
|-------|---------|
| `invalid_metrics_rows` | Dropped before join |
| `invalid_app_log_rows` | Dropped before indexing |
| `invalid_trace_rows` | Dropped before indexing |
| `invalid_env_rows` | Dropped before indexing |

Optional `invalid_row_details` (artifact, source line, reason) for `--verbose` only.

Invalid **metrics** rows are excluded from join output. Invalid auxiliary rows are excluded from indexes; counts must be visible in CLI.

### `JoinContext` (immutable)

```text
JoinContext
    env_by_run_id
    strategy_events      # session_id → events
    consolidations       # session_id → events
    missing_artifacts    # frozenset internally; list when serializing
```

Every resolver: `(metrics_row, context) -> Segment`.

### `JoinArtifacts` (build output)

```text
JoinArtifacts
    context: JoinContext
    validation: ValidationSummary
    validated_metrics: list[dict]
```

### Segment model

`BaseSegment` with `join_status`, `join_confidence`. Subtypes: `PlanReuseSegment`, `StrategySegment`, `ConsolidationSegment`, `EnvironmentSegment`.

**Public `to_dict()`:** protocol shape — value fields `null` when `join_status ≠ RESOLVED`.

**Diagnostics:** `EnvironmentSegment` retains `observed_env_hash` / `expected_env_hash` on `FINGERPRINT_MISMATCH` (internal / `--verbose`; omitted from golden diff).

### Environment resolver (edge case locked)

| Case | `join_status` |
|------|---------------|
| `run_id` match, metrics `env_hash` **absent** | `RESOLVED` (skip mismatch check) |
| `run_id` match, both hashes present, **equal** | `RESOLVED` |
| `run_id` match, both present, **differ** | `FINGERPRINT_MISMATCH` + observed/expected preserved |
| `run_id` not in index | `UNRESOLVED` |
| env file missing | `MISSING_ARTIFACT` |

**Absence ≠ disagreement.**

### Strategy tie-break (deterministic)

1. Latest `timestamp_ms` (descending)
2. `STRATEGY_INJECTION` over `PLANNER_CONTEXT_ASSEMBLY`
3. Lexicographic `strategy_id` (descending; empty last)
4. `_source_line` ascending

### App log field names

Use `event_name` (not `event`) per `StructuredLogger` output in `logger.cpp`.

### Public API

- `prepare_join(...) -> JoinArtifacts`
- `join_goals_from_records(...) -> list[JoinedGoalRecord]`
- `join_goals(paths...) -> list[JoinedGoalRecord]`

### CLI

```bash
python3 scripts/c6_longitudinal_join.py \
  --metrics ... --app-log ... --decision-trace ... --benchmark-env ... \
  [--pretty] [--verbose] [--json-only]
```

**Summary before JSON** (unless `--json-only`):

```text
C6 longitudinal join (JC v1.0)
  goals_joined:                     N
  invalid_metrics_rows:             N
  invalid_app_log_rows:             N
  invalid_trace_rows:               N
  invalid_env_rows:                 N
  environment_resolved:             N
  environment_fingerprint_mismatch: N
  strategy_resolved:                N
  strategy_stale_event:             N
  consolidation_resolved:           N
  missing_artifacts:              (none|...)
```

**Exit codes:** `0` success · `1` invalid arguments · `2` failed to load required artifacts (metrics unreadable)

### Step 2 success criteria

1. All resolvers per JC v1.0; `JoinContext` + loader/validator/indexer separation
2. `ValidationSummary` in `prepare_join` + CLI
3. Fingerprint mismatch retains observed/expected internally
4. `normalize_timestamp_ms` handles int, float.0, numeric string
5. CLI summary + exit codes; manual smoke on Step 1 fixtures
6. **Automated golden diff deferred to Step 3**

### Step 2 explicitly out of scope

Analyzer, plotting, statistics, benchmark/C++ changes, runtime behavior, CTest.

---

## C6.3-01 Step 3 — Golden regression ✅

**Status:** ✅ **Complete** — 2026-07-10  
**Deliverable:** `scripts/test_c6_longitudinal_join.py`  
**Depends on:** Steps 1–2 ✅, JC v1.0, `golden_join_results.json` (checkpoint contract)

### Objective

Automated proof that `c6_longitudinal_join.py` matches golden public output, handles C6-05 missing artifact, is **idempotent**, **non-mutating** on inputs, and produces **deterministic** serialized output.

Direct import of `c6_longitudinal_join` (no subprocess). Fixture paths resolved via `__file__` (any cwd).

### Default fixture paths

`tests/fixtures/cognitive_longitudinal/` — `metrics.jsonl`, `app_log.jsonl`, `decision_trace.jsonl`, `benchmark_env.jsonl`, `golden_join_results.json`

Override: `--fixtures-dir`

### Test suite

| ID | Name | Assert |
|----|------|--------|
| **T1** | Golden regression | Per `plan_id`, `to_public_dict(verbose=False)` deep-equals `golden.records[plan_id]`; no extra/missing plans; use `golden.case_map` in failures |
| **T2** | Deterministic serialization | Sort by `plan_id`; `json.dumps(..., sort_keys=True, separators=(",", ":"))` identical across serializations; sorted keys match golden key set |
| **T3** | Idempotence | Two full joins from identical inputs → identical public dicts by `plan_id` (fresh `prepare_join` each run) |
| **T4** | Input immutability | `deepcopy` inputs before `join_goals_from_records()`; assert rows unchanged after join |
| **T5** | Validation smoke | `total_invalid == 0`; `missing_artifacts` empty; `validation.issues` empty; no duplicate `plan_id` in output or validated metrics |
| **T6** | C6-05 missing trace | `trace_rows=[]`, `trace_exists=False`; on `plan-c603`: consolidation `MISSING_ARTIFACT`, strategy + environment `RESOLVED` |

### Failure output (structured)

On mismatch, emit:

```text
FAIL C6-09 plan-c609 environment.join_status
  expected: FINGERPRINT_MISMATCH
  actual:   RESOLVED
```

Helper walks nested segments: `plan_reused`, `strategy`, `consolidation`, `environment`.

`--verbose`: unified diff; default: first N field failures + summary count.

### CLI

```bash
python3 scripts/test_c6_longitudinal_join.py
python3 scripts/test_c6_longitudinal_join.py --fixtures-dir ...
python3 scripts/test_c6_longitudinal_join.py --verbose
```

**Exit codes:** `0` all pass · `1` assertion failure · `2` missing/invalid fixture or golden JSON

### Success output

```text
C6.3-01 join regression (JC v1.0)
  golden_cases:           10/10
  deterministic_output:     ok
  idempotence:              ok
  input_immutability:       ok
  validation_smoke:         ok
  missing_artifact_case:    ok
  OK
```

### Implementation order

1. Fixture/golden loader + repo-root resolution  
2. `diff_public_dict()` failure reporter  
3. T1–T6  
4. `main()` + exit codes  

Single file ~180–250 lines; **no pytest**, no CTest.

### Step 3 success criteria

1. `python3 scripts/test_c6_longitudinal_join.py` exits `0`  
2. T1–T6 pass  
3. C6.3-01 checkpoint sealed in `completed_improvements_log.md`  
4. No analyzer, C++, or CTest  

### Step 3 explicitly out of scope

`analyze_cognitive_longitudinal.py`, CMake wiring. (Production smoke is Step 4 — see § Step 4.)

### C6.3-01 checkpoint close (after Step 3 green)

- Mark Step 3 ✅ in protocol + join contract  
- `completed_improvements_log.md` entry for **C6.3-01** join library + regression (Steps 0–3)  
- Step 4 remains — see § Step 4 for full C6.3-01 close

---

## Revision history

| Version | Date | Change |
|---------|------|--------|
| JC v1.0 | 2026-07-10 | Initial lock — C6.3-01 Step 0 |
| JC v1.0 | 2026-07-10 | C6.3-01 Step 2 join library implemented |
| JC v1.0 | 2026-07-10 | C6.3-01 Step 3 golden regression implemented (T1–T6 green) |
| JC v1.0 | 2026-07-10 | Env validator skips non-`BENCHMARK_ENV` sidecar rows (terminal events) without invalid count |

---

**Contract lock:** JC v1.0 is locked with C6.3 v0.2.1. Amend only via explicit protocol + contract revision (JC v1.1+).
