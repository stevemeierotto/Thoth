#!/usr/bin/env python3
"""
test_c6_longitudinal_analyzer.py — C6.3-02 analyzer regression (AC v1.0).

Contract: docs/C6_phase3_analyzer_contract.md § Test plan
"""

from __future__ import annotations

import argparse
import json
import sys
import tempfile
from pathlib import Path
from typing import Any

SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

import analyze_cognitive_longitudinal as analyzer  # noqa: E402
import c6_longitudinal_join as join  # noqa: E402
import c6_longitudinal_threats as threats  # noqa: E402
import plot_cognitive_longitudinal as longitudinal_plots  # noqa: E402

REPO_ROOT = SCRIPT_DIR.parent
DEFAULT_FIXTURES_DIR = REPO_ROOT / "tests" / "fixtures" / "cognitive_longitudinal"
AS_OF_MS = 1_800_002_000_000
FLOAT_EPS = 1e-5

FIXTURE_WINDOW = {
    "start_ms": 1_797_581_800_000,
    "end_ms": 1_800_001_000_000,
    "days": 28,
    "anchor": "official_benchmark_execution",
}


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
        return json.load(handle)


def fixture_paths(fixtures_dir: Path) -> dict[str, Path]:
    return {
        "metrics": fixtures_dir / "metrics.jsonl",
        "app_log": fixtures_dir / "app_log.jsonl",
        "decision_trace": fixtures_dir / "decision_trace.jsonl",
        "benchmark_env": fixtures_dir / "benchmark_env.jsonl",
    }


def official_fixture_paths(fixtures_dir: Path) -> dict[str, Path]:
    return {
        "metrics": fixtures_dir / "metrics_official_longitudinal.jsonl",
        "app_log": fixtures_dir / "app_log_official_longitudinal.jsonl",
        "decision_trace": fixtures_dir / "decision_trace_official_longitudinal.jsonl",
        "benchmark_env": fixtures_dir / "benchmark_env_official_longitudinal.jsonl",
    }


def run_official_analysis(fixtures_dir: Path, *, as_of_ms: int = AS_OF_MS) -> dict[str, Any]:
    paths = official_fixture_paths(fixtures_dir)
    result = analyzer.analyze_longitudinal(
        paths["metrics"],
        paths["app_log"],
        paths["decision_trace"],
        paths["benchmark_env"],
        generated_at_ms=as_of_ms,
    )
    return result.payload


def run_analysis(
    fixtures_dir: Path,
    *,
    cohort: str = "official",
    as_of_ms: int = AS_OF_MS,
    decision_trace: Path | None = None,
    prior_longitudinal_path: Path | None = None,
    benchmark_env: Path | None = None,
) -> dict[str, Any]:
    paths = fixture_paths(fixtures_dir)
    trace_path = decision_trace or paths["decision_trace"]
    env_path = benchmark_env or paths["benchmark_env"]
    result = analyzer.analyze_longitudinal(
        paths["metrics"],
        paths["app_log"],
        trace_path,
        env_path,
        cohort=cohort,
        generated_at_ms=as_of_ms,
        prior_longitudinal_path=prior_longitudinal_path,
    )
    return result.payload


def analyze_result(
    fixtures_dir: Path,
    *,
    cohort: str = "official",
    as_of_ms: int = AS_OF_MS,
    decision_trace: Path | None = None,
    prior_longitudinal_path: Path | None = None,
    benchmark_env: Path | None = None,
) -> analyzer.AnalysisResult:
    paths = fixture_paths(fixtures_dir)
    trace_path = decision_trace or paths["decision_trace"]
    env_path = benchmark_env or paths["benchmark_env"]
    return analyzer.analyze_longitudinal(
        paths["metrics"],
        paths["app_log"],
        trace_path,
        env_path,
        cohort=cohort,
        generated_at_ms=as_of_ms,
        prior_longitudinal_path=prior_longitudinal_path,
    )


def assert_close(actual: float, expected: float, path: str, eps: float = FLOAT_EPS) -> None:
    if abs(actual - expected) > eps:
        raise TestFailure(f"{path}: expected {expected}, got {actual}")


