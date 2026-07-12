# C6 Phase 3 — Reporting Contract (Normative)

**Contract version:** RC v1.0  
**Status:** 🔒 **Locked** — C6.3-03 Step 03-0 (2026-07-10)  
**Parent protocol:** [`C6_phase3_protocol.md`](C6_phase3_protocol.md) **C6.3 v0.2.1**  
**Data dependency:** [`C6_phase3_analyzer_contract.md`](C6_phase3_analyzer_contract.md) **AC v1.0** 🔒  
**Join dependency:** [`C6_phase3_join_contract.md`](C6_phase3_join_contract.md) **JC v1.0** 🔒  
**Checkpoint:** C6.3-03 (Step 03-0 ✅ · Step 03-1 ✅ · Step 03-2 ✅ · Step 03-3 ✅ · Step 03-4 ✅ · Step 03-5 ✅ · Step 03-6 ✅ · Step 03-7 ✅ · **sealed**)

> **Governance:** If this contract and the parent protocol disagree, **the protocol wins**. If reporting code and this contract disagree, **this contract wins** — update the implementation, never the reverse. Analyzer data-production semantics (windows, Wilson CI, trend labels, JSONL full payload) are owned by **AC v1.0** and **must not** be redefined here. Join semantics are owned by **JC v1.0**.

---

## Purpose

This document specifies **how** C6.3-03 turns analyzer payloads into human-readable reports, plots, expanded threat disclosure, and wired safety gates. The parent protocol defines *what* constitutes valid longitudinal evidence; AC v1.0 defines deterministic analytics; **this contract defines interpretation, presentation, and gate semantics**.

C6.3-03 answers:

> *Given a reproducible analyzer payload, can operators and CI consume longitudinal evidence without misreading it?*

**Boundary:** Reporting **consumes** the analyzer payload (or `cognitive_longitudinal_summary.json`). It **SHALL NOT** re-run join logic, recompute windows, or alter category statistics inline.

---

## Layer stack

```
C6_phase3_protocol.md (v0.2.1)              →  invariants, four artifacts, threats table
C6_phase3_join_contract.md (JC v1.0)        →  join only
C6_phase3_analyzer_contract.md (AC v1.0)    →  data production (frozen)
C6_phase3_reporting_contract.md (RC v1.0)   →  interpretation & reporting
scripts/analyze_cognitive_longitudinal.py   →  orchestrator (C6.3-02 + C6.3-03 extensions)
scripts/plot_cognitive_longitudinal.py      →  plots (C6.3-03)
scripts/test_c6_longitudinal_reporting.py   →  reporting regression (C6.3-03)
```

---

## Primary rules

| Rule | Requirement |
|------|-------------|
| Read-only | Reporting SHALL NOT influence runtime behavior |
| Payload-only input | Markdown and plots read analyzer output; no inline re-analysis |
| No causal claims | Prose MUST be observational; see § Prose discipline |
| Threat parity | Markdown threats section MUST match JSON `threats_disclosed` (same IDs, same order) |
| Incomplete discipline | Incomplete reports MUST carry banner; plots skipped per § Plot skip rules |
| Conservative gates | Missing gate evidence ≠ green; see § Safety gate interpretation |
| Frozen outputs | New markdown sections or plot files require **RC v1.1+** revision |

---

## Version bump (C6.3-03)

When reporting and safety gates ship:

| Field | C6.3-02 value | C6.3-03 value |
|-------|---------------|---------------|
| `analyzer_version` | `0.1.0` | **`0.2.0`** |
| `metric_schema_version` | `1.0` | **`1.0`** (unchanged — safety *interpretation* only) |
| Stub flags removed | `benchmark_regression_not_wired`, `official_gates_not_wired` | **Removed** when gates wire |

---

## Output artifacts

### 1. `logs/cognitive_longitudinal_report.md` (human report)

Overwrite each run (latest snapshot only — same discipline as summary JSON).

**Generation:** ALWAYS attempted when analyzer writes outputs, unless `--no-report`.

### 2. `logs/plots/longitudinal/*.png` (visual summary)

Optional charts; see § Plot schema. Skipped when § Plot skip rules apply.

### 3–4. JSONL + summary JSON

Owned by AC v1.0. C6.3-03 **extends** their `categories.safety`, `threats_disclosed`, and `flags` per this contract. Write order unchanged (see § Step 03-5 — JSONL immutability and two-phase summary):

1. Compute full payload (analyzer + reporting extensions)  
2. Append JSONL row (**analytical payload only** — no `plots_skipped:*`)  
3. Overwrite summary JSON (phase 1 — analytical payload)  
4. Write markdown report (unless `--no-report`; pre-plot payload — see § Step 03-5 markdown timing)  
5. Write plots (unless `--no-plots`; plot skip rules apply)  
6. If step 5 returns `PlotResult.flags`: merge into summary `flags`, sort/dedupe, **rewrite summary JSON only** (phase 2)

---

## Orchestration

Default: **single command** produces all four protocol artifacts.

```bash
python3 scripts/analyze_cognitive_longitudinal.py
python3 scripts/analyze_cognitive_longitudinal.py --no-report    # escape hatch
python3 scripts/analyze_cognitive_longitudinal.py --no-plots     # escape hatch
```

| Flag | Default | Rule |
|------|---------|------|
| `--no-report` | off | Skip markdown; JSON artifacts still written |
| `--no-plots` | off | Skip PNG generation |
| `--dry-run` | off | Stdout JSON only; no file writes (AC v1.0) |

Two-step manual invocation (analyze then plot-only) is permitted for debugging; see [`cognitive_longitudinal_ops.md`](cognitive_longitudinal_ops.md) (maintainer/debug — not the default path).

---

## Markdown schema (frozen)

### Section order (normative)

1. **Title** — `# Cognitive Longitudinal Report`  
2. **Incomplete banner** — only when `report_completeness: incomplete` (see below)  
3. **Reproducibility header** — key-value block  
4. **Disclaimer** — fixed text (§ Disclaimer)  
5. **Evidence summary** — `evidence_scope`, `confidence_label`, window bounds  
6. **Performance** — success rate, CI, trend, eligible count  
7. **Categories** — efficiency, memory, planning (when non-empty)  
8. **Safety** — reproducibility, tier contamination, gate status  
9. **Segments** — markdown table  
10. **Threats to validity** — bulleted `threats_disclosed` IDs with human labels  
11. **Flags and warnings** — sorted lists  

Adding, removing, or reordering sections requires **RC v1.1+**.

### Frozen section registry (implementation)

The renderer **SHALL** expose a module-level constant in `scripts/c6_longitudinal_report.py`:

```python
REPORT_SECTIONS = [
    "title",
    "incomplete_banner",      # omitted from body when not incomplete; still a named section slot
    "reproducibility",
    "disclaimer",
    "evidence_summary",
    "performance",
    "categories",
    "safety",
    "segments",
    "threats",
    "flags",
]
```

| Rule | Requirement |
|------|-------------|
| Order | `REPORT_SECTIONS` order MUST match the normative list above |
| Coverage | `render_markdown_report()` MUST produce output for every section that applies (banner omitted when complete) |
| Testability | Tests MAY assert `render_report_sections(payload)` returns keys == expected subset of `REPORT_SECTIONS` without parsing markdown heuristically |
| Revision | Changing `REPORT_SECTIONS` requires **RC v1.1+** |

**Recommended API:** `render_markdown_report(payload) -> str` for file output; optional `render_report_sections(payload) -> dict[str, str]` mapping section id → markdown fragment for tests.

### Incomplete banner (frozen text)

When `report_completeness == "incomplete"`, insert immediately after the title:

```markdown
> **INCOMPLETE — exploratory use only.** Required authoritative artifacts or validation rows are missing. Do not use for official longitudinal claims or F-series promotion.
```

### Reproducibility header (required fields)

Every markdown report SHALL include these fields in a fenced or bullet header block:

| Field | Source |
|-------|--------|
| `protocol_version` | payload |
| `metric_schema_version` | payload |
| `analyzer_version` | payload |
| `analyzer_commit_hash` | payload |
| `generated_at_ms` | payload (ISO-8601 UTC human-readable permitted) |
| `report_completeness` | payload |
| `evidence_scope` | payload |
| `confidence_label` | payload |
| `window.start_ms`, `window.end_ms`, `window.days`, `window.anchor` | payload |
| `reproducibility.input_log_path` | payload |
| `reproducibility.benchmark_run_ids_consumed` | payload (count + optional truncated list) |
| `threat_schema_version` | renderer constant `THREAT_SCHEMA_VERSION` (see § Threat detection) |

### Disclaimer (frozen text)

Insert verbatim after the reproducibility header:

```markdown
**Observational evidence only.** This report describes statistical trends in immutable benchmark artifacts over a preregistered window. It does **not** establish causal relationships between code changes, configuration changes, and observed performance. Correlation in a rolling window is not proof of causation. Official learning claims require controlled benchmarks (E2, E3, B1, C3/C5) in addition to longitudinal evidence.
```

### Prose discipline (forbidden)

Markdown body text MUST NOT:

| Forbidden | Example |
|-----------|---------|
| Causal verbs | "because", "caused by", "proves that", "due to the refactor" |
| Official claims when `evidence_scope: exploratory_only` | "the system is improving" |
| Omitting threats | Non-empty `threats_disclosed` MUST appear in § Threats |
| Hiding incompleteness | `report_completeness: incomplete` without banner |
| Directional hype | "significant improvement" without citing CI and `confidence_label` |

Permitted: "success rate **increased** from X to Y (95% CI …)" when `trend` is `improving` and `evidence_scope` is not blocked by incompleteness alone.

---

## Plot schema (frozen v1.0)

**Input:** `logs/cognitive_longitudinal_summary.json` **only** — no raw `cognitive_metrics.jsonl` reads in v1.0.

**Output directory:** `logs/plots/longitudinal/` (repo-root-relative; overridable via `--plot-dir`).

| Filename | Content |
|----------|---------|
| `success_rate_trend.png` | Current-window `categories.performance.success_rate` with Wilson CI error bars (`ci_low` / `ci_high`). **Current window only** — no JSONL reads; prior marker only if an explicit future summary field exists (none in RC v1.0) |
| `segment_success.png` | Horizontal bar chart of segment `success_rate` for keys present in `segments` |
| `efficiency_p50.png` | Bar chart of `categories.efficiency.total_wall_clock_ms_p50` and `total_tokens_p50` |

**Library:** `matplotlib` with `Agg` backend (same pattern as `plot_cognitive_metrics.py`).

**Style:** Default matplotlib styling; no custom branding required in v1.0.

Adding plot files requires **RC v1.1+**.

### Plot skip rules

| Condition | Behavior |
|-----------|----------|
| `report_completeness == "incomplete"` | **Skip all plots**; append flag `plots_skipped:incomplete_report` |
| `--no-plots` | Skip; no flag |
| `matplotlib` import failure | Skip; append flag `plots_skipped:matplotlib_unavailable`; exit code **0** (analysis succeeded) |
| Empty `segments` for segment chart | Emit `segment_success.png` with **"no segment data"** annotation; **do not** emit `plots_skipped:empty_segments` (frozen 03-4) |

**Forbidden in v1.0:** Watermarked or partial plots on incomplete reports.

---

## Threat detection (complete protocol table)

### Threat label schema (versioned)

Threat human labels are owned by the reporting layer. The renderer **SHALL** define:

```python
THREAT_SCHEMA_VERSION = "1.0"
THREAT_LABELS = {
    "model_hash_drift": "Changing models",
    # ... all canonical IDs below
}
```

| Rule | Requirement |
|------|-------------|
| Version in report | Reproducibility header MUST include `threat_schema_version: 1.0` |
| Label lookup | Markdown § Threats uses `THREAT_LABELS[threat_id]`; unknown IDs render as `(unlabeled: <id>)` with flag `unlabeled_threat:<id>` in tests only |
| Taxonomy change | Adding/removing/renaming threat IDs or changing labels requires **`THREAT_SCHEMA_VERSION` bump** and **RC v1.1+** |
| **03-2 lock** | At Step 03-2 umbrella lock, `THREAT_SCHEMA_VERSION = "1.0"` becomes **normative for detection + reporting** — taxonomy is part of research claims |

This is separate from `metric_schema_version` (analyzer computations) and `protocol_version` (longitudinal protocol).

Threat IDs in JSON `threats_disclosed` and markdown MUST use these canonical IDs. Detection runs during analyzer/reporting pass; results are merged into payload before write.

