# C6 Phase 3 — Analyzer Contract (Normative)

**Contract version:** AC v1.0  
**Status:** 🔒 **Locked** — C6.3-02 Step 0 (2026-07-10)  
**Parent protocol:** [`C6_phase3_protocol.md`](C6_phase3_protocol.md) **C6.3 v0.2.1**  
**Join dependency:** [`C6_phase3_join_contract.md`](C6_phase3_join_contract.md) **JC v1.0** 🔒  
**Checkpoint:** C6.3-02 (Step 0 ✅ · Steps 02-1–02-7 ✅)

> **Governance:** If this contract and the parent protocol disagree, **the protocol wins**. If the analyzer and this contract disagree, **this contract wins** — update the analyzer, never the reverse. Join semantics are owned by JC v1.0; the analyzer **imports** `c6_longitudinal_join` and **must not** redefine them.

---

## Purpose

This document specifies **how** the C6.3-02 longitudinal analyzer computes windows, cohorts, statistics, and output artifacts. The parent protocol defines *what* constitutes valid longitudinal evidence; this contract defines deterministic algorithms, frozen allowlists, output shapes, and test obligations.

C6.3-02 answers:

> *Given joined, windowed goals, can we produce a reproducible longitudinal evidence package?*

---

## Layer stack

```
C6_phase3_protocol.md (v0.2.1)       →  invariants, metrics, windows, evidence_scope
C6_phase3_join_contract.md (JC v1.0) →  join inputs only
C6_phase3_analyzer_contract.md (AC v1.0) →  analyzer algorithms, outputs, tests
C6_phase3_reporting_contract.md (RC v1.0) →  markdown, plots, gates (C6.3-03)
scripts/c6_longitudinal_join.py    →  join library (import)
scripts/analyze_cognitive_longitudinal.py → implementation (C6.3-02 + C6.3-03 orchestration)
tests/fixtures/cognitive_longitudinal/ →  synthetic proof (never production logs)
```

---

## Primary rules

| Rule | Requirement |
|------|-------------|
| Read-only | Analyzer SHALL NOT influence runtime behavior |
| Join import | Use `c6_longitudinal_join`; never redefine join algorithms |
| Full JSONL payload | Each append carries complete analytical content for that run |
| Frozen terminal allowlist | Official anchors use exact event names only — **no suffix inference** |
| No silent exclusion | Failed goals SHALL NOT be dropped without flags |

---

## Output artifacts

### 1. `logs/cognitive_longitudinal.jsonl` (append-only historical record)

Each analyzer run appends **one** row with `event: COGNITIVE_LONGITUDINAL_SUMMARY`.

**Each JSONL row SHALL carry the full computed payload** for that run: `categories`, `segments`, `threats_disclosed`, `flags`, `warnings`, plus all reproducibility metadata.

**Forbidden:** JSONL rows that only reference `cognitive_longitudinal_summary.json` via a pointer. The summary file is overwritten each run; pointer-only JSONL would destroy historical evidence.

### 2. `logs/cognitive_longitudinal_summary.json` (latest snapshot only)

Overwrite with the **same analytical content** as the JSONL row just appended (convenience for operators and downstream tooling). Optional `summary_written_at_ms` permitted; not required for history.

### Write order

1. Compute full payload  
2. Append JSONL row (full payload)  
3. Overwrite summary JSON (identical analytical content)

---

## JSONL / summary minimum shape

Both artifacts share the same analytical fields (summary may add `summary_written_at_ms`):

```json
{
  "event": "COGNITIVE_LONGITUDINAL_SUMMARY",
  "protocol_version": "C6.3 v0.2.1",
  "metric_schema_version": "1.0",
  "analyzer_version": "0.2.0",
  "analyzer_commit_hash": "abc123",
  "generated_at_ms": 0,
  "report_completeness": "complete",
  "evidence_scope": "official_longitudinal",
  "confidence_label": "high",
  "window": {
    "start_ms": 0,
    "end_ms": 0,
    "days": 28,
    "anchor": "official_benchmark_execution"
  },
  "reproducibility": {
    "input_log_path": "logs/cognitive_metrics.jsonl",
    "input_artifact_paths": [],
    "benchmark_run_ids_consumed": []
  },
  "categories": {
    "performance": {},
    "efficiency": {},
    "memory": {},
    "planning": {},
    "safety": {}
  },
  "segments": {},
  "threats_disclosed": [],
  "flags": [],
  "warnings": []
}
```

