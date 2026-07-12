# C6 Phase 3 — Longitudinal fixture catalog

**Checkpoints:** C6.3-06-1 (fixture catalog) · C6.3-06-2 (intentionally absent fixture) · C6.3-06-3 (official longitudinal corpus) · C6.3-06-4 (official golden + F1)  
**Location:** `tests/fixtures/cognitive_longitudinal/`

## Authority

This catalog documents **fixture intent only**. Behavioral semantics are defined exclusively by:

- **Analyzer Contract (AC):** `docs/C6_phase3_analyzer_contract.md`
- **Reporting Contract (RC):** `docs/C6_phase3_reporting_contract.md`
- **Join Contract (JC):** `docs/C6_phase3_join_contract.md`

In case of conflict, those contracts take precedence.

---

## 1. Overview

### Philosophy

- **Synthetic only** — never commit production logs from `agent_workspace/` or `logs/`.
- Anchored by `golden_join_results.json` (`fixture_philosophy: synthetic_only`).
- Default corpus exercises **exploratory** longitudinal evidence (`evidence_scope: exploratory_only`); official-scope companions are planned under C6.3-06-3+.

### Default vs companion fixtures

| Layer | Role |
|-------|------|
| **Default inputs** | `metrics.jsonl`, `app_log.jsonl`, `decision_trace.jsonl`, `benchmark_env.jsonl` — baseline join + analyzer + reporting |
| **Companion variants** | Named overrides (e.g. `benchmark_env_gates_green.jsonl`, `metrics_gates_green.jsonl`) — opt-in per test |
| **Goldens** | Committed expected outputs; diff-gated |

### Maintainer rules

1. Normal regression is **read-only** against committed fixtures and goldens.
2. Golden refresh is **opt-in** only (`--write-goldens` on reporting harness; explicit maintainer steps elsewhere).
3. Default exploratory regression outputs shall remain **identical after normalization** using existing golden normalization rules (pinned `generated_at_ms`, stripped `analyzer_commit_hash`, etc.). Differences beyond approved version fields constitute a build failure.
4. Companion fixtures **shall never replace** default exploratory fixtures.
5. Large repetitive synthetic datasets should be produced by a **deterministic generator** or **documented transformation process** rather than extensive manual editing. Generation scripts are maintainer tools only and **shall not** execute during normal regression.

### Fixture states

| Fixture state | Meaning |
|---------------|---------|
| **Present** | File exists; analyzer/join consumes it as a normal input artifact |
| **Empty** | File exists with zero valid JSONL records (`trace_exists=true` under JC; not a missing-artifact scenario) |
| **Intentionally absent** | Path referenced by tests but **must not exist on disk**; absence is the fixture |

### Out-of-directory note

The plot regression suite (`scripts/test_c6_longitudinal_plots.py`, P1–P8) reads **`analyzer_golden_summary.json`** from this directory but otherwise uses **inline synthetic payloads** for edge cases. Provenance and threat suites (`test_c6_longitudinal_provenance.py`, parts of `test_c6_longitudinal_threats.py`) use inline or partial fixture data — see per-file rows below.

---

## 2. Reserved namespaces

| Namespace | Reserved for |
|-----------|--------------|
| `plan-c6xx` | Default exploratory C6 corpus (current) |
| `plan-olxx` | Official longitudinal companion corpus (C6.3-06-3) |
| `plan-negxx` | Future negative fixtures (deferred) |

Reserved namespaces **shall not** be reused for unrelated scenarios. New fixture families must allocate a distinct namespace to avoid golden collisions and join ambiguity.

---

## 3. Master fixture table

Columns: **Fixture · Fixture state · Scenario · Purpose · Consumed by · Expected outcome · Golden · Maintainer**

### Committed fixtures (present)

