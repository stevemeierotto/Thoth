#!/usr/bin/env python3
"""C6.4 v1.0 longitudinal analysis. Synthetic and prospective rows only.

Membership comes from the Phase 5 window assignment. This module does not
open a window, does not rewrite historical rows, and does not promote Thoth.
Wilson intervals and the 0.05 directional test reuse the C6.3 definitions
named by the sealed protocol. A first official window with no qualifying
prior is official_baseline, trend=not_evaluable, reason=no_prior_window.
"""

from __future__ import annotations

import json
from pathlib import Path
from typing import Any, Mapping

import analyze_cognitive_longitudinal as c63
import c6_longitudinal_join as join

PROTOCOL_VERSION = "C6.4 v1.0"
METRIC_SCHEMA_VERSION = "1.0"
ENVIRONMENT_SCHEMA_VERSION = "c64-env-1"
WINDOW_SPAN_MS = 28 * 86_400_000
MIN_GOALS = 30
MIN_SEGMENT = 10
TREND_MIN_ABS_DELTA = 0.05


def load_jsonl(path: Path) -> list[dict[str, Any]]:
    rows: list[dict[str, Any]] = []
    for line_number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), start=1):
        if not line.strip():
            continue
        row = json.loads(line)
        row["_source_line"] = line_number
        rows.append(row)
    return rows


def _number(row: Mapping[str, Any], key: str) -> float | None:
    value = row.get(key)
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        return None
    return float(value)


def _mean(values: list[float]) -> float | None:
    if not values:
        return None
    return sum(values) / len(values)


def _pin(window: Mapping[str, Any]) -> Mapping[str, Any]:
    pin = window.get("pin")
    return pin if isinstance(pin, Mapping) else {}


def _provider(window: Mapping[str, Any]) -> str:
    inference = _pin(window).get("inference")
    if isinstance(inference, Mapping) and isinstance(inference.get("backend_name"), str):
        return inference["backend_name"]
    value = window.get("inference_backend_name")
    return value if isinstance(value, str) else ""


def _model(window: Mapping[str, Any]) -> str:
    model = _pin(window).get("model")
    if isinstance(model, Mapping) and isinstance(model.get("llm_model"), str):
        return model["llm_model"]
    value = window.get("llm_model")
    return value if isinstance(value, str) else ""


def _sha(window: Mapping[str, Any], key: str) -> str:
    prov = _pin(window).get("prov")
    if isinstance(prov, Mapping) and isinstance(prov.get(key), str):
        return prov[key]
    value = window.get(key)
    return value if isinstance(value, str) else ""


def _row_text(row: Mapping[str, Any], key: str, default: str = "") -> str:
    value = row.get(key)
    return value if isinstance(value, str) else default


def _gates_ok(window: Mapping[str, Any]) -> tuple[bool, str]:
    thoth = _sha(window, "thoth_git_sha")
    engine = _sha(window, "basic_agent_git_sha")
    for name in ("c3_gate", "c5_gate"):
        gate = window.get(name)
        if not isinstance(gate, Mapping) or gate.get("passed") is not True:
            return False, f"missing_{name}"
        if gate.get("evaluation_tier") != "mock":
            return False, f"{name}_not_mock"
        if gate.get("thoth_git_sha") != thoth or gate.get("basic_agent_git_sha") != engine:
            return False, f"{name}_wrong_code"
    return True, ""


def _episodic_ok(window: Mapping[str, Any]) -> tuple[bool, str]:
    gate = window.get("episodic_gate")
    if not isinstance(gate, Mapping) or gate.get("satisfied") is not True:
        return False, "episodic_gate_unsatisfied"
    if gate.get("evaluation_tier") == "mock" or gate.get("authoritative_valid") is False:
        return False, "episodic_mock_rejected"
    if gate.get("evaluation_id") != "episodic_authoritative_v2":
        return False, "episodic_wrong_id"
    if gate.get("inference_backend_name") != _provider(window):
        return False, "episodic_provider_mismatch"
    model = gate.get("llm_model")
    if isinstance(model, str) and model != _model(window):
        return False, "episodic_model_mismatch"
    return True, ""


def _dedupe(rows: list[dict[str, Any]]) -> tuple[list[dict[str, Any]], int]:
    grouped: dict[str, list[dict[str, Any]]] = {}
    for row in rows:
        grouped.setdefault(str(row["plan_id"]), []).append(row)
    kept: list[dict[str, Any]] = []
    conflicts = 0
    for items in grouped.values():
        signatures = {
            (
                item.get("outcome"),
                item.get("goal_started_at_ms"),
                item.get("window_id"),
                item.get("c64_cohort_fingerprint"),
            )
            for item in items
        }
        if len(signatures) > 1:
            conflicts += 1
            continue
        kept.append(items[0])
    return kept, conflicts


