#!/usr/bin/env python3
"""
analyze_cognitive_longitudinal.py — C6.3-02 longitudinal analyzer (AC v1.0).

Read-only analysis over joined cognitive metrics. Contract: docs/C6_phase3_analyzer_contract.md
"""

from __future__ import annotations

import argparse
import json
import math
import statistics
import subprocess
import sys
import time
from collections.abc import Mapping, Sequence
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Literal

import c6_longitudinal_join as join
import c6_longitudinal_report as report
import c6_longitudinal_threats as threats
import plot_cognitive_longitudinal as longitudinal_plots

PROTOCOL_VERSION = "C6.3 v0.2.1"
METRIC_SCHEMA_VERSION = "1.0"
ANALYZER_VERSION = "0.2.0"
CONTRACT_VERSION = "AC v1.0"

MIN_GOALS_LONGITUDINAL = 30
MIN_GOALS_PER_COHORT = 10
DEFAULT_WINDOW_DAYS = 28
TREND_MIN_ABS_DELTA = 0.05
CONFIDENCE_LEVEL = 0.95
OFFICIAL_TIER = "full"
MS_PER_DAY = 86_400_000
WILSON_Z = 1.96

OFFICIAL_TERMINAL_EVENTS = frozenset(
    {
        "TEST_SUITE_COMPLETE",
        "TEST_SUITE_ABORTED",
        "REFLECTION_AB_COMPLETE",
        "REFLECTION_AB_ABORTED",
        "ROBUSTNESS_COMPLETE",
        "ROBUSTNESS_ABORTED",
        "CHAT_RAG_BENCHMARK_COMPLETE",
        "CHAT_RAG_BENCHMARK_ABORTED",
        "GRAG_BENCHMARK_COMPLETE",
        "GRAG_BENCHMARK_ABORTED",
        "EPISODIC_LEARNING_COMPLETE",
        "EPISODIC_LEARNING_ABORTED",
    }
)

ELIGIBLE_OUTCOMES = frozenset({"completed", "failed"})

REQUIRED_GATE_HARNESSES: tuple[tuple[str, str, str], ...] = (
    ("reflection_ab", "REFLECTION_AB_COMPLETE", "REFLECTION_AB_ABORTED"),
    ("robustness", "ROBUSTNESS_COMPLETE", "ROBUSTNESS_ABORTED"),
    ("episodic_learning", "EPISODIC_LEARNING_COMPLETE", "EPISODIC_LEARNING_ABORTED"),
)


@dataclass
class AnalysisGoalRecord:
    plan_id: str
    session_id: str
    goal_started_at_ms: int
    benchmark_run_id: str
    outcome: str
    join_record: join.JoinedGoalRecord
    metrics: dict[str, Any]
    official_eligible: bool = False
    exclusion_reasons: list[str] = field(default_factory=list)


@dataclass(frozen=True)
class WindowBounds:
    start_ms: int
    end_ms: int
    days: int
    anchor: str
    used_official_anchor: bool


@dataclass(frozen=True)
class SuccessRateStats:
    goal_count: int
    eligible_count: int
    success_count: int
    success_rate: float
    ci_low: float
    ci_high: float
    variance: float
    confidence_label: str


def repo_root() -> Path:
    return Path(__file__).resolve().parent.parent


def production_default_paths() -> dict[str, Path]:
    root = repo_root()
    return {
        "metrics": root / "logs" / "cognitive_metrics.jsonl",
        "app_log": root / "agent_workspace" / "app_log.jsonl",
        "decision_trace": root / "agent_workspace" / "decision_trace.jsonl",
        "benchmark_env": root / "logs" / "benchmark_env.jsonl",
    }


@dataclass(frozen=True)
class WriteOptions:
    write_report: bool = True
    write_plots: bool = True
    plot_dir: Path | None = None


def default_plot_dir() -> Path:
    return repo_root() / "logs" / "plots" / "longitudinal"


def default_output_paths(output_dir: Path) -> dict[str, Path]:
    return {
        "jsonl": output_dir / "cognitive_longitudinal.jsonl",
        "summary": output_dir / "cognitive_longitudinal_summary.json",
        "report": output_dir / "cognitive_longitudinal_report.md",
    }


def display_path(path: Path) -> str:
    resolved = path.resolve()
    root = repo_root().resolve()
    try:
        return str(resolved.relative_to(root))
    except ValueError:
        return str(resolved)


