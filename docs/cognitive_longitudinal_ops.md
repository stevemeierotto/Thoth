# Cognitive Longitudinal Analysis — Operator Guide

**Checkpoint:** C6.3-05  
**Status:** Operator procedures (not a normative specification)

This guide references the [Analyzer Contract](C6_phase3_analyzer_contract.md) (AC v1.0) and [Reporting Contract](C6_phase3_reporting_contract.md) (RC v1.0) for implementation details. It intentionally does not duplicate implementation semantics.

> **Invariant:** This document describes supported operator workflows only. It is not a normative specification of analyzer behavior.

---

## Purpose

Answer for operators:

1. **What do I run?**
2. **When do I run it?**
3. **What should I expect?**

For constitutional context (invariants, artifact immutability, testing tiers), see [C6 Phase 3 Protocol](C6_phase3_protocol.md).

Developer regression and fixture-based tests are documented in the Analyzer Contract, Join Contract, and CTest configuration — not in this guide.

---

## Authority

| Document | Role |
|----------|------|
| **This guide** | Supported operator workflows, expectations, troubleshooting |
| [C6_phase3_analyzer_contract.md](C6_phase3_analyzer_contract.md) | CLI flags, exit codes, inputs, analytical semantics |
| [C6_phase3_reporting_contract.md](C6_phase3_reporting_contract.md) | Report/plot artifacts, skip rules, safety interpretation |
| [C6_phase3_join_contract.md](C6_phase3_join_contract.md) | Optional join `--smoke` pre-flight |
| [improvements.md](improvements.md) | F-series promotion policy (C6.3-04) |

Sample-size thresholds, window rules, and gate algorithms are defined in AC/RC — not restated here.

---

## Prerequisites

Before running:

1. **Working directory:** repository root (`Thoth/`).
2. **Inputs accumulating:** production JSONL logs from benchmark and goal activity (default paths below).
3. **No fixture regression required** for a production run.

### Default input locations

| Input | Default path |
|-------|--------------|
| Cognitive metrics | `logs/cognitive_metrics.jsonl` |
| App log | `agent_workspace/app_log.jsonl` |
| Decision trace | `agent_workspace/decision_trace.jsonl` |
| Benchmark environment | `logs/benchmark_env.jsonl` |

Override paths with CLI flags — see AC § CLI.

`tests/fixtures/cognitive_longitudinal/` is **synthetic only** (development); never use as production defaults.

---

## Manual invocation

**Default (recommended):**

```bash
python3 scripts/analyze_cognitive_longitudinal.py
```

**Common variants:**

| Flag | Operator effect |
|------|-----------------|
| `--dry-run` | Print summary JSON to stdout; **no files written** |
| `--no-report` | Skip markdown report; JSON artifacts still written |
| `--no-plots` | Skip PNG generation |

Full flag list and semantics: AC § CLI · RC § Orchestration.

---

## Example scheduling patterns

Organizations may choose different cadences (nightly, weekly, after benchmark campaigns, etc.). The protocol requires only that **official evaluation windows be documented** when used for claims. **This guide does not prescribe a single schedule.**

**Example only** — weekly cron after benchmark activity (adjust paths and schedule to your environment):

```cron
# Example: Sunday 06:00 UTC, after logs have accumulated
0 6 * * 0 cd /path/to/Thoth && python3 scripts/analyze_cognitive_longitudinal.py
```

Longitudinal analysis is **observational** and **non-gating** — failures or incomplete reports inform investigation; they do not block merges or deployments by themselves.

---

## Operator workflow

1. **Pre-flight (optional)** — Join health check (non-gating):

   ```bash
   python3 scripts/c6_longitudinal_join.py --smoke
   ```

   See JC § Step 4 for smoke semantics.

2. **Run analysis** — Default command in § Manual invocation.

3. **Review outputs** — See § Expected successful run and § Output locations.

**If promotion is being considered:** follow [improvements.md](improvements.md) § **C6 Phase 3 — F-series promotion gate (C6.3-04)**. This guide does not duplicate promotion gates.

---

## Expected successful run

| Check | Expectation |
|-------|-------------|
| Exit code | `0` |
| Summary JSON | `logs/cognitive_longitudinal_summary.json` updated (`generated_at_ms` reflects this run) |
| Report | `logs/cognitive_longitudinal_report.md` written (unless `--no-report`) |
| Plots | PNGs under `logs/plots/longitudinal/` when plotting is enabled and the report is complete (see RC if absent) |
| JSONL history | New row appended to `logs/cognitive_longitudinal.jsonl` (unless `--dry-run`) |

**Note:** Exit code `0` with `report_completeness: incomplete` is possible. That is not a successful **official** window for promotion or official longitudinal claims — see RC incomplete banner.

---

## Output locations

| Artifact | Default path |
|----------|--------------|
| Latest summary | `logs/cognitive_longitudinal_summary.json` |
| Human report | `logs/cognitive_longitudinal_report.md` |
| Append-only history | `logs/cognitive_longitudinal.jsonl` |
| Plots | `logs/plots/longitudinal/` |

Field definitions and write semantics: Reporting Contract · Analyzer Contract.

---

## Troubleshooting

| Problem | Likely cause | Reference |
|---------|--------------|-----------|
| Exit code `2` | Missing or unreadable metrics artifact | AC § CLI / exit codes |
| Exit code `1` | Invalid CLI arguments | AC § CLI |
| Incomplete report banner in markdown | Missing required authoritative inputs | RC § Incomplete banner |
| `gate_evidence_missing:*` in summary `flags` | Required benchmark harness absent in window | RC § Safety gate interpretation |
| No PNG plots | `--no-plots`, matplotlib unavailable, or incomplete report | RC § Plot skip rules |
| `evidence_scope: exploratory_only` | Official longitudinal scope gates not met | AC § `evidence_scope` |
| `benchmark_regression: true` | Official `*_ABORTED` in evaluation window | RC § Safety gates |

---

## Governance notes

- **Observational only** — longitudinal analysis must not influence execution (protocol Primary Invariant).
- **Not a CI merge gate** — production runs are for operators and evidence review, not PR blocking.
- **Incomplete reports** — not valid for F-series promotion (C6.3-04) or official longitudinal claims.
- **C6.3-05 scope** — documentation only; no analyzer, schema, or automation changes.
