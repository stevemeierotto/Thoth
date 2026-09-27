#!/usr/bin/env python3
"""Deterministic C6.4 analyzer fixtures. No inference and no real window."""

from __future__ import annotations

import math
import sys
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

import analyze_cognitive_longitudinal as c63  # noqa: E402
import c64_longitudinal_analyzer as analyzer  # noqa: E402


class TestFailure(Exception):
    pass


SPAN = analyzer.WINDOW_SPAN_MS
START = 1_700_000_000_000
FINGERPRINT = "a" * 64
OTHER = "b" * 64


def gates(sha: str = "product-sha") -> tuple[dict, dict]:
    common = {
        "passed": True,
        "evaluation_tier": "mock",
        "thoth_git_sha": sha,
        "basic_agent_git_sha": "engine-sha",
    }
    return dict(common), dict(common)


def episodic(ok: bool = True) -> dict:
    if not ok:
        return {"satisfied": False}
    return {
        "satisfied": True,
        "evaluation_id": "episodic_authoritative_v2",
        "inference_backend_name": "llama_cpp",
        "llm_model": "configured-llm",
        "authoritative_valid": True,
        "evaluation_tier": "authoritative",
    }


def window(window_id: str, start: int, *, end: int | None = None, fingerprint: str = FINGERPRINT, episodic_gate: dict | None = None, c3: dict | None = None, c5: dict | None = None, sha: str = "product-sha") -> dict:
    c3_gate, c5_gate = gates(sha)
    return {
        "window_id": window_id,
        "status": "closed",
        "protocol_version": "C6.4 v1.0",
        "metric_schema_version": "1.0",
        "environment_schema_version": "c64-env-1",
        "evaluation_tier": "authoritative",
        "window_start_ms": start,
        "window_end_ms": start + SPAN if end is None else end,
        "c64_cohort_fingerprint": fingerprint,
        "pin": {
            "inference": {"backend_name": "llama_cpp"},
            "model": {"llm_model": "configured-llm", "embedding_model": "configured-embed", "embedding_method": "External", "embedding_dimension": 768},
            "prov": {"thoth_git_sha": sha, "basic_agent_git_sha": "engine-sha"},
        },
        "c3_gate": c3 if c3 is not None else c3_gate,
        "c5_gate": c5 if c5 is not None else c5_gate,
        "episodic_gate": episodic() if episodic_gate is None else episodic_gate,
    }


def goals(window_id: str, start: int, completed: int, failed: int = 0, aborted: int = 0, **overrides) -> list[dict]:
    rows = []
    specs = [("completed", completed), ("failed", failed), ("aborted", aborted)]
    index = 0
    for outcome, count in specs:
        for _ in range(count):
            row = {
                "event": "GOAL_COGNITIVE_METRICS",
                "plan_id": f"{window_id}-{index}",
                "session_id": f"session-{window_id}",
                "goal_started_at_ms": start + 1 + index,
                "goal_finished_at_ms": start + 10 + index,
                "outcome": outcome,
                "window_id": window_id,
                "protocol_version": "C6.4 v1.0",
                "environment_schema_version": "c64-env-1",
                "evaluation_tier": "authoritative",
                "c64_cohort_fingerprint": FINGERPRINT,
                "inference_backend_name": "llama_cpp",
                "llm_model": "configured-llm",
                "thoth_git_sha": "product-sha",
                "basic_agent_git_sha": "engine-sha",
                "plan_reused": index < 9,
                "total_wall_clock_ms": 100 + index,
                "reflection_count": 1,
                "revisions_count": 0,
                "total_tokens": 50,
                "retrieved_chunk_count": 2,
                "grag_alpha": 0.4,
            }
            row.update(overrides)
            if "plan_id" not in overrides:
                row["plan_id"] = f"{window_id}-{index}"
            rows.append(row)
            index += 1
    return rows


