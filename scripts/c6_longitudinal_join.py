#!/usr/bin/env python3
"""
c6_longitudinal_join.py — C6.3-01 join library (JC v1.0).

Read-only joins for GOAL_COGNITIVE_METRICS rows against authoritative benchmark artifacts.
Contract: docs/C6_phase3_join_contract.md
"""

from __future__ import annotations

import argparse
import json
import sys
from collections import Counter
from dataclasses import dataclass, field
from enum import StrEnum
from pathlib import Path
from typing import Any

# Parent protocol — docs/C6_phase3_protocol.md
MAX_STRATEGY_JOIN_GAP_MS = 300_000
CONTRACT_VERSION = "JC v1.0"
PROTOCOL_VERSION = "C6.3 v0.2.1"

STRATEGY_EVENT_NAMES = frozenset({"STRATEGY_INJECTION", "PLANNER_CONTEXT_ASSEMBLY"})
EVENT_PRIORITY = {"STRATEGY_INJECTION": 0, "PLANNER_CONTEXT_ASSEMBLY": 1}


class JoinStatus(StrEnum):
    RESOLVED = "RESOLVED"
    UNRESOLVED = "UNRESOLVED"
    MISSING_ARTIFACT = "MISSING_ARTIFACT"
    FINGERPRINT_MISMATCH = "FINGERPRINT_MISMATCH"
    STALE_EVENT = "STALE_EVENT"
    INVALID_INPUT = "INVALID_INPUT"


class JoinConfidence(StrEnum):
    EXACT = "exact"
    TIME_PROXIMITY = "time_proximity"
    SYNTHETIC = "synthetic"


@dataclass(frozen=True)
class ValidationIssue:
    artifact: str
    source_line: int
    reason: str


@dataclass
class ValidationSummary:
    invalid_metrics_rows: int = 0
    invalid_app_log_rows: int = 0
    invalid_trace_rows: int = 0
    invalid_env_rows: int = 0
    issues: list[ValidationIssue] = field(default_factory=list)

    @property
    def total_invalid(self) -> int:
        return (
            self.invalid_metrics_rows
            + self.invalid_app_log_rows
            + self.invalid_trace_rows
            + self.invalid_env_rows
        )


@dataclass(frozen=True)
class StrategyEvent:
    session_id: str
    event_name: str
    timestamp_ms: int
    strategy_id: str
    strategy_injection: bool | None
    source_line: int


@dataclass(frozen=True)
class ConsolidationEvent:
    session_id: str
    finished_at_ms: int
    source_line: int


@dataclass(frozen=True)
class JoinContext:
    env_by_run_id: dict[str, dict[str, Any]]
    strategy_events: dict[str, list[StrategyEvent]]
    consolidations: dict[str, list[ConsolidationEvent]]
    missing_artifacts: frozenset[str]


@dataclass
class JoinArtifacts:
    context: JoinContext
    validation: ValidationSummary
    validated_metrics: list[dict[str, Any]]


@dataclass
class BaseSegment:
    join_status: JoinStatus
    join_confidence: JoinConfidence | None = None

    def _status_confidence_dict(self) -> dict[str, Any]:
        return {
            "join_status": self.join_status.value,
            "join_confidence": self.join_confidence.value if self.join_confidence else None,
        }


@dataclass
class PlanReuseSegment(BaseSegment):
    value: bool | None = None

    def to_public_dict(self) -> dict[str, Any]:
        out = {"value": self.value if self.join_status == JoinStatus.RESOLVED else None}
        out.update(self._status_confidence_dict())
        return out


@dataclass
class StrategySegment(BaseSegment):
    strategy_injected: bool | None = None
    strategy_id: str | None = None

    def to_public_dict(self) -> dict[str, Any]:
        resolved = self.join_status == JoinStatus.RESOLVED
        return {
            "strategy_injected": self.strategy_injected if resolved else None,
            "strategy_id": self.strategy_id if resolved else None,
            **self._status_confidence_dict(),
        }


@dataclass
class ConsolidationSegment(BaseSegment):
    post_consolidation: bool | None = None
    consolidation_id: str | None = None

    def to_public_dict(self) -> dict[str, Any]:
        resolved = self.join_status == JoinStatus.RESOLVED
        return {
            "post_consolidation": self.post_consolidation if resolved else None,
            "consolidation_id": self.consolidation_id if resolved else None,
            **self._status_confidence_dict(),
        }