def normalize_payload(payload: dict[str, Any]) -> dict[str, Any]:
    cleaned = json.loads(json.dumps(payload))
    cleaned.pop("analyzer_commit_hash", None)
    cleaned.pop("summary_written_at_ms", None)
    return cleaned


def test_merger(fixtures_dir: Path) -> None:
    paths = fixture_paths(fixtures_dir)
    records, artifacts, _ = join.join_goals_with_load_counts(
        paths["metrics"],
        paths["app_log"],
        paths["decision_trace"],
        paths["benchmark_env"],
    )
    merged = analyzer.merge_analysis_goals(records, artifacts.validated_metrics, cohort="exploratory")
    if len(merged) != len(records):
        raise TestFailure(f"A1: expected {len(records)} merged goals, got {len(merged)}")


def test_fingerprint_exclusion(fixtures_dir: Path) -> None:
    payload = run_analysis(fixtures_dir)
    run_ids = payload["reproducibility"]["benchmark_run_ids_consumed"]
    if "run-c609" in run_ids:
        raise TestFailure("A2: fingerprint-mismatch run-c609 must be excluded from official rollup")


def test_window_bounds(fixtures_dir: Path) -> None:
    golden = load_json(fixtures_dir / "analyzer_trend_golden.json")["anchor"]
    payload = run_analysis(fixtures_dir)
    window = payload["window"]
    if window["end_ms"] != golden["window_end_ms"]:
        raise TestFailure(f"A3: end_ms expected {golden['window_end_ms']}, got {window['end_ms']}")
    if window["start_ms"] != golden["window_start_ms"]:
        raise TestFailure(f"A3: start_ms expected {golden['window_start_ms']}, got {window['start_ms']}")
    if window["anchor"] != "official_benchmark_execution":
        raise TestFailure("A3: expected official_benchmark_execution anchor")


def test_allowlist_negative(fixtures_dir: Path) -> None:
    env_rows, _ = join.load_jsonl(fixtures_dir / "benchmark_env.jsonl")
    end_ms, used_official, _ = analyzer.resolve_window_anchor(env_rows, AS_OF_MS)
    if not used_official:
        raise TestFailure("A3b: expected official anchor from allowlisted terminal event")
    if end_ms == 1_999_999_999_999:
        raise TestFailure("A3b: FOO_COMPLETE must not become anchor")


def test_wilson_golden(fixtures_dir: Path) -> None:
    spec = load_json(fixtures_dir / "analyzer_trend_golden.json")["wilson"]
    rate, ci_low, ci_high = analyzer.wilson_ci(spec["successes"], spec["n"])
    assert_close(rate, spec["rate"], "A4.rate")
    assert_close(ci_low, spec["ci_low"], "A4.ci_low", eps=1e-4)
    assert_close(ci_high, spec["ci_high"], "A4.ci_high", eps=1e-4)


def test_confidence_labels(fixtures_dir: Path) -> None:
    golden = load_json(fixtures_dir / "analyzer_trend_golden.json")["confidence"]
    for key in ("high", "low"):
        spec = golden[key]
        _, ci_low, ci_high = analyzer.wilson_ci(spec["successes"], spec["n"])
        label = analyzer.confidence_label_for(spec["n"], ci_low, ci_high)
        if label != spec["expected_label"]:
            raise TestFailure(f"A4{'b' if key == 'high' else 'c'}: expected {spec['expected_label']}, got {label}")


def test_trend_labels(fixtures_dir: Path) -> None:
    golden = load_json(fixtures_dir / "analyzer_trend_golden.json")["trend"]
    mapping = {
        "improving": "A4d",
        "stable_delta": "A4e",
        "stable_ci": "A4f",
        "stable_confidence": "A4g",
        "declining": "A4h",
    }
    for name, test_id in mapping.items():
        spec = golden[name]
        trend = analyzer.directional_trend_label(
            spec["current_rate"],
            spec["prior_rate"],
            spec["prior_ci_low"],
            spec["prior_ci_high"],
            spec["confidence_label"],
        )
        if trend != spec["expected"]:
            raise TestFailure(f"{test_id}: expected {spec['expected']}, got {trend}")