def _recognized_metrics(rows: list[dict[str, Any]]) -> tuple[list[dict[str, Any]], list[dict[str, Any]], int]:
    """Historical rows without a window_id stay out. A claimed window_id is recognized."""
    official: list[dict[str, Any]] = []
    excluded = 0
    invalid = 0
    for row in rows:
        if row.get("event") not in (None, "GOAL_COGNITIVE_METRICS"):
            continue
        if not row.get("window_id"):
            excluded += 1
            continue
        session_id = row.get("session_id")
        if (
            not row.get("plan_id")
            or not isinstance(session_id, str)
            or not session_id
            or join.normalize_timestamp_ms(row.get("goal_started_at_ms")) is None
        ):
            invalid += 1
            continue
        official.append(row)
    return official, [], invalid


def _descriptive(rows: list[dict[str, Any]]) -> dict[str, float | None]:
    return {
        "total_wall_clock_ms_p50": c63.percentile([v for row in rows if (v := _number(row, "total_wall_clock_ms")) is not None], 50) if rows else None,
        "total_wall_clock_ms_p95": c63.percentile([v for row in rows if (v := _number(row, "total_wall_clock_ms")) is not None], 95) if rows else None,
        "reflection_count_mean": _mean([v for row in rows if (v := _number(row, "reflection_count")) is not None]),
        "revisions_count_mean": _mean([v for row in rows if (v := _number(row, "revisions_count")) is not None]),
        "total_tokens_p50": c63.percentile([v for row in rows if (v := _number(row, "total_tokens")) is not None], 50) if rows else None,
        "retrieved_chunk_count_mean": _mean([v for row in rows if (v := _number(row, "retrieved_chunk_count")) is not None]),
        "grag_alpha_mean": _mean([v for row in rows if (v := _number(row, "grag_alpha")) is not None]),
    }


def _rate(rows: list[dict[str, Any]]) -> dict[str, Any]:
    eligible = [row for row in rows if row.get("outcome") in c63.ELIGIBLE_OUTCOMES]
    successes = sum(1 for row in eligible if row.get("outcome") == "completed")
    n = len(eligible)
    point, low, high = c63.wilson_ci(successes, n)
    return {
        "eligible_count": n,
        "successes": successes,
        "success_rate": point if n else None,
        "wilson95": {"low": low, "high": high},
        "confidence_label": c63.confidence_label_for(n, low, high) if n else "low",
    }


def _delta_direction(current_successes: int, current_n: int, prior_successes: int, prior_n: int) -> int:
    """Return 1, -1, or 0 using the sealed absolute threshold of 0.05.

    The comparison is integer: 20 * (rate difference) >= 1. A mathematical
    difference of exactly 0.05 meets the threshold. Binary subtraction of the
    two rates is not used.
    """
    if current_n <= 0 or prior_n <= 0:
        return 0
    difference = current_successes * prior_n - prior_successes * current_n
    scale = current_n * prior_n
    if 20 * difference >= scale:
        return 1
    if 20 * difference <= -scale:
        return -1
    return 0


def _trend(current: Mapping[str, Any], prior: Mapping[str, Any] | None) -> dict[str, Any]:
    if prior is None or current.get("success_rate") is None or prior.get("success_rate") is None:
        return {"trend": "not_evaluable", "reason": "no_prior_window", "delta": None}
    delta = float(current["success_rate"]) - float(prior["success_rate"])
    direction = _delta_direction(
        int(current["successes"]),
        int(current["eligible_count"]),
        int(prior["successes"]),
        int(prior["eligible_count"]),
    )
    contained = c63.prior_ci_contains(
        float(current["success_rate"]),
        float(prior["wilson95"]["low"]),
        float(prior["wilson95"]["high"]),
    )
    if current.get("confidence_label") == "low" or direction == 0 or contained:
        return {
            "trend": "stable",
            "reason": "below_delta_or_within_prior_wilson_or_low_confidence",
            "delta": delta,
        }
    return {
        "trend": "improving" if direction > 0 else "declining",
        "reason": "directional_rule",
        "delta": delta,
    }