| Threat ID | Human label (markdown) | Detection rule |
|-----------|------------------------|----------------|
| `model_hash_drift` | Changing models | >1 distinct normalized `model_hash` among `BENCHMARK_ENV` rows whose `run_id` ∈ `benchmark_run_ids_consumed` |
| `corpus_fingerprint_drift` | Corpus growth | >1 distinct `corpus_fingerprint` among same env rows |
| `runtime_environment_drift` | Runtime environment drift | >1 distinct `environment_hash` (`env_hash` on row) while both `model_hash` and `corpus_fingerprint` sets have cardinality 1 among same env rows — see § `runtime_environment_drift` intent |
| `prompt_evolution` | Prompt / config evolution | >1 distinct `provenance_pin` tuple among consumed `BENCHMARK_ENV` rows — see § Provenance fields (locked) |
| `protocol_revision_mid_window` | Protocol revision mid-window | >1 distinct `metric_schema_version` among `COGNITIVE_LONGITUDINAL_SUMMARY` JSONL rows whose `window.end_ms` falls within `[current.start_ms, current.end_ms]`; if no prior JSONL rows, omit |
| `small_sample` | Small sample | `categories.performance.eligible_count` < `MIN_GOALS_LONGITUDINAL` (30) |
| `tier_mixing` | Tier mixing | `categories.safety.tier_contamination == true` |
| `incomplete_data` | Incomplete data | `report_completeness == "incomplete"` OR any `flags` entry `missing_artifact:*` |
| `selection_bias` | Selection bias risk | `validation_rows_dropped` ∈ `flags` OR join smoke reported `invalid_rows_skipped` > 0 for consumed inputs |
| `env_hash_drift` | Environment hash drift | `categories.safety.reproducibility_ok == false` |
| `fingerprint_mismatch` | Fingerprint mismatch on goals | `fingerprint_mismatch` ∈ `flags` |
| `no_official_anchor` | No official window anchor | `window.anchor != "official_benchmark_execution"` OR `no_official_anchor` ∈ `flags` |
| `benchmark_gate_failure` | Official benchmark gate failure | `categories.safety.benchmark_regression == true` |
| `gate_evidence_missing` | Missing gate terminal evidence | any `gate_evidence_missing:*` ∈ `flags` |

**Order:** `threats_disclosed` SHALL be sorted lexicographically by threat ID.

**AC v1.0 carryover:** Existing `build_threats()` IDs remain; C6.3-03 **extends** with rows above not already covered.

### `runtime_environment_drift` intent (normative)

E1 `environment_hash` / JSONL `env_hash` is **not hardware-only**. It captures the pinned benchmark environment snapshot at `BenchmarkContext::create()` (model, corpus, runtime tier, provenance, etc. — see [`benchmark_environment.md`](benchmark_environment.md)).

This threat fires when **model and corpus fingerprints are stable** across consumed runs but **environment hashes differ**. That pattern indicates runtime or deployment context changed — e.g. different machine, container image, OS package state, or other non-model/non-corpus environment delta — **without** implying a specific hardware-only root cause.

| Distinct from | Rule |
|---------------|------|
| `env_hash_drift` | Goal-level `reproducibility_ok == false` (metrics `env_hash` ≠ sidecar on joined goals) |
| `runtime_environment_drift` | Window-level: multiple sidecar `env_hash` values among consumed `BENCHMARK_ENV` rows while `model_hash` and `corpus_fingerprint` each have cardinality 1 |

Both MAY appear in the same report.

**Renamed from draft `hardware_env_drift`** before implementation (03-2 umbrella lock) — ID never shipped in production payloads.

---

## Provenance fields (locked — Step 03-2a)

**Status:** 🔒 Locked · **implemented** 2026-07-10 (`c6_longitudinal_provenance.py` + P1–P7)
**Purpose:** Authoritative JSON paths and composite-pin semantics for `prompt_evolution` detection (03-2b).

### Authoritative source

Threat detection reads **`BENCHMARK_ENV` rows from `benchmark_env.jsonl`** (same artifact as analyzer). Sidecar `logs/benchmark_env.latest.json` uses wrapper key `environment` — **not** used directly for longitudinal threat detection.

### JSON paths (frozen)

| Component | Path on JSONL row | Role |
|-----------|-------------------|------|
| Thoth repo pin | `env.prov.thoth_git_sha` | Primary pin component |
| Agent library pin | `env.prov.basic_agent_git_sha` | Primary pin component |
| Capture time | `env.prov.captured_at_ms` | **Informative only** — MUST NOT affect `prompt_evolution` |

**Forbidden for detection:** bare `env.git`, top-level `git_sha`, sidecar-only `environment.prov` without reading JSONL `env.prov`.

Verified production shape (2026-07-10): `env` keys include `corpus`, `model`, `ollama`, `prov`, `runtime`; `prov` carries `thoth_git_sha`, `basic_agent_git_sha`, `captured_at_ms`.

### Normalization

| Input | Normalized value |
|-------|------------------|
| Non-empty string | Strip leading/trailing whitespace; use as-is (short SHA permitted) |
| Missing key, `null`, `""`, `"unknown"` | `None` |
| Non-string type | `None` |

### Composite `provenance_pin` (frozen)

For each consumed `BENCHMARK_ENV` row (`run_id` ∈ `benchmark_run_ids_consumed`):

```text
provenance_pin = (thoth_git_sha_norm, basic_agent_git_sha_norm)
```

| Rule | Requirement |
|------|-------------|
| Row exclusion | If **both** components are `None`, row is **excluded** from pin cardinality |
| Partial pins | `(sha, None)` and `(None, sha)` are valid distinct tuples |
| `prompt_evolution` | Fire when the set of non-excluded `provenance_pin` values has **cardinality > 1** |
| Single pin | Cardinality 1 or 0 → **no** `prompt_evolution` |

**Rejected alternative:** per-field OR (`thoth` drift OR `basic_agent` drift alone) — composite tuple is normative.

### H5 fixture spec (frozen — physical edits in 03-2b)

Consumed `run_id`s on default fixture (per `analyzer_golden_summary.json`):  
`run-c601`, `run-c602`, `run-c603`, `run-c604`, `run-c606`, `run-c608`, `run-c610`, `run-c611`.

| `run_id` | `env.prov` (minimum) |
|----------|----------------------|
| `run-c601` | `{"thoth_git_sha": "aaa1111", "basic_agent_git_sha": "bbb1111"}` |
| `run-c602` | `{"thoth_git_sha": "ccc2222", "basic_agent_git_sha": "bbb1111"}` |

→ tuples `("aaa1111", "bbb1111")` vs `("ccc2222", "bbb1111")` → H5 expects `prompt_evolution`.

Other consumed rows: omit `prov` or use an identical tuple — MUST NOT alone trigger H5.

### 03-2a close-out

Production survey recorded in **`completed_improvements_log.md`** only (no separate survey script or notes file).

---

## Safety gate interpretation

### Terminal allowlist (frozen — same as AC v1.0)

Reporting gate checks use the **same 12-event allowlist** as window anchoring. **Suffix inference is forbidden.**

```
TEST_SUITE_COMPLETE / TEST_SUITE_ABORTED
REFLECTION_AB_COMPLETE / REFLECTION_AB_ABORTED
ROBUSTNESS_COMPLETE / ROBUSTNESS_ABORTED
CHAT_RAG_BENCHMARK_COMPLETE / CHAT_RAG_BENCHMARK_ABORTED
GRAG_BENCHMARK_COMPLETE / GRAG_BENCHMARK_ABORTED
EPISODIC_LEARNING_COMPLETE / EPISODIC_LEARNING_ABORTED
```

### Harness registry

| `harness_key` | Complete event | Aborted event | Required for `official_gates_green` |
|---------------|----------------|---------------|-------------------------------------|
| `reflection_ab` | `REFLECTION_AB_COMPLETE` | `REFLECTION_AB_ABORTED` | **Yes** |
| `robustness` | `ROBUSTNESS_COMPLETE` | `ROBUSTNESS_ABORTED` | **Yes** |
| `episodic_learning` | `EPISODIC_LEARNING_COMPLETE` | `EPISODIC_LEARNING_ABORTED` | **Yes** |
| `test_suite` | `TEST_SUITE_COMPLETE` | `TEST_SUITE_ABORTED` | No (observational) |
| `chat_rag` | `CHAT_RAG_BENCHMARK_COMPLETE` | `CHAT_RAG_BENCHMARK_ABORTED` | No (observational) |
| `grag` | `GRAG_BENCHMARK_COMPLETE` | `GRAG_BENCHMARK_ABORTED` | No (observational) |

### Official execution filter

A terminal row counts for gate evaluation only when:

1. `event` ∈ frozen allowlist, AND  
2. Row joins to `BENCHMARK_ENV` with same `run_id`, AND  
3. `env.runtime.tier` normalizes to `full` (`OFFICIAL_TIER`), AND  
4. `ts` ∈ `[window.start_ms, window.end_ms]` (inclusive)

Same tier normalization as AC v1.0 / JC v1.0.

### `benchmark_regression`

| Value | Rule |
|-------|------|
| `true` | ≥1 official execution terminal in window is an `*_ABORTED` event (any harness in registry) |
| `false` | No official `*_ABORTED` in window |

**Not inferred from missing evidence.** Absence of terminals does not set `benchmark_regression: true`.

When `true`, append threat `benchmark_gate_failure`.

### `official_gates_green`

| Value | Rule |
|-------|------|
| `true` | **All** of: (1) `benchmark_regression == false`; (2) `reproducibility_ok == true`; (3) each **required** harness has ≥1 official `*_COMPLETE` in window; (4) no required harness has official `*_ABORTED` in window; (5) no `gate_evidence_missing:*` flags |
| `false` | Any condition above fails |

### Missing evidence (conservative)

For each **required** harness: if no official terminal row (`COMPLETE` or `ABORTED`) exists in window:

- Append flag `gate_evidence_missing:<harness_key>`  
- Append threat `gate_evidence_missing` (once, if any missing)  
- `official_gates_green` **cannot** be `true`

**Forbidden:** Treating missing terminal as green or as regression.

### Stub removal

C6.3-03 implementation **SHALL** remove:

- `benchmark_regression_not_wired` flag  
- `official_gates_not_wired` flag (if present)  
- Hard-coded `False` stubs in `compute_safety()` for wired fields  

Replace with algorithms above.

### Optional harness observational block

Markdown **Safety** section SHOULD list observational harnesses (`test_suite`, `chat_rag`, `grag`) with last terminal status in window (`COMPLETE`, `ABORTED`, or `no_evidence`). These do not block `official_gates_green`.

---

## CI / CTest registration

Engineering validation used to continuously enforce the reporting contract in local development and CI. **The reporting contract is independent of any particular CI system** — CTest is repository infrastructure, not part of the scientific protocol.

Add to `tests/CMakeLists.txt`:

| CTest name | Command | Labels |
|------------|---------|--------|
| `c6-longitudinal-join` | `python3 scripts/test_c6_longitudinal_join.py` | `pr;fast` |
| `c6-longitudinal-analyzer` | `python3 scripts/test_c6_longitudinal_analyzer.py` | `pr;fast` |
| `c6-longitudinal-reporting` | `python3 scripts/test_c6_longitudinal_reporting.py` | `pr;fast` |

| Rule | Requirement |
|------|-------------|
| Working directory | `${CMAKE_SOURCE_DIR}` |
| Timeout | 120s each |
| Matplotlib plot smoke | **Not** in CTest `fast` — covered only if reporting tests mock/stub plot calls |

---

## C6.3-03 implementation steps

| Step | Deliverable | Blocked by |
|------|-------------|------------|
| **03-0** | RC v1.0 locked ✅ | — |
| **03-1** | `c6_longitudinal_report.py`, goldens, R1/R1b/R2 ✅ | 03-0 |
| **03-2** | Threat detection umbrella 🔒 — substeps 03-2a–03-2d | 03-1 |
| **03-2a** | `c6_longitudinal_provenance.py` + P1–P7 ✅ | 03-2 umbrella |
| **03-2b** | `c6_longitudinal_threats.py` + H1–H9 ✅ | 03-2a |
| **03-2c** | Analyzer integration (`_build_threat_inputs` + I1–I5) ✅ | 03-2b |
| **03-2d** | Golden seal + H10; 03-2 umbrella ✅ | 03-2c |
| **03-3** | Safety gate wiring (`evaluate_safety_gates` + gate threats + golden) ✅ | 03-2 |
| **03-4** | `plot_cognitive_longitudinal.py` per § Plot schema ✅ | 03-0 |
| **03-5** | Orchestration in analyzer CLI (`--no-report`, `--no-plots`); write order ✅ | 03-1–03-4 |
| **03-6** | CTest + expand `test_c6_longitudinal_reporting.py` (R3–R9) | ✅ 2026-07-11 |
| **03-7** | Golden updates, `analyzer_version` 0.2.0, `completed_improvements_log.md` seal | ✅ 2026-07-11 |

