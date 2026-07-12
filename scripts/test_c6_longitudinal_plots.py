#!/usr/bin/env python3
"""
test_c6_longitudinal_plots.py — C6.3-03 plot regression (RC v1.0 § Step 03-4).

Contract: docs/C6_phase3_reporting_contract.md § Test plan (P1–P8)
"""

from __future__ import annotations

import argparse
import copy
import json
import sys
import tempfile
from pathlib import Path
from typing import Any
from unittest import mock

SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

import plot_cognitive_longitudinal as plots  # noqa: E402

REPO_ROOT = SCRIPT_DIR.parent
DEFAULT_FIXTURES_DIR = REPO_ROOT / "tests" / "fixtures" / "cognitive_longitudinal"
GOLDEN_SUMMARY = DEFAULT_FIXTURES_DIR / "analyzer_golden_summary.json"


class TestFailure(Exception):
    pass


def resolve_fixtures_dir(path: Path | None) -> Path:
    fixtures = path if path is not None else DEFAULT_FIXTURES_DIR
    fixtures = fixtures.resolve()
    if not fixtures.is_dir():
        raise FileNotFoundError(f"fixtures directory not found: {fixtures}")
    return fixtures


def load_json(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8") as handle:
        data = json.load(handle)
    if not isinstance(data, dict):
        raise TestFailure(f"expected JSON object in {path}")
    return data


def assert_plot_flags_only(flags: tuple[str, ...]) -> None:
    for flag in flags:
        if not flag.startswith("plots_skipped:"):
            raise TestFailure(f"plot module emitted non-plot flag: {flag!r}")


def test_p1_complete_fixture_writes_three_pngs() -> None:
    if not GOLDEN_SUMMARY.is_file():
        raise TestFailure(f"missing golden summary: {GOLDEN_SUMMARY}")
    payload = load_json(GOLDEN_SUMMARY)
    with tempfile.TemporaryDirectory() as tmp:
        plot_dir = Path(tmp)
        result = plots.write_longitudinal_plots(payload, plot_dir)
        assert_plot_flags_only(result.flags)
        if not result.matplotlib_available:
            raise TestFailure("P1: expected matplotlib available in test environment")
        if len(result.written_paths) != 3:
            raise TestFailure(f"P1: expected 3 PNGs, got {result.written_paths}")
        for name in plots.PLOT_FILENAMES:
            path = plot_dir / name
            if not path.is_file() or path.stat().st_size == 0:
                raise TestFailure(f"P1: missing or empty plot file: {path}")


def test_p2_incomplete_skips_all() -> None:
    payload = load_json(GOLDEN_SUMMARY)
    payload["report_completeness"] = "incomplete"
    with tempfile.TemporaryDirectory() as tmp:
        plot_dir = Path(tmp)
        result = plots.write_longitudinal_plots(payload, plot_dir)
        if result.written_paths:
            raise TestFailure(f"P2: expected no PNGs, got {result.written_paths}")
        if result.flags != ("plots_skipped:incomplete_report",):
            raise TestFailure(f"P2: expected incomplete skip flag, got {result.flags}")
        assert_plot_flags_only(result.flags)


def test_p3_matplotlib_unavailable() -> None:
    payload = load_json(GOLDEN_SUMMARY)
    with tempfile.TemporaryDirectory() as tmp:
        plot_dir = Path(tmp)
        with mock.patch.object(plots, "_import_matplotlib", return_value=None):
            result = plots.write_longitudinal_plots(payload, plot_dir)
        if result.written_paths:
            raise TestFailure(f"P3: expected no PNGs, got {result.written_paths}")
        if result.flags != ("plots_skipped:matplotlib_unavailable",):
            raise TestFailure(f"P3: expected matplotlib skip flag, got {result.flags}")
        if result.matplotlib_available:
            raise TestFailure("P3: expected matplotlib_available=False")
        assert_plot_flags_only(result.flags)


def test_p4_segment_entries_match_segments() -> None:
    payload = load_json(GOLDEN_SUMMARY)
    entries = plots.segment_success_entries(payload)
    segments = payload.get("segments", {})
    if len(entries) != len(segments):
        raise TestFailure(f"P4: expected {len(segments)} segment entries, got {len(entries)}")
    expected_keys = sorted(segments)
    actual_keys = [key for key, _ in entries]
    if actual_keys != expected_keys:
        raise TestFailure(f"P4: segment labels {actual_keys} != sorted keys {expected_keys}")


def test_p5_success_rate_values() -> None:
    payload = load_json(GOLDEN_SUMMARY)
    rate, ci_low, ci_high = plots.performance_ci_values(payload)
    performance = payload["categories"]["performance"]
    if rate != performance["success_rate"]:
        raise TestFailure("P5: success_rate mismatch")
    if ci_low != performance["ci_low"]:
        raise TestFailure("P5: ci_low mismatch")
    if ci_high != performance["ci_high"]:
        raise TestFailure("P5: ci_high mismatch")
    with tempfile.TemporaryDirectory() as tmp:
        result = plots.write_longitudinal_plots(payload, Path(tmp))
        trend_path = Path(tmp) / plots.PLOT_FILENAMES[0]
        if trend_path not in result.written_paths and result.matplotlib_available:
            raise TestFailure("P5: success_rate_trend.png not written")


def test_p6_efficiency_p50_values() -> None:
    payload = load_json(GOLDEN_SUMMARY)
    wall_clock, tokens = plots.efficiency_p50_values(payload)
    efficiency = payload["categories"]["efficiency"]
    if wall_clock != efficiency["total_wall_clock_ms_p50"]:
        raise TestFailure("P6: wall_clock p50 mismatch")
    if tokens != efficiency["total_tokens_p50"]:
        raise TestFailure("P6: tokens p50 mismatch")
    with tempfile.TemporaryDirectory() as tmp:
        result = plots.write_longitudinal_plots(payload, Path(tmp))
        eff_path = Path(tmp) / plots.PLOT_FILENAMES[2]
        if eff_path not in result.written_paths and result.matplotlib_available:
            raise TestFailure("P6: efficiency_p50.png not written")


def test_p7_empty_segments_annotation() -> None:
    payload = copy.deepcopy(load_json(GOLDEN_SUMMARY))
    payload["segments"] = {}
    with tempfile.TemporaryDirectory() as tmp:
        plot_dir = Path(tmp)
        result = plots.write_longitudinal_plots(payload, plot_dir)
        if not result.matplotlib_available:
            raise TestFailure("P7: expected matplotlib available")
        segment_path = plot_dir / plots.PLOT_FILENAMES[1]
        if not segment_path.is_file():
            raise TestFailure("P7: expected segment_success.png for empty segments")
        if "plots_skipped:empty_segments" in result.flags:
            raise TestFailure("P7: must not emit plots_skipped:empty_segments")
        assert_plot_flags_only(result.flags)


def test_p8_idempotent_overwrite() -> None:
    payload = load_json(GOLDEN_SUMMARY)
    with tempfile.TemporaryDirectory() as tmp:
        plot_dir = Path(tmp)
        first = plots.write_longitudinal_plots(payload, plot_dir)
        if not first.matplotlib_available:
            raise TestFailure("P8: expected matplotlib available")
        mtime_first = {name: (plot_dir / name).stat().st_mtime_ns for name in plots.PLOT_FILENAMES}
        second = plots.write_longitudinal_plots(payload, plot_dir)
        if first.written_paths != second.written_paths:
            raise TestFailure("P8: written path tuple changed on re-run")
        for name in plots.PLOT_FILENAMES:
            path = plot_dir / name
            if not path.is_file():
                raise TestFailure(f"P8: missing file after re-run: {path}")
            if path.stat().st_mtime_ns < mtime_first[name]:
                raise TestFailure(f"P8: file not overwritten: {path}")


def run_all_tests(fixtures_dir: Path, verbose: bool = False) -> dict[str, bool]:
    del fixtures_dir  # golden path is fixed for P1–P8

    tests = [
        ("p1_complete_three_pngs", test_p1_complete_fixture_writes_three_pngs),
        ("p2_incomplete_skip", test_p2_incomplete_skips_all),
        ("p3_matplotlib_unavailable", test_p3_matplotlib_unavailable),
        ("p4_segment_entries", test_p4_segment_entries_match_segments),
        ("p5_success_rate_values", test_p5_success_rate_values),
        ("p6_efficiency_p50", test_p6_efficiency_p50_values),
        ("p7_empty_segments", test_p7_empty_segments_annotation),
        ("p8_idempotent_overwrite", test_p8_idempotent_overwrite),
    ]

    results: dict[str, bool] = {}
    for name, fn in tests:
        try:
            fn()
            results[name] = True
            if verbose:
                print(f"  {name}: ok")
        except TestFailure as exc:
            results[name] = False
            print(str(exc), file=sys.stderr)
            raise

    return results


def build_arg_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="C6.3-03 plot regression (RC § Step 03-4)")
    parser.add_argument("--fixtures-dir", type=Path, default=None)
    parser.add_argument("--verbose", action="store_true")
    return parser


def main(argv: list[str] | None = None) -> int:
    parser = build_arg_parser()
    args = parser.parse_args(argv)

    try:
        fixtures_dir = resolve_fixtures_dir(args.fixtures_dir)
    except FileNotFoundError as exc:
        print(str(exc), file=sys.stderr)
        return 2

    print("C6.3-03 plot module (RC § Step 03-4 / P1–P8)")
    try:
        results = run_all_tests(fixtures_dir, verbose=args.verbose)
    except TestFailure:
        return 1

    for name in results:
        if not args.verbose:
            print(f"  {name}: ok")
    print("  OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