def _segment_rows(rows: list[dict[str, Any]], key: str, value: bool) -> list[dict[str, Any]]:
    return [row for row in rows if bool(row.get(key)) is value and row.get("outcome") in c63.ELIGIBLE_OUTCOMES]


def _segments(rows: list[dict[str, Any]], prior_rows: list[dict[str, Any]] | None, has_prior: bool) -> dict[str, Any]:
    names = {
        "plan_reused_true": ("plan_reused", True),
        "plan_reused_false": ("plan_reused", False),
        "strategy_injected_true": ("strategy_injected", True),
        "strategy_injected_false": ("strategy_injected", False),
        "post_consolidation_true": ("post_consolidation", True),
        "post_consolidation_false": ("post_consolidation", False),
    }
    out: dict[str, Any] = {}
    for name, (key, value) in names.items():
        current_rows = _segment_rows(rows, key, value)
        stats = _rate(current_rows)
        if stats["eligible_count"] < MIN_SEGMENT:
            stats["segment_eligible"] = False
            stats["trend"] = "not_evaluable"
            stats["reason"] = "segment_n_below_10"
            stats["delta"] = None
        elif not has_prior or prior_rows is None:
            stats["segment_eligible"] = True
            stats["trend"] = "not_evaluable"
            stats["reason"] = "no_prior_window"
            stats["delta"] = None
        else:
            prior_stats = _rate(_segment_rows(prior_rows, key, value))
            trend = _trend(stats, prior_stats if prior_stats["eligible_count"] else None)
            stats["segment_eligible"] = True
            stats.update(trend)
        out[name] = stats
    return out


def _qualifying_prior(current: Mapping[str, Any], earlier: list[dict[str, Any]]) -> tuple[dict[str, Any] | None, str]:
    start = int(current["window_start_ms"])
    expected_start = start - WINDOW_SPAN_MS
    matches = [
        item
        for item in earlier
        if item.get("evidence_scope") in {"official_baseline", "official_longitudinal"}
        and item.get("c64_cohort_fingerprint") == current.get("c64_cohort_fingerprint")
        and item.get("protocol_version") == current.get("protocol_version")
        and item.get("metric_schema_version") == current.get("metric_schema_version")
        and item.get("environment_schema_version") == current.get("environment_schema_version")
        and int(item["window_start_ms"]) == expected_start
        and int(item["window_end_ms"]) == start
    ]
    overlaps = [
        item
        for item in earlier
        if int(item["window_start_ms"]) < int(current["window_end_ms"])
        and int(item["window_end_ms"]) > start
    ]
    if overlaps:
        return None, "overlapping_windows"
    if len(matches) == 1:
        return matches[0], ""
    return None, "no_prior_window"