Pause between steps: regressions green → confirm before next.

---

## Step 03-1 implementation lock 🔒 (2026-07-10)

**Status:** Plan locked · **implemented** 2026-07-10 (R1/R1b/R2 green)

Presentation-layer checkpoint only. No gates, plots, CLI wiring, CTest, or analyzer changes.

### Files (03-1)

| File | Action |
|------|--------|
| `scripts/c6_longitudinal_report.py` | **New** — renderer module |
| `tests/fixtures/cognitive_longitudinal/report_header_golden.md` | **New** — complete-case header slice |
| `tests/fixtures/cognitive_longitudinal/report_header_incomplete_golden.md` | **New** — incomplete banner slice |
| `scripts/test_c6_longitudinal_reporting.py` | **New (minimal)** — R1, R1b, R2 only |

`scripts/analyze_cognitive_longitudinal.py` — **no changes** until Step 03-5.

### Module API (frozen for 03-1)

```python
REPORT_SECTIONS = [
    "title",
    "incomplete_banner",
    "reproducibility",
    "disclaimer",
    "evidence_summary",
    "performance",
    "categories",
    "safety",
    "segments",
    "threats",
    "flags",
]

THREAT_SCHEMA_VERSION = "1.0"
THREAT_LABELS = { ... }  # 14 canonical IDs — see § Threat detection

def render_report_sections(payload: dict) -> dict[str, str]: ...
def render_markdown_report(payload: dict) -> str: ...
```

| Rule | Requirement |
|------|-------------|
| Input | Analyzer payload only — no join re-run, no `benchmark_env.jsonl` reads |
| `incomplete_banner` | Omitted from `render_report_sections()` keys when `report_completeness == "complete"` |
| `threat_schema_version` | Emitted in reproducibility header as `1.0` |
| `analyzer_version` | Read from payload as-is — **no bump until 03-7** |

### Tests shipped in 03-1

| ID | Assert |
|----|--------|
| **R1** | Normalized whitespace; exact required header field values; disclaimer verbatim — **not** full-file raw compare |
| **R1b** | `render_report_sections()` keys == expected `REPORT_SECTIONS` subset |
| **R2** | Incomplete → banner present; complete → banner **absent** |

**Determinism:** Tests pin `analyzer_commit_hash = "golden"` and fixed `generated_at_ms` before render.

**Golden scope:** Header slice only (sections `title` through `disclaimer`) — not full report body.

### 03-1 stop criteria

1. `REPORT_SECTIONS`, `THREAT_SCHEMA_VERSION`, `THREAT_LABELS` in module  
2. R1, R1b, R2 green in `test_c6_longitudinal_reporting.py`  
3. `test_c6_longitudinal_analyzer.py` + `test_c6_longitudinal_join.py` unchanged and green  
4. No production write to `logs/cognitive_longitudinal_report.md`  

### Explicitly out of scope (03-1)

Gate wiring (03-3) · expanded threat detection (03-2) · plots (03-4) · CLI/`--no-report` (03-5) · CTest (03-6) · R3–R9 · `analyzer_version` 0.2.0 (03-7)

**Next review point after 03-1 lands:** Step **03-2** (reporting consumes richer analytical interpretation).

---

## Step 03-2 umbrella lock 🔒 (2026-07-10)

**Status:** Umbrella locked · substeps execute individually · **no implementation until each substep is locked**

First analyzer-payload enrichment step. Threat taxonomy (`THREAT_SCHEMA_VERSION` **1.0**) becomes normative for detection **and** reporting — part of future research claims.

### Decomposition (execute in order)

```
03-2a  Provenance contract discovery     → lock ✅ · survey in completed log
03-2b  Threat engine + H1–H9 tests       → lock ✅ · implement ✅
03-2c  Analyzer integration              → lock ✅ · implement ✅
03-2d  Golden update + checkpoint close  → lock ✅ · implement ✅
```

| Substep | Deliverable | Stop criterion |
|---------|-------------|----------------|
| **03-2a** | § Provenance fields + H5 fixture spec | ✅ locked 2026-07-10 |
| **03-2b** | `c6_longitudinal_threats.py` + tests (H1–H8, H-null, H9) | ✅ implemented 2026-07-10 |
| **03-2c** | Wire `detect_threats()` into analyzer (`_build_threat_inputs`) | ✅ implemented 2026-07-10 |
| **03-2d** | Update `analyzer_golden_summary.json`; seal in `completed_improvements_log.md` | ✅ implemented 2026-07-10 |

**Gate-dependent threats** (`benchmark_gate_failure`, `gate_evidence_missing`) — **03-3 only**.

### Threat engine scope (03-2b)

| Threat ID | 03-2b |
|-----------|-------|
| `model_hash_drift` | ✅ |
| `corpus_fingerprint_drift` | ✅ |
| `runtime_environment_drift` | ✅ (expect on default fixture) |
| `prompt_evolution` | ✅ (after 03-2a paths locked) |
| `protocol_revision_mid_window` | ✅ |
| `incomplete_data` | ✅ extend (`report_completeness: incomplete`) |
| `selection_bias` | ✅ |
| AC v1.0 carryover (6 IDs) | ✅ preserve |
| `benchmark_gate_failure` | ✅ 03-3 |
| `gate_evidence_missing` | ✅ 03-3 |

**Output:** `threats_disclosed` sorted lexicographically.

### Test plan H1–H9 (locked for 03-2b — see § Step 03-2b implementation lock)

| ID | Name | Assert |
|----|------|--------|
| **H1** | Deterministic sorted output | `out == sorted(out)`; repeated calls identical |
| **H2** | `runtime_environment_drift` | Default fixture env rows + consumed `run_id`s → threat present |
| **H3** | `model_hash_drift` | Second `model_hash` on consumed `run_id` |
| **H4** | `corpus_fingerprint_drift` | Second corpus fingerprint on consumed run |
| **H5** | `prompt_evolution` | Two distinct `provenance_pin` tuples per § Provenance fields |
| **H6** | `protocol_revision_mid_window` | Prior JSONL with different `metric_schema_version` in window |
| **H7** | `incomplete_data` | `report_completeness: incomplete` → threat |
| **H8** | `selection_bias` | `validation_rows_dropped` or `total_invalid > 0` |
| **H-null-1..5** | Null/empty + registry | See § Step 03-2b implementation lock |
| **H9** | Negative drift | Varying `model_hash` → no `runtime_environment_drift` |

**H10** (golden summary end-to-end) — **03-2d**. Analyzer integration tests — **03-2c**.

### Step 03-2a implementation lock 🔒 (2026-07-10)

**Status:** **Implemented** 2026-07-10 — provenance helpers + P1–P7 tests green

| Deliverable | Location |
|-------------|----------|
| JSON paths, normalization, composite `provenance_pin` | RC § **Provenance fields (locked)** |
| H5 fixture spec | Same section |
| Production survey | `completed_improvements_log.md` |
| `scripts/c6_longitudinal_provenance.py` | `normalize_pin_value`, `extract_provenance_pin`, `prompt_evolution_detected` |
| `scripts/test_c6_longitudinal_provenance.py` | P1–P7 |

**Regression gate:** `python3 scripts/test_c6_longitudinal_provenance.py`

**Next:** Implement **03-2b** per § Step 03-2b implementation lock.

---

## Step 03-2b implementation lock 🔒 (2026-07-10)

**Status:** **Implemented** 2026-07-10 — H1–H9 + H-null green; analyzer unchanged

| Deliverable | Location |
|-------------|----------|
| `THREAT_IDS` + frozen `ThreatInputs` + pure `detect_threats()` | `scripts/c6_longitudinal_threats.py` |
| H1–H8, H-null-1..5, H9 | `scripts/test_c6_longitudinal_threats.py` |
| H5 `env.prov` on `run-c601`, `run-c602` | `benchmark_env.jsonl` |
| H6 prior rows | `prior_longitudinal.jsonl` |

**Regression gate:** `python3 scripts/test_c6_longitudinal_threats.py` — 14/14 checks green.

**Next:** **03-2c** — locked; implement per § Step 03-2c implementation lock.

### Boundary (frozen)

| Rule | Requirement |
|------|-------------|
| Create | `scripts/c6_longitudinal_threats.py` + `scripts/test_c6_longitudinal_threats.py` only |
| Forbidden | `analyze_cognitive_longitudinal.py`, `analyzer_golden_summary.json`, reporting modules, CLI, `analyzer_version` bump |
| `build_threats()` | **Not removed** until **03-2c** |

### Module API (frozen)

```python
THREAT_IDS: frozenset[str] = frozenset({
    "model_hash_drift",
    "corpus_fingerprint_drift",
    "runtime_environment_drift",
    "prompt_evolution",
    "protocol_revision_mid_window",
    "small_sample",
    "tier_mixing",
    "incomplete_data",
    "selection_bias",
    "env_hash_drift",
    "fingerprint_mismatch",
    "no_official_anchor",
})
# Excluded until 03-3: benchmark_gate_failure, gate_evidence_missing

@dataclass(frozen=True)
class ThreatInputs:
    eligible_count: int
    safety: Mapping[str, Any]
    flags: tuple[str, ...]
    used_official_anchor: bool
    report_completeness: str
    env_rows: tuple[dict[str, Any], ...]
    benchmark_run_ids_consumed: tuple[str, ...]
    validation_total_invalid: int
    window: Mapping[str, Any]
    prior_longitudinal_rows: tuple[dict[str, Any], ...] | None = None

def distinct_non_null(values: Iterable[Any]) -> set[str]: ...

def extract_env_fields_for_threats(env_row: dict[str, Any]) -> tuple[str|None, str|None, str|None, str|None]:
    """Parity: c6_longitudinal_join._extract_env_fields (JC v1.0). Do not diverge."""

def detect_threats(inputs: ThreatInputs) -> list[str]:
    """Pure: ThreatInputs → sorted list[str]. MUST NOT mutate inputs."""
```

| Rule | Requirement |
|------|-------------|
| Purity | `detect_threats()` has no side effects; does not mutate `ThreatInputs` or nested dicts |
| Output | Lexicographically sorted; every ID ∈ `THREAT_IDS` |
| Drift | Use `distinct_non_null()`; empty/null-only fields MUST NOT produce drift threats |
| Provenance | Delegate `prompt_evolution` to `c6_longitudinal_provenance.prompt_evolution_detected` |
| Env extract | `extract_env_fields_for_threats` copies join paths only — no join semantic changes |

### Threat detection (03-2b active set)

Same rules as § Threat detection table for the 12 IDs in `THREAT_IDS`. Gate IDs deferred to 03-3.

### Fixtures (03-2b implement only)

| File | Purpose |
|------|---------|
| `benchmark_env.jsonl` | H5: add `env.prov` on `run-c601`, `run-c602` per § Provenance fields |
| `prior_longitudinal.jsonl` | **New** — H6 protocol revision rows |

**Not modified in 03-2b:** `analyzer_golden_summary.json`, report goldens.

### Test plan (locked)

| ID | Assert |
|----|--------|
| **H1** | `out == sorted(out)`; `detect_threats(x) == detect_threats(x)` |
| **H2** | Fixture env + consumed `run_id`s → `runtime_environment_drift` |
| **H3** | Inline rows → `model_hash_drift` |
| **H4** | Inline rows → `corpus_fingerprint_drift` |
| **H5** | Fixture `env.prov` → `prompt_evolution` |
| **H6** | `prior_longitudinal.jsonl` → `protocol_revision_mid_window` |
| **H7** | `report_completeness: incomplete` → `incomplete_data` |
| **H8** | `validation_rows_dropped` or `validation_total_invalid > 0` → `selection_bias` |
| **H-null-1** | Empty consumed `run_id`s → no env-based drift |
| **H-null-2** | All-null env fields → no env drift threats |
| **H-null-3** | Single env_hash + single model + single corpus → no `runtime_environment_drift` |
| **H-null-4** | No prior JSONL → no `protocol_revision_mid_window` |
| **H-null-5** | Every output ID ∈ `THREAT_IDS` |
| **H9** | Different `model_hash` across runs → `model_hash_drift` yes; `runtime_environment_drift` no |

**Test isolation:** Build `ThreatInputs` directly from inline dicts or fixture JSONL reads. **No** `analyze_cognitive_longitudinal` import in default test path (integration → **03-2c**).

### 03-2b stop criteria