Category and segment computation definitions: parent protocol § Trend categories (metric_schema_version 1.0).

---

## Default production paths

Repo-root-relative (same as join smoke defaults):

| Artifact | Path |
|----------|------|
| Metrics | `logs/cognitive_metrics.jsonl` |
| App log | `agent_workspace/app_log.jsonl` |
| Decision trace | `agent_workspace/decision_trace.jsonl` |
| Benchmark env | `logs/benchmark_env.jsonl` |

All overridable via CLI. Fixtures under `tests/fixtures/cognitive_longitudinal/` are synthetic only.

---

## Pipeline

```
load artifacts → join (import c6_longitudinal_join)
    → merge JoinedGoalRecord + metrics row by plan_id
    → cohort filter (official / exploratory)
    → resolve window anchor (frozen terminal allowlist)
    → filter current + prior windows
    → compute categories + segments
    → statistical reporting (Wilson CI, trend labels, confidence_label)
    → threats_disclosed + evidence_scope
    → append JSONL (full) + write summary snapshot
```

---

## Internal model — `AnalysisGoalRecord`

Merge join output with raw `GOAL_COGNITIVE_METRICS` row keyed by `plan_id`:

| Source | Fields |
|--------|--------|
| Join library | segments, `plan_id`, `session_id`, `goal_started_at_ms`, `benchmark_run_id` |
| Metrics row | `outcome`, latency, token, reflection, GRAG fields per protocol § 1.0 |

**Rollup exclusion:**

- Environment `FINGERPRINT_MISMATCH` → exclude from official rollup; flag
- Non-`full` tier in official cohort → exclude; flag tier contamination
- Never silently drop `outcome == failed`

---

## Official terminal event allowlist (frozen)

Terminal detection uses **exact event name match** against this allowlist only.

**Suffix fallback (`_COMPLETE` / `_ABORTED` pattern matching) is forbidden** as official C6.3 behavior.

Authority: [`benchmark_environment.md`](benchmark_environment.md) § Harness terminal events.

| Harness | Allowlisted events |
|---------|-------------------|
| `run_test_suite` | `TEST_SUITE_COMPLETE`, `TEST_SUITE_ABORTED` |
| `run_reflection_ab_benchmark` | `REFLECTION_AB_COMPLETE`, `REFLECTION_AB_ABORTED` |
| `run_robustness_suite` | `ROBUSTNESS_COMPLETE`, `ROBUSTNESS_ABORTED` |
| `run_chat_rag_benchmark` | `CHAT_RAG_BENCHMARK_COMPLETE`, `CHAT_RAG_BENCHMARK_ABORTED` |
| `run_grag_benchmark` | `GRAG_BENCHMARK_COMPLETE`, `GRAG_BENCHMARK_ABORTED` |
| `run_episodic_learning_benchmark` | `EPISODIC_LEARNING_COMPLETE`, `EPISODIC_LEARNING_ABORTED` |

**Complete frozen set (12):**

```
TEST_SUITE_COMPLETE
TEST_SUITE_ABORTED
REFLECTION_AB_COMPLETE
REFLECTION_AB_ABORTED
ROBUSTNESS_COMPLETE
ROBUSTNESS_ABORTED
CHAT_RAG_BENCHMARK_COMPLETE
CHAT_RAG_BENCHMARK_ABORTED
GRAG_BENCHMARK_COMPLETE
GRAG_BENCHMARK_ABORTED
EPISODIC_LEARNING_COMPLETE
EPISODIC_LEARNING_ABORTED
```

Adding an event name requires **AC v1.1+** (or parent protocol revision) — not runtime suffix inference.

Unknown `event` values in `benchmark_env.jsonl` are **ignored** for anchor resolution.

---

## Window anchor algorithm

**Sidecar schema (verified):** terminal rows use `event`, `run_id`, `ts` (epoch ms). Join to `BENCHMARK_ENV` on `run_id`. Tier at `env.runtime.tier` (normalize case; `OFFICIAL_TIER` = `full`).

| Step | Rule |
|------|------|
| 1 | Collect rows where `event` ∈ frozen allowlist |
| 2 | Join each to `BENCHMARK_ENV` for same `run_id` |
| 3 | **Official execution** = terminal row + tier normalizes to `full` |
| 4 | **Anchor** = `ts` of most recent official terminal row |
| 5 | Tie-break same `ts` | Prefer `*_COMPLETE` over `*_ABORTED` (lexicographic event name as secondary tie-break if needed) |
| 6 | No qualifying anchor | `window_end_ms = generated_at_ms`; `flags: ["no_official_anchor"]`; cap `evidence_scope` at `exploratory_only` |