| Fixture | Fixture state | Scenario | Purpose | Consumed by | Expected outcome | Golden | Maintainer |
|---------|---------------|----------|---------|-------------|------------------|--------|------------|
| `metrics.jsonl` | Present | Default exploratory corpus | Baseline goal metrics (`plan-c601`–`plan-c611`, 10 goals; `plan-c605` absent by design) | Join T1–T5; Analyzer merger, window, Wilson, trend, exploratory scope, I*, G1/G2/G5, O*, `golden_summary`; Reporting R1–R3, R9; Threats H2–H5, H8 (partial) | With full trace: `report_completeness: complete`, `evidence_scope: exploratory_only`, `confidence_label: low`, 8 eligible in official rollup | `analyzer_golden_summary.json`, `analyzer_trend_golden.json`, `golden_join_results.json` | Hand-authored C6.3-01; do not expand without catalog update |
| `app_log.jsonl` | Present | Strategy join evidence | App-log rows for plan/session strategy joins | Join T1–T5; default analyzer/reporting paths | Strategy joins resolve per join golden (`time_proximity` or `exact`) | `golden_join_results.json` | Paired with `metrics.jsonl` |
| `decision_trace.jsonl` | Present | Consolidation join evidence | Trace rows for consolidation joins | Join T1–T5; default analyzer/reporting paths | Consolidation joins resolve for plans with trace coverage | `golden_join_results.json` | Paired with `metrics.jsonl` |
| `benchmark_env.jsonl` | Present | Default environment anchor | Env hash, tier, fingerprint, gate terminals | Join T1–T5; default analyzer; gate case `default_missing_gates` | Default path: required gate harness evidence missing (`gate_evidence_missing:*`) | `gate_status_golden.json` (`default_missing_gates`) | Default env for exploratory corpus |
| `benchmark_env_gates_aborted.jsonl` | Present | Aborted benchmark gate | Benchmark regression terminal | Analyzer G2; Reporting R5; Threats H10; `gate_status_golden` `aborted_regression` | `categories.safety.benchmark_regression: true` | `gate_status_golden.json` | Companion override only |
| `benchmark_env_gates_green.jsonl` | Present | Green benchmark gates | Official gates green terminals | Analyzer G3; Reporting R6; `gate_status_golden` `gates_green` | With green metrics: `official_gates_green: true` | `gate_status_golden.json` | Pair with `metrics_gates_green.jsonl` |
| `metrics_gates_green.jsonl` | Present | Green-gate metrics companion | Metrics slice aligned with green env (7 goals) | Analyzer G3; Reporting R6; `gate_status_golden` `gates_green` | With green env: `reproducibility_ok: true` | `gate_status_golden.json` | Subset of default metrics; companion only |
| `prior_longitudinal.jsonl` | Present | Prior window payload | Frozen prior summary row for two-window analysis | Analyzer I3; Threats H6 | Prior window loadable; `protocol_revision_mid_window` threat path when combined with current run | (inline assertions; no dedicated golden file) | Single-row JSONL; hand-authored |
| `golden_join_results.json` | Present | Join public-output golden | JC regression anchor for public join shape | Join `golden_cases` (T1) | 10 `plan_id` records match committed public join dicts | Self (golden) | Refresh only via explicit join golden update |
| `analyzer_golden_summary.json` | Present | Analyzer summary golden | AC exploratory baseline payload | Analyzer `golden_summary` (H10); Plots P1–P8 (complete payload source) | Normalized full summary matches after pin/strip rules | Self (golden) | Diff-gated; `ANALYZER_VERSION` field may change on analyzer bump |
| `analyzer_trend_golden.json` | Present | Trend / Wilson / window anchor | Statistical sub-slices independent of full summary | Analyzer `window_bounds`, `wilson_ci`, `confidence_labels`, `trend_labels` | Wilson rates, confidence buckets, trend specs, window anchor match | Self (golden) | Contains synthetic high/low n slices not all from default metrics alone |
| `gate_status_golden.json` | Present | Gate scenario index | Maps env/metrics companions → expected safety flags | Reporting R5–R7; referenced by Analyzer G1–G3 | Per-case `safety` object and optional `flags_contains` | Self (golden) | Index only — not a runtime input |
| `report_header_golden.md` | Present | Complete report header slice | RC markdown header regression | Reporting R1, R1b (complete path) | Header markdown matches pinned complete payload | Self (golden) | Regenerate via `test_c6_longitudinal_reporting.py --write-goldens` |
| `report_header_incomplete_golden.md` | Present | Incomplete report header | RC incomplete banner regression | Reporting R2, R1b (incomplete path) | Incomplete banner when decision trace path is absent | Self (golden) | Regenerate via `--write-goldens` with missing trace path |

### Official longitudinal companion (C6.3-06-3 — present)

Consumed together via explicit companion paths. Regression: **F1** (`test_f1_official_golden_summary`).