1. `detect_threats()` pure + `THREAT_IDS` validation  
2. H1–H8, H-null-1..5, H9 green  
3. `test_c6_longitudinal_provenance.py` + join + analyzer + reporting regressions unchanged and green  
4. No analyzer / golden / reporting changes  

**Regression gate:** `python3 scripts/test_c6_longitudinal_threats.py`

**Next:** **03-2c** — implement per § Step 03-2c implementation lock.

---

## Step 03-2c implementation lock 🔒 (2026-07-10)

**Status:** **Implemented** 2026-07-10 — `_build_threat_inputs` + I1–I5 green; golden deferred 03-2d

| Deliverable | Location |
|-------------|----------|
| `_build_threat_inputs()` + `load_prior_longitudinal_rows()` | `scripts/analyze_cognitive_longitudinal.py` |
| `build_threats()` removed; `detect_threats()` wired | Same |
| `prior_longitudinal_path`; CLI read-before-write | Same |
| I1–I5 integration tests | `scripts/test_c6_longitudinal_analyzer.py` |

**Regression gate:** `python3 scripts/test_c6_longitudinal_analyzer.py` — I1–I5 + A1–A9 green; `golden_summary` skipped until 03-2d.

**Next:** **03-2d** — update `analyzer_golden_summary.json`.

### Boundary (frozen — historical 03-2c lock)

| Layer | Responsibility |
|-------|----------------|
| `analyze_cognitive_longitudinal.py` | Assemble `ThreatInputs` via `_build_threat_inputs()`; call `detect_threats()`; assign `threats_disclosed` |
| `c6_longitudinal_threats.py` | **All** threat detection rules — analyzer MUST NOT duplicate detection logic |

**Forbidden in analyzer:** inline drift cardinality checks, provenance comparison, or any `if … append("<threat_id>")` beyond delegating to `detect_threats()`.

### Boundary (frozen)

| Rule | Requirement |
|------|-------------|
| Modify | `scripts/analyze_cognitive_longitudinal.py` + `scripts/test_c6_longitudinal_analyzer.py` (I1–I5) only |
| Remove | `build_threats()` — replaced by `detect_threats(_build_threat_inputs(...))` |
| Forbidden | `analyzer_golden_summary.json` (**03-2d**), reporting modules, gates (**03-3**), plots, CLI orchestration (**03-5**), `analyzer_version` bump (**03-7**) |
| Engine | `c6_longitudinal_threats.py` unchanged |

### Analyzer API (frozen)

```python
def load_prior_longitudinal_rows(path: Path | None) -> tuple[dict[str, Any], ...] | None:
    """Read-only. Returns None if path missing."""

def _build_threat_inputs(
    *,
    perf: SuccessRateStats,
    safety: Mapping[str, Any],
    flags: Sequence[str],
    used_official_anchor: bool,
    report_completeness: str,
    env_rows: Sequence[dict[str, Any]],
    benchmark_run_ids_consumed: Sequence[str],
    validation_total_invalid: int,
    window: Mapping[str, Any],
    prior_longitudinal_rows: Sequence[dict[str, Any]] | None = None,
) -> threats.ThreatInputs:
    """Single assembly path. Packages inputs only — no detection rules."""

def analyze_longitudinal(
    ...,
    prior_longitudinal_path: Path | None = None,
) -> AnalysisResult:
```

| Rule | Requirement |
|------|-------------|
| Assembly | `_build_threat_inputs()` is the **only** production path to `ThreatInputs` |
| Immutability | Returns frozen `ThreatInputs`; convert `flags`/`env_rows`/`benchmark_run_ids_consumed` to `tuple`; `prior_longitudinal_rows` → `tuple` or `None` |
| Detection | `threats_disclosed = threats.detect_threats(_build_threat_inputs(...))` |
| `benchmark_run_ids` | Computed **before** threat evaluation; same set as `reproducibility.benchmark_run_ids_consumed` |
| Prior JSONL | `prior_longitudinal_path` read **before** `write_outputs()`; current run MUST NOT evaluate itself as prior evidence |
| CLI | `main()` auto-loads `output_dir / "cognitive_longitudinal.jsonl"` when file exists, passes to `analyze_longitudinal` before append |

### Prior JSONL semantics (frozen)

| Context | Source |
|---------|--------|
| Production | `output_dir / "cognitive_longitudinal.jsonl"` if exists **before** `write_outputs()` |
| I3 test | `fixtures/prior_longitudinal.jsonl` via `prior_longitudinal_path` |
| Default regression | `None` — no `protocol_revision_mid_window` |

### Expected default-fixture threats (post-wire)

Carryover: `env_hash_drift`, `small_sample`, `fingerprint_mismatch`  
New: `prompt_evolution`, `runtime_environment_drift`

Sorted:

```text
["env_hash_drift", "fingerprint_mismatch", "prompt_evolution",
 "runtime_environment_drift", "small_sample"]
```

### Integration tests (locked — `test_c6_longitudinal_analyzer.py`)

All tests use `_build_threat_inputs()` — **no** duplicated `ThreatInputs(...)` construction in test file.

| ID | Assert |
|----|--------|
| **I1** | Default fixture → `runtime_environment_drift` and `prompt_evolution` ∈ `threats_disclosed` |
| **I2** | Delegation parity: `threats_disclosed == detect_threats(_build_threat_inputs(...))` |
| **I3** | `prior_longitudinal_path=fixtures/prior_longitudinal.jsonl` → `protocol_revision_mid_window` |
| **I4** | `threats_disclosed == sorted(threats_disclosed)` |
| **I5** | Every emitted ID ∈ `threats.THREAT_IDS` |

**H10** (`golden_summary` end-to-end) — **03-2d** only.

### Golden deferral (frozen)

| Rule | Requirement |
|------|-------------|
| `analyzer_golden_summary.json` | **Not modified** in 03-2c |
| `test_golden_summary` | **Expected FAIL** after 03-2c implement until **03-2d** updates golden |
| 03-2c regression gate | I1–I5 + A1–A9 (except `golden_summary`) + join + reporting R1–R2 + threat engine H1–H9 |

### 03-2c stop criteria

1. `build_threats()` removed; `_build_threat_inputs()` + `detect_threats()` wired  
2. I1–I5 green  
3. A1–A9 (except `golden_summary`) green  
4. Join + reporting + threat engine regressions green  
5. No golden / reporting / version / gate changes  

**Regression gate:** `python3 scripts/test_c6_longitudinal_analyzer.py` (I1–I5; `golden_summary` fail documented)

**Next:** **03-2d** — implement per § Step 03-2d implementation lock.

---

## Step 03-2d implementation lock 🔒 (2026-07-10)

**Status:** **Implemented** 2026-07-10 — golden diff gate passed; H10 green; 03-2 umbrella sealed

| Deliverable | Location |
|-------------|----------|
| `analyzer_golden_summary.json` — `threats_disclosed` only | `tests/fixtures/cognitive_longitudinal/` |
| H10 re-enabled (full payload compare) | `scripts/test_c6_longitudinal_analyzer.py` |
| 03-2 evidence block | `completed_improvements_log.md` |

**Golden diff gate:** only `threats_disclosed` changed (3 → 5 IDs, engine-sorted).

**Regression gate:** all six C6 suites green; no production-source changes.

**03-2 umbrella:** 03-2a–d ✅ complete.

**Next:** (historical) superseded by § Step 03-3 implementation lock

### Scope (frozen — historical 03-2d lock)

| In 03-2d | Forbidden (scope violation) |
|----------|----------------------------|
| `analyzer_golden_summary.json` — `threats_disclosed` only | `analyze_cognitive_longitudinal.py` |
| `test_c6_longitudinal_analyzer.py` — re-enable H10 | `c6_longitudinal_threats.py`, `c6_longitudinal_provenance.py` |
| Doc close-out (RC, protocol, `completed_improvements_log.md`) | `c6_longitudinal_report.py`, gates (**03-3**), `analyzer_version` (**03-7**) |

**Production source rule:** Any change to analyzer, threat engine, or reporting during 03-2d is a **scope violation** — stop and reopen **03-2c** or **03-2b** as appropriate.

### Golden update procedure (frozen)

Execute in order; **do not write golden until step 4 passes.**

| Step | Action |
|------|--------|
| 1 | Run `analyze_longitudinal(default fixtures, AS_OF_MS=1_800_002_000_000)` |
| 2 | `actual = normalize_payload(payload)`; `golden = normalize_payload(load golden)` |
| 3 | `changed_fields = {k for k in keys if actual[k] != golden[k]}` |
| 4 | **GATE:** `changed_fields == {"threats_disclosed"}` — **STOP** if any other field differs |
| 5 | Write `golden["threats_disclosed"] = actual["threats_disclosed"]` — verbatim `detect_threats()` output (sorted); **no manual reorder** |
| 6 | Verify `python3 scripts/test_c6_longitudinal_analyzer.py` exits `0` |

**Expected sole diff:**

| Old (3 IDs) | New (5 IDs, engine-sorted) |
|-------------|----------------------------|
| `env_hash_drift`, `small_sample`, `fingerprint_mismatch` | `env_hash_drift`, `fingerprint_mismatch`, `prompt_evolution`, `runtime_environment_drift`, `small_sample` |

### Test changes (frozen)

| Change | Requirement |
|--------|-------------|
| Re-enable `golden_summary` in `run_all_tests()` | Remove 03-2c skip stub |
| **H10** | **Full** normalized payload compare — `normalize_payload(actual) == golden`; **not** threats-only |
| I1–I5 | Unchanged — remain green |

### Documentation close-out evidence (required)

`completed_improvements_log.md` entry **must** record:

| Substep | Evidence | Regression gate |
|---------|----------|-----------------|
| **03-2b** | Threat engine H1–H9 (14 checks) | `test_c6_longitudinal_threats.py` |
| **03-2c** | Analyzer integration I1–I5 | `test_c6_longitudinal_analyzer.py` |
| **03-2d** | Golden seal H10 | `test_c6_longitudinal_analyzer.py` (full suite, no skips) |

Mark **03-2 umbrella (03-2a–d) complete**; next: **03-3** (gates).

### 03-2d stop criteria

1. Golden diff gate passed — only `threats_disclosed` changed  
2. H10 re-enabled; no skipped analyzer tests  
3. All six regression suites green  
4. Zero production-source modifications  
5. Docs sealed with evidence block  

**Regression gate:**

```bash
python3 scripts/test_c6_longitudinal_analyzer.py      # A1–A9 + I1–I5 + H10
python3 scripts/test_c6_longitudinal_threats.py
python3 scripts/test_c6_longitudinal_provenance.py
python3 scripts/test_c6_longitudinal_join.py
python3 scripts/test_c6_longitudinal_reporting.py
```

**Next:** **03-3** — implement per § Step 03-3 implementation lock.

---

## Step 03-3 implementation lock 🔒 (2026-07-10)

**Status:** ✅ **Complete** 2026-07-10 · single atomic checkpoint (internal substeps 03-3a–d)

### Decomposition (execute in order — single lock)

```
03-3a  evaluate_safety_gates()     → lock ✅ · implement
03-3b  compute_safety() integrate  → lock ✅ · implement
03-3c  threat engine gate threats  → lock ✅ · implement
03-3d  golden refresh              → lock ✅ · implement
```

### Atomic pipeline (frozen)

```
benchmark_env terminal evidence
    ↓
evaluate_safety_gates()          # analyzer — gate rules
    ↓
compute_safety()                 # analyzer — safety payload
    ↓
detect_threats()                 # threat engine — disclosed threats only
```

### Safety interpretation lock (normative)

| Rule | Requirement |
|------|-------------|
| Regression | **Only** official `*_ABORTED` terminal evidence inside the valid analysis window sets `benchmark_regression: true` |
| Missing ≠ regression | Missing `*_COMPLETE` evidence **does not** imply regression |
| Missing evidence | Affects `official_gates_green` only; emits `gate_evidence_missing:<harness_key>` per required harness |
| Unknown events | Terminal events outside the frozen 12-event allowlist remain **ignored** (no suffix inference) |

### Architectural boundary lock (normative)

| Layer | Owns | MUST NOT |
|-------|------|----------|
| **Analyzer** | Gate rules; terminal interpretation; `evaluate_safety_gates()`; `compute_safety()` | Emit threat IDs inline; inspect threats beyond delegating to `detect_threats()` |
| **Threat engine** | Threat identifiers; mapping analyzer outputs (`safety`, `flags`) into `threats_disclosed` | Read `benchmark_env` directly; duplicate gate terminal logic |

No layer may absorb the responsibility of another.

### Deferred unchanged (frozen)