def git_commit_hash() -> str:
    try:
        result = subprocess.run(
            ["git", "rev-parse", "--short", "HEAD"],
            cwd=repo_root(),
            capture_output=True,
            text=True,
            check=True,
            timeout=5,
        )
        return result.stdout.strip() or "unknown"
    except (OSError, subprocess.SubprocessError):
        return "unknown"


def normalize_tier(value: Any) -> str | None:
    if not isinstance(value, str) or not value.strip():
        return None
    return value.strip().lower()


def wilson_ci(successes: int, n: int, z: float = WILSON_Z) -> tuple[float, float, float]:
    if n <= 0:
        return 0.0, 0.0, 0.0
    p = successes / n
    z2 = z * z
    denom = 1.0 + z2 / n
    center = (p + z2 / (2.0 * n)) / denom
    margin = z * math.sqrt((p * (1.0 - p) / n + z2 / (4.0 * n * n)) / denom)
    return p, max(0.0, center - margin), min(1.0, center + margin)


def confidence_label_for(n: int, ci_low: float, ci_high: float) -> str:
    width = ci_high - ci_low
    if n >= 30 and width <= 0.20:
        return "high"
    if n >= 10 and width <= 0.35:
        return "medium"
    return "low"


def prior_ci_contains(current_rate: float, prior_ci_low: float, prior_ci_high: float) -> bool:
    return prior_ci_low <= current_rate <= prior_ci_high


def directional_trend_label(
    current_rate: float,
    prior_rate: float,
    prior_ci_low: float,
    prior_ci_high: float,
    confidence: str,
) -> Literal["improving", "declining", "stable"]:
    delta = current_rate - prior_rate
    if confidence == "low":
        return "stable"
    if abs(delta) < TREND_MIN_ABS_DELTA:
        return "stable"
    if prior_ci_contains(current_rate, prior_ci_low, prior_ci_high):
        return "stable"
    if delta >= TREND_MIN_ABS_DELTA:
        return "improving"
    if delta <= -TREND_MIN_ABS_DELTA:
        return "declining"
    return "stable"


def percentile(values: list[float], pct: float) -> float:
    if not values:
        return 0.0
    ordered = sorted(values)
    idx = int(round((pct / 100.0) * (len(ordered) - 1)))
    return float(ordered[max(0, min(idx, len(ordered) - 1))])


def success_rate_stats(goals: list[AnalysisGoalRecord]) -> SuccessRateStats:
    eligible = [goal for goal in goals if goal.outcome in ELIGIBLE_OUTCOMES]
    successes = sum(1 for goal in eligible if goal.outcome == "completed")
    n = len(eligible)
    rate, ci_low, ci_high = wilson_ci(successes, n)
    variance = rate * (1.0 - rate) / n if n > 0 else 0.0
    return SuccessRateStats(
        goal_count=len(goals),
        eligible_count=n,
        success_count=successes,
        success_rate=rate,
        ci_low=ci_low,
        ci_high=ci_high,
        variance=variance,
        confidence_label=confidence_label_for(n, ci_low, ci_high),
    )


def merge_analysis_goals(
    records: list[join.JoinedGoalRecord],
    validated_metrics: list[dict[str, Any]],
    *,
    cohort: str,
) -> list[AnalysisGoalRecord]:
    metrics_by_plan = {str(row["plan_id"]): row for row in validated_metrics if row.get("plan_id")}
    merged: list[AnalysisGoalRecord] = []

    for record in records:
        metrics_row = metrics_by_plan.get(record.plan_id)
        if metrics_row is None:
            continue
        outcome = metrics_row.get("outcome")
        if not isinstance(outcome, str):
            outcome = "unknown"

        exclusion_reasons: list[str] = []
        official_eligible = True

        env_status = record.environment.join_status
        tier = normalize_tier(record.environment.tier)

        if env_status == join.JoinStatus.FINGERPRINT_MISMATCH:
            official_eligible = False
            exclusion_reasons.append("fingerprint_mismatch")
        elif env_status != join.JoinStatus.RESOLVED:
            official_eligible = False
            exclusion_reasons.append(f"environment_{env_status.value.lower()}")
        elif tier != OFFICIAL_TIER:
            official_eligible = False
            exclusion_reasons.append("tier_not_full")

        if cohort == "official" and not official_eligible:
            continue

        merged.append(
            AnalysisGoalRecord(
                plan_id=record.plan_id,
                session_id=record.session_id,
                goal_started_at_ms=record.goal_started_at_ms,
                benchmark_run_id=record.benchmark_run_id,
                outcome=outcome,
                join_record=record,
                metrics=metrics_row,
                official_eligible=official_eligible,
                exclusion_reasons=exclusion_reasons,
            )
        )

    return merged