def analyze_windows(
    windows: list[Mapping[str, Any]],
    metrics_rows: list[dict[str, Any]],
    app_log_rows: list[dict[str, Any]],
    trace_rows: list[dict[str, Any]],
    *,
    app_log_present: bool = True,
    trace_present: bool = True,
) -> dict[str, Any]:
    summary = join.ValidationSummary()
    missing_artifacts = [name for name, present in (("app_log", app_log_present), ("decision_trace", trace_present)) if not present]
    valid_app = join.validate_app_log_rows(app_log_rows, summary, False) if app_log_present else []
    valid_trace = join.validate_trace_rows(trace_rows, summary, False) if trace_present else []
    context = join.build_join_context(
        valid_app,
        valid_trace,
        [],
        app_log_exists=app_log_present,
        trace_exists=trace_present,
        env_exists=True,
    )
    recognized, _, metric_invalid = _recognized_metrics(metrics_rows)
    reports: list[dict[str, Any]] = []
    ordered = sorted(windows, key=lambda item: int(item["window_start_ms"]))
    for window in ordered:
        window_id = str(window["window_id"])
        claimed = [row for row in recognized if row.get("window_id") == window_id]
        deduped, conflicts = _dedupe(claimed)
        invalid = summary.total_invalid + metric_invalid + conflicts
        prepared: list[dict[str, Any]] = []
        retrospective = 0
        for row in deduped:
            started = int(join.normalize_timestamp_ms(row.get("goal_started_at_ms")))
            if not (int(window["window_start_ms"]) <= started <= int(window["window_end_ms"])):
                retrospective += 1
                continue
            if int(window["window_start_ms"]) > started:
                retrospective += 1
                continue
            copy = dict(row)
            strategy = join.resolve_strategy(copy, context)
            consolidation = join.resolve_consolidation(copy, context)
            copy["strategy_injected"] = bool(strategy.strategy_injected)
            copy["post_consolidation"] = bool(consolidation.post_consolidation)
            copy["plan_reused"] = bool(copy.get("plan_reused"))
            prepared.append(copy)

        aborted = [row for row in prepared if row.get("outcome") == "aborted"]
        open_rows = [row for row in prepared if row.get("outcome") not in {"completed", "failed", "aborted"}]
        eligible = [row for row in prepared if row.get("outcome") in c63.ELIGIBLE_OUTCOMES]
        rate = _rate(prepared)
        fingerprints = {row.get("c64_cohort_fingerprint") for row in prepared}
        tiers = {row.get("evaluation_tier", window.get("evaluation_tier")) for row in prepared}
        providers = {row.get("inference_backend_name", _provider(window)) for row in prepared}
        models = {row.get("llm_model", _model(window)) for row in prepared}
        product_shas = {row.get("thoth_git_sha", _sha(window, "thoth_git_sha")) for row in prepared}
        engine_shas = {row.get("basic_agent_git_sha", _sha(window, "basic_agent_git_sha")) for row in prepared}
        reasons: list[str] = []
        if invalid or missing_artifacts:
            scope = "incomplete"
            if conflicts:
                reasons.append("conflicting_plan_id")
            if metric_invalid:
                reasons.append("malformed_metrics")
            if summary.total_invalid:
                reasons.append("malformed_recognized_log")
            if missing_artifacts:
                reasons.append("missing_artifact")
        elif retrospective:
            scope = "exploratory"
            reasons.append("retrospective_window_id")
        elif not prepared and not claimed:
            scope = "insufficient"
            reasons.append("no_attributed_goals")
        elif (
            len(fingerprints) != 1
            or window.get("c64_cohort_fingerprint") not in fingerprints
            or len(providers) != 1
            or len(models) != 1
            or len(product_shas) != 1
            or len(engine_shas) != 1
            or "authoritative" not in tiers
            or len(tiers) != 1
        ):
            scope = "exploratory"
            reasons.append("cohort_or_stratum_mismatch")
        else:
            gates_ok, gate_reason = _gates_ok(window)
            episodic_ok, episodic_reason = _episodic_ok(window)
            if not gates_ok or not episodic_ok:
                scope = "exploratory"
                if gate_reason:
                    reasons.append(gate_reason)
                if episodic_reason:
                    reasons.append(episodic_reason)
            elif rate["eligible_count"] < MIN_GOALS or rate["confidence_label"] == "low":
                scope = "insufficient"
                reasons.append("below_minimum_n" if rate["eligible_count"] < MIN_GOALS else "confidence_low")
            else:
                scope = "pending"
        reports.append(
            {
                "window_id": window_id,
                "protocol_version": window.get("protocol_version"),
                "metric_schema_version": window.get("metric_schema_version"),
                "environment_schema_version": window.get("environment_schema_version"),
                "window_start_ms": window.get("window_start_ms"),
                "window_end_ms": window.get("window_end_ms"),
                "c64_cohort_fingerprint": window.get("c64_cohort_fingerprint"),
                "provider": _provider(window),
                "model": _model(window),
                "embedding_model": (_pin(window).get("model") or {}).get("embedding_model") if isinstance(_pin(window).get("model"), Mapping) else None,
                "thoth_git_sha": _sha(window, "thoth_git_sha"),
                "basic_agent_git_sha": _sha(window, "basic_agent_git_sha"),
                "attributed_count": len(prepared),
                "completed_count": sum(1 for row in prepared if row.get("outcome") == "completed"),
                "failed_count": sum(1 for row in prepared if row.get("outcome") == "failed"),
                "aborted_count": len(aborted),
                "open_at_analysis_count": len(open_rows),
                "unattributed_historical_excluded": excluded_count_for(metrics_rows),
                "denominator_n": rate["eligible_count"],
                "successes": rate["successes"],
                "success_rate": None if scope in {"exploratory", "incomplete"} else rate["success_rate"],
                "pooled": False if scope in {"exploratory", "incomplete"} else True,
                "wilson95": None if scope in {"exploratory", "incomplete"} else rate["wilson95"],
                "confidence_label": rate["confidence_label"],
                "evidence_scope": scope,
                "reasons": reasons,
                "gates": {"c3_c5_ok": _gates_ok(window)[0], "episodic_ok": _episodic_ok(window)[0]},
                "descriptive": _descriptive(eligible),
                "segments": None,
                "_rows": prepared,
                "_eligible": eligible,
                "_rate": rate,
            }
        )

    for index, report in enumerate(reports):
        if report["evidence_scope"] != "pending":
            report["trend"] = "not_evaluable"
            report["trend_reason"] = report["reasons"][0] if report["reasons"] else "not_official"
            report["delta"] = None
            report["prior_window_id"] = None
            report["segments"] = _segments(report["_rows"], None, False)
            continue
        prior, prior_reason = _qualifying_prior(report, reports[:index])
        if prior_reason == "overlapping_windows":
            report["evidence_scope"] = "exploratory"
            report["reasons"] = ["overlapping_windows"]
            report["success_rate"] = None
            report["pooled"] = False
            report["wilson95"] = None
            report["trend"] = "not_evaluable"
            report["trend_reason"] = "overlapping_windows"
            report["delta"] = None
            report["prior_window_id"] = None
            report["segments"] = _segments(report["_rows"], None, False)
            continue
        if prior is None:
            report["evidence_scope"] = "official_baseline"
            report["trend"] = "not_evaluable"
            report["trend_reason"] = "no_prior_window"
            report["delta"] = None
            report["prior_window_id"] = None
            report["segments"] = _segments(report["_rows"], None, False)
            continue
        report["evidence_scope"] = "official_longitudinal"
        trend = _trend(report["_rate"], prior["_rate"])
        report["trend"] = trend["trend"]
        report["trend_reason"] = trend["reason"]
        report["delta"] = trend["delta"]
        report["prior_window_id"] = prior["window_id"]
        report["segments"] = _segments(report["_rows"], prior["_rows"], True)

    public = [{key: value for key, value in report.items() if not key.startswith("_")} for report in reports]
    promotion = promotion_status(public)
    return {
        "protocol_version": PROTOCOL_VERSION,
        "windows": public,
        "promotion": promotion,
        "total_invalid": summary.total_invalid + metric_invalid,
        "automatic_promotion": False,
    }


