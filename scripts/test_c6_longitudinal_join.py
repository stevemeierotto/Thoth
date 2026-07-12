#!/usr/bin/env python3
"""
test_c6_longitudinal_join.py — C6.3-01 Step 3 golden regression (JC v1.0).

Contract: docs/C6_phase3_join_contract.md § Step 3
"""

from __future__ import annotations

import argparse
import copy
import json
import sys
from pathlib import Path
from typing import Any

SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

import c6_longitudinal_join as join  # noqa: E402

REPO_ROOT = SCRIPT_DIR.parent
DEFAULT_FIXTURES_DIR = REPO_ROOT / "tests" / "fixtures" / "cognitive_longitudinal"
MAX_FAILURE_LINES = 20


class TestFailure(Exception):
    pass


def resolve_fixtures_dir(path: Path | None) -> Path:
    fixtures = path if path is not None else DEFAULT_FIXTURES_DIR
    fixtures = fixtures.resolve()
    if not fixtures.is_dir():
        raise FileNotFoundError(f"fixtures directory not found: {fixtures}")
    return fixtures


def load_golden(fixtures_dir: Path) -> dict[str, Any]:
    golden_path = fixtures_dir / "golden_join_results.json"
    if not golden_path.is_file():
        raise FileNotFoundError(f"golden file not found: {golden_path}")
    with golden_path.open("r", encoding="utf-8") as handle:
        return json.load(handle)


def load_fixture_rows(fixtures_dir: Path) -> tuple[list[dict], list[dict], list[dict], list[dict]]:
    metrics, metrics_exists = join.load_jsonl(fixtures_dir / "metrics.jsonl")
    if not metrics_exists:
        raise FileNotFoundError(f"missing metrics fixture: {fixtures_dir / 'metrics.jsonl'}")

    app_log, app_exists = join.load_jsonl(fixtures_dir / "app_log.jsonl")
    trace, trace_exists = join.load_jsonl(fixtures_dir / "decision_trace.jsonl")
    env, env_exists = join.load_jsonl(fixtures_dir / "benchmark_env.jsonl")

    if not app_exists:
        raise FileNotFoundError(f"missing app_log fixture: {fixtures_dir / 'app_log.jsonl'}")
    if not trace_exists:
        raise FileNotFoundError(f"missing decision_trace fixture: {fixtures_dir / 'decision_trace.jsonl'}")
    if not env_exists:
        raise FileNotFoundError(f"missing benchmark_env fixture: {fixtures_dir / 'benchmark_env.jsonl'}")

    return metrics, app_log, trace, env


def public_dicts_by_plan_id(records: list[join.JoinedGoalRecord], verbose: bool = False) -> dict[str, dict]:
    return {record.plan_id: record.to_public_dict(verbose=verbose) for record in records}


def canonical_serialization(public_by_plan_id: dict[str, dict]) -> str:
    ordered = [public_by_plan_id[plan_id] for plan_id in sorted(public_by_plan_id)]
    return json.dumps(ordered, sort_keys=True, separators=(",", ":"))


def diff_values(path: str, expected: Any, actual: Any, out: list[str]) -> None:
    if expected == actual:
        return
    out.append(f"FAIL {path}")
    out.append(f"  expected: {json.dumps(expected, sort_keys=True)}")
    out.append(f"  actual:   {json.dumps(actual, sort_keys=True)}")


def diff_public_dict(
    plan_id: str,
    case_id: str,
    expected: dict[str, Any],
    actual: dict[str, Any],
) -> list[str]:
    failures: list[str] = []
    prefix = f"{case_id} {plan_id}"

    top_keys = {"plan_id", "session_id", "goal_started_at_ms", "benchmark_run_id"}
    for key in sorted(top_keys):
        path = f"{prefix} {key}"
        diff_values(path, expected.get(key), actual.get(key), failures)

    for segment in ("plan_reused", "strategy", "consolidation", "environment"):
        expected_segment = expected.get(segment, {})
        actual_segment = actual.get(segment, {})
        if not isinstance(expected_segment, dict):
            expected_segment = {}
        if not isinstance(actual_segment, dict):
            actual_segment = {}
        all_keys = sorted(set(expected_segment) | set(actual_segment))
        for field_name in all_keys:
            path = f"{prefix} {segment}.{field_name}"
            diff_values(
                path,
                expected_segment.get(field_name),
                actual_segment.get(field_name),
                failures,
            )

    return failures