@dataclass
class EnvironmentSegment(BaseSegment):
    env_hash: str | None = None
    model_hash: str | None = None
    corpus_fingerprint: str | None = None
    tier: str | None = None
    observed_env_hash: str | None = None
    expected_env_hash: str | None = None

    def to_public_dict(self, verbose: bool = False) -> dict[str, Any]:
        resolved = self.join_status == JoinStatus.RESOLVED
        out = {
            "env_hash": self.env_hash if resolved else None,
            "model_hash": self.model_hash if resolved else None,
            "corpus_fingerprint": self.corpus_fingerprint if resolved else None,
            "tier": self.tier if resolved else None,
            **self._status_confidence_dict(),
        }
        if verbose and self.join_status == JoinStatus.FINGERPRINT_MISMATCH:
            out["observed_env_hash"] = self.observed_env_hash
            out["expected_env_hash"] = self.expected_env_hash
        return out


@dataclass
class JoinedGoalRecord:
    plan_id: str
    session_id: str
    goal_started_at_ms: int
    benchmark_run_id: str
    plan_reused: PlanReuseSegment
    strategy: StrategySegment
    consolidation: ConsolidationSegment
    environment: EnvironmentSegment

    def to_public_dict(self, verbose: bool = False) -> dict[str, Any]:
        return {
            "plan_id": self.plan_id,
            "session_id": self.session_id,
            "goal_started_at_ms": self.goal_started_at_ms,
            "benchmark_run_id": self.benchmark_run_id,
            "plan_reused": self.plan_reused.to_public_dict(),
            "strategy": self.strategy.to_public_dict(),
            "consolidation": self.consolidation.to_public_dict(),
            "environment": self.environment.to_public_dict(verbose=verbose),
        }


def normalize_timestamp_ms(value: Any) -> int | None:
    if value is None:
        return None
    if isinstance(value, bool):
        return None
    if isinstance(value, int):
        return value if value >= 0 else None
    if isinstance(value, float):
        if value < 0 or not value.is_integer():
            return None
        return int(value)
    if isinstance(value, str):
        stripped = value.strip()
        if not stripped:
            return None
        try:
            numeric = float(stripped)
        except ValueError:
            return None
        if numeric < 0 or not numeric.is_integer():
            return None
        return int(numeric)
    return None


def load_jsonl(path: Path) -> tuple[list[dict[str, Any]], bool]:
    """Load JSONL rows. Returns (rows, file_exists)."""
    if not path.exists():
        return [], False
    rows: list[dict[str, Any]] = []
    with path.open("r", encoding="utf-8") as handle:
        for line_no, line in enumerate(handle, start=1):
            stripped = line.strip()
            if not stripped:
                continue
            try:
                row = json.loads(stripped)
            except json.JSONDecodeError:
                continue
            if isinstance(row, dict):
                row = dict(row)
                row["_source_line"] = line_no
                rows.append(row)
    return rows, True


def _record_invalid(
    summary: ValidationSummary,
    artifact: str,
    source_line: int,
    reason: str,
    verbose_issues: bool,
) -> None:
    if artifact == "metrics":
        summary.invalid_metrics_rows += 1
    elif artifact == "app_log":
        summary.invalid_app_log_rows += 1
    elif artifact == "trace":
        summary.invalid_trace_rows += 1
    elif artifact == "env":
        summary.invalid_env_rows += 1
    if verbose_issues:
        summary.issues.append(ValidationIssue(artifact, source_line, reason))


def validate_metrics_rows(rows: list[dict[str, Any]], summary: ValidationSummary, verbose_issues: bool) -> list[dict[str, Any]]:
    valid: list[dict[str, Any]] = []
    for row in rows:
        line = int(row.get("_source_line", 0))
        if row.get("event") != "GOAL_COGNITIVE_METRICS":
            _record_invalid(summary, "metrics", line, "event != GOAL_COGNITIVE_METRICS", verbose_issues)
            continue
        if not row.get("plan_id"):
            _record_invalid(summary, "metrics", line, "missing plan_id", verbose_issues)
            continue
        if normalize_timestamp_ms(row.get("goal_started_at_ms")) is None:
            _record_invalid(summary, "metrics", line, "invalid goal_started_at_ms", verbose_issues)
            continue
        valid.append(row)
    return valid