def test_exploratory_scope(fixtures_dir: Path) -> None:
    payload = run_analysis(fixtures_dir)
    if payload["evidence_scope"] != "exploratory_only":
        raise TestFailure(f"A5: expected exploratory_only, got {payload['evidence_scope']}")


def test_incomplete_report(fixtures_dir: Path) -> None:
    payload = run_analysis(fixtures_dir, decision_trace=fixtures_dir / "missing_decision_trace.jsonl")
    if payload["report_completeness"] != "incomplete":
        raise TestFailure(f"A6: expected incomplete, got {payload['report_completeness']}")
    if not any(flag.startswith("missing_artifact:") for flag in payload["flags"]):
        raise TestFailure("A6: expected missing_artifact flag")


def test_full_jsonl_history(fixtures_dir: Path) -> None:
    with tempfile.TemporaryDirectory() as tmp:
        out = Path(tmp)
        payload1 = normalize_payload(run_analysis(fixtures_dir, as_of_ms=AS_OF_MS))
        analyzer.write_outputs(payload1, out, summary_written_at_ms=AS_OF_MS)
        payload2 = json.loads(json.dumps(payload1))
        payload2["generated_at_ms"] = AS_OF_MS + 1
        payload2["warnings"] = ["synthetic_second_run"]
        analyzer.write_outputs(payload2, out, summary_written_at_ms=AS_OF_MS + 1)

        jsonl_path = out / "cognitive_longitudinal.jsonl"
        lines = [json.loads(line) for line in jsonl_path.read_text(encoding="utf-8").splitlines() if line.strip()]
        if len(lines) != 2:
            raise TestFailure(f"A7: expected 2 JSONL rows, got {len(lines)}")
        if lines[0]["generated_at_ms"] != AS_OF_MS:
            raise TestFailure("A7: first JSONL row mutated")
        if lines[0].get("warnings") == lines[1].get("warnings"):
            raise TestFailure("A7: expected distinct payloads per append")

        summary = json.loads((out / "cognitive_longitudinal_summary.json").read_text(encoding="utf-8"))
        if summary["generated_at_ms"] != AS_OF_MS + 1:
            raise TestFailure("A7: summary must match latest run only")


def test_reproducibility_false(fixtures_dir: Path) -> None:
    payload = run_analysis(fixtures_dir)
    if payload["categories"]["safety"]["reproducibility_ok"] is not False:
        raise TestFailure("A8: expected reproducibility_ok false on fingerprint fixture")


def test_idempotence(fixtures_dir: Path) -> None:
    first = normalize_payload(run_analysis(fixtures_dir))
    second = normalize_payload(run_analysis(fixtures_dir))
    if first != second:
        raise TestFailure("A9: identical inputs must produce identical analytical payload")


def test_golden_summary(fixtures_dir: Path) -> None:
    golden_path = fixtures_dir / "analyzer_golden_summary.json"
    if not golden_path.is_file():
        raise TestFailure(f"missing golden summary: {golden_path}")
    golden = load_json(golden_path)
    actual = normalize_payload(run_analysis(fixtures_dir))
    if actual != golden:
        raise TestFailure("golden summary mismatch — run analyzer to refresh if intentional")


def test_f1_official_golden_summary(fixtures_dir: Path) -> None:
    golden_path = fixtures_dir / "analyzer_golden_official_longitudinal.json"
    if not golden_path.is_file():
        raise TestFailure(f"F1: missing official golden: {golden_path}")
    payload = run_official_analysis(fixtures_dir)
    if payload["report_completeness"] != "complete":
        raise TestFailure(f"F1: expected complete report, got {payload['report_completeness']!r}")
    if payload["evidence_scope"] != "official_longitudinal":
        raise TestFailure(f"F1: expected official_longitudinal, got {payload['evidence_scope']!r}")
    golden = load_json(golden_path)
    actual = normalize_payload(payload)
    if actual != golden:
        raise TestFailure(
            "F1: official golden mismatch — identify source (fixture, analyzer, or regression) "
            "before running --write-official-golden"
        )