def run_full_join(
    metric_rows: list[dict],
    app_log_rows: list[dict],
    trace_rows: list[dict],
    env_rows: list[dict],
    *,
    app_log_exists: bool = True,
    trace_exists: bool = True,
    env_exists: bool = True,
) -> tuple[list[join.JoinedGoalRecord], join.JoinArtifacts]:
    records, artifacts = join.join_goals_from_records(
        metric_rows,
        app_log_rows,
        trace_rows,
        env_rows,
        app_log_exists=app_log_exists,
        trace_exists=trace_exists,
        env_exists=env_exists,
    )
    return records, artifacts


def test_golden_regression(golden: dict[str, Any], public_by_plan_id: dict[str, dict]) -> None:
    expected_records = golden.get("records", {})
    if not isinstance(expected_records, dict):
        raise TestFailure("golden.records is not an object")

    case_map = golden.get("case_map", {})
    expected_ids = set(expected_records)
    actual_ids = set(public_by_plan_id)

    failures: list[str] = []
    missing = sorted(expected_ids - actual_ids)
    extra = sorted(actual_ids - expected_ids)
    for plan_id in missing:
        case_id = case_map.get(plan_id, "C6-??")
        failures.append(f"FAIL {case_id} {plan_id} missing from join output")
    for plan_id in extra:
        failures.append(f"FAIL unknown {plan_id} extra in join output")

    for plan_id in sorted(expected_ids & actual_ids):
        case_id = case_map.get(plan_id, "C6-??")
        failures.extend(
            diff_public_dict(
                plan_id,
                case_id,
                expected_records[plan_id],
                public_by_plan_id[plan_id],
            )
        )

    if failures:
        raise TestFailure("\n".join(failures[:MAX_FAILURE_LINES]) + (
            f"\n... and {len(failures) - MAX_FAILURE_LINES} more" if len(failures) > MAX_FAILURE_LINES else ""
        ))

    if len(public_by_plan_id) != len(expected_records):
        raise TestFailure(
            f"record count mismatch: expected {len(expected_records)}, got {len(public_by_plan_id)}"
        )


def test_deterministic_output(public_by_plan_id: dict[str, dict], golden: dict[str, Any]) -> None:
    expected_records = golden.get("records", {})
    sorted_actual_keys = sorted(public_by_plan_id)
    sorted_expected_keys = sorted(expected_records)
    if sorted_actual_keys != sorted_expected_keys:
        raise TestFailure(
            f"plan_id ordering mismatch:\n  expected keys: {sorted_expected_keys}\n  actual keys:   {sorted_actual_keys}"
        )

    first = canonical_serialization(public_by_plan_id)
    second = canonical_serialization(dict(public_by_plan_id))
    if first != second:
        raise TestFailure("canonical serialization is not stable across repeated dumps")


def test_idempotence(
    metric_rows: list[dict],
    app_log_rows: list[dict],
    trace_rows: list[dict],
    env_rows: list[dict],
) -> None:
    records_one, _ = run_full_join(metric_rows, app_log_rows, trace_rows, env_rows)
    records_two, _ = run_full_join(metric_rows, app_log_rows, trace_rows, env_rows)
    public_one = public_dicts_by_plan_id(records_one)
    public_two = public_dicts_by_plan_id(records_two)
    if public_one != public_two:
        failures: list[str] = []
        for plan_id in sorted(set(public_one) | set(public_two)):
            if public_one.get(plan_id) != public_two.get(plan_id):
                failures.append(f"FAIL idempotence {plan_id}")
        raise TestFailure("\n".join(failures[:MAX_FAILURE_LINES]))


def test_input_immutability(
    metric_rows: list[dict],
    app_log_rows: list[dict],
    trace_rows: list[dict],
    env_rows: list[dict],
) -> None:
    metrics_copy = copy.deepcopy(metric_rows)
    app_copy = copy.deepcopy(app_log_rows)
    trace_copy = copy.deepcopy(trace_rows)
    env_copy = copy.deepcopy(env_rows)

    run_full_join(metric_rows, app_log_rows, trace_rows, env_rows)

    if metric_rows != metrics_copy:
        raise TestFailure("metrics rows mutated during join")
    if app_log_rows != app_copy:
        raise TestFailure("app_log rows mutated during join")
    if trace_rows != trace_copy:
        raise TestFailure("decision_trace rows mutated during join")
    if env_rows != env_copy:
        raise TestFailure("benchmark_env rows mutated during join")