def validate_app_log_rows(rows: list[dict[str, Any]], summary: ValidationSummary, verbose_issues: bool) -> list[dict[str, Any]]:
    valid: list[dict[str, Any]] = []
    for row in rows:
        line = int(row.get("_source_line", 0))
        session_id = row.get("session_id")
        if not isinstance(session_id, str) or not session_id:
            _record_invalid(summary, "app_log", line, "missing session_id", verbose_issues)
            continue
        event_name = row.get("event_name")
        if event_name not in STRATEGY_EVENT_NAMES:
            _record_invalid(summary, "app_log", line, "unsupported event_name", verbose_issues)
            continue
        if normalize_timestamp_ms(row.get("timestamp_ms")) is None:
            _record_invalid(summary, "app_log", line, "invalid timestamp_ms", verbose_issues)
            continue
        valid.append(row)
    return valid


def validate_trace_rows(rows: list[dict[str, Any]], summary: ValidationSummary, verbose_issues: bool) -> list[dict[str, Any]]:
    valid: list[dict[str, Any]] = []
    for row in rows:
        line = int(row.get("_source_line", 0))
        if not row.get("trace_type"):
            _record_invalid(summary, "trace", line, "missing trace_type", verbose_issues)
            continue
        if normalize_timestamp_ms(row.get("finished_at_ms")) is None:
            _record_invalid(summary, "trace", line, "invalid finished_at_ms", verbose_issues)
            continue
        if not isinstance(row.get("stages"), list):
            _record_invalid(summary, "trace", line, "missing stages", verbose_issues)
            continue
        valid.append(row)
    return valid


def validate_env_rows(rows: list[dict[str, Any]], summary: ValidationSummary, verbose_issues: bool) -> list[dict[str, Any]]:
    valid: list[dict[str, Any]] = []
    for row in rows:
        if row.get("event") != "BENCHMARK_ENV":
            continue
        line = int(row.get("_source_line", 0))
        if not row.get("run_id"):
            _record_invalid(summary, "env", line, "missing run_id", verbose_issues)
            continue
        valid.append(row)
    return valid


def _parse_strategy_event(row: dict[str, Any]) -> StrategyEvent:
    metadata = row.get("metadata") if isinstance(row.get("metadata"), dict) else {}
    strategy_id = metadata.get("strategy_id", "")
    strategy_id = strategy_id if isinstance(strategy_id, str) else str(strategy_id)
    injection = metadata.get("strategy_injection")
    if not isinstance(injection, bool):
        injection = None
    return StrategyEvent(
        session_id=str(row["session_id"]),
        event_name=str(row["event_name"]),
        timestamp_ms=int(normalize_timestamp_ms(row["timestamp_ms"])),
        strategy_id=strategy_id,
        strategy_injection=injection,
        source_line=int(row.get("_source_line", 0)),
    )


def _parse_consolidation_event(row: dict[str, Any]) -> ConsolidationEvent | None:
    if row.get("trace_type") != "memory_consolidation":
        return None
    finished_at_ms = normalize_timestamp_ms(row.get("finished_at_ms"))
    if finished_at_ms is None:
        return None
    for stage in row.get("stages", []):
        if not isinstance(stage, dict):
            continue
        if stage.get("name") != "consolidation_committed" or not stage.get("success", False):
            continue
        metadata = stage.get("metadata") if isinstance(stage.get("metadata"), dict) else {}
        session_id = metadata.get("session_id")
        if not isinstance(session_id, str) or not session_id:
            return None
        return ConsolidationEvent(
            session_id=session_id,
            finished_at_ms=finished_at_ms,
            source_line=int(row.get("_source_line", 0)),
        )
    return None


def build_join_context(
    app_log_rows: list[dict[str, Any]],
    trace_rows: list[dict[str, Any]],
    env_rows: list[dict[str, Any]],
    *,
    app_log_exists: bool,
    trace_exists: bool,
    env_exists: bool,
) -> JoinContext:
    missing: set[str] = set()
    if not app_log_exists:
        missing.add("app_log")
    if not trace_exists:
        missing.add("decision_trace")
    if not env_exists:
        missing.add("benchmark_env")

    env_by_run_id: dict[str, dict[str, Any]] = {}
    for row in env_rows:
        run_id = row.get("run_id")
        if isinstance(run_id, str) and run_id:
            env_by_run_id[run_id] = row

    strategy_events: dict[str, list[StrategyEvent]] = {}
    for row in app_log_rows:
        event = _parse_strategy_event(row)
        strategy_events.setdefault(event.session_id, []).append(event)

    consolidations: dict[str, list[ConsolidationEvent]] = {}
    for row in trace_rows:
        event = _parse_consolidation_event(row)
        if event is not None:
            consolidations.setdefault(event.session_id, []).append(event)

    return JoinContext(
        env_by_run_id=env_by_run_id,
        strategy_events=strategy_events,
        consolidations=consolidations,
        missing_artifacts=frozenset(missing),
    )