def run(windows: list[dict], rows: list[dict], app_log: list[dict] | None = None, trace: list[dict] | None = None) -> dict:
    return analyzer.analyze_windows(windows, rows, app_log or [], trace or [])


def one(payload: dict) -> dict:
    if len(payload["windows"]) != 1:
        raise TestFailure(f"expected one window, got {len(payload['windows'])}")
    return payload["windows"][0]


def test_wilson_known_values() -> None:
    cases = [
        (0, 30, 0.0, 0.0, 0.117042),
        (30, 30, 1.0, 0.882958, 1.0),
        (15, 30, 0.5, 0.321077, 0.678923),
        (80, 100, 0.8, 0.709690, 0.868113),
        (800, 1000, 0.8, 0.774033, 0.823671),
    ]
    for successes, n, point, low, high in cases:
        got_point, got_low, got_high = c63.wilson_ci(successes, n)
        manual = _wilson(successes, n)
        for name, actual, expected in (
            ("point", got_point, point),
            ("low", got_low, low),
            ("high", got_high, high),
            ("manual_point", manual[0], point),
            ("manual_low", manual[1], low),
            ("manual_high", manual[2], high),
        ):
            if abs(actual - expected) > 1e-5:
                raise TestFailure(f"wilson {successes}/{n} {name} {actual} != {expected}")


def _wilson(successes: int, n: int) -> tuple[float, float, float]:
    z = 1.96
    p = successes / n
    z2 = z * z
    denom = 1.0 + z2 / n
    center = (p + z2 / (2.0 * n)) / denom
    margin = z * math.sqrt((p * (1.0 - p) / n + z2 / (4.0 * n * n)) / denom)
    return p, max(0.0, center - margin), min(1.0, center + margin)


def test_baseline_and_minimum_n() -> None:
    base = window("w-base", START)
    report = one(run([base], goals("w-base", START, 30)))
    if report["evidence_scope"] != "official_baseline" or report["trend"] != "not_evaluable" or report["trend_reason"] != "no_prior_window":
        raise TestFailure(f"baseline classification wrong: {report['evidence_scope']} {report['trend']} {report['trend_reason']}")
    if report["trend"] == "stable":
        raise TestFailure("baseline was stable")
    low = one(run([base], goals("w-base", START, 29)))
    if low["evidence_scope"] != "insufficient" or low["denominator_n"] != 29:
        raise TestFailure("n=29 was not insufficient")
    wide = one(run([base], goals("w-base", START, 15, failed=15)))
    if wide["evidence_scope"] != "insufficient" or wide["confidence_label"] != "low":
        raise TestFailure("wide n=30 interval was not low-confidence insufficient")


def test_denominator() -> None:
    rows = goals("w-den", START, 20, failed=10, aborted=7)
    report = one(run([window("w-den", START)], rows))
    if report["completed_count"] != 20 or report["failed_count"] != 10 or report["aborted_count"] != 7:
        raise TestFailure("outcome counts were wrong")
    if report["denominator_n"] != 30 or report["successes"] != 20 or abs(report["success_rate"] - (20 / 30)) > 1e-12:
        raise TestFailure("aborted or failed denominator was wrong")
    if report["evidence_scope"] != "official_baseline":
        raise TestFailure("mixed terminal window was not a baseline")