| Fixture | Fixture state | Scenario | Purpose | Consumed by | Expected outcome | Golden | Maintainer |
|---------|---------------|----------|---------|-------------|------------------|--------|------------|
| `metrics_official_longitudinal.jsonl` | Present | Official longitudinal corpus | 32 eligible goals (`plan-ol01`–`plan-ol32`) in 28-day window | F1 (`f1_official_golden_summary`); maintainer `generate_c6_official_longitudinal_fixtures.py --validate` | `evidence_scope: official_longitudinal`, `report_completeness: complete` | `analyzer_golden_official_longitudinal.json` | Regenerate via generator; golden via `--write-official-golden` |
| `benchmark_env_official_longitudinal.jsonl` | Present | Official env + anchor + gates | Per-`run_id` `BENCHMARK_ENV`, `TEST_SUITE_COMPLETE` anchor, in-window gate terminals | F1 companion set | Safety gates green with metrics companion | (payload fields in official golden) | Paired with metrics companion |
| `app_log_official_longitudinal.jsonl` | Present | Sparse strategy evidence | Empty JSONL (valid present file) | F1 companion set | Join complete | — | Generator-written |
| `decision_trace_official_longitudinal.jsonl` | Present | Sparse trace evidence | Empty JSONL (valid present file) | F1 companion set | Join complete | — | Generator-written |

#### Provenance — `official_longitudinal_v1`

| Field | Value |
|-------|-------|
| Synthetic corpus name | `official_longitudinal_v1` |
| Generated from | `scripts/generate_c6_official_longitudinal_fixtures.py` |
| Generator version | `1.0.0` |
| Last regenerated | 2026-07-11 |
| Protocol version | `C6.3 v0.2.1` |
| Analyzer version | `0.2.0` |
| Metric schema version | `1.0` |
| Plan namespace | `plan-ol01`–`plan-ol32` (`run-olxx`, `session-olxx`) |

**Regeneration (maintainer only — not part of CTest):**

```bash
python3 scripts/generate_c6_official_longitudinal_fixtures.py --validate
python3 scripts/test_c6_longitudinal_analyzer.py --write-official-golden
```

Use `--write-official-golden` **only** after identifying why F1 differs from the committed golden (fixture change, analyzer change, or regression).

### Intentionally absent fixture

| Fixture | Fixture state | Scenario | Purpose | Consumed by | Expected outcome | Golden | Maintainer |
|---------|---------------|----------|---------|-------------|------------------|--------|------------|
| `missing_decision_trace.jsonl` | **Intentionally absent** | Missing decision trace | Incomplete-report regression: `decision_trace` artifact missing | Analyzer A6 (`incomplete_report`), O6 (`o6_incomplete_jsonl_immutable`); Reporting R1b, R2, R8 | `report_completeness: incomplete`; `missing_artifact:decision_trace` in flags; `load_jsonl` returns `file_exists=false` | `report_header_incomplete_golden.md` | **Do not create this file** unless absent-vs-empty trace semantics are formally revised in AC/JC |

> This fixture is intentionally represented by the **absence** of the file. Creating the file (even as zero bytes) changes the analyzer input semantics under current JC rules: an existing empty file sets `trace_exists=true`, which **does not** produce `missing_artifact:decision_trace` and would invalidate this scenario.

---

## 4. Join case map

Source: `golden_join_results.json` → `case_map` and `notes`.

| Plan ID | Case ID | Join golden | Notes |
|---------|---------|-------------|-------|
| `plan-c601` | C6-01 | Yes | Baseline resolved joins; strategy `time_proximity` |
| `plan-c602` | C6-02 | Yes | Strategy not injected |
| `plan-c603` | C6-03 | Yes | Used in T6 missing-trace scenario |
| `plan-c604` | C6-04 | Yes | `plan_reused: true` |
| *(none)* | **C6-05** | **No** | **MISSING_ARTIFACT** for consolidation — exercised in join T6 (`missing_artifact_case`) with in-memory `trace_rows=[]`, `trace_exists=False`; not a golden row. **Distinct** from the intentionally absent `missing_decision_trace.jsonl` analyzer/reporting path (§3). |
| `plan-c606` | C6-06 | Yes | |
| `plan-c607` | C6-07 | Yes | `run_id: run-missing` — env join edge |
| `plan-c608` | C6-08 | Yes | `outcome: failed` |
| `plan-c609` | C6-09 | Yes | Environment `FINGERPRINT_MISMATCH` (`hash-metrics-mismatch`) |
| `plan-c610` | C6-10 | Yes | |
| `plan-c611` | C6-11 | Yes | Empty `session_id` — join still resolves |