def prepare_join(
    metric_rows: list[dict[str, Any]],
    app_log_rows: list[dict[str, Any]],
    trace_rows: list[dict[str, Any]],
    env_rows: list[dict[str, Any]],
    *,
    app_log_exists: bool = True,
    trace_exists: bool = True,
    env_exists: bool = True,
    verbose_issues: bool = False,
) -> JoinArtifacts:
    validation = ValidationSummary()
    validated_metrics = validate_metrics_rows(metric_rows, validation, verbose_issues)
    validated_app_log = validate_app_log_rows(app_log_rows, validation, verbose_issues)
    validated_trace = validate_trace_rows(trace_rows, validation, verbose_issues)
    validated_env = validate_env_rows(env_rows, validation, verbose_issues)
    context = build_join_context(
        validated_app_log,
        validated_trace,
        validated_env,
        app_log_exists=app_log_exists,
        trace_exists=trace_exists,
        env_exists=env_exists,
    )
    return JoinArtifacts(context=context, validation=validation, validated_metrics=validated_metrics)


def resolve_plan_reused(metrics_row: dict[str, Any]) -> PlanReuseSegment:
    if "plan_reused" not in metrics_row:
        return PlanReuseSegment(join_status=JoinStatus.INVALID_INPUT)
    value = metrics_row.get("plan_reused")
    if not isinstance(value, bool):
        return PlanReuseSegment(join_status=JoinStatus.INVALID_INPUT)
    return PlanReuseSegment(
        value=value,
        join_status=JoinStatus.RESOLVED,
        join_confidence=JoinConfidence.EXACT,
    )


def _extract_env_fields(env_row: dict[str, Any]) -> tuple[str | None, str | None, str | None, str | None]:
    env = env_row.get("env") if isinstance(env_row.get("env"), dict) else {}
    corpus = env.get("corpus") if isinstance(env.get("corpus"), dict) else {}
    model = env.get("model") if isinstance(env.get("model"), dict) else {}
    runtime = env.get("runtime") if isinstance(env.get("runtime"), dict) else {}

    env_hash = env_row.get("env_hash")
    env_hash = env_hash if isinstance(env_hash, str) and env_hash else None

    corpus_fp = corpus.get("fingerprint")
    corpus_fp = corpus_fp if isinstance(corpus_fp, str) and corpus_fp else None

    model_hash = model.get("model_hash")
    if not isinstance(model_hash, str) or not model_hash:
        embedding_model = model.get("embedding_model")
        model_hash = embedding_model if isinstance(embedding_model, str) and embedding_model else None

    tier = runtime.get("tier")
    tier = tier.upper() if isinstance(tier, str) and tier else None

    return env_hash, model_hash, corpus_fp, tier


def resolve_environment(metrics_row: dict[str, Any], context: JoinContext) -> EnvironmentSegment:
    if "benchmark_env" in context.missing_artifacts:
        return EnvironmentSegment(join_status=JoinStatus.MISSING_ARTIFACT)

    run_id = metrics_row.get("run_id")
    if not isinstance(run_id, str) or not run_id:
        return EnvironmentSegment(join_status=JoinStatus.UNRESOLVED)

    env_row = context.env_by_run_id.get(run_id)
    if env_row is None:
        return EnvironmentSegment(join_status=JoinStatus.UNRESOLVED)

    expected_env_hash, model_hash, corpus_fp, tier = _extract_env_fields(env_row)
    metrics_env_hash = metrics_row.get("env_hash")
    metrics_env_hash = metrics_env_hash if isinstance(metrics_env_hash, str) and metrics_env_hash else None

    if metrics_env_hash and expected_env_hash and metrics_env_hash != expected_env_hash:
        return EnvironmentSegment(
            join_status=JoinStatus.FINGERPRINT_MISMATCH,
            observed_env_hash=metrics_env_hash,
            expected_env_hash=expected_env_hash,
        )

    return EnvironmentSegment(
        env_hash=expected_env_hash or metrics_env_hash,
        model_hash=model_hash,
        corpus_fingerprint=corpus_fp,
        tier=tier,
        join_status=JoinStatus.RESOLVED,
        join_confidence=JoinConfidence.EXACT,
    )


def _strategy_sort_key(event: StrategyEvent) -> tuple[Any, ...]:
    strategy_key: tuple[int, ...]
    if event.strategy_id:
        strategy_key = tuple(-ord(c) for c in event.strategy_id)
    else:
        strategy_key = (0,)
    return (
        -event.timestamp_ms,
        EVENT_PRIORITY.get(event.event_name, 99),
        strategy_key,
        event.source_line,
    )