def index_benchmark_env(rows: list[dict[str, Any]]) -> dict[str, dict[str, Any]]:
    indexed: dict[str, dict[str, Any]] = {}
    for row in rows:
        if row.get("event") == "BENCHMARK_ENV":
            run_id = row.get("run_id")
            if isinstance(run_id, str) and run_id:
                indexed[run_id] = row
    return indexed


def terminal_sort_key(row: dict[str, Any]) -> tuple[int, int, str]:
    event = str(row.get("event", ""))
    ts = row.get("ts")
    ts_val = int(ts) if isinstance(ts, (int, float)) and not isinstance(ts, bool) else 0
    complete_rank = 0 if event.endswith("_COMPLETE") else 1
    return (-ts_val, complete_rank, event)


def resolve_window_anchor(
    env_rows: list[dict[str, Any]],
    generated_at_ms: int,
) -> tuple[int, bool, list[str]]:
    env_by_run = index_benchmark_env(env_rows)
    flags: list[str] = []
    official_terminals: list[dict[str, Any]] = []

    for row in env_rows:
        event = row.get("event")
        if not isinstance(event, str) or event not in OFFICIAL_TERMINAL_EVENTS:
            continue
        run_id = row.get("run_id")
        if not isinstance(run_id, str) or not run_id:
            continue
        env_row = env_by_run.get(run_id)
        if env_row is None:
            continue
        env_obj = env_row.get("env") if isinstance(env_row.get("env"), dict) else {}
        runtime = env_obj.get("runtime") if isinstance(env_obj.get("runtime"), dict) else {}
        tier = normalize_tier(runtime.get("tier"))
        if tier != OFFICIAL_TIER:
            continue
        official_terminals.append(row)

    if not official_terminals:
        flags.append("no_official_anchor")
        return generated_at_ms, False, flags

    winner = sorted(official_terminals, key=terminal_sort_key)[0]
    ts = winner.get("ts")
    end_ms = int(ts) if isinstance(ts, (int, float)) and not isinstance(ts, bool) else generated_at_ms
    return end_ms, True, flags


def window_bounds(end_ms: int, window_days: int, used_official_anchor: bool) -> WindowBounds:
    span = window_days * MS_PER_DAY
    anchor = "official_benchmark_execution" if used_official_anchor else "generated_at_ms"
    return WindowBounds(
        start_ms=end_ms - span,
        end_ms=end_ms,
        days=window_days,
        anchor=anchor,
        used_official_anchor=used_official_anchor,
    )


def filter_goals_in_window(goals: list[AnalysisGoalRecord], bounds: WindowBounds) -> list[AnalysisGoalRecord]:
    return [
        goal
        for goal in goals
        if bounds.start_ms <= goal.goal_started_at_ms <= bounds.end_ms
    ]


def prior_window_bounds(current: WindowBounds) -> WindowBounds:
    span = current.days * MS_PER_DAY
    return WindowBounds(
        start_ms=current.end_ms - 2 * span,
        end_ms=current.end_ms - span,
        days=current.days,
        anchor=current.anchor,
        used_official_anchor=current.used_official_anchor,
    )


def numeric_metric(goals: list[AnalysisGoalRecord], field_name: str) -> list[float]:
    values: list[float] = []
    for goal in goals:
        value = goal.metrics.get(field_name)
        if isinstance(value, bool):
            continue
        if isinstance(value, (int, float)):
            values.append(float(value))
    return values


def efficiency_summary(goals: list[AnalysisGoalRecord]) -> dict[str, Any]:
    wall = numeric_metric(goals, "total_wall_clock_ms")
    tokens = numeric_metric(goals, "total_tokens")
    reflections = numeric_metric(goals, "reflection_count")
    revisions = numeric_metric(goals, "revisions_count")
    return {
        "total_wall_clock_ms_p50": percentile(wall, 50),
        "total_wall_clock_ms_p95": percentile(wall, 95),
        "total_tokens_p50": percentile(tokens, 50),
        "reflection_count_mean": statistics.fmean(reflections) if reflections else 0.0,
        "revisions_count_mean": statistics.fmean(revisions) if revisions else 0.0,
    }