def excluded_count_for(rows: list[dict[str, Any]]) -> int:
    return sum(
        1
        for row in rows
        if row.get("event") in (None, "GOAL_COGNITIVE_METRICS") and not row.get("window_id")
    )


def promotion_status(reports: list[dict[str, Any]]) -> dict[str, Any]:
    pair = None
    for later in reports:
        if later.get("evidence_scope") != "official_longitudinal":
            continue
        earlier = next((item for item in reports if item.get("window_id") == later.get("prior_window_id")), None)
        if earlier and earlier.get("evidence_scope") in {"official_baseline", "official_longitudinal"}:
            pair = (earlier["window_id"], later["window_id"])
            break
    return {
        "evidence_prerequisites_met": pair is not None,
        "window_pair": list(pair) if pair else [],
        "promoted": False,
        "owner_signed": False,
    }


def render_report(payload: Mapping[str, Any]) -> str:
    lines = [
        "# C6.4 longitudinal report",
        "",
        "This report classifies measurements. It does not promote Thoth and it does not state a cause.",
        "",
        f"- protocol_version: {payload.get('protocol_version')}",
        f"- automatic_promotion: {payload.get('automatic_promotion')}",
        f"- evidence_prerequisites_met: {payload.get('promotion', {}).get('evidence_prerequisites_met')}",
        f"- promoted: {payload.get('promotion', {}).get('promoted')}",
    ]
    for window in payload.get("windows", []):
        lines.extend(
            [
                "",
                f"## {window.get('window_id')}",
                f"- evidence_scope: {window.get('evidence_scope')}",
                f"- trend: {window.get('trend')}",
                f"- trend_reason: {window.get('trend_reason')}",
                f"- prior_window_id: {window.get('prior_window_id')}",
                f"- attributed_count: {window.get('attributed_count')}",
                f"- completed_count: {window.get('completed_count')}",
                f"- failed_count: {window.get('failed_count')}",
                f"- aborted_count: {window.get('aborted_count')}",
                f"- denominator_n: {window.get('denominator_n')}",
                f"- successes: {window.get('successes')}",
                f"- success_rate: {window.get('success_rate')}",
                f"- wilson95: {window.get('wilson95')}",
                f"- delta: {window.get('delta')}",
                f"- confidence_label: {window.get('confidence_label')}",
                f"- c64_cohort_fingerprint: {window.get('c64_cohort_fingerprint')}",
                f"- provider: {window.get('provider')}",
                f"- model: {window.get('model')}",
            ]
        )
    return "\n".join(lines) + "\n"