def resolve_strategy(metrics_row: dict[str, Any], context: JoinContext) -> StrategySegment:
    session_id = metrics_row.get("session_id")
    if not isinstance(session_id, str) or not session_id:
        return StrategySegment(join_status=JoinStatus.INVALID_INPUT)

    if "app_log" in context.missing_artifacts:
        return StrategySegment(join_status=JoinStatus.MISSING_ARTIFACT)

    goal_started_at_ms = normalize_timestamp_ms(metrics_row.get("goal_started_at_ms"))
    if goal_started_at_ms is None:
        return StrategySegment(join_status=JoinStatus.INVALID_INPUT)

    events = context.strategy_events.get(session_id, [])
    if not events:
        return StrategySegment(
            strategy_injected=False,
            join_status=JoinStatus.RESOLVED,
            join_confidence=JoinConfidence.EXACT,
        )

    in_gap = [
        event
        for event in events
        if event.timestamp_ms <= goal_started_at_ms
        and (goal_started_at_ms - event.timestamp_ms) <= MAX_STRATEGY_JOIN_GAP_MS
    ]
    stale_candidates = [
        event
        for event in events
        if event.timestamp_ms <= goal_started_at_ms
        and (goal_started_at_ms - event.timestamp_ms) > MAX_STRATEGY_JOIN_GAP_MS
    ]

    if not in_gap:
        if stale_candidates:
            return StrategySegment(join_status=JoinStatus.STALE_EVENT)
        return StrategySegment(
            strategy_injected=False,
            join_status=JoinStatus.RESOLVED,
            join_confidence=JoinConfidence.EXACT,
        )

    winner = sorted(in_gap, key=_strategy_sort_key)[0]

    if winner.event_name == "STRATEGY_INJECTION":
        return StrategySegment(
            strategy_injected=True,
            strategy_id=winner.strategy_id or None,
            join_status=JoinStatus.RESOLVED,
            join_confidence=JoinConfidence.TIME_PROXIMITY,
        )

    if winner.strategy_injection is True:
        return StrategySegment(
            strategy_injected=True,
            strategy_id=None,
            join_status=JoinStatus.RESOLVED,
            join_confidence=JoinConfidence.TIME_PROXIMITY,
        )

    return StrategySegment(
        strategy_injected=False,
        strategy_id=None,
        join_status=JoinStatus.RESOLVED,
        join_confidence=JoinConfidence.EXACT,
    )


def _consolidation_sort_key(event: ConsolidationEvent) -> tuple[int, int]:
    return (-event.finished_at_ms, event.source_line)


def resolve_consolidation(metrics_row: dict[str, Any], context: JoinContext) -> ConsolidationSegment:
    session_id = metrics_row.get("session_id")
    if not isinstance(session_id, str) or not session_id:
        return ConsolidationSegment(join_status=JoinStatus.INVALID_INPUT)

    if "decision_trace" in context.missing_artifacts:
        return ConsolidationSegment(join_status=JoinStatus.MISSING_ARTIFACT)

    goal_started_at_ms = normalize_timestamp_ms(metrics_row.get("goal_started_at_ms"))
    if goal_started_at_ms is None:
        return ConsolidationSegment(join_status=JoinStatus.INVALID_INPUT)

    candidates = [
        event
        for event in context.consolidations.get(session_id, [])
        if event.finished_at_ms < goal_started_at_ms
    ]
    if not candidates:
        return ConsolidationSegment(
            post_consolidation=False,
            join_status=JoinStatus.RESOLVED,
            join_confidence=JoinConfidence.EXACT,
        )

    winner = sorted(candidates, key=_consolidation_sort_key)[0]
    consolidation_id = f"{winner.session_id}:{winner.finished_at_ms}"
    return ConsolidationSegment(
        post_consolidation=True,
        consolidation_id=consolidation_id,
        join_status=JoinStatus.RESOLVED,
        join_confidence=JoinConfidence.SYNTHETIC,
    )


def join_single_goal(metrics_row: dict[str, Any], context: JoinContext) -> JoinedGoalRecord:
    goal_started_at_ms = int(normalize_timestamp_ms(metrics_row.get("goal_started_at_ms")))
    run_id = metrics_row.get("run_id")
    benchmark_run_id = run_id if isinstance(run_id, str) else ""

    return JoinedGoalRecord(
        plan_id=str(metrics_row.get("plan_id", "")),
        session_id=str(metrics_row.get("session_id", "")),
        goal_started_at_ms=goal_started_at_ms,
        benchmark_run_id=benchmark_run_id,
        plan_reused=resolve_plan_reused(metrics_row),
        strategy=resolve_strategy(metrics_row, context),
        consolidation=resolve_consolidation(metrics_row, context),
        environment=resolve_environment(metrics_row, context),
    )