def test_i1_expanded_threats(fixtures_dir: Path) -> None:
    payload = run_analysis(fixtures_dir)
    disclosed = payload["threats_disclosed"]
    for threat_id in ("runtime_environment_drift", "prompt_evolution"):
        if threat_id not in disclosed:
            raise TestFailure(f"I1: expected {threat_id!r} in threats_disclosed, got {disclosed}")


def test_i2_delegation_parity(fixtures_dir: Path) -> None:
    result = analyze_result(fixtures_dir)
    expected = threats.detect_threats(result.threat_inputs)
    actual = result.payload["threats_disclosed"]
    if actual != expected:
        raise TestFailure(f"I2: threats_disclosed {actual} != detect_threats(_build_threat_inputs(...)) {expected}")


def test_i3_protocol_revision_prior(fixtures_dir: Path) -> None:
    prior_path = fixtures_dir / "prior_longitudinal.jsonl"
    payload = run_analysis(fixtures_dir, prior_longitudinal_path=prior_path)
    if "protocol_revision_mid_window" not in payload["threats_disclosed"]:
        raise TestFailure(
            f"I3: expected protocol_revision_mid_window in threats_disclosed, got {payload['threats_disclosed']}"
        )


def test_i4_threats_sorted(fixtures_dir: Path) -> None:
    disclosed = run_analysis(fixtures_dir)["threats_disclosed"]
    if disclosed != sorted(disclosed):
        raise TestFailure(f"I4: threats_disclosed must be sorted, got {disclosed}")


def test_i5_threat_registry(fixtures_dir: Path) -> None:
    disclosed = run_analysis(fixtures_dir)["threats_disclosed"]
    for threat_id in disclosed:
        if threat_id not in threats.THREAT_IDS:
            raise TestFailure(f"I5: {threat_id!r} not in THREAT_IDS registry")


def test_g1_default_missing_gate_evidence(fixtures_dir: Path) -> None:
    payload = run_analysis(fixtures_dir)
    flags = payload["flags"]
    if "benchmark_regression_not_wired" in flags:
        raise TestFailure("G1: benchmark_regression_not_wired must be removed")
    for harness in ("reflection_ab", "robustness", "episodic_learning"):
        expected = f"gate_evidence_missing:{harness}"
        if expected not in flags:
            raise TestFailure(f"G1: expected {expected!r} in flags, got {flags}")
    safety = payload["categories"]["safety"]
    if safety["benchmark_regression"] is not False:
        raise TestFailure("G1: expected benchmark_regression false on default fixture")
    if safety["official_gates_green"] is not False:
        raise TestFailure("G1: expected official_gates_green false on default fixture")


def test_g2_aborted_sets_regression(fixtures_dir: Path) -> None:
    env_path = fixtures_dir / "benchmark_env_gates_aborted.jsonl"
    payload = run_analysis(fixtures_dir, benchmark_env=env_path)
    if payload["categories"]["safety"]["benchmark_regression"] is not True:
        raise TestFailure("G2: expected benchmark_regression true for ABORTED terminal")
    if "benchmark_gate_failure" not in payload["threats_disclosed"]:
        raise TestFailure("G2: expected benchmark_gate_failure in threats_disclosed")


def test_g3_official_gates_green() -> None:
    env_rows = (
        {
            "event": "BENCHMARK_ENV",
            "run_id": "run-gates-green",
            "env_hash": "env-gates-green",
            "env": {
                "corpus": {"fingerprint": "corpus-fp-fixture-1"},
                "model": {"model_hash": "model-hash-fixture-1"},
                "runtime": {"tier": "full"},
            },
        },
        {"event": "REFLECTION_AB_COMPLETE", "run_id": "run-gates-green", "ts": 1_799_000_000_000},
        {"event": "ROBUSTNESS_COMPLETE", "run_id": "run-gates-green", "ts": 1_799_100_000_000},
        {"event": "EPISODIC_LEARNING_COMPLETE", "run_id": "run-gates-green", "ts": 1_799_200_000_000},
    )
    gate_eval = analyzer.evaluate_safety_gates(env_rows, FIXTURE_WINDOW, reproducibility_ok=True)
    if not gate_eval.official_gates_green:
        raise TestFailure(f"G3: expected official_gates_green true, got {gate_eval}")
    if gate_eval.benchmark_regression:
        raise TestFailure("G3: expected benchmark_regression false")