| Item | Deferred to |
|------|-------------|
| `derive_evidence_scope()` | **Unchanged** — AC v1.0 rules; no gate cap in 03-3 |
| Promotion gate logic | **C6.3-04** ✅ (parent protocol § C6.3-04) |
| `analyzer_version` → `0.2.0` | **03-7** |
| Reporting `THREAT_LABELS` | **Unchanged** — labels already present |
| R5–R7 reporting tests + `gate_status_golden.json` | **03-6** |
| Markdown observational harness block | **03-5/03-6** |

### Internal execution substeps (single 03-3 lock — implement in order)

```
03-3a  evaluate_safety_gates()     → gate evaluator
03-3b  compute_safety() integrate  → remove stubs
03-3c  threat engine gate threats  → THREAT_IDS 12 → 14
03-3d  golden refresh              → diff gate + H10
```

#### 03-3a — `evaluate_safety_gates()` (analyzer)

| Deliverable | Requirement |
|-------------|-------------|
| `GateEvaluation` | `@dataclass(frozen=True)` — `benchmark_regression`, `official_gates_green`, `gate_flags: tuple[str, ...]` |
| `REQUIRED_GATE_HARNESSES` | Registry per § Harness registry (`reflection_ab`, `robustness`, `episodic_learning`) |
| Terminal filter | Reuse **existing** helpers — `OFFICIAL_TERMINAL_EVENTS`, `index_benchmark_env`, `normalize_tier`; **do not duplicate** anchor filtering logic |
| Missing evidence | No official terminal for required harness → `gate_evidence_missing:<harness_key>` in `gate_flags` |
| Missing ≠ regression | Absence of terminals MUST NOT set `benchmark_regression: true` |

**Checkpoint:** Gate evaluator correctly distinguishes regression · missing evidence · green gates.

**Tests:** G1 (default missing), G2 (ABORTED → regression), G3 (all COMPLETE → green), G4 (non-full tier ignored), G5 (unknown event ignored).

#### 03-3b — Integrate into `compute_safety()` (analyzer)

| Remove | Replace with |
|--------|--------------|
| `benchmark_regression_not_wired` flag | (deleted) |
| Hard-coded `benchmark_regression: False` | `evaluate_safety_gates()` result |
| Hard-coded `official_gates_green: False` | `evaluate_safety_gates()` result |

| Preserve | Requirement |
|----------|-------------|
| `reproducibility_ok` logic | Unchanged |
| `tier_contamination` logic | Unchanged |
| Gate delegation | **Only** through `evaluate_safety_gates()` |

Extend `compute_safety()` signature: `env_rows`, `window` (after `current_bounds` known).

**Checkpoint:** `categories.safety` contains **computed** gate values, not stubs.

#### 03-3c — Activate threat engine gate threats

| Change | Detail |
|--------|--------|
| `THREAT_IDS` | Expand **12 → 14**: add `benchmark_gate_failure`, `gate_evidence_missing` |
| `detect_threats()` | `benchmark_regression is True` → `benchmark_gate_failure` |
| `detect_threats()` | any `gate_evidence_missing:*` in `inputs.flags` → `gate_evidence_missing` |
| Boundary | Threat engine consumes `ThreatInputs.safety` + `ThreatInputs.flags` only — **no** `benchmark_env` reads |

**Checkpoint:** Threat mapping parity — regression → `benchmark_gate_failure`; missing flags → `gate_evidence_missing`.

**Tests:** H10 (regression → gate threat), H11 (missing flag → gate threat); H-null-5 registry covers 14 IDs.

#### 03-3d — Golden refresh

Same discipline as § Step 03-2d:

| Step | Action |
|------|--------|
| 1 | Run analyzer on default fixtures (`AS_OF_MS=1_800_002_000_000`) |
| 2 | Normalize (`strip analyzer_commit_hash`) |
| 3 | Diff against existing `analyzer_golden_summary.json` |
| 4 | **GATE:** only expected fields changed: `flags`, `threats_disclosed`, computed `categories.safety` gate fields (values may match stubs but must be computed) |
| 5 | **STOP** on unexpected drift |
| 6 | Re-enable is already active — H10 must pass |

**Expected default-fixture deltas:**

| Field | Change |
|-------|--------|
| `flags` | Remove `benchmark_regression_not_wired`; add `gate_evidence_missing:episodic_learning`, `gate_evidence_missing:reflection_ab`, `gate_evidence_missing:robustness` |
| `threats_disclosed` | Add `gate_evidence_missing` (sorted among existing 5 IDs) |
| `safety` | `benchmark_regression: false`, `official_gates_green: false` — computed, not stubbed |

### Module API (frozen — 03-3a)

```python
REQUIRED_GATE_HARNESSES: tuple[tuple[str, str, str], ...]  # (harness_key, complete_event, aborted_event)

@dataclass(frozen=True)
class GateEvaluation:
    benchmark_regression: bool
    official_gates_green: bool
    gate_flags: tuple[str, ...]   # gate_evidence_missing:<harness_key> only

def evaluate_safety_gates(
    env_rows: Sequence[dict[str, Any]],
    window: Mapping[str, Any],
    *,
    reproducibility_ok: bool,
) -> GateEvaluation:
    """Official terminal scan for window. Pure gate rules — no threat IDs."""
```

### Threat engine delta (frozen — 03-3c)

```python
# THREAT_IDS: 12 → 14
"benchmark_gate_failure",
"gate_evidence_missing",
```

### Test plan (locked — 03-3)

| ID | Layer | Assert |
|----|-------|--------|
| **G1** | Analyzer | Default fixture: no `benchmark_regression_not_wired`; 3× `gate_evidence_missing:*`; `official_gates_green: false` |
| **G2** | Analyzer | Official `*_ABORTED` in window → `benchmark_regression: true` |
| **G3** | Analyzer | All required `*_COMPLETE` + `reproducibility_ok` → `official_gates_green: true` |
| **G4** | Analyzer | Non-`full` tier terminal ignored |
| **G5** | Analyzer | Unknown event (`FOO_COMPLETE`) ignored |
| **H10** | Threat engine | `benchmark_regression=True` → `benchmark_gate_failure` |
| **H11** | Threat engine | `gate_evidence_missing:*` flag → `gate_evidence_missing` |
| **H10** (golden) | Analyzer | Full normalized payload == `analyzer_golden_summary.json` (post 03-3d) |

Fixture slices (03-3a/G2–G3): `benchmark_env_gates_*.jsonl` under `tests/fixtures/cognitive_longitudinal/`.

### Files (03-3)

| File | Substep |
|------|---------|
| `scripts/analyze_cognitive_longitudinal.py` | 03-3a, 03-3b |
| `scripts/c6_longitudinal_threats.py` | 03-3c |
| `scripts/test_c6_longitudinal_analyzer.py` | G1–G5 |
| `scripts/test_c6_longitudinal_threats.py` | H10–H11 |
| `tests/fixtures/cognitive_longitudinal/benchmark_env_gates_*.jsonl` | 03-3a |
| `tests/fixtures/cognitive_longitudinal/analyzer_golden_summary.json` | 03-3d |

**Not touched:** `c6_longitudinal_report.py`, `derive_evidence_scope()`, `analyzer_version`, promotion gate docs.

### 03-3 stop criteria

1. Stubs removed; gate fields computed via `evaluate_safety_gates()`  
2. `THREAT_IDS` = 14; gate threats active  
3. G1–G5, H10–H11, golden H10 green  
4. Join + provenance + reporting R1–R2 regressions green  
5. Architectural boundary honored — no layer crossover  

**Regression gate:**

```bash
python3 scripts/test_c6_longitudinal_analyzer.py
python3 scripts/test_c6_longitudinal_threats.py
python3 scripts/test_c6_longitudinal_provenance.py
python3 scripts/test_c6_longitudinal_join.py
python3 scripts/test_c6_longitudinal_reporting.py
```

**Next:** **03-4** — implement per § Step 03-4 implementation lock.

---

## Step 03-4 implementation lock 🔒 (2026-07-10)

**Status:** ✅ **Complete** 2026-07-10 · single atomic checkpoint (internal substeps 03-4a–d)

### Decomposition (execute in order — single lock)

```
03-4a  Module skeleton + skip handling     → lock ✅ · implement
03-4b  Three chart renderers               → lock ✅ · implement
03-4c  Standalone CLI                      → lock ✅ · implement
03-4d  Test suite P1–P8                    → lock ✅ · implement
```

### Atomic pipeline (frozen)

```
cognitive_longitudinal_summary.json  (or summary Mapping)
    ↓
plot module — skip / matplotlib check
    ↓
PNG artifacts (0–3) + plots_skipped:* flags
```

### Architectural boundary lock (normative)

| Layer | Owns | MUST NOT |
|-------|------|----------|
| **Plot module** | Matplotlib rendering; PNG creation; plot skip decisions; matplotlib availability handling | Read `cognitive_metrics.jsonl`; perform joins; recalculate analyzer fields; inspect `benchmark_env.jsonl`; modify `evidence_scope`; modify safety/threat logic |
| **Analyzer** | Payload generation (`COGNITIVE_LONGITUDINAL_SUMMARY`) | Inline matplotlib; plot skip flags; PNG writes |

No layer may absorb the responsibility of another.

### Payload-only contract (frozen)

| Input | Rule |
|-------|------|
| Primary | `Mapping[str, Any]` — analyzer summary payload |
| Standalone CLI | Optional path to `cognitive_longitudinal_summary.json` (loads JSON → same mapping) |

**Required consumption:** `cognitive_longitudinal_summary.json` shape only.

**Forbidden dependencies:**

- Analyzer internals (no `import analyze_cognitive_longitudinal` for computation)
- Raw benchmark data (`benchmark_env.jsonl`, `cognitive_metrics.jsonl`)
- Runtime state (join artifacts, env rows, prior JSONL for plot derivation)

### Skip behavior lock (normative)

| Condition | PNGs | Flag | Exit (analyzer path) |
|-----------|------|------|----------------------|
| `report_completeness == "incomplete"` | **None** | `plots_skipped:incomplete_report` | Success (plots skipped; analysis succeeded) |
| `matplotlib` unavailable | **None** | `plots_skipped:matplotlib_unavailable` | **0** — success |
| `--no-plots` | **None** | (none) | **03-5** — plot module never sees this flag |
| Empty `segments` | `segment_success.png` with annotation | (none) | Normal |

**Forbidden:** Partial or watermarked plots on incomplete reports.

### Resolved decisions (frozen)

#### Decision A — `success_rate_trend.png` prior window

| Rule | Requirement |
|------|-------------|
| Scope | **Current window only** — `categories.performance.success_rate` + `ci_low` / `ci_high` |
| JSONL | **Do not** read `cognitive_longitudinal.jsonl` or infer prior windows |
| Prior marker | Supported **only** if an explicit future summary field exists (none in RC v1.0) |
| Documentation | Module docstring MUST state current-window-only rule |

#### Decision B — Empty `segments`

| Rule | Requirement |
|------|-------------|
| Behavior | **Annotation approach** — always emit `segment_success.png` |
| Content | Include **"no segment data"** annotation when `segments` is empty |
| Flag | **Do not** emit `plots_skipped:empty_segments` |
| Rationale | Predictable artifact count (3 PNGs on complete runs when matplotlib available) |

### Internal execution substeps (single 03-4 lock — implement in order)

#### 03-4a — Module skeleton

| Deliverable | Requirement |
|-------------|-------------|
| `Agg` backend | Same pattern as `plot_cognitive_metrics.py` |
| `PlotResult` | Frozen dataclass per § Module API |
| `should_skip_all_plots()` | Returns skip flag or `None` |
| `write_longitudinal_plots()` | Orchestrates skip + render |

**Checkpoint:** API and skip behavior exist.

#### 03-4b — Chart renderers

| File | Source fields |
|------|---------------|
| `success_rate_trend.png` | `categories.performance` — rate + CI |
| `segment_success.png` | `segments[*].success_rate`; empty → annotation |
| `efficiency_p50.png` | `categories.efficiency.total_wall_clock_ms_p50`, `total_tokens_p50` |

**Output directory:** `logs/plots/longitudinal/` (default); overridable via `--plot-dir`.

**Checkpoint:** Three RC-defined artifacts render from payload only.

#### 03-4c — Standalone CLI

```bash
python3 scripts/plot_cognitive_longitudinal.py
python3 scripts/plot_cognitive_longitudinal.py --summary logs/cognitive_longitudinal_summary.json --plot-dir logs/plots/longitudinal
```

| Rule | Requirement |
|------|-------------|
| Flags | `--summary`, `--plot-dir` only |
| Orchestration | **No** analyzer `write_outputs()` changes |
| Purpose | Debug / two-step manual path (default orchestration = **03-5**) |