def join_goals_from_artifacts(artifacts: JoinArtifacts) -> list[JoinedGoalRecord]:
    return [join_single_goal(row, artifacts.context) for row in artifacts.validated_metrics]


def join_goals_from_records(
    metric_rows: list[dict[str, Any]],
    app_log_rows: list[dict[str, Any]],
    trace_rows: list[dict[str, Any]],
    env_rows: list[dict[str, Any]],
    *,
    app_log_exists: bool = True,
    trace_exists: bool = True,
    env_exists: bool = True,
    verbose_issues: bool = False,
) -> tuple[list[JoinedGoalRecord], JoinArtifacts]:
    artifacts = prepare_join(
        metric_rows,
        app_log_rows,
        trace_rows,
        env_rows,
        app_log_exists=app_log_exists,
        trace_exists=trace_exists,
        env_exists=env_exists,
        verbose_issues=verbose_issues,
    )
    return join_goals_from_artifacts(artifacts), artifacts


def join_goals(
    metrics_path: Path,
    app_log_path: Path,
    decision_trace_path: Path,
    benchmark_env_path: Path,
    *,
    verbose_issues: bool = False,
) -> tuple[list[JoinedGoalRecord], JoinArtifacts]:
    records, artifacts, _ = join_goals_with_load_counts(
        metrics_path,
        app_log_path,
        decision_trace_path,
        benchmark_env_path,
        verbose_issues=verbose_issues,
    )
    return records, artifacts


def join_goals_with_load_counts(
    metrics_path: Path,
    app_log_path: Path,
    decision_trace_path: Path,
    benchmark_env_path: Path,
    *,
    verbose_issues: bool = False,
) -> tuple[list[JoinedGoalRecord], JoinArtifacts, dict[str, int]]:
    metric_rows, metrics_exists = load_jsonl(metrics_path)
    if not metrics_exists:
        raise FileNotFoundError(f"metrics artifact not found: {metrics_path}")

    app_log_rows, app_log_exists = load_jsonl(app_log_path)
    trace_rows, trace_exists = load_jsonl(decision_trace_path)
    env_rows, env_exists = load_jsonl(benchmark_env_path)

    records, artifacts = join_goals_from_records(
        metric_rows,
        app_log_rows,
        trace_rows,
        env_rows,
        app_log_exists=app_log_exists,
        trace_exists=trace_exists,
        env_exists=env_exists,
        verbose_issues=verbose_issues,
    )
    load_counts = {
        "metrics_rows": len(metric_rows),
        "app_log_rows": len(app_log_rows),
        "decision_trace_rows": len(trace_rows),
        "benchmark_env_rows": len(env_rows),
    }
    return records, artifacts, load_counts


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
class SmokeSummary:
    goals_analyzed: int
    segment_joins_resolved: int
    segment_joins_unresolved: int
    segment_joins_missing_artifact: int
    segment_joins_fingerprint_mismatch: int
    segment_joins_stale_event: int
    invalid_rows_skipped: int
    report_completeness: str
    segment_joins_invalid_input: int = 0
    metrics_rows: int = 0
    app_log_rows: int = 0
    decision_trace_rows: int = 0
    benchmark_env_rows: int = 0
    earliest_goal_ms: int | None = None
    latest_goal_ms: int | None = None


def _segment_status_counter(records: list[JoinedGoalRecord]) -> Counter[str]:
    counts: Counter[str] = Counter()
    for record in records:
        for segment in (record.environment, record.strategy, record.consolidation):
            counts[segment.join_status.value] += 1
        if record.plan_reused.join_status == JoinStatus.INVALID_INPUT:
            counts[JoinStatus.INVALID_INPUT.value] += 1
    return counts


def derive_report_completeness(artifacts: JoinArtifacts) -> str:
    if artifacts.context.missing_artifacts:
        return "incomplete"
    if artifacts.validation.total_invalid > 0:
        return "incomplete"
    return "complete"