def test_g4_non_full_tier_ignored() -> None:
    env_rows = (
        {
            "event": "BENCHMARK_ENV",
            "run_id": "run-dev",
            "env_hash": "env-dev",
            "env": {"runtime": {"tier": "dev"}, "model": {}, "corpus": {}},
        },
        {"event": "REFLECTION_AB_ABORTED", "run_id": "run-dev", "ts": 1_799_000_000_000},
    )
    gate_eval = analyzer.evaluate_safety_gates(env_rows, FIXTURE_WINDOW, reproducibility_ok=True)
    if gate_eval.benchmark_regression:
        raise TestFailure("G4: non-full tier ABORTED must not set benchmark_regression")


def test_g5_unknown_event_ignored(fixtures_dir: Path) -> None:
    payload = run_analysis(fixtures_dir)
    if payload["categories"]["safety"]["benchmark_regression"] is not False:
        raise TestFailure("G5: FOO_COMPLETE must not affect benchmark_regression")


def orchestrate_payload(
    payload: dict[str, Any],
    output_dir: Path,
    plot_dir: Path,
    *,
    write_report: bool = True,
    write_plots: bool = True,
) -> None:
    analyzer.write_outputs(
        payload,
        output_dir,
        summary_written_at_ms=AS_OF_MS,
        options=analyzer.WriteOptions(
            write_report=write_report,
            write_plots=write_plots,
            plot_dir=plot_dir,
        ),
    )


def test_o1_default_orchestration_writes_artifacts(fixtures_dir: Path) -> None:
    payload = normalize_payload(run_analysis(fixtures_dir))
    with tempfile.TemporaryDirectory() as tmp:
        out = Path(tmp) / "output"
        plot_dir = Path(tmp) / "plots"
        orchestrate_payload(payload, out, plot_dir)

        for name in ("cognitive_longitudinal.jsonl", "cognitive_longitudinal_summary.json", "cognitive_longitudinal_report.md"):
            path = out / name
            if not path.is_file():
                raise TestFailure(f"O1: expected {name} to exist")

        plot_probe = longitudinal_plots._import_matplotlib()
        if plot_probe is not None:
            for name in longitudinal_plots.PLOT_FILENAMES:
                path = plot_dir / name
                if not path.is_file() or path.stat().st_size == 0:
                    raise TestFailure(f"O1: expected plot {name}")


def test_o2_no_report_skips_markdown(fixtures_dir: Path) -> None:
    payload = normalize_payload(run_analysis(fixtures_dir))
    with tempfile.TemporaryDirectory() as tmp:
        out = Path(tmp) / "output"
        plot_dir = Path(tmp) / "plots"
        orchestrate_payload(payload, out, plot_dir, write_report=False)

        if (out / "cognitive_longitudinal_report.md").exists():
            raise TestFailure("O2: report must be skipped")
        if not (out / "cognitive_longitudinal.jsonl").is_file():
            raise TestFailure("O2: JSONL must still be written")
        if not (out / "cognitive_longitudinal_summary.json").is_file():
            raise TestFailure("O2: summary must still be written")