def memory_summary(goals: list[AnalysisGoalRecord]) -> dict[str, Any]:
    chunks = numeric_metric(goals, "retrieved_chunk_count")
    alphas = numeric_metric(goals, "grag_alpha")
    post = [g for g in goals if g.join_record.consolidation.join_status == join.JoinStatus.RESOLVED and g.join_record.consolidation.post_consolidation is True]
    pre = [g for g in goals if g.join_record.consolidation.join_status == join.JoinStatus.RESOLVED and g.join_record.consolidation.post_consolidation is False]
    post_stats = success_rate_stats(post)
    pre_stats = success_rate_stats(pre)
    delta = post_stats.success_rate - pre_stats.success_rate if post and pre else 0.0
    return {
        "retrieved_chunk_count_mean": statistics.fmean(chunks) if chunks else 0.0,
        "grag_alpha_mean": statistics.fmean(alphas) if alphas else 0.0,
        "post_consolidation_success_rate_delta": delta,
    }


def segment_success_block(goals: list[AnalysisGoalRecord], prior_goals: list[AnalysisGoalRecord]) -> dict[str, Any]:
    stats = success_rate_stats(goals)
    prior_stats = success_rate_stats(prior_goals)
    trend = "stable"
    if prior_stats.eligible_count > 0 and stats.eligible_count >= MIN_GOALS_PER_COHORT:
        trend = directional_trend_label(
            stats.success_rate,
            prior_stats.success_rate,
            prior_stats.ci_low,
            prior_stats.ci_high,
            stats.confidence_label,
        )
    return {
        "goal_count": stats.goal_count,
        "eligible_count": stats.eligible_count,
        "success_rate": round(stats.success_rate, 6),
        "ci_low": round(stats.ci_low, 6),
        "ci_high": round(stats.ci_high, 6),
        "confidence_label": stats.confidence_label,
        "trend": trend,
    }


def build_segments(
    goals: list[AnalysisGoalRecord],
    prior_goals: list[AnalysisGoalRecord],
) -> dict[str, Any]:
    segments: dict[str, Any] = {}

    def add_segment(key: str, filtered: list[AnalysisGoalRecord], prior_filtered: list[AnalysisGoalRecord]) -> None:
        if not filtered:
            return
        segments[key] = segment_success_block(filtered, prior_filtered)

    plan_true = [g for g in goals if g.join_record.plan_reused.join_status == join.JoinStatus.RESOLVED and g.join_record.plan_reused.value is True]
    plan_false = [g for g in goals if g.join_record.plan_reused.join_status == join.JoinStatus.RESOLVED and g.join_record.plan_reused.value is False]
    prior_plan_true = [g for g in prior_goals if g.join_record.plan_reused.join_status == join.JoinStatus.RESOLVED and g.join_record.plan_reused.value is True]
    prior_plan_false = [g for g in prior_goals if g.join_record.plan_reused.join_status == join.JoinStatus.RESOLVED and g.join_record.plan_reused.value is False]
    add_segment("plan_reused_true", plan_true, prior_plan_true)
    add_segment("plan_reused_false", plan_false, prior_plan_false)

    strat_true = [g for g in goals if g.join_record.strategy.join_status == join.JoinStatus.RESOLVED and g.join_record.strategy.strategy_injected is True]
    strat_false = [g for g in goals if g.join_record.strategy.join_status == join.JoinStatus.RESOLVED and g.join_record.strategy.strategy_injected is False]
    prior_strat_true = [g for g in prior_goals if g.join_record.strategy.join_status == join.JoinStatus.RESOLVED and g.join_record.strategy.strategy_injected is True]
    prior_strat_false = [g for g in prior_goals if g.join_record.strategy.join_status == join.JoinStatus.RESOLVED and g.join_record.strategy.strategy_injected is False]
    add_segment("strategy_injected_true", strat_true, prior_strat_true)
    add_segment("strategy_injected_false", strat_false, prior_strat_false)

    cons_true = [g for g in goals if g.join_record.consolidation.join_status == join.JoinStatus.RESOLVED and g.join_record.consolidation.post_consolidation is True]
    cons_false = [g for g in goals if g.join_record.consolidation.join_status == join.JoinStatus.RESOLVED and g.join_record.consolidation.post_consolidation is False]
    prior_cons_true = [g for g in prior_goals if g.join_record.consolidation.join_status == join.JoinStatus.RESOLVED and g.join_record.consolidation.post_consolidation is True]
    prior_cons_false = [g for g in prior_goals if g.join_record.consolidation.join_status == join.JoinStatus.RESOLVED and g.join_record.consolidation.post_consolidation is False]
    add_segment("post_consolidation_true", cons_true, prior_cons_true)
    add_segment("post_consolidation_false", cons_false, prior_cons_false)

    return segments