**Checkpoint:** Debug path works independently.

#### 03-4d — Test suite

Implement **P1–P8** in `scripts/test_c6_longitudinal_plots.py`.

**Checkpoint:** Plot test harness passes.

### Module API (frozen — 03-4a)

```python
@dataclass(frozen=True)
class PlotResult:
    written_paths: tuple[Path, ...]
    flags: tuple[str, ...]           # plots_skipped:* only
    matplotlib_available: bool

def should_skip_all_plots(payload: Mapping[str, Any]) -> str | None:
    """Return plots_skipped:* flag if all plots must be skipped, else None."""

def write_longitudinal_plots(
    payload: Mapping[str, Any],
    plot_dir: Path,
) -> PlotResult:
    """Write 0–3 PNGs per § Plot schema. Payload-only — no artifact reads."""
```

**Flag discipline:** This module emits **only** `plots_skipped:*` flags. Analyzer flags, threat flags, and gate flags MUST NOT be emitted here.

### Test plan (locked — 03-4)

| ID | Assert |
|----|--------|
| **P1** | Complete fixture (`analyzer_golden_summary.json`) → exactly 3 PNG files |
| **P2** | Incomplete payload → 0 files; `plots_skipped:incomplete_report` |
| **P3** | Matplotlib unavailable (mock) → 0 files; `plots_skipped:matplotlib_unavailable` |
| **P4** | `segment_success.png` — bar count == `len(segments)`; labels match keys |
| **P5** | `success_rate_trend.png` — uses `success_rate`, `ci_low`, `ci_high` |
| **P6** | `efficiency_p50.png` — uses both efficiency p50 fields (zeros OK) |
| **P7** | Empty `segments` → `segment_success.png` exists with "no segment data"; **no** `plots_skipped:empty_segments` |
| **P8** | Idempotent re-run overwrites same filenames |

Fixture: `tests/fixtures/cognitive_longitudinal/analyzer_golden_summary.json` (read-only).

### Golden discipline (frozen)

| Rule | Requirement |
|------|-------------|
| `analyzer_golden_summary.json` | **Do not modify** unless true schema mismatch discovered |
| Plot tests | Consume golden payload; plots do not alter it |

### Files (03-4)

| File | Substep |
|------|---------|
| `scripts/plot_cognitive_longitudinal.py` | 03-4a, 03-4b, 03-4c |
| `scripts/test_c6_longitudinal_plots.py` | 03-4d |

**Not touched:** `analyze_cognitive_longitudinal.py`, `c6_longitudinal_report.py`, `analyzer_golden_summary.json` (unless schema mismatch), CTest, `analyzer_version`.

### Explicitly deferred (not 03-4)

| Item | Checkpoint |
|------|------------|
| CLI orchestration (`write_outputs` plot step) | **03-5** |
| `--no-plots` handling | **03-5** |
| Analyzer write order (steps 4–5) | **03-5** |
| R8 (plots skipped incomplete), R9 (orchestration) | **03-6** |
| `analyzer_version` → `0.2.0` | **03-7** |
| CTest changes | **03-6** |
| `completed_improvements_log.md` close-out | **03-7** |

### 03-4 stop criteria

1. `plot_cognitive_longitudinal.py` implements § Plot schema (3 files max on complete + matplotlib available)  
2. Skip rules and flag discipline honored (`plots_skipped:*` only)  
3. Architectural boundary honored — payload-only, no join/analyzer recomputation  
4. P1–P8 green  
5. Existing five C6 regression suites green (no analyzer changes)  

**Regression gate:**

```bash
python3 scripts/test_c6_longitudinal_plots.py          # new
python3 scripts/test_c6_longitudinal_analyzer.py
python3 scripts/test_c6_longitudinal_threats.py
python3 scripts/test_c6_longitudinal_provenance.py
python3 scripts/test_c6_longitudinal_join.py
python3 scripts/test_c6_longitudinal_reporting.py
```

**Next:** **03-5** — implement per § Step 03-5 implementation lock.

---

## Step 03-5 implementation lock 🔒 (2026-07-10)

**Status:** ✅ **Complete** 2026-07-10 · single atomic checkpoint (internal substeps 03-5a–d)

### Decomposition (execute in order — single lock)

```
03-5a  WriteOptions + path registry + write_outputs extension  → lock ✅ · implement
03-5b  CLI argument wiring (--no-report, --no-plots)           → lock ✅ · implement
03-5c  Delegation + summary phase-2 patch                    → lock ✅ · implement
03-5d  Orchestration tests O1–O6                             → lock ✅ · implement
```

### Atomic pipeline (frozen)

```
analyze_longitudinal()
        ↓
analytical payload
        ↓
orchestration (write_outputs)
        ├─→ append JSONL              (analytical only)
        ├─→ write summary JSON        (phase 1)
        ├─→ render markdown report    (unless --no-report)
        ├─→ generate plots            (unless --no-plots)
        └─→ patch summary JSON        (phase 2, if PlotResult.flags)
```

### Artifact pipeline ownership (normative)

| Layer | Owns | MUST NOT |
|-------|------|----------|
| **Analyzer orchestration** | CLI arguments; execution order; delegation; output paths | Render markdown inline; generate matplotlib plots inline; recompute analytical values; inspect raw benchmark data beyond `analyze_longitudinal()` |
| **`c6_longitudinal_report`** | `render_markdown_report()` / `render_report_sections()` | File I/O (orchestrator writes `.md`) |
| **`plot_cognitive_longitudinal`** | `write_longitudinal_plots()` + `plots_skipped:*` flags | Analyzer internals; analytical recomputation |

The analyzer CLI is the **orchestrator only**. No layer may absorb another’s responsibility.

### Write order lock (normative — no reordering)

| Step | Action | Artifact class |
|------|--------|----------------|
| 1 | Compute payload | Analysis |
| 2 | Append JSONL | **Immutable analytical history** |
| 3 | Write summary JSON (phase 1) | Derived snapshot |
| 4 | Write markdown | Presentation (unless `--no-report`) |
| 5 | Write plots | Presentation (unless `--no-plots`) |
| 6 | Patch summary (phase 2) | Derived snapshot update (plot flags only) |

**Rationale:** JSONL is immutable analytical history; summary is derived; report and plots are presentation artifacts.

### JSONL immutability lock (normative)

| Rule | Requirement |
|------|-------------|
| Content | JSONL rows MUST contain **only** the analytical payload from `analyze_longitudinal()` |
| Forbidden in JSONL | `plots_skipped:*`; presentation-layer flags; matplotlib state |
| Plot flags | Belong in **summary JSON only** (phase-2 patch after step 5) |
| Mutation | JSONL MUST NOT be modified after append |

### Summary patch behavior (frozen — two-phase write)

| Phase | When | Content |
|-------|------|---------|
| **Phase 1** | Step 3 | Analytical summary (same payload as JSONL row + optional `summary_written_at_ms`) |
| **Phase 2** | After step 5 | If `PlotResult.flags` non-empty: merge into summary `flags`, **sort and deduplicate**, rewrite summary JSON **only** |

`write_outputs()` docstring MUST document this two-phase summary discipline. JSONL is never modified in phase 2.

### Markdown timing lock (frozen)

| Rule | Requirement |
|------|-------------|
| Order | Markdown generated at **step 4**, **before** plots (step 5) |
| Plot skip flags in markdown | `plots_skipped:incomplete_report` (and other plot skip flags) **may be absent** from markdown on the same run |
| Authority | Summary JSON (post phase 2) is authoritative for plot skip flags |
| Scope | **No** second markdown render pass |

### CLI lock (frozen)

| Flag | Default | Behavior |
|------|---------|----------|
| `--no-report` | off | Skip markdown only; JSONL + summary still written |
| `--no-plots` | off | Skip plot module entirely; **no** `plots_skipped:*` flags emitted |
| `--dry-run` | off | Compute payload only; stdout JSON; **no** filesystem writes |

```bash
python3 scripts/analyze_cognitive_longitudinal.py
python3 scripts/analyze_cognitive_longitudinal.py --no-report
python3 scripts/analyze_cognitive_longitudinal.py --no-plots
```

### Internal execution substeps (single 03-5 lock — implement in order)

#### 03-5a — `WriteOptions` + path registry + `write_outputs()` extension

| Deliverable | Requirement |
|-------------|-------------|
| `default_output_paths()` | Add `report` → `{output_dir}/cognitive_longitudinal_report.md` |
| `default_plot_dir()` | `repo_root() / "logs" / "plots" / "longitudinal"` |
| `WriteOptions` | Frozen dataclass per § Module API |
| `write_outputs()` | Implements steps 2–6 in frozen order |

**Checkpoint:** Output sequencing API exists.

#### 03-5b — CLI argument wiring

| Deliverable | Requirement |
|-------------|-------------|
| `--no-report`, `--no-plots` | Wired to `WriteOptions` |
| `main()` | Passes options when not `--dry-run`; preserves existing defaults |
| Analysis | CLI controls presentation artifacts only — **no** change to `analyze_longitudinal()` |

**Checkpoint:** CLI controls presentation without changing analysis.

#### 03-5c — Delegation + summary patch

| Delegate | Call |
|----------|------|
| Markdown | `c6_longitudinal_report.render_markdown_report(payload)` → write `paths["report"]` |
| Plots | `plot_cognitive_longitudinal.write_longitudinal_plots(payload, plot_dir)` |

| Rule | Requirement |
|------|-------------|
| No inline rendering | Analyzer MUST NOT contain markdown or matplotlib logic |
| Phase-2 patch | Merge `PlotResult.flags` into summary `flags` only |
| `--no-plots` | Do not call plot module; no plot flags |

**Checkpoint:** Analyzer delegates only; no rendering logic in orchestrator.

#### 03-5d — Orchestration tests O1–O6

Implement in `scripts/test_c6_longitudinal_analyzer.py`.

**Test isolation (mandatory):**

- Temporary `output_dir` for JSONL / summary / markdown
- Temporary `plot_dir` via `WriteOptions.plot_dir`
- **Never** write to production `logs/plots/longitudinal/` in tests

**Checkpoint:** Pipeline behavior verified.

### Module API (frozen — 03-5a)

```python
@dataclass(frozen=True)
class WriteOptions:
    write_report: bool = True
    write_plots: bool = True
    plot_dir: Path | None = None   # default: default_plot_dir()

def default_plot_dir() -> Path: ...

def write_outputs(
    payload: dict[str, Any],
    output_dir: Path,
    *,
    summary_written_at_ms: int | None = None,
    options: WriteOptions | None = None,
) -> None:
    """
    Orchestrate JSONL append, summary write (two-phase), markdown, and plots.

    Phase 1: analytical summary. Phase 2: merge plots_skipped:* into summary
    flags only if plot module returns flags. JSONL is never patched.
    """
```

Defaults MUST preserve pre-03-5 JSONL + summary behavior when `write_report=False` and `write_plots=False` (backward compatible for A7 and direct callers).

### Test plan (locked — 03-5)

| ID | Assert |
|----|--------|
| **O1** | Default options on complete fixture → JSONL + summary + `.md`; 3 PNGs if matplotlib available |
| **O2** | `write_report=False` → no `.md`; JSONL + summary written |
| **O3** | `write_plots=False` → no PNGs; no `plots_skipped:*` in summary |
| **O4** | Write order — JSONL append before summary before report (mtime or call-order) |
| **O5** | `--dry-run` equivalent — no filesystem writes |
| **O6** | Incomplete fixture → no PNGs; summary `flags` contains `plots_skipped:incomplete_report` after orchestration; JSONL `flags` unchanged vs analytical payload |

**Deferred to 03-6:** R3–R9 (including formal R8/R9).

### Files (03-5)

| File | Substep |
|------|---------|
| `scripts/analyze_cognitive_longitudinal.py` | 03-5a, 03-5b, 03-5c |
| `scripts/test_c6_longitudinal_analyzer.py` | 03-5d (O1–O6) |

**Not touched:** `c6_longitudinal_report.py`, `plot_cognitive_longitudinal.py`, `analyzer_golden_summary.json`, CTest, `analyzer_version`.

### Explicitly deferred (not 03-5)

| Item | Checkpoint |
|------|------------|
| R3–R9 formal reporting validation | **03-6** |
| CTest registration | **03-6** |
| `gate_status_golden.json` | **03-6** |
| `analyzer_version` → `0.2.0` | **03-7** |
| Final golden refresh | **03-7** |
| Operational documentation (C6.3-05) | **C6.3-05** ✅ |

### 03-5 stop criteria