def test_o3_no_plots_skips_pngs_and_flags(fixtures_dir: Path) -> None:
    payload = normalize_payload(run_analysis(fixtures_dir))
    with tempfile.TemporaryDirectory() as tmp:
        out = Path(tmp) / "output"
        plot_dir = Path(tmp) / "plots"
        orchestrate_payload(payload, out, plot_dir, write_plots=False)

        for name in longitudinal_plots.PLOT_FILENAMES:
            if (plot_dir / name).exists():
                raise TestFailure(f"O3: plot {name} must not exist")

        summary = load_json(out / "cognitive_longitudinal_summary.json")
        if any(flag.startswith("plots_skipped:") for flag in summary.get("flags", [])):
            raise TestFailure(f"O3: summary must not contain plots_skipped flags, got {summary.get('flags')}")


def test_o4_write_order(fixtures_dir: Path) -> None:
    payload = normalize_payload(run_analysis(fixtures_dir))
    with tempfile.TemporaryDirectory() as tmp:
        out = Path(tmp) / "output"
        plot_dir = Path(tmp) / "plots"
        orchestrate_payload(payload, out, plot_dir)

        jsonl_path = out / "cognitive_longitudinal.jsonl"
        summary_path = out / "cognitive_longitudinal_summary.json"
        report_path = out / "cognitive_longitudinal_report.md"
        jsonl_mtime = jsonl_path.stat().st_mtime_ns
        report_mtime = report_path.stat().st_mtime_ns
        summary_mtime = summary_path.stat().st_mtime_ns
        if jsonl_mtime > report_mtime:
            raise TestFailure("O4: JSONL must be written before markdown report")

        summary = load_json(summary_path)
        has_plot_skip = any(flag.startswith("plots_skipped:") for flag in summary.get("flags", []))
        if has_plot_skip:
            if report_mtime > summary_mtime:
                raise TestFailure("O4: final summary patch must follow markdown report")
        elif summary_mtime > report_mtime:
            raise TestFailure("O4: phase-1 summary must be written before markdown report")


def test_o5_dry_run_writes_nothing(fixtures_dir: Path) -> None:
    paths = fixture_paths(fixtures_dir)
    with tempfile.TemporaryDirectory() as tmp:
        out = Path(tmp) / "output"
        argv = [
            "--metrics",
            str(paths["metrics"]),
            "--app-log",
            str(paths["app_log"]),
            "--decision-trace",
            str(paths["decision_trace"]),
            "--benchmark-env",
            str(paths["benchmark_env"]),
            "--output-dir",
            str(out),
            "--as-of-ms",
            str(AS_OF_MS),
            "--dry-run",
        ]
        rc = analyzer.main(argv)
        if rc != 0:
            raise TestFailure(f"O5: expected exit 0, got {rc}")
        if out.exists() and any(out.iterdir()):
            raise TestFailure("O5: dry-run must not create output files")


def test_o6_incomplete_jsonl_immutable_summary_patched(fixtures_dir: Path) -> None:
    payload = normalize_payload(
        run_analysis(fixtures_dir, decision_trace=fixtures_dir / "missing_decision_trace.jsonl")
    )
    if payload["report_completeness"] != "incomplete":
        raise TestFailure("O6: expected incomplete fixture")
    analytical_flags = list(payload["flags"])

    with tempfile.TemporaryDirectory() as tmp:
        out = Path(tmp) / "output"
        plot_dir = Path(tmp) / "plots"
        orchestrate_payload(payload, out, plot_dir)

        jsonl_lines = [
            json.loads(line)
            for line in (out / "cognitive_longitudinal.jsonl").read_text(encoding="utf-8").splitlines()
            if line.strip()
        ]
        if jsonl_lines[0]["flags"] != analytical_flags:
            raise TestFailure("O6: JSONL flags must match analytical payload only")

        for name in longitudinal_plots.PLOT_FILENAMES:
            if (plot_dir / name).exists():
                raise TestFailure(f"O6: incomplete run must not write {name}")

        summary = load_json(out / "cognitive_longitudinal_summary.json")
        if "plots_skipped:incomplete_report" not in summary.get("flags", []):
            raise TestFailure(f"O6: summary must contain plots_skipped:incomplete_report, got {summary.get('flags')}")


