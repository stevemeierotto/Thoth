#!/usr/bin/env python3
"""
test_c6_longitudinal_provenance.py — C6.3-03 Step 03-2a provenance contract tests.

Contract: docs/C6_phase3_reporting_contract.md § Provenance fields (locked)
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import Any

SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

import c6_longitudinal_provenance as prov  # noqa: E402

REPO_ROOT = SCRIPT_DIR.parent
DEFAULT_BENCHMARK_ENV = REPO_ROOT / "logs" / "benchmark_env.jsonl"


class TestFailure(Exception):
    pass


def env_row(
    run_id: str,
    *,
    thoth: str | None = None,
    basic_agent: str | None = None,
) -> dict[str, Any]:
    row: dict[str, Any] = {"event": "BENCHMARK_ENV", "run_id": run_id, "env_hash": f"env-{run_id}"}
    prov_fields: dict[str, Any] = {}
    if thoth is not None:
        prov_fields["thoth_git_sha"] = thoth
    if basic_agent is not None:
        prov_fields["basic_agent_git_sha"] = basic_agent
    if prov_fields:
        row["env"] = {"prov": prov_fields}
    return row


def test_p1_normalize_pin_value() -> None:
    cases = [
        ("abc123", "abc123"),
        ("  def456  ", "def456"),
        ("", None),
        ("unknown", None),
        ("UNKNOWN", None),
        (None, None),
        (123, None),
    ]
    for raw, expected in cases:
        actual = prov.normalize_pin_value(raw)
        if actual != expected:
            raise TestFailure(f"P1: normalize_pin_value({raw!r}) expected {expected!r}, got {actual!r}")


def test_p2_extract_provenance_pin() -> None:
    pin = prov.extract_provenance_pin(env_row("r1", thoth="aaa1111", basic_agent="bbb1111"))
    if pin != ("aaa1111", "bbb1111"):
        raise TestFailure(f"P2: expected tuple pin, got {pin}")

    if prov.extract_provenance_pin(env_row("r2")) is not None:
        raise TestFailure("P2: missing prov should exclude row")

    partial = prov.extract_provenance_pin(env_row("r3", thoth="only-thoth"))
    if partial != ("only-thoth", None):
        raise TestFailure(f"P2: partial pin expected ('only-thoth', None), got {partial}")


def test_p3_prompt_evolution_two_tuples() -> None:
    rows = [
        env_row("run-c601", thoth="aaa1111", basic_agent="bbb1111"),
        env_row("run-c602", thoth="ccc2222", basic_agent="bbb1111"),
    ]
    run_ids = {"run-c601", "run-c602"}
    if not prov.prompt_evolution_detected(rows, run_ids):
        raise TestFailure("P3: expected prompt_evolution for two distinct provenance_pin tuples")


def test_p4_prompt_evolution_single_tuple() -> None:
    rows = [
        env_row("run-c601", thoth="aaa1111", basic_agent="bbb1111"),
        env_row("run-c602", thoth="aaa1111", basic_agent="bbb1111"),
    ]
    if prov.prompt_evolution_detected(rows, {"run-c601", "run-c602"}):
        raise TestFailure("P4: identical tuples must not trigger prompt_evolution")


def test_p5_prompt_evolution_excluded_rows() -> None:
    rows = [env_row("run-c601"), env_row("run-c602", thoth="solo")]
    if prov.prompt_evolution_detected(rows, {"run-c601", "run-c602"}):
        raise TestFailure("P5: single non-excluded pin must not trigger prompt_evolution")


def test_p6_ignores_non_consumed_run_ids() -> None:
    rows = [
        env_row("run-c601", thoth="aaa1111", basic_agent="bbb1111"),
        env_row("run-other", thoth="zzz9999", basic_agent="yyy9999"),
    ]
    if prov.prompt_evolution_detected(rows, {"run-c601"}):
        raise TestFailure("P6: non-consumed run_id must not affect detection")


def test_p7_production_survey_shape() -> None:
    if not DEFAULT_BENCHMARK_ENV.is_file():
        return
    rows: list[dict[str, Any]] = []
    with DEFAULT_BENCHMARK_ENV.open("r", encoding="utf-8") as handle:
        for line in handle:
            line = line.strip()
            if not line:
                continue
            try:
                row = json.loads(line)
            except json.JSONDecodeError:
                continue
            if row.get("event") == "BENCHMARK_ENV":
                rows.append(row)
    if not rows:
        raise TestFailure("P7: expected BENCHMARK_ENV rows in production benchmark_env.jsonl")

    sample = rows[-1]
    env = sample.get("env")
    if not isinstance(env, dict):
        raise TestFailure("P7: production row missing env object")
    prov_obj = env.get("prov")
    if not isinstance(prov_obj, dict):
        raise TestFailure("P7: production row missing env.prov")
    for key in ("thoth_git_sha", "basic_agent_git_sha"):
        if key not in prov_obj:
            raise TestFailure(f"P7: production env.prov missing {key}")


def run_all_tests(verbose: bool = False) -> dict[str, bool]:
    tests = [
        ("p1_normalize", test_p1_normalize_pin_value),
        ("p2_extract_pin", test_p2_extract_provenance_pin),
        ("p3_two_tuples", test_p3_prompt_evolution_two_tuples),
        ("p4_single_tuple", test_p4_prompt_evolution_single_tuple),
        ("p5_excluded_rows", test_p5_prompt_evolution_excluded_rows),
        ("p6_consumed_only", test_p6_ignores_non_consumed_run_ids),
        ("p7_production_shape", test_p7_production_survey_shape),
    ]
    results: dict[str, bool] = {}
    for name, fn in tests:
        fn()
        results[name] = True
        if verbose:
            print(f"  {name}: ok")
    return results


def build_arg_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="C6.3-03 Step 03-2a provenance contract tests")
    parser.add_argument("--verbose", action="store_true")
    return parser


def main(argv: list[str] | None = None) -> int:
    parser = build_arg_parser()
    args = parser.parse_args(argv)
    print("C6.3-03 Step 03-2a provenance contract (RC § Provenance fields)")
    try:
        results = run_all_tests(verbose=args.verbose)
    except TestFailure as exc:
        print(str(exc), file=sys.stderr)
        return 1

    for name in results:
        if not args.verbose:
            print(f"  {name}: ok")
    print("  OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