@dataclass(frozen=True)
class GateEvaluation:
    benchmark_regression: bool
    official_gates_green: bool
    gate_flags: tuple[str, ...]


def _official_terminal_rows_in_window(
    env_rows: Sequence[dict[str, Any]],
    window: Mapping[str, Any],
) -> list[dict[str, Any]]:
    """Official terminals in window — same filter as resolve_window_anchor + ts bounds."""
    start_ms = window.get("start_ms")
    end_ms = window.get("end_ms")
    if not isinstance(start_ms, int) or not isinstance(end_ms, int):
        return []

    env_by_run = index_benchmark_env(list(env_rows))
    terminals: list[dict[str, Any]] = []
    for row in env_rows:
        event = row.get("event")
        if not isinstance(event, str) or event not in OFFICIAL_TERMINAL_EVENTS:
            continue
        run_id = row.get("run_id")
        if not isinstance(run_id, str) or not run_id:
            continue
        env_row = env_by_run.get(run_id)
        if env_row is None:
            continue
        env_obj = env_row.get("env") if isinstance(env_row.get("env"), dict) else {}
        runtime = env_obj.get("runtime") if isinstance(env_obj.get("runtime"), dict) else {}
        if normalize_tier(runtime.get("tier")) != OFFICIAL_TIER:
            continue
        ts = row.get("ts")
        if not isinstance(ts, (int, float)) or isinstance(ts, bool):
            continue
        ts_val = int(ts)
        if start_ms <= ts_val <= end_ms:
            terminals.append(row)
    return terminals


def evaluate_safety_gates(
    env_rows: Sequence[dict[str, Any]],
    window: Mapping[str, Any],
    *,
    reproducibility_ok: bool,
) -> GateEvaluation:
    """Gate rules from RC § Safety gate interpretation. Does not emit threat IDs."""
    terminals = _official_terminal_rows_in_window(env_rows, window)
    terminal_events = {
        row.get("event") for row in terminals if isinstance(row.get("event"), str)
    }

    benchmark_regression = any(event.endswith("_ABORTED") for event in terminal_events)

    gate_flags: list[str] = []
    all_required_complete = True
    any_required_aborted = False

    for harness_key, complete_event, aborted_event in REQUIRED_GATE_HARNESSES:
        has_complete = complete_event in terminal_events
        has_aborted = aborted_event in terminal_events
        if not has_complete and not has_aborted:
            gate_flags.append(f"gate_evidence_missing:{harness_key}")
        if not has_complete:
            all_required_complete = False
        if has_aborted:
            any_required_aborted = True

    official_gates_green = (
        not benchmark_regression
        and reproducibility_ok
        and all_required_complete
        and not any_required_aborted
        and not gate_flags
    )

    return GateEvaluation(
        benchmark_regression=benchmark_regression,
        official_gates_green=official_gates_green,
        gate_flags=tuple(sorted(gate_flags)),
    )