def run_all_tests(fixtures_dir: Path, verbose: bool = False) -> dict[str, bool]:
    tests = [
        ("merger", lambda: test_merger(fixtures_dir)),
        ("fingerprint_exclusion", lambda: test_fingerprint_exclusion(fixtures_dir)),
        ("window_bounds", lambda: test_window_bounds(fixtures_dir)),
        ("allowlist_negative", lambda: test_allowlist_negative(fixtures_dir)),
        ("wilson_ci", lambda: test_wilson_golden(fixtures_dir)),
        ("confidence_labels", lambda: test_confidence_labels(fixtures_dir)),
        ("trend_labels", lambda: test_trend_labels(fixtures_dir)),
        ("exploratory_scope", lambda: test_exploratory_scope(fixtures_dir)),
        ("incomplete_report", lambda: test_incomplete_report(fixtures_dir)),
        ("full_jsonl_history", lambda: test_full_jsonl_history(fixtures_dir)),
        ("reproducibility_false", lambda: test_reproducibility_false(fixtures_dir)),
        ("idempotence", lambda: test_idempotence(fixtures_dir)),
        ("i1_expanded_threats", lambda: test_i1_expanded_threats(fixtures_dir)),
        ("i2_delegation_parity", lambda: test_i2_delegation_parity(fixtures_dir)),
        ("i3_protocol_revision_prior", lambda: test_i3_protocol_revision_prior(fixtures_dir)),
        ("i4_threats_sorted", lambda: test_i4_threats_sorted(fixtures_dir)),
        ("i5_threat_registry", lambda: test_i5_threat_registry(fixtures_dir)),
        ("g1_missing_gate_evidence", lambda: test_g1_default_missing_gate_evidence(fixtures_dir)),
        ("g2_aborted_regression", lambda: test_g2_aborted_sets_regression(fixtures_dir)),
        ("g3_official_gates_green", test_g3_official_gates_green),
        ("g4_non_full_ignored", test_g4_non_full_tier_ignored),
        ("g5_unknown_event_ignored", lambda: test_g5_unknown_event_ignored(fixtures_dir)),
        ("o1_default_orchestration", lambda: test_o1_default_orchestration_writes_artifacts(fixtures_dir)),
        ("o2_no_report", lambda: test_o2_no_report_skips_markdown(fixtures_dir)),
        ("o3_no_plots", lambda: test_o3_no_plots_skips_pngs_and_flags(fixtures_dir)),
        ("o4_write_order", lambda: test_o4_write_order(fixtures_dir)),
        ("o5_dry_run", lambda: test_o5_dry_run_writes_nothing(fixtures_dir)),
        ("o6_incomplete_jsonl_immutable", lambda: test_o6_incomplete_jsonl_immutable_summary_patched(fixtures_dir)),
        ("golden_summary", lambda: test_golden_summary(fixtures_dir)),
        ("f1_official_golden_summary", lambda: test_f1_official_golden_summary(fixtures_dir)),
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


def write_official_golden(fixtures_dir: Path) -> None:
    golden_path = fixtures_dir / "analyzer_golden_official_longitudinal.json"
    payload = normalize_payload(run_official_analysis(fixtures_dir))
    golden_path.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
    print(f"Wrote {golden_path}")


def build_arg_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="C6.3-02 analyzer regression (AC v1.0)")
    parser.add_argument("--fixtures-dir", type=Path, default=None)
    parser.add_argument("--verbose", action="store_true")
    parser.add_argument(
        "--write-official-golden",
        action="store_true",
        help="Write analyzer_golden_official_longitudinal.json only (maintainer opt-in)",
    )
    return parser


def main(argv: list[str] | None = None) -> int:
    parser = build_arg_parser()
    args = parser.parse_args(argv)

    try:
        fixtures_dir = resolve_fixtures_dir(args.fixtures_dir)
    except FileNotFoundError as exc:
        print(str(exc), file=sys.stderr)
        return 2

    if args.write_official_golden:
        write_official_golden(fixtures_dir)
        return 0

    print("C6.3-02 analyzer regression + C6.3-03 (I1–I5 + G1–G5 + O1–O6 + H10 golden + F1 official)")
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