1. Frozen write order steps 1–6 honored  
2. JSONL immutability — no `plots_skipped:*` in JSONL  
3. Two-phase summary patch documented and implemented  
4. `--no-report`, `--no-plots`, `--dry-run` per CLI lock  
5. Delegation only — no inline markdown/matplotlib in analyzer  
6. O1–O6 green with tempfile isolation  
7. All seven C6 regression suites green  

**Regression gate:**

```bash
python3 scripts/test_c6_longitudinal_analyzer.py    # + O1–O6
python3 scripts/test_c6_longitudinal_plots.py
python3 scripts/test_c6_longitudinal_reporting.py
python3 scripts/test_c6_longitudinal_threats.py
python3 scripts/test_c6_longitudinal_provenance.py
python3 scripts/test_c6_longitudinal_join.py
```

**Next:** **03-7** — implement per § Step 03-7 implementation lock.

---

## Step 03-6 implementation lock 🔒 (2026-07-11)

**Status:** ✅ **Implemented** 2026-07-11 · single atomic checkpoint (internal phases 03-6a–d are execution order only — **not** separate locks)

### Implementation invariant (normative)

> **Step 03-6 SHALL NOT modify analyzer behavior or reporting semantics.** Its purpose is validation and CI enforcement only.

No production logic changes belong in this checkpoint. Tests, expected-output fixtures, and CTest wiring only.

### Objective

1. **Protocol / research contract validation** — R3–R7 in `test_c6_longitudinal_reporting.py` (RC-governed).
2. **Engineering / infrastructure validation** — R8–R9 + CTest entries (enforce contract in dev/CI; not additional research claims).

The reporting contract governs reporting behavior. CTest is engineering infrastructure that enforces the contract.

### Validation layers (frozen)

#### Layer A — Protocol / research contract (RC-governed)

| ID | Name | Assert |
|----|------|--------|
| **R3** | Threat parity | Markdown § Threats bullets match JSON `threats_disclosed` (sorted IDs + `THREAT_LABELS`) |
| **R4** | Forbidden prose guard | No `FORBIDDEN_PROSE` phrases in `c6_longitudinal_report.py` static template text |
| **R5** | Benchmark regression | `*_ABORTED` in window → `benchmark_regression: true` |
| **R6** | Official gates green | All required `*_COMPLETE` + `reproducibility_ok` → `official_gates_green: true` |
| **R7** | Missing gate evidence | Required harness absent → `gate_evidence_missing:*`; `official_gates_green: false` |

**Unchanged:** R1, R1b, R2.

**Fixture:** `gate_status_golden.json` (R5–R7) + `benchmark_env_gates_green.jsonl` (R6).

#### Layer B — Engineering / infrastructure (not RC semantics)

| ID | Name | Assert |
|----|------|--------|
| **R8** | Orchestration — incomplete plots | See § R8 lock below |
| **R9** | Artifact generation | See § R9 lock below |
| **CTest** | CI registration | Three `pr;fast` tests per § CI / CTest registration |

Overlap with analyzer G/O tests is intentional: analyzer owns computation; reporting suite owns formal contract + orchestration smoke.

### R8 lock (engineering — strengthened)

After incomplete run via `write_outputs()` (temp `output_dir` + temp `plot_dir`; fixture `missing_decision_trace.jsonl`):

| Check | Requirement |
|-------|-------------|
| Plot files | **Zero** — no files under `plot_dir` |
| Plot directory | Temp `plot_dir` remains **empty** |
| Summary flags | `plots_skipped:incomplete_report` in **summary JSON** (phase-2 patch) |
| JSONL immutability | JSONL `flags` match analytical payload only; **no** `plots_skipped:*` in JSONL |

### R9 lock (engineering — required vs optional)

| Artifact | Required? |
|----------|-----------|
| `cognitive_longitudinal.jsonl` | **Yes** |
| `cognitive_longitudinal_summary.json` | **Yes** |
| `cognitive_longitudinal_report.md` | **Yes** |
| PNG plots (`PLOT_FILENAMES`) | **Optional** — only when matplotlib available |

Pipeline **must succeed** without plotting support. Tests use temp `output_dir` + temp `plot_dir`; **never** `logs/plots/longitudinal/`.

### CTest lock (engineering — not protocol)

CTest entries enforce the contract in local dev and CI. The RC remains CI-system-independent.

| CTest name | Command | Labels | Timeout | `WORKING_DIRECTORY` |
|------------|---------|--------|---------|---------------------|
| `c6-longitudinal-join` | `python3 scripts/test_c6_longitudinal_join.py` | `pr;fast` | 120s | `${CMAKE_SOURCE_DIR}` |
| `c6-longitudinal-analyzer` | `python3 scripts/test_c6_longitudinal_analyzer.py` | `pr;fast` | 120s | `${CMAKE_SOURCE_DIR}` |
| `c6-longitudinal-reporting` | `python3 scripts/test_c6_longitudinal_reporting.py` | `pr;fast` | 120s | `${CMAKE_SOURCE_DIR}` |

Plots / threats / provenance suites: seven-suite **script** regression gate only (out of 03-6 CTest scope per RC table).

### `gate_status_golden.json` (frozen shape — R5–R7)

```json
{
  "cases": {
    "default_missing_gates": {
      "benchmark_env": "benchmark_env.jsonl",
      "safety": { "benchmark_regression": false, "official_gates_green": false },
      "flags_contains": [
        "gate_evidence_missing:reflection_ab",
        "gate_evidence_missing:robustness",
        "gate_evidence_missing:episodic_learning"
      ]
    },
    "aborted_regression": {
      "benchmark_env": "benchmark_env_gates_aborted.jsonl",
      "safety": { "benchmark_regression": true, "official_gates_green": false }
    },
    "gates_green": {
      "benchmark_env": "benchmark_env_gates_green.jsonl",
      "safety": {
        "benchmark_regression": false,
        "official_gates_green": true,
        "reproducibility_ok": true
      }
    }
  }
}
```

### Internal implementation phases (single 03-6 lock — not separate protocol checkpoints)

```
03-6a  gate_status_golden.json + benchmark_env_gates_green.jsonl + R5–R7  (protocol)
03-6b  R3 threat parity + R4 prose guard                               (protocol)
03-6c  R8 / R9 orchestration tests (tempfile isolation)                 (engineering)
03-6d  CTest entries in tests/CMakeLists.txt                            (engineering)
```

#### 03-6a — Gate golden + R5–R7

**Checkpoint:** `gate_status_golden.json` drives deterministic gate assertions via `analyze_longitudinal()`.

#### 03-6b — R3 + R4

**Checkpoint:** Threat parity and static prose guard green.

#### 03-6c — R8 + R9

**Checkpoint:** Orchestration smoke with tempfile isolation; R8 empty `plot_dir`; R9 required artifacts always.

#### 03-6d — CTest

**Checkpoint:** `ctest -R c6-longitudinal` passes after configure.

### Files (03-6)

| File | Phase |
|------|-------|
| `scripts/test_c6_longitudinal_reporting.py` | 03-6a–c |
| `tests/fixtures/cognitive_longitudinal/gate_status_golden.json` | 03-6a |
| `tests/fixtures/cognitive_longitudinal/benchmark_env_gates_green.jsonl` | 03-6a |
| `tests/fixtures/cognitive_longitudinal/metrics_gates_green.jsonl` | 03-6a |
| `tests/CMakeLists.txt` | 03-6d |

**Not touched:** `analyze_cognitive_longitudinal.py`, `c6_longitudinal_report.py` (unless R3 exposes bug), `analyzer_golden_summary.json`, `analyzer_version`, observational harness markdown block (outside R1–R9).

### Fixture discipline (frozen)

| Rule | Requirement |
|------|-------------|
| Golden / expected outputs | Successful test runs **must not modify** `report_header_*.md`, `gate_status_golden.json`, `analyzer_golden_summary.json`, or other expected-output files |
| Fixture inputs | All regression tests **read-only** with respect to fixture inputs |
| Maintainer writes | `--write-goldens` (or equivalent) remains **explicit opt-in** only — never default test path |

### Explicitly deferred (not 03-6)

| Item | Checkpoint |
|------|------------|
| `analyzer_version` → `0.2.0` | **03-7** |
| Final golden refresh / C6.3-03 log seal | **03-7** |
| Observational harness block in § Safety markdown | Future / RC v1.1+ |
| C6.3-05 ops documentation | **C6.3-05** ✅ |
| CTest for plots / threats / provenance | Out of scope (RC lists three CTests) |
| Unified `scripts/run_c6_regression.py` | Future improvement (non-normative) |

### Future improvement note (non-normative)

A future regression runner (e.g. `scripts/run_c6_regression.py`) may invoke the seven Python harnesses in one command and optionally wrap `ctest -R c6-longitudinal`. **Explicitly outside** Step 03-6 scope.

### 03-6 stop criteria

1. R1–R9 green in `test_c6_longitudinal_reporting.py`  
2. Implementation invariant honored — no analyzer/reporting semantic changes  
3. R8: empty temp `plot_dir`; summary patch; JSONL analytically immutable  
4. R9: three required artifacts; PNGs optional per matplotlib  
5. CTest: `ctest -R c6-longitudinal` passes  
6. Seven-suite script regression gate green  
7. Test runs do not modify golden / expected-output files  
8. Fixture inputs read-only in normal test execution  

**Regression gate:**

```bash
python3 scripts/test_c6_longitudinal_reporting.py
python3 scripts/test_c6_longitudinal_analyzer.py
python3 scripts/test_c6_longitudinal_plots.py
python3 scripts/test_c6_longitudinal_threats.py
python3 scripts/test_c6_longitudinal_provenance.py
python3 scripts/test_c6_longitudinal_join.py
```

**Next:** **03-7** — implement per § Step 03-7 implementation lock.

---

## Step 03-7 implementation lock 🔒 (2026-07-11)

**Status:** ✅ **Implemented** 2026-07-11 · single atomic checkpoint (internal phases 03-7a–d are execution order only — **not** separate locks)

### Implementation invariant (normative)

> **Step 03-7 SHALL NOT change analyzer semantics, reporting semantics, gate logic, threat detection, plot behavior, or fixture inputs.** Only the version constant, version-bearing goldens, and documentation may change.

If golden refresh reveals **any** diff beyond `analyzer_version` (and report-header lines derived from it), implementation **SHALL stop** and diagnose before proceeding.

`metric_schema_version` remains **`1.0`** (unchanged — see § Version bump).

### Objective

Seal **C6.3-03** (reporting layer close-out):

1. Bump `analyzer_version` **`0.1.0` → `0.2.0`** (RC-mandated C6.3-03 ship marker).
2. Refresh version-sensitive goldens (diff-gated).
3. Sync contract/protocol doc examples and status lines.
4. Record **C6.3-03 complete** in `completed_improvements_log.md`.

This is a **version + documentation checkpoint**, not new functionality.

### Version bump (frozen)

| Field | Before | After |
|-------|--------|-------|
| `analyzer_version` | `0.1.0` | **`0.2.0`** |
| `metric_schema_version` | `1.0` | **`1.0`** (unchanged) |
| `protocol_version` | `C6.3 v0.2.1` | **`C6.3 v0.2.1`** (unchanged) |

Source constant: `ANALYZER_VERSION` in `scripts/analyze_cognitive_longitudinal.py`.

Stub removal (`benchmark_regression_not_wired`, `official_gates_not_wired`, hard-coded gate stubs) was **03-3** — **not** re-opened in 03-7. G1 regression remains the guard.

### Internal implementation phases (single 03-7 lock — not separate protocol checkpoints)

```
03-7a  Bump ANALYZER_VERSION constant
03-7b  Refresh version-sensitive goldens (diff-gated)
03-7c  Sync contract/protocol doc examples + status
03-7d  Seal completed_improvements_log + final regression
```

#### 03-7a — Version constant

| File | Action |
|------|--------|
| `scripts/analyze_cognitive_longitudinal.py` | `ANALYZER_VERSION = "0.2.0"` |

No other production-code changes unless a test or doc hardcodes `"0.1.0"` for analyzer version (grep before/after).

#### 03-7b — Golden refresh (diff-gated)

| File | Process | Expected diff |
|------|---------|---------------|
| `tests/fixtures/cognitive_longitudinal/analyzer_golden_summary.json` | Run default fixture at `AS_OF_MS`; apply `normalize_payload()` (strips `analyzer_commit_hash`, `summary_written_at_ms`); compare to golden | **`analyzer_version` only** |
| `tests/fixtures/cognitive_longitudinal/report_header_golden.md` | Regenerate via `python3 scripts/test_c6_longitudinal_reporting.py --write-goldens` (after 03-7a) | **`analyzer_version` line only** |
| `tests/fixtures/cognitive_longitudinal/report_header_incomplete_golden.md` | Same `--write-goldens` pass | **`analyzer_version` line only** |