def test_direction() -> None:
    prior = window("w-prior", START)
    current = window("w-now", START + SPAN)
    prior_rows = goals("w-prior", START, 320, failed=80)
    exact = run([prior, current], prior_rows + goals("w-now", START + SPAN, 340, failed=60))
    below = run([prior, current], prior_rows + goals("w-now", START + SPAN, 336, failed=64))
    above = run([prior, current], prior_rows + goals("w-now", START + SPAN, 360, failed=40))
    down = run([prior, current], prior_rows + goals("w-now", START + SPAN, 280, failed=120))
    contained = run(
        [window("w-prior", START), window("w-now", START + SPAN)],
        goals("w-prior", START, 50, failed=50) + goals("w-now", START + SPAN, 56, failed=44),
    )
    checks = [
        (exact, "improving"),
        (below, "stable"),
        (above, "improving"),
        (down, "declining"),
        (contained, "stable"),
    ]
    for payload, expected in checks:
        later = payload["windows"][1]
        if later["evidence_scope"] != "official_longitudinal" or later["trend"] != expected:
            raise TestFailure(f"expected {expected}, got {later['evidence_scope']} {later['trend']} delta={later['delta']}")
        if later["prior_window_id"] != "w-prior":
            raise TestFailure("prior window was not the adjacent official window")
    if abs((340 / 400) - (320 / 400) - 0.05) > 1e-12:
        raise TestFailure("exact fixture is not a 0.05 mathematical difference")
    if exact["promotion"]["evidence_prerequisites_met"] is not True or exact["promotion"]["promoted"] is not False:
        raise TestFailure("two-window prerequisite was not reported without promotion")
    alone = run([prior], prior_rows)
    if alone["promotion"]["evidence_prerequisites_met"] or alone["promotion"]["promoted"]:
        raise TestFailure("one baseline satisfied promotion")


def test_cohort_and_overlap() -> None:
    rows = goals("w-mix", START, 30)
    rows[0]["c64_cohort_fingerprint"] = OTHER
    mixed = one(run([window("w-mix", START)], rows))
    if mixed["evidence_scope"] != "exploratory" or mixed["pooled"] or mixed["success_rate"] is not None:
        raise TestFailure("mismatched fingerprints were pooled")
    providers = goals("w-provider", START, 30)
    providers[0]["inference_backend_name"] = "ollama"
    mixed_provider = one(run([window("w-provider", START)], providers))
    if mixed_provider["evidence_scope"] != "exploratory" or mixed_provider["pooled"]:
        raise TestFailure("provider strata were pooled")
    shas = goals("w-sha", START, 30)
    shas[0]["thoth_git_sha"] = "other-product"
    if one(run([window("w-sha", START)], shas))["evidence_scope"] != "exploratory":
        raise TestFailure("product SHA mismatch was official")
    overlap_prior = window("w-old", START, end=START + SPAN + 5)
    overlap_now = window("w-new", START + SPAN)
    overlap = run([overlap_prior, overlap_now], goals("w-old", START, 30) + goals("w-new", START + SPAN, 30))
    if overlap["windows"][1]["evidence_scope"] != "exploratory" or overlap["windows"][1]["trend_reason"] != "overlapping_windows":
        raise TestFailure("overlapping windows were compared")


def test_gates_and_history() -> None:
    base_rows = goals("w-gate", START, 30)
    missing = dict(gates()[0])
    missing["passed"] = False
    if one(run([window("w-gate", START, c3=missing)], base_rows))["evidence_scope"] != "exploratory":
        raise TestFailure("failed C3 gate was official")
    wrong = dict(gates()[0])
    wrong["thoth_git_sha"] = "older-sha"
    if one(run([window("w-gate", START, c3=wrong)], base_rows))["evidence_scope"] != "exploratory":
        raise TestFailure("wrong-code C3 gate was official")
    mock = {"satisfied": True, "evaluation_tier": "mock", "evaluation_id": "episodic_authoritative_v2", "authoritative_valid": False, "inference_backend_name": "llama_cpp"}
    if one(run([window("w-gate", START, episodic_gate=mock)], base_rows))["gates"]["episodic_ok"]:
        raise TestFailure("mock episodic gate was accepted")
    if one(run([window("w-gate", START, episodic_gate=episodic(True))], base_rows))["evidence_scope"] != "official_baseline":
        raise TestFailure("synthetic authoritative episodic gate did not allow a baseline")
    historical = {"event": "GOAL_COGNITIVE_METRICS", "plan_id": "old-plan", "session_id": "old", "goal_started_at_ms": START - 50, "outcome": "completed"}
    chat = {"event": "CHAT_MESSAGE", "plan_id": "", "text": "hello"}
    report = one(run([window("w-gate", START)], base_rows + [historical, chat]))
    if report["denominator_n"] != 30 or report["unattributed_historical_excluded"] != 1:
        raise TestFailure("historical or chat row entered the official denominator")