def build_smoke_summary(
    records: list[JoinedGoalRecord],
    artifacts: JoinArtifacts,
    *,
    metrics_rows: int,
    app_log_rows: int,
    decision_trace_rows: int,
    benchmark_env_rows: int,
) -> SmokeSummary:
    segment_counts = _segment_status_counter(records)
    goal_times = [record.goal_started_at_ms for record in records]
    return SmokeSummary(
        goals_analyzed=len(records),
        segment_joins_resolved=segment_counts[JoinStatus.RESOLVED.value],
        segment_joins_unresolved=segment_counts[JoinStatus.UNRESOLVED.value],
        segment_joins_missing_artifact=segment_counts[JoinStatus.MISSING_ARTIFACT.value],
        segment_joins_fingerprint_mismatch=segment_counts[JoinStatus.FINGERPRINT_MISMATCH.value],
        segment_joins_stale_event=segment_counts[JoinStatus.STALE_EVENT.value],
        invalid_rows_skipped=artifacts.validation.total_invalid,
        report_completeness=derive_report_completeness(artifacts),
        segment_joins_invalid_input=segment_counts[JoinStatus.INVALID_INPUT.value],
        metrics_rows=metrics_rows,
        app_log_rows=app_log_rows,
        decision_trace_rows=decision_trace_rows,
        benchmark_env_rows=benchmark_env_rows,
        earliest_goal_ms=min(goal_times) if goal_times else None,
        latest_goal_ms=max(goal_times) if goal_times else None,
    )


def print_smoke_summary(summary: SmokeSummary, *, verbose: bool = False) -> None:
    print("C6.3-01 smoke (non-gating)")
    print(f"  goals_analyzed:                     {summary.goals_analyzed}")
    print(f"  segment_joins_resolved:             {summary.segment_joins_resolved}")
    print(f"  segment_joins_unresolved:           {summary.segment_joins_unresolved}")
    print(f"  segment_joins_missing_artifact:     {summary.segment_joins_missing_artifact}")
    print(f"  segment_joins_fingerprint_mismatch: {summary.segment_joins_fingerprint_mismatch}")
    print(f"  segment_joins_stale_event:          {summary.segment_joins_stale_event}")
    print(f"  invalid_rows_skipped:               {summary.invalid_rows_skipped}")
    print(f"  report_completeness:                {summary.report_completeness}")
    if verbose:
        print(f"  metrics_rows:           {summary.metrics_rows}")
        print(f"  app_log_rows:           {summary.app_log_rows}")
        print(f"  decision_trace_rows:    {summary.decision_trace_rows}")
        print(f"  benchmark_env_rows:     {summary.benchmark_env_rows}")
        earliest = summary.earliest_goal_ms
        latest = summary.latest_goal_ms
        print(f"  earliest_goal_ms:       {earliest if earliest is not None else '(none)'}")
        print(f"  latest_goal_ms:         {latest if latest is not None else '(none)'}")
        if summary.segment_joins_invalid_input:
            print(f"  segment_joins_invalid_input: {summary.segment_joins_invalid_input}")


def summarize_records(records: list[JoinedGoalRecord]) -> Counter[str]:
    counts: Counter[str] = Counter()
    counts["goals_joined"] = len(records)
    for record in records:
        env_status = record.environment.join_status
        counts[f"environment_{env_status.value.lower()}"] += 1
        strat_status = record.strategy.join_status
        counts[f"strategy_{strat_status.value.lower()}"] += 1
        cons_status = record.consolidation.join_status
        counts[f"consolidation_{cons_status.value.lower()}"] += 1
    return counts


def print_summary(records: list[JoinedGoalRecord], artifacts: JoinArtifacts) -> None:
    counts = summarize_records(records)
    missing = sorted(artifacts.context.missing_artifacts)
    missing_display = ", ".join(missing) if missing else "(none)"

    print(f"C6 longitudinal join ({CONTRACT_VERSION})")
    print(f"  goals_joined:                     {counts['goals_joined']}")
    print(f"  invalid_metrics_rows:             {artifacts.validation.invalid_metrics_rows}")
    print(f"  invalid_app_log_rows:             {artifacts.validation.invalid_app_log_rows}")
    print(f"  invalid_trace_rows:               {artifacts.validation.invalid_trace_rows}")
    print(f"  invalid_env_rows:               {artifacts.validation.invalid_env_rows}")
    print(f"  environment_resolved:             {counts.get('environment_resolved', 0)}")
    print(
        "  environment_fingerprint_mismatch: "
        f"{counts.get('environment_fingerprint_mismatch', 0)}"
    )
    print(f"  environment_unresolved:           {counts.get('environment_unresolved', 0)}")
    print(f"  strategy_resolved:                {counts.get('strategy_resolved', 0)}")
    print(f"  strategy_stale_event:             {counts.get('strategy_stale_event', 0)}")
    print(f"  consolidation_resolved:           {counts.get('consolidation_resolved', 0)}")
    print(f"  missing_artifacts:                {missing_display}")
    if artifacts.validation.total_invalid > 0:
        print("  note: invalid rows dropped before join — see --verbose")