**Do not touch:** `gate_status_golden.json`, `analyzer_trend_golden.json`, `golden_join_results.json`, or any input JSONL fixtures.

**Fixture discipline:** Normal test runs **must not** write goldens; `--write-goldens` is maintainer opt-in only during 03-7b.

#### 03-7c — Documentation sync

| File | Updates |
|------|---------|
| `docs/C6_phase3_reporting_contract.md` | Mark 03-7 ✅; revision-history row |
| `docs/C6_phase3_protocol.md` | Add Step 03-7 ✅; seal C6.3-03 umbrella row; example JSON `analyzer_version` |
| `docs/C6_phase3_analyzer_contract.md` | Example JSON `analyzer_version: "0.2.0"` (doc sync — AC semantics unchanged) |

Optional light touch if already tracking C6.3-03 status: `docs/INDEX.md`, `docs/improvements.md` active-work line.

#### 03-7d — Log seal

Prepend to `docs/completed_improvements_log.md`:

**C6 Phase 3 — C6.3-03 reporting layer sealed ✅**

| Deliverable | Requirement |
|-------------|-------------|
| Steps 03-0 through 03-6 | Prior entries ✅ |
| `analyzer_version` → `0.2.0` | 03-7a |
| Golden refresh | 03-7b diff-gate passed |
| RC § Step 03 success criteria 1–7 | All satisfied |
| Seven-suite + CTest regression | Green |

Update E2 glance line: `C6.3-03 ✅ sealed` (remove “03-7 pending”).

### Files (03-7)

| File | Phase |
|------|-------|
| `scripts/analyze_cognitive_longitudinal.py` | 03-7a |
| `tests/fixtures/cognitive_longitudinal/analyzer_golden_summary.json` | 03-7b |
| `tests/fixtures/cognitive_longitudinal/report_header_golden.md` | 03-7b |
| `tests/fixtures/cognitive_longitudinal/report_header_incomplete_golden.md` | 03-7b |
| `docs/C6_phase3_reporting_contract.md` | 03-7c |
| `docs/C6_phase3_protocol.md` | 03-7c |
| `docs/C6_phase3_analyzer_contract.md` | 03-7c |
| `docs/completed_improvements_log.md` | 03-7d |

**Not touched:** `c6_longitudinal_report.py`, `c6_longitudinal_threats.py`, `plot_cognitive_longitudinal.py`, gate logic, CTest definitions, input fixtures, `gate_status_golden.json`.

### Explicitly deferred (not 03-7)

| Item | Checkpoint |
|------|------------|
| Observational harness block in § Safety markdown | RC v1.1+ / future |
| Promotion gate prose | **C6.3-04** ✅ (parent protocol § C6.3-04) |
| Ops / nightly invocation docs | **C6.3-05** ✅ → [`cognitive_longitudinal_ops.md`](cognitive_longitudinal_ops.md) |
| Fixture corpus expansion | **C6.3-06** ✅ → [`completed_improvements_log.md`](completed_improvements_log.md) |
| `scripts/run_c6_regression.py` | Future (non-normative) |
| New R10+ tests | Not required — existing goldens + suites suffice |
| `protocol_version` bump | Stays **C6.3 v0.2.1** |
| C++ / runtime changes | Forbidden |

### 03-7 stop criteria

1. `ANALYZER_VERSION == "0.2.0"` in `analyze_cognitive_longitudinal.py`  
2. Golden diff-gate passed — only `analyzer_version` (and derived header lines) changed  
3. RC § Step 03 success criteria 1–7 satisfied (including C6.3-03 sealed in `completed_improvements_log.md`)  
4. Implementation invariant honored — no semantic changes  
5. Seven-suite script regression gate green  
6. `ctest -R c6-longitudinal` passes (after `cmake --preset debug` reconfigure)  
7. Normal test runs do not modify golden / expected-output files  

**Regression gate:**

```bash
python3 scripts/test_c6_longitudinal_reporting.py
python3 scripts/test_c6_longitudinal_analyzer.py
python3 scripts/test_c6_longitudinal_plots.py
python3 scripts/test_c6_longitudinal_threats.py
python3 scripts/test_c6_longitudinal_provenance.py
python3 scripts/test_c6_longitudinal_join.py
ctest -R c6-longitudinal --test-dir build/debug --output-on-failure
```

**Next:** **C6.3-06** ✅ — fixture expansion sealed — see [`completed_improvements_log.md`](completed_improvements_log.md).

### Files (03-2 substeps)

| File | Substep |
|------|---------|
| `scripts/c6_longitudinal_threats.py` | 03-2b |
| `scripts/test_c6_longitudinal_threats.py` | 03-2b |
| `scripts/analyze_cognitive_longitudinal.py` | 03-2c |
| `tests/fixtures/cognitive_longitudinal/benchmark_env.jsonl` | 03-2b (H5) |
| `tests/fixtures/cognitive_longitudinal/prior_longitudinal.jsonl` | 03-2b (H6) |
| `tests/fixtures/cognitive_longitudinal/analyzer_golden_summary.json` | 03-2d |

**Not touched in 03-2b:** `c6_longitudinal_report.py` · gates · plots · CLI · CTest · `analyzer_version` bump.

### Explicitly out of scope (03-2 umbrella)

Gate wiring (03-3) · markdown orchestration (03-5) · R3–R9 (03-6) · `analyzer_version` 0.2.0 (03-7).

### Files (03-2 umbrella)

## Test plan (locked)

Fixture paths: `tests/fixtures/cognitive_longitudinal/` + new golden files as listed.

| ID | Name | Assert |
|----|------|--------|
| **R1** | Markdown header | Normalized whitespace; exact required header field values; disclaimer verbatim; **not** full-file raw string compare |
| **R1b** | Section coverage | `render_report_sections()` keys match expected subset of `REPORT_SECTIONS` (complete: no `incomplete_banner`; incomplete: includes `incomplete_banner`) |
| **R2** | Incomplete banner | Incomplete → banner present; complete → banner **absent** (both directions) |
| **R3** | Threat parity | Markdown threat bullets match JSON `threats_disclosed` (sorted) |
| **R4** | Prose guard | No forbidden causal phrases in template strings (static lint) |
| **R5** | Gate ABORTED | Fixture terminal `*_ABORTED` in window → `benchmark_regression: true` |
| **R6** | Gate COMPLETE | Required harnesses all `*_COMPLETE` in window → `official_gates_green: true` (when reproducibility ok) |
| **R7** | Missing evidence | Required harness absent in window → `gate_evidence_missing:*` flag; `official_gates_green: false` |
| **R8** | Plots skipped incomplete (engineering) | Incomplete orchestration run: temp `plot_dir` **empty** (no plot files); summary contains `plots_skipped:incomplete_report`; JSONL analytical `flags` unchanged (no `plots_skipped:*` in JSONL) |
| **R9** | Artifact generation (engineering) | **Required:** JSONL + summary + markdown. **Optional:** PNG plots when matplotlib available; pipeline succeeds without plotting support |

Golden files:

- `tests/fixtures/cognitive_longitudinal/report_header_golden.md` (R1 — complete)
- `tests/fixtures/cognitive_longitudinal/report_header_incomplete_golden.md` (R1, R2 — incomplete)
- `tests/fixtures/cognitive_longitudinal/gate_status_golden.json` (R5–R7)

Existing analyzer tests (A1–A9) MUST remain green after stub removal; update `analyzer_golden_summary.json` in 03-7.

No pytest; stdlib harness like `test_c6_longitudinal_join.py`.

---

## Step 03 success criteria

1. RC v1.0 locked ✅  
2. `python3 scripts/test_c6_longitudinal_reporting.py` exits `0`  
3. `python3 scripts/test_c6_longitudinal_analyzer.py` exits `0`  
4. `python3 scripts/test_c6_longitudinal_join.py` exits `0`  
5. Default analyzer invocation produces all **four** protocol artifacts  
6. `benchmark_regression` and `official_gates_green` computed per § Safety gate interpretation — not stubbed  
7. C6.3-03 sealed in `completed_improvements_log.md`

---

## Explicitly out of scope (C6.3-03)

| Item | Checkpoint |
|------|------------|
| Promotion gate prose in `improvements.md` | C6.3-04 ✅ |
| Ops / nightly invocation docs | C6.3-05 ✅ |
| Full fixture expansion | C6.3-06 ✅ |
| Raw-metrics overlay plots | RC v1.1+ |
| Runtime / C++ changes | Forbidden |
| Join semantic changes | JC v1.0 only |
| Analyzer window/stat changes | AC v1.0 only |

---

## Revision history

| Version | Date | Change |
|---------|------|--------|
| RC v1.0 | 2026-07-10 | C6.3-03 Step 03-0 — reporting contract locked |
| RC v1.0 errata | 2026-07-10 | Step 03-1 lock notes: `REPORT_SECTIONS`, `THREAT_SCHEMA_VERSION`, R1b section-coverage test |
| Step 03-1 lock | 2026-07-10 | Renderer plan locked — § Step 03-1 implementation lock |
| Step 03-1 impl | 2026-07-10 | `c6_longitudinal_report.py` + R1/R1b/R2 + header goldens |
| Step 03-2 umbrella | 2026-07-10 | Threat detection substeps 03-2a–d; `runtime_environment_drift` rename; `THREAT_SCHEMA_VERSION` 1.0 normative |
| Step 03-2a lock | 2026-07-10 | § Provenance fields; composite `provenance_pin`; H5 fixture spec |
| Step 03-2a impl | 2026-07-10 | `c6_longitudinal_provenance.py` + P1–P7 tests |
| Step 03-2b lock | 2026-07-10 | § Step 03-2b — frozen `ThreatInputs`, `THREAT_IDS`, H1–H9, purity firewall |
| Step 03-2b impl | 2026-07-10 | `c6_longitudinal_threats.py` + H1–H9 + fixtures |
| Step 03-2c lock | 2026-07-10 | § Step 03-2c — `_build_threat_inputs`, prior read-before-write, I1–I5, golden deferred 03-2d |
| Step 03-2c impl | 2026-07-10 | Analyzer wired to `detect_threats()`; I1–I5 green |
| Step 03-2d lock | 2026-07-10 | § Step 03-2d — golden diff gate, H10 full compare, 03-2 umbrella close |
| Step 03-2d impl | 2026-07-10 | Golden `threats_disclosed` updated; H10 green; 03-2 umbrella sealed |
| Step 03-3 lock | 2026-07-10 | § Step 03-3 — safety gates atomic lock; substeps 03-3a–d; boundary + interpretation locks |
| Step 03-3 impl | 2026-07-10 | `evaluate_safety_gates()` + gate threats; G1–G5, H10–H11; golden flags/threats refresh |
| Step 03-4 lock | 2026-07-10 | § Step 03-4 — plot module atomic lock; payload-only boundary; skip rules; P1–P8; empty-segments annotation |
| Step 03-4 impl | 2026-07-10 | `plot_cognitive_longitudinal.py` + P1–P8; golden summary read-only |
| Step 03-5 lock | 2026-07-10 | § Step 03-5 — orchestration atomic lock; JSONL immutability; two-phase summary; O1–O6 |
| Step 03-5 impl | 2026-07-10 | `write_outputs()` orchestration + CLI flags; O1–O6 green |
| Step 03-6 lock | 2026-07-11 | § Step 03-6 — validation/CI atomic lock; protocol vs engineering layers; R8/R9 strengthened; fixture read-only discipline |
| Step 03-6 impl | 2026-07-11 | R3–R9 in `test_c6_longitudinal_reporting.py`; gate golden fixtures; CTest `c6-longitudinal-*`; seven-suite gate green |
| Step 03-7 lock | 2026-07-11 | § Step 03-7 — close-out atomic lock; version bump + diff-gated golden refresh; C6.3-03 seal; semantic-change invariant |
| Step 03-7 impl | 2026-07-11 | `analyzer_version` 0.2.0; golden refresh; C6.3-03 sealed in log; seven-suite + CTest green |
| C6.3-04 lock | 2026-07-11 | Parent protocol § C6.3-04 — F-series promotion policy; mandatory vs supporting; `improvements.md` ownership |
| C6.3-04 impl | 2026-07-11 | Normative policy in `improvements.md`; `cursor_list.md` pointers; §9 open question resolved |
| C6.3-05 impl | 2026-07-11 | `cognitive_longitudinal_ops.md` operator guide; protocol doc map |

---

**Contract lock:** RC v1.0 is locked with C6.3 v0.2.1. Amend only via explicit protocol + contract revision (RC v1.1+).