def compute_safety(
    all_merged: list[AnalysisGoalRecord],
    cohort_goals: list[AnalysisGoalRecord],
    flags: list[str],
    *,
    env_rows: Sequence[dict[str, Any]],
    window: Mapping[str, Any],
) -> dict[str, Any]:
    fingerprint_issues = any(
        goal.join_record.environment.join_status == join.JoinStatus.FINGERPRINT_MISMATCH for goal in all_merged
    )
    tier_contamination = any(
        goal.official_eligible is False and normalize_tier(goal.join_record.environment.tier) not in (None, OFFICIAL_TIER)
        for goal in all_merged
    ) or any("tier_not_full" in goal.exclusion_reasons for goal in all_merged)

    reproducibility_ok = not fingerprint_issues
    if not reproducibility_ok:
        flags.append("fingerprint_mismatch")

    if tier_contamination:
        flags.append("tier_contamination")

    gate_eval = evaluate_safety_gates(env_rows, window, reproducibility_ok=reproducibility_ok)
    flags.extend(gate_eval.gate_flags)

    return {
        "reproducibility_ok": reproducibility_ok,
        "tier_contamination": tier_contamination,
        "benchmark_regression": gate_eval.benchmark_regression,
        "official_gates_green": gate_eval.official_gates_green,
    }


def load_prior_longitudinal_rows(path: Path | None) -> tuple[dict[str, Any], ...] | None:
    """Read-only prior rows. Returns None if path is missing."""
    if path is None or not path.is_file():
        return None
    rows, _ = join.load_jsonl(path)
    return tuple(rows)


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
    """Single assembly path for ThreatInputs. Packages inputs only — no detection rules."""
    return threats.ThreatInputs(
        eligible_count=perf.eligible_count,
        safety=safety,
        flags=tuple(flags),
        used_official_anchor=used_official_anchor,
        report_completeness=report_completeness,
        env_rows=tuple(env_rows),
        benchmark_run_ids_consumed=tuple(benchmark_run_ids_consumed),
        validation_total_invalid=validation_total_invalid,
        window=window,
        prior_longitudinal_rows=(
            tuple(prior_longitudinal_rows) if prior_longitudinal_rows is not None else None
        ),
    )


def derive_evidence_scope(
    perf: SuccessRateStats,
    report_completeness: str,
    flags: list[str],
    used_official_anchor: bool,
    safety: dict[str, Any],
) -> str:
    if perf.eligible_count < MIN_GOALS_LONGITUDINAL:
        return "exploratory_only"
    if report_completeness != "complete":
        return "exploratory_only"
    if perf.confidence_label == "low":
        return "exploratory_only"
    if not used_official_anchor:
        return "exploratory_only"
    if safety.get("tier_contamination"):
        return "exploratory_only"
    if not safety.get("reproducibility_ok"):
        return "exploratory_only"
    if "fingerprint_mismatch" in flags:
        return "exploratory_only"
    return "official_longitudinal"


def derive_report_completeness(artifacts: join.JoinArtifacts, flags: list[str]) -> str:
    if artifacts.context.missing_artifacts:
        for name in sorted(artifacts.context.missing_artifacts):
            flags.append(f"missing_artifact:{name}")
        return "incomplete"
    if artifacts.validation.total_invalid > 0:
        flags.append("validation_rows_dropped")
        return "incomplete"
    return "complete"


@dataclass
class AnalysisResult:
    payload: dict[str, Any]
    threat_inputs: threats.ThreatInputs