Join regression tests (`scripts/test_c6_longitudinal_join.py`):

| Test name | JC ID | Description |
|-----------|-------|-------------|
| `golden_cases` | T1 | Deep-equals `golden.records[plan_id]` |
| `deterministic_output` | T2 | Stable JSON serialization |
| `idempotence` | T3 | Repeated join identical |
| `input_immutability` | T4 | Inputs unchanged after join |
| `validation_smoke` | T5 | No invalid rows; no missing artifacts on default corpus |
| `missing_artifact_case` | T6 | C6-05 consolidation `MISSING_ARTIFACT` on `plan-c603` |

---

## 5. Companion / variant matrix

Source: `gate_status_golden.json` → `cases`.

| Case key | `benchmark_env` | `metrics` | Expected safety | Extra flags |
|----------|-----------------|-----------|-----------------|-------------|
| `default_missing_gates` | `benchmark_env.jsonl` | *(default `metrics.jsonl`)* | `benchmark_regression: false`, `official_gates_green: false` | `gate_evidence_missing:reflection_ab`, `robustness`, `episodic_learning` |
| `aborted_regression` | `benchmark_env_gates_aborted.jsonl` | *(default)* | `benchmark_regression: true`, `official_gates_green: false` | — |
| `gates_green` | `benchmark_env_gates_green.jsonl` | `metrics_gates_green.jsonl` | `benchmark_regression: false`, `official_gates_green: true`, `reproducibility_ok: true` | — |

**Analyzer gate tests** swap env/metrics explicitly:

- **G1** — default env → missing gate evidence flags  
- **G2** — `benchmark_env_gates_aborted.jsonl`  
- **G3** — green env + `metrics_gates_green.jsonl`  
- **G4** — inline non-FULL tier row (no fixture file)  
- **G5** — default env + unknown event in metrics context  

**Reporting gate tests:** R5 (`aborted_regression`), R6 (`gates_green`), R7 (`default_missing_gates` flags).

---

## 6. Planned fixtures (forward pointers)

| Planned fixture | Sub-checkpoint | Status |
|-----------------|----------------|--------|
| Negative slices (`plan-negxx`) | Deferred | Out of scope for C6.3-06 |

Official companion JSONL corpus — **C6.3-06-3** ✅ · Official analyzer golden + F1 — **C6.3-06-4** ✅.

---

## 7. Golden normalization

Regression harnesses normalize analyzer payloads before comparison:

| Field | Treatment |
|-------|-----------|
| `generated_at_ms` | Pinned to `AS_OF_MS` (`1_800_002_000_000`) in analyzer/reporting tests |
| `analyzer_commit_hash` | Stripped |
| `summary_written_at_ms` | Stripped where applicable |

**Rules (frozen under C6.3-06 lock):**

- Default exploratory regression outputs shall remain identical after normalization using these rules.
- Official companion goldens (future) shall be maintained **independently** of `analyzer_golden_summary.json`.
- Any differences beyond approved version fields (`analyzer_version`, `protocol_version`, `metric_schema_version` when intentionally bumped) constitute a build failure.

Plot tests (P1–P8) load `analyzer_golden_summary.json` directly; incomplete/infrastructure cases mutate copies in memory.

---

## Regression gate

```bash
python3 scripts/test_c6_longitudinal_reporting.py
python3 scripts/test_c6_longitudinal_analyzer.py
python3 scripts/test_c6_longitudinal_plots.py
python3 scripts/test_c6_longitudinal_threats.py
python3 scripts/test_c6_longitudinal_provenance.py
python3 scripts/test_c6_longitudinal_join.py
ctest -R c6-longitudinal --test-dir build/debug --output-on-failure
```

---

## Related documentation

| Need | Document |
|------|----------|
| Join algorithms + golden regression | `docs/C6_phase3_join_contract.md` |
| Analyzer semantics + thresholds | `docs/C6_phase3_analyzer_contract.md` |
| Reporting + plots + gates | `docs/C6_phase3_reporting_contract.md` |
| Operator invocation | `docs/cognitive_longitudinal_ops.md` |
| C6.3-06 lock + sub-checkpoints | `docs/C6_phase3_protocol.md` § C6.3-06 |