def test_malformed_duplicate_and_segments() -> None:
    rows = goals("w-seg", START, 30)
    for index, row in enumerate(rows):
        row["plan_reused"] = index < 9
    report = one(run([window("w-seg", START)], rows))
    small = report["segments"]["plan_reused_true"]
    eligible = report["segments"]["plan_reused_false"]
    if small["eligible_count"] != 9 or small["segment_eligible"] or small["reason"] != "segment_n_below_10":
        raise TestFailure("segment n=9 was eligible")
    if not eligible["segment_eligible"] or eligible["eligible_count"] != 21 or eligible["trend"] != "not_evaluable":
        raise TestFailure("segment n>=10 was not eligible on a baseline")
    exact_rows = goals("w-seg10", START, 30)
    for index, row in enumerate(exact_rows):
        row["plan_reused"] = index < 10
    exact = one(run([window("w-seg10", START)], exact_rows))["segments"]["plan_reused_true"]
    if not exact["segment_eligible"] or exact["eligible_count"] != 10:
        raise TestFailure("segment n=10 was not eligible")
    duplicate = goals("w-dup", START, 30)
    duplicate.append(dict(duplicate[0]))
    once = one(run([window("w-dup", START)], duplicate))
    if once["denominator_n"] != 30:
        raise TestFailure("exact duplicate was counted twice")
    conflict = goals("w-conflict", START, 30)
    other = dict(conflict[0])
    other["outcome"] = "failed"
    if one(run([window("w-conflict", START)], conflict + [other]))["evidence_scope"] != "incomplete":
        raise TestFailure("conflicting plan_id stayed complete")
    malformed = goals("w-bad", START, 30)
    malformed.append({"event": "GOAL_COGNITIVE_METRICS", "window_id": "w-bad", "plan_id": "x"})
    if one(run([window("w-bad", START)], malformed))["evidence_scope"] != "incomplete":
        raise TestFailure("malformed recognized metrics stayed complete")
    app = [{"event_name": "USER_NOTE", "session_id": "", "timestamp_ms": START}]
    app.append({"event_name": "STRATEGY_INJECTION", "session_id": "", "timestamp_ms": START})
    ignored = run([window("w-ok", START)], goals("w-ok", START, 30), app_log=[{"event_name": "USER_NOTE", "timestamp_ms": START}])
    if ignored["total_invalid"] != 0 or one(ignored)["evidence_scope"] != "official_baseline":
        raise TestFailure("unrelated app-log row was invalid")
    bad_log = run([window("w-log", START)], goals("w-log", START, 30), app_log=[{"event_name": "STRATEGY_INJECTION", "session_id": "", "timestamp_ms": START}])
    if bad_log["total_invalid"] < 1 or bad_log["windows"][0]["evidence_scope"] != "incomplete":
        raise TestFailure("malformed recognized strategy row was ignored")
    text = analyzer.render_report(ignored)
    if "promote" not in text or "learned" in text.lower() or "caused" in text.lower():
        raise TestFailure("report language was not conservative")


def main() -> int:
    tests = [
        test_wilson_known_values,
        test_baseline_and_minimum_n,
        test_denominator,
        test_direction,
        test_cohort_and_overlap,
        test_gates_and_history,
        test_malformed_duplicate_and_segments,
    ]
    try:
        for test in tests:
            test()
    except TestFailure as exc:
        print(f"FAIL {exc}", file=sys.stderr)
        return 1
    print("c64 longitudinal analyzer")
    for test in tests:
        print(f"  {test.__name__}: ok")
    print("  OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