def build_arg_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="C6.3 longitudinal join library (JC v1.0)")
    parser.add_argument(
        "--smoke",
        action="store_true",
        help="Production smoke summary on default log paths (C6.3-01 Step 4)",
    )
    parser.add_argument("--metrics", type=Path, default=None, help="Path to cognitive_metrics.jsonl")
    parser.add_argument("--app-log", type=Path, default=None, help="Path to app_log.jsonl")
    parser.add_argument("--decision-trace", type=Path, default=None, help="Path to decision_trace.jsonl")
    parser.add_argument("--benchmark-env", type=Path, default=None, help="Path to benchmark_env.jsonl")
    parser.add_argument("--pretty", action="store_true", help="Pretty-print JSON output")
    parser.add_argument(
        "--verbose",
        action="store_true",
        help="Diagnostics: validation issues (dev mode); row counts and goal range (smoke mode)",
    )
    parser.add_argument("--json-only", action="store_true", help="Suppress summary; emit JSON only")
    return parser


def resolve_artifact_paths(args: argparse.Namespace) -> tuple[Path, Path, Path, Path] | None:
    if args.smoke:
        defaults = production_default_paths()
        return (
            args.metrics or defaults["metrics"],
            args.app_log or defaults["app_log"],
            args.decision_trace or defaults["decision_trace"],
            args.benchmark_env or defaults["benchmark_env"],
        )

    missing = [
        name
        for name, value in (
            ("--metrics", args.metrics),
            ("--app-log", args.app_log),
            ("--decision-trace", args.decision_trace),
            ("--benchmark-env", args.benchmark_env),
        )
        if value is None
    ]
    if missing:
        print(f"error: required arguments missing: {', '.join(missing)}", file=sys.stderr)
        return None
    return args.metrics, args.app_log, args.decision_trace, args.benchmark_env


def main(argv: list[str] | None = None) -> int:
    parser = build_arg_parser()
    args = parser.parse_args(argv)

    if args.smoke and (args.pretty or args.json_only):
        print("error: --smoke cannot be combined with --pretty or --json-only", file=sys.stderr)
        return 1

    paths = resolve_artifact_paths(args)
    if paths is None:
        return 1
    metrics_path, app_log_path, decision_trace_path, benchmark_env_path = paths

    try:
        if args.smoke:
            records, artifacts, load_counts = join_goals_with_load_counts(
                metrics_path,
                app_log_path,
                decision_trace_path,
                benchmark_env_path,
                verbose_issues=args.verbose,
            )
        else:
            records, artifacts = join_goals(
                metrics_path,
                app_log_path,
                decision_trace_path,
                benchmark_env_path,
                verbose_issues=args.verbose,
            )
    except FileNotFoundError as exc:
        print(str(exc), file=sys.stderr)
        return 2

    if args.smoke:
        summary = build_smoke_summary(
            records,
            artifacts,
            metrics_rows=load_counts["metrics_rows"],
            app_log_rows=load_counts["app_log_rows"],
            decision_trace_rows=load_counts["decision_trace_rows"],
            benchmark_env_rows=load_counts["benchmark_env_rows"],
        )
        print_smoke_summary(summary, verbose=args.verbose)
        if args.verbose and artifacts.validation.issues:
            print("", file=sys.stderr)
            print("Validation issues:", file=sys.stderr)
            for issue in artifacts.validation.issues:
                print(f"  {issue.artifact}:{issue.source_line} — {issue.reason}", file=sys.stderr)
        return 0

    if not args.json_only:
        print_summary(records, artifacts)
        print("")

    payload = [record.to_public_dict(verbose=args.verbose) for record in records]
    if args.pretty:
        print(json.dumps(payload, indent=2, sort_keys=True))
    else:
        print(json.dumps(payload))

    if args.verbose and artifacts.validation.issues:
        print("", file=sys.stderr)
        print("Validation issues:", file=sys.stderr)
        for issue in artifacts.validation.issues:
            print(f"  {issue.artifact}:{issue.source_line} — {issue.reason}", file=sys.stderr)

    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except SystemExit:
        raise
    except Exception as exc:
        print(f"error: {exc}", file=sys.stderr)
        raise SystemExit(1) from exc