def analyze_longitudinal(
    metrics_path: Path,
    app_log_path: Path,
    decision_trace_path: Path,
    benchmark_env_path: Path,
    *,
    cohort: str = "official",
    window_days: int = DEFAULT_WINDOW_DAYS,
    generated_at_ms: int | None = None,
    input_paths: dict[str, str] | None = None,
    prior_longitudinal_path: Path | None = None,
) -> AnalysisResult:
    if generated_at_ms is None:
        generated_at_ms = int(time.time() * 1000)

    records, artifacts, _ = join.join_goals_with_load_counts(
        metrics_path,
        app_log_path,
        decision_trace_path,
        benchmark_env_path,
        verbose_issues=False,
    )

    env_rows, _ = join.load_jsonl(benchmark_env_path)
    end_ms, used_official_anchor, anchor_flags = resolve_window_anchor(env_rows, generated_at_ms)

    flags: list[str] = list(anchor_flags)
    warnings: list[str] = []
    report_completeness = derive_report_completeness(artifacts, flags)

    all_merged = merge_analysis_goals(records, artifacts.validated_metrics, cohort="exploratory")
    cohort_goals = merge_analysis_goals(records, artifacts.validated_metrics, cohort=cohort)

    current_bounds = window_bounds(end_ms, window_days, used_official_anchor)
    prior_bounds = prior_window_bounds(current_bounds)
    window_payload = {
        "start_ms": current_bounds.start_ms,
        "end_ms": current_bounds.end_ms,
        "days": current_bounds.days,
        "anchor": current_bounds.anchor,
    }

    current_goals = filter_goals_in_window(cohort_goals, current_bounds)
    prior_goals = filter_goals_in_window(cohort_goals, prior_bounds)

    perf = success_rate_stats(current_goals)
    prior_perf = success_rate_stats(prior_goals)
    performance_trend = "stable"
    if prior_perf.eligible_count > 0:
        performance_trend = directional_trend_label(
            perf.success_rate,
            prior_perf.success_rate,
            prior_perf.ci_low,
            prior_perf.ci_high,
            perf.confidence_label,
        )

    safety = compute_safety(
        all_merged,
        cohort_goals,
        flags,
        env_rows=env_rows,
        window=window_payload,
    )

    benchmark_run_ids = sorted({goal.benchmark_run_id for goal in current_goals if goal.benchmark_run_id})
    prior_rows = load_prior_longitudinal_rows(prior_longitudinal_path)

    threat_inputs = _build_threat_inputs(
        perf=perf,
        safety=safety,
        flags=flags,
        used_official_anchor=used_official_anchor,
        report_completeness=report_completeness,
        env_rows=env_rows,
        benchmark_run_ids_consumed=benchmark_run_ids,
        validation_total_invalid=artifacts.validation.total_invalid,
        window=window_payload,
        prior_longitudinal_rows=prior_rows,
    )
    threats_disclosed = threats.detect_threats(threat_inputs)
    evidence_scope = derive_evidence_scope(perf, report_completeness, flags, used_official_anchor, safety)

    if input_paths is None:
        input_paths = {
            "metrics": display_path(metrics_path),
            "app_log": display_path(app_log_path),
            "decision_trace": display_path(decision_trace_path),
            "benchmark_env": display_path(benchmark_env_path),
        }

    payload: dict[str, Any] = {
        "event": "COGNITIVE_LONGITUDINAL_SUMMARY",
        "protocol_version": PROTOCOL_VERSION,
        "metric_schema_version": METRIC_SCHEMA_VERSION,
        "analyzer_version": ANALYZER_VERSION,
        "analyzer_commit_hash": git_commit_hash(),
        "generated_at_ms": generated_at_ms,
        "report_completeness": report_completeness,
        "evidence_scope": evidence_scope,
        "confidence_label": perf.confidence_label,
        "window": window_payload,
        "reproducibility": {
            "input_log_path": input_paths["metrics"],
            "input_artifact_paths": [
                input_paths["metrics"],
                input_paths["app_log"],
                input_paths["decision_trace"],
                input_paths["benchmark_env"],
            ],
            "benchmark_run_ids_consumed": benchmark_run_ids,
        },
        "categories": {
            "performance": {
                "success_rate": round(perf.success_rate, 6),
                "ci_low": round(perf.ci_low, 6),
                "ci_high": round(perf.ci_high, 6),
                "variance": round(perf.variance, 6),
                "trend": performance_trend,
                "eligible_count": perf.eligible_count,
            },
            "efficiency": efficiency_summary(current_goals),
            "memory": memory_summary(current_goals),
            "planning": {},
            "safety": safety,
        },
        "segments": build_segments(current_goals, prior_goals),
        "threats_disclosed": threats_disclosed,
        "flags": sorted(set(flags)),
        "warnings": warnings,
    }

    return AnalysisResult(payload=payload, threat_inputs=threat_inputs)


def _write_summary_json(path: Path, summary_payload: dict[str, Any]) -> None:
    with path.open("w", encoding="utf-8") as handle:
        json.dump(summary_payload, handle, indent=2, sort_keys=True)
        handle.write("\n")


def _merge_sorted_flags(existing: Sequence[str], extra: Sequence[str]) -> list[str]:
    merged = set(existing)
    merged.update(extra)
    return sorted(merged)