**Window bounds** (parent protocol): rolling `DEFAULT_WINDOW_DAYS` = 28; inclusive `goal_started_at_ms ∈ [end − 28d, end]`; prior window `[end − 56d, end − 28d]`.

### Step 02-2a gate (mandatory before window code freezes)

| Task | Deliverable |
|------|-------------|
| Inspect production `benchmark_env.jsonl` | Record observed events + tiers in implementation notes |
| Extend fixture `benchmark_env.jsonl` | Allowlisted `TEST_SUITE_COMPLETE` + `tier: full` + `ts` |
| Negative test | `event: "FOO_COMPLETE"` (not on allowlist) must **not** become anchor |
| Golden anchor test | Fixture terminal `ts` → expected `window_end_ms` |

**Do not implement Step 02-2** until 02-2a passes.

---

## Statistical reporting

Constants from parent protocol: `MIN_GOALS_LONGITUDINAL` = 30, `MIN_GOALS_PER_COHORT` = 10, `TREND_MIN_ABS_DELTA` = 0.05, `CONFIDENCE_LEVEL` = 0.95.

### Wilson CI

Success rate: Wilson interval at `CONFIDENCE_LEVEL` for proportions.

### Confidence label

| Label | Rule |
|-------|------|
| `high` | n ≥ 30 **and** CI width ≤ 0.20 |
| `medium` | n ≥ 10 **and** CI width ≤ 0.35 |
| `low` | otherwise |

### Directional trend label (`improving` / `declining` / `stable`)

Requires **all three** (AND):

1. `|delta| ≥ TREND_MIN_ABS_DELTA` (current vs prior window point estimate)  
2. Prior-window CI does **not** fully contain current point estimate  
3. `confidence_label ≠ low`

If any condition fails → `stable` (or omit directional claim per category).

### `evidence_scope`

`official_longitudinal` only when **all**: n ≥ `MIN_GOALS_LONGITUDINAL`, `report_completeness: complete`, `confidence_label` ≠ `low`, no tier mixing, official anchor present, no fingerprint mismatch in official rollup.

Otherwise → `exploratory_only`.

---

## Safety category (C6.3-02 scope)

| Field | C6.3-02 | Rule |
|-------|---------|------|
| `reproducibility_ok` | ✅ **Implement** | `false` if any official-cohort goal has environment `FINGERPRINT_MISMATCH` or metrics `env_hash` ≠ joined expected hash; disclose `env_hash_drift` |
| `tier_contamination` | ✅ **Implement** | Flag if non-`full` tier goals enter official rollup |
| `benchmark_regression` | ✅ **C6.3-03** | Computed via `evaluate_safety_gates()` — wiring owned by [`C6_phase3_reporting_contract.md`](C6_phase3_reporting_contract.md) RC v1.0 § Safety gate interpretation |
| `official_gates_green` | ✅ **C6.3-03** | Same — computed via `evaluate_safety_gates()` |

---

## Segment keys (minimum v0.2)

When parent segment join is `RESOLVED`:

| Key | Split |
|-----|-------|
| `plan_reused_true` / `plan_reused_false` | `plan_reused.value` |
| `strategy_injected_true` / `strategy_injected_false` | `strategy.strategy_injected` |
| `post_consolidation_true` / `post_consolidation_false` | `consolidation.post_consolidation` |

Per segment: `goal_count`, `success_rate`, `ci_low`, `ci_high`, `confidence_label`, `trend` (if n ≥ `MIN_GOALS_PER_COHORT`).

E3 SCR: omit when E3 JSONL absent — do not stub as zero.

---

## CLI

```bash
python3 scripts/analyze_cognitive_longitudinal.py
python3 scripts/analyze_cognitive_longitudinal.py --cohort official --window-days 28
python3 scripts/analyze_cognitive_longitudinal.py --dry-run --verbose
python3 scripts/analyze_cognitive_longitudinal.py --as-of-ms <epoch_ms>   # tests only
```

| Flag | Default |
|------|---------|
| Artifact paths | Production defaults (overridable) |
| `--window-days` | 28 |
| `--cohort` | `official` · `exploratory` |
| `--dry-run` | Stdout only; no file writes |
| `--as-of-ms` | Wall clock UTC ms (pin for deterministic tests) |

**Exit codes:** `0` analysis completed (even if incomplete) · `1` invalid arguments · `2` metrics file missing/unreadable