def test_validation_smoke(artifacts: join.JoinArtifacts, records: list[join.JoinedGoalRecord]) -> None:
    if artifacts.validation.total_invalid != 0:
        raise TestFailure(f"expected total_invalid == 0, got {artifacts.validation.total_invalid}")
    if artifacts.context.missing_artifacts:
        raise TestFailure(f"expected no missing artifacts, got {sorted(artifacts.context.missing_artifacts)}")
    if artifacts.validation.issues:
        raise TestFailure(f"expected no validation issues, got {len(artifacts.validation.issues)}")

    plan_ids = [record.plan_id for record in records]
    if len(plan_ids) != len(set(plan_ids)):
        raise TestFailure("duplicate plan_id in join output")

    validated_ids = [row.get("plan_id") for row in artifacts.validated_metrics]
    if len(validated_ids) != len(set(validated_ids)):
        raise TestFailure("duplicate plan_id in validated metrics")


def test_missing_decision_trace(
    metric_rows: list[dict],
    app_log_rows: list[dict],
    env_rows: list[dict],
) -> None:
    records, artifacts = run_full_join(
        metric_rows,
        app_log_rows,
        [],
        env_rows,
        trace_exists=False,
    )
    if "decision_trace" not in artifacts.context.missing_artifacts:
        raise TestFailure("expected decision_trace in missing_artifacts")

    record = next((item for item in records if item.plan_id == "plan-c603"), None)
    if record is None:
        raise TestFailure("plan-c603 missing from join output for C6-05 case")

    if record.consolidation.join_status != join.JoinStatus.MISSING_ARTIFACT:
        raise TestFailure(
            f"plan-c603 consolidation expected MISSING_ARTIFACT, got {record.consolidation.join_status}"
        )
    if record.consolidation.to_public_dict()["post_consolidation"] is not None:
        raise TestFailure("plan-c603 post_consolidation should be null in public output")

    if record.strategy.join_status != join.JoinStatus.RESOLVED:
        raise TestFailure(f"plan-c603 strategy expected RESOLVED, got {record.strategy.join_status}")
    if record.environment.join_status != join.JoinStatus.RESOLVED:
        raise TestFailure(f"plan-c603 environment expected RESOLVED, got {record.environment.join_status}")


def run_all_tests(fixtures_dir: Path, verbose: bool) -> dict[str, bool]:
    golden = load_golden(fixtures_dir)
    metric_rows, app_log_rows, trace_rows, env_rows = load_fixture_rows(fixtures_dir)

    records, artifacts = run_full_join(metric_rows, app_log_rows, trace_rows, env_rows)
    public_by_plan_id = public_dicts_by_plan_id(records)

    results: dict[str, bool] = {}
    tests = [
        ("golden_cases", lambda: test_golden_regression(golden, public_by_plan_id)),
        ("deterministic_output", lambda: test_deterministic_output(public_by_plan_id, golden)),
        ("idempotence", lambda: test_idempotence(metric_rows, app_log_rows, trace_rows, env_rows)),
        (
            "input_immutability",
            lambda: test_input_immutability(metric_rows, app_log_rows, trace_rows, env_rows),
        ),
        ("validation_smoke", lambda: test_validation_smoke(artifacts, records)),
        (
            "missing_artifact_case",
            lambda: test_missing_decision_trace(metric_rows, app_log_rows, env_rows),
        ),
    ]

    for name, fn in tests:
        try:
            fn()
            results[name] = True
        except TestFailure as exc:
            results[name] = False
            print(str(exc), file=sys.stderr)
            raise

    expected_count = len(golden.get("records", {}))
    results["golden_count"] = expected_count
    return results


def print_success_banner(results: dict[str, bool]) -> None:
    count = results.get("golden_count", 0)
    print(f"C6.3-01 join regression ({join.CONTRACT_VERSION})")
    print(f"  golden_cases:           {count}/{count}")
    print("  deterministic_output:     ok")
    print("  idempotence:              ok")
    print("  input_immutability:       ok")
    print("  validation_smoke:         ok")
    print("  missing_artifact_case:    ok")
    print("  OK")


def build_arg_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="C6.3-01 join golden regression tests")
    parser.add_argument(
        "--fixtures-dir",
        type=Path,
        default=None,
        help=f"Fixture directory (default: {DEFAULT_FIXTURES_DIR})",
    )
    parser.add_argument("--verbose", action="store_true", help="Show detailed failure output")
    return parser


def main(argv: list[str] | None = None) -> int:
    parser = build_arg_parser()
    args = parser.parse_args(argv)

    try:
        fixtures_dir = resolve_fixtures_dir(args.fixtures_dir)
        results = run_all_tests(fixtures_dir, args.verbose)
    except FileNotFoundError as exc:
        print(str(exc), file=sys.stderr)
        return 2
    except json.JSONDecodeError as exc:
        print(f"invalid golden JSON: {exc}", file=sys.stderr)
        return 2
    except TestFailure as exc:
        print(str(exc), file=sys.stderr)
        return 1

    print_success_banner(results)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