def write_outputs(
    payload: dict[str, Any],
    output_dir: Path,
    *,
    summary_written_at_ms: int | None = None,
    options: WriteOptions | None = None,
) -> None:
    """
    Orchestrate JSONL append, summary write (two-phase), markdown, and plots.

    Phase 1 writes the analytical summary. Phase 2 merges plots_skipped:* into
    summary flags only when the plot module returns flags. JSONL is never patched.
    """
    opts = options if options is not None else WriteOptions(write_report=False, write_plots=False)
    paths = default_output_paths(output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    with paths["jsonl"].open("a", encoding="utf-8") as handle:
        handle.write(json.dumps(payload, sort_keys=True, separators=(",", ":")) + "\n")

    summary_payload = dict(payload)
    if summary_written_at_ms is not None:
        summary_payload["summary_written_at_ms"] = summary_written_at_ms
    _write_summary_json(paths["summary"], summary_payload)

    if opts.write_report:
        markdown = report.render_markdown_report(payload)
        paths["report"].write_text(markdown, encoding="utf-8")

    plot_flags: tuple[str, ...] = ()
    if opts.write_plots:
        plot_dir = opts.plot_dir or default_plot_dir()
        plot_result = longitudinal_plots.write_longitudinal_plots(payload, plot_dir)
        plot_flags = plot_result.flags

    if plot_flags:
        patched = dict(summary_payload)
        patched["flags"] = _merge_sorted_flags(list(summary_payload.get("flags", [])), plot_flags)
        _write_summary_json(paths["summary"], patched)


def build_arg_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="C6.3 longitudinal analyzer (AC v1.0)")
    parser.add_argument("--metrics", type=Path, default=None)
    parser.add_argument("--app-log", type=Path, default=None)
    parser.add_argument("--decision-trace", type=Path, default=None)
    parser.add_argument("--benchmark-env", type=Path, default=None)
    parser.add_argument("--output-dir", type=Path, default=None, help="Default: logs/")
    parser.add_argument("--cohort", choices=("official", "exploratory"), default="official")
    parser.add_argument("--window-days", type=int, default=DEFAULT_WINDOW_DAYS)
    parser.add_argument("--as-of-ms", type=int, default=None, help="Pin analysis time (tests)")
    parser.add_argument("--dry-run", action="store_true", help="Print summary JSON only")
    parser.add_argument("--no-report", action="store_true", help="Skip markdown; JSON artifacts still written")
    parser.add_argument("--no-plots", action="store_true", help="Skip PNG generation")
    parser.add_argument("--verbose", action="store_true")
    return parser


def resolve_paths(args: argparse.Namespace) -> tuple[Path, Path, Path, Path, Path]:
    defaults = production_default_paths()
    metrics = args.metrics or defaults["metrics"]
    app_log = args.app_log or defaults["app_log"]
    decision_trace = args.decision_trace or defaults["decision_trace"]
    benchmark_env = args.benchmark_env or defaults["benchmark_env"]
    output_dir = args.output_dir or (repo_root() / "logs")
    return metrics, app_log, decision_trace, benchmark_env, output_dir


def main(argv: list[str] | None = None) -> int:
    parser = build_arg_parser()
    args = parser.parse_args(argv)

    metrics_path, app_log_path, decision_trace_path, benchmark_env_path, output_dir = resolve_paths(args)

    if not metrics_path.is_file():
        print(f"metrics artifact not found: {metrics_path}", file=sys.stderr)
        return 2

    prior_path = output_dir / "cognitive_longitudinal.jsonl"
    prior_longitudinal_path = prior_path if prior_path.is_file() else None

    try:
        result = analyze_longitudinal(
            metrics_path,
            app_log_path,
            decision_trace_path,
            benchmark_env_path,
            cohort=args.cohort,
            window_days=args.window_days,
            generated_at_ms=args.as_of_ms,
            prior_longitudinal_path=prior_longitudinal_path,
        )
    except FileNotFoundError as exc:
        print(str(exc), file=sys.stderr)
        return 2

    payload = result.payload

    if args.verbose:
        print(
            f"C6 longitudinal analyze ({CONTRACT_VERSION}) goals_in_window="
            f"{payload['categories']['performance'].get('eligible_count', 0)}",
            file=sys.stderr,
        )

    if args.dry_run:
        print(json.dumps(payload, indent=2, sort_keys=True))
        return 0

    write_outputs(
        payload,
        output_dir,
        summary_written_at_ms=int(time.time() * 1000),
        options=WriteOptions(
            write_report=not args.no_report,
            write_plots=not args.no_plots,
        ),
    )
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except SystemExit:
        raise
    except Exception as exc:
        print(f"error: {exc}", file=sys.stderr)
        raise SystemExit(1) from exc