---

## C6.3-02 implementation steps

| Step | Deliverable | Blocked by |
|------|-------------|------------|
| **02-0** | AC v1.0 locked ✅ | — |
| **02-1** | Skeleton + `AnalysisGoalRecord` merger + CLI | 02-0 |
| **02-2a** | Anchor schema verification + fixture terminal rows | 02-1 |
| **02-2** | Cohort filter + window resolver | **02-2a ✅** |
| **02-3** | Performance/efficiency + trend/confidence tests | 02-2 |
| **02-4** | Memory + planning segments | 02-3 |
| **02-5** | `reproducibility_ok`, threats, `evidence_scope` | 02-4 |
| **02-6** | Full-payload JSONL + summary writers | 02-5 |
| **02-7** | `scripts/test_c6_longitudinal_analyzer.py` | 02-6 |

Pause between steps: tests green → confirm before next.

---

## Test plan (locked)

Fixture paths: `tests/fixtures/cognitive_longitudinal/` + new golden files as listed.

| ID | Name | Assert |
|----|------|--------|
| **A1** | Merger | Merged goal count matches join on fixtures |
| **A2** | Fingerprint exclusion | `FINGERPRINT_MISMATCH` goals excluded from official rollup |
| **A3** | Window bounds | Known anchor `ts` → correct `[start_ms, end_ms]` |
| **A3b** | Allowlist negative | `FOO_COMPLETE` not on allowlist → not anchor |
| **A4** | Wilson CI | Point + `ci_low`/`ci_high` match golden |
| **A4b** | Confidence high | n=30, narrow CI → `high` |
| **A4c** | Confidence low | n=5 → `low` |
| **A4d** | Trend improving | delta≥0.05, prior CI excludes current, confidence≠low → `improving` |
| **A4e** | Trend stable (delta) | delta<0.05 → `stable` |
| **A4f** | Trend stable (CI) | delta ok but prior CI contains current → `stable` |
| **A4g** | Trend stable (confidence) | delta ok, CI ok, confidence=low → `stable` |
| **A4h** | Trend declining | Symmetric negative delta case |
| **A5** | Exploratory scope | n < 30 → `exploratory_only` |
| **A6** | Incomplete report | Missing join artifact → `report_completeness: incomplete` |
| **A7** | Full JSONL history | Two runs: JSONL has two full payloads; summary matches latest only; row 1 unchanged after row 2 |
| **A8** | Reproducibility | `reproducibility_ok: false` on fingerprint fixture |
| **A9** | Idempotence | Same inputs + `--as-of-ms` → identical payload (except optional write timestamps) |

Golden files:

- `tests/fixtures/cognitive_longitudinal/analyzer_trend_golden.json` (A4–A4h)
- `tests/fixtures/cognitive_longitudinal/analyzer_golden_summary.json` (end-to-end A7)

No pytest; stdlib harness like `test_c6_longitudinal_join.py`.

---

## Step 02 success criteria

1. `python3 scripts/test_c6_longitudinal_analyzer.py` exits `0`  
2. `python3 scripts/analyze_cognitive_longitudinal.py` runs on production logs  
3. JSONL rows contain full payload; summary is latest snapshot only  
4. `reproducibility_ok` computed — not stubbed  
5. `python3 scripts/test_c6_longitudinal_join.py` still exits `0`  
6. C6.3-02 sealed in `completed_improvements_log.md`

---

## Explicitly out of scope (C6.3-02)

| Item | Checkpoint |
|------|------------|
| `plot_cognitive_longitudinal.py` | C6.3-03 |
| Markdown report | C6.3-03 |
| CTest / CMake | C6.3-03 |
| `benchmark_regression` wiring | C6.3-03 |
| Promotion gate in `improvements.md` | C6.3-04 ✅ (parent protocol § C6.3-04) |
| Ops / nightly docs | C6.3-05 |
| Full analyzer regression fixtures expansion | C6.3-06 ✅ → [`completed_improvements_log.md`](completed_improvements_log.md) |
| Runtime / C++ changes | Forbidden |
| Join semantic changes | JC v1.0 only via contract revision |

---

## Revision history

| Version | Date | Change |
|---------|------|--------|
| AC v1.0 | 2026-07-10 | C6.3-02 analyzer + regression implemented (Steps 02-1–02-7) |

---

**Contract lock:** AC v1.0 is locked with C6.3 v0.2.1. Amend only via explicit protocol + contract revision (AC v1.1+).
