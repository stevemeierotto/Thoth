#!/usr/bin/env python3
"""
test_c6_longitudinal_reporting.py — C6.3-03 reporting regression (RC v1.0).

Contract: docs/C6_phase3_reporting_contract.md § Test plan (R1–R9)
Layer A (R3–R7): protocol / research contract validation
Layer B (R8–R9): engineering / infrastructure validation
"""

from __future__ import annotations

import argparse
import copy
import json
import re
import sys
import tempfile
from pathlib import Path
from typing import Any

SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

import analyze_cognitive_longitudinal as analyzer  # noqa: E402
import c6_longitudinal_report as report  # noqa: E402
import plot_cognitive_longitudinal as longitudinal_plots  # noqa: E402

REPO_ROOT = SCRIPT_DIR.parent
DEFAULT_FIXTURES_DIR = REPO_ROOT / "tests" / "fixtures" / "cognitive_longitudinal"
AS_OF_MS = 1_800_002_000_000
PINNED_COMMIT = "golden"

REQUIRED_HEADER_FIELDS = (
    "protocol_version",
    "metric_schema_version",
    "analyzer_version",
    "analyzer_commit_hash",
    "generated_at_ms",
    "report_completeness",
    "evidence_scope",
    "confidence_label",
    "window.start_ms",
    "window.end_ms",
    "window.days",
    "window.anchor",
    "reproducibility.input_log_path",
    "reproducibility.benchmark_run_ids_consumed",
    "threat_schema_version",
)

THREAT_LINE_RE = re.compile(r"^- `([^`]+)` — (.+)$")


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


def fixture_paths(fixtures_dir: Path) -> dict[str, Path]:
    return {
        "metrics": fixtures_dir / "metrics.jsonl",
        "app_log": fixtures_dir / "app_log.jsonl",
        "decision_trace": fixtures_dir / "decision_trace.jsonl",
        "benchmark_env": fixtures_dir / "benchmark_env.jsonl",
    }


def run_analysis(
    fixtures_dir: Path,
    *,
    decision_trace: Path | None = None,
    metrics: Path | None = None,
    benchmark_env: Path | None = None,
    as_of_ms: int = AS_OF_MS,
) -> dict[str, Any]:
    paths = fixture_paths(fixtures_dir)
    trace_path = decision_trace or paths["decision_trace"]
    metrics_path = metrics or paths["metrics"]
    env_path = benchmark_env or paths["benchmark_env"]
    result = analyzer.analyze_longitudinal(
        metrics_path,
        paths["app_log"],
        trace_path,
        env_path,
        generated_at_ms=as_of_ms,
    )
    return result.payload


def pin_payload(payload: dict[str, Any]) -> dict[str, Any]:
    pinned = copy.deepcopy(payload)
    pinned["analyzer_commit_hash"] = PINNED_COMMIT
    pinned["generated_at_ms"] = AS_OF_MS
    return pinned


def normalize_whitespace(text: str) -> str:
    lines = [line.rstrip() for line in text.strip().splitlines()]
    return "\n".join(lines) + "\n"


def extract_header_field(text: str, field: str) -> str | None:
    pattern = rf"^- \*\*{re.escape(field)}:\*\* (.+)$"
    for line in text.splitlines():
        match = re.match(pattern, line)
        if match:
            return match.group(1)
    return None


def expected_section_keys(payload: dict[str, Any]) -> list[str]:
    keys = list(report.REPORT_SECTIONS)
    if payload.get("report_completeness") != "incomplete":
        keys.remove("incomplete_banner")
    return keys


def extract_threat_entries(markdown: str) -> list[tuple[str, str]]:
    entries: list[tuple[str, str]] = []
    in_threats = False
    for line in markdown.splitlines():
        if line.startswith("## Threats to Validity"):
            in_threats = True
            continue
        if in_threats and line.startswith("## "):
            break
        if not in_threats:
            continue
        match = THREAT_LINE_RE.match(line)
        if match:
            entries.append((match.group(1), match.group(2)))
    return entries


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


def run_gate_golden_cases(fixtures_dir: Path) -> dict[str, dict[str, Any]]:
    golden_path = fixtures_dir / "gate_status_golden.json"
    if not golden_path.is_file():
        raise TestFailure(f"missing gate status golden: {golden_path}")
    spec = load_json(golden_path)
    cases = spec.get("cases")
    if not isinstance(cases, dict):
        raise TestFailure("gate_status_golden.json must contain cases object")

    paths = fixture_paths(fixtures_dir)
    results: dict[str, dict[str, Any]] = {}
    for case_name, case in cases.items():
        if not isinstance(case, dict):
            raise TestFailure(f"gate case {case_name!r} must be an object")
        env_name = case.get("benchmark_env")
        if not isinstance(env_name, str):
            raise TestFailure(f"gate case {case_name!r} missing benchmark_env")
        metrics_path = paths["metrics"]
        metrics_name = case.get("metrics")
        if isinstance(metrics_name, str):
            metrics_path = fixtures_dir / metrics_name
        payload = run_analysis(
            fixtures_dir,
            metrics=metrics_path,
            benchmark_env=fixtures_dir / env_name,
        )
        results[case_name] = payload
    return results


def test_r1_markdown_header(fixtures_dir: Path) -> None:
    payload = pin_payload(run_analysis(fixtures_dir))
    rendered = report.header_slice(payload)
    normalized = normalize_whitespace(rendered)

    golden_path = fixtures_dir / "report_header_golden.md"
    if not golden_path.is_file():
        raise TestFailure(f"R1: missing golden header: {golden_path}")
    golden = normalize_whitespace(golden_path.read_text(encoding="utf-8"))
    if normalized != golden:
        raise TestFailure("R1: header slice mismatch with report_header_golden.md")

    if report.DISCLAIMER not in rendered:
        raise TestFailure("R1: disclaimer not verbatim in header slice")

    for field in REQUIRED_HEADER_FIELDS:
        value = extract_header_field(rendered, field)
        if value is None:
            raise TestFailure(f"R1: missing required header field: {field}")

    if extract_header_field(rendered, "analyzer_commit_hash") != PINNED_COMMIT:
        raise TestFailure("R1: analyzer_commit_hash not pinned")
    if extract_header_field(rendered, "threat_schema_version") != report.THREAT_SCHEMA_VERSION:
        raise TestFailure("R1: threat_schema_version mismatch")


def test_r1b_section_coverage(fixtures_dir: Path) -> None:
    complete_payload = pin_payload(run_analysis(fixtures_dir))
    complete_keys = sorted(report.render_report_sections(complete_payload).keys())
    expected_complete = sorted(expected_section_keys(complete_payload))
    if complete_keys != expected_complete:
        raise TestFailure(f"R1b: complete sections expected {expected_complete}, got {complete_keys}")
    if "incomplete_banner" in complete_keys:
        raise TestFailure("R1b: complete report must not include incomplete_banner section")

    incomplete_payload = pin_payload(
        run_analysis(fixtures_dir, decision_trace=fixtures_dir / "missing_decision_trace.jsonl")
    )
    incomplete_keys = sorted(report.render_report_sections(incomplete_payload).keys())
    expected_incomplete = sorted(expected_section_keys(incomplete_payload))
    if incomplete_keys != expected_incomplete:
        raise TestFailure(f"R1b: incomplete sections expected {expected_incomplete}, got {incomplete_keys}")
    if "incomplete_banner" not in incomplete_keys:
        raise TestFailure("R1b: incomplete report must include incomplete_banner section")


def test_r2_incomplete_banner(fixtures_dir: Path) -> None:
    complete_payload = pin_payload(run_analysis(fixtures_dir))
    complete_md = report.render_markdown_report(complete_payload)
    if report.INCOMPLETE_BANNER in complete_md:
        raise TestFailure("R2: complete report must not contain incomplete banner")

    incomplete_payload = pin_payload(
        run_analysis(fixtures_dir, decision_trace=fixtures_dir / "missing_decision_trace.jsonl")
    )
    incomplete_md = report.render_markdown_report(incomplete_payload)
    if report.INCOMPLETE_BANNER not in incomplete_md:
        raise TestFailure("R2: incomplete report must contain incomplete banner")

    golden_path = fixtures_dir / "report_header_incomplete_golden.md"
    if not golden_path.is_file():
        raise TestFailure(f"R2: missing golden header: {golden_path}")
    golden = normalize_whitespace(golden_path.read_text(encoding="utf-8"))
    rendered = normalize_whitespace(report.header_slice(incomplete_payload))
    if rendered != golden:
        raise TestFailure("R2: incomplete header slice mismatch with report_header_incomplete_golden.md")


def test_r3_threat_parity(fixtures_dir: Path) -> None:
    payload = pin_payload(run_analysis(fixtures_dir))
    markdown = report.render_markdown_report(payload)
    entries = extract_threat_entries(markdown)
    expected_ids = sorted(payload.get("threats_disclosed") or [])
    actual_ids = [threat_id for threat_id, _ in entries]
    if actual_ids != expected_ids:
        raise TestFailure(f"R3: threat IDs {actual_ids} != threats_disclosed {expected_ids}")
    for threat_id, label in entries:
        expected_label = report.threat_label(threat_id)
        if label != expected_label:
            raise TestFailure(f"R3: label for {threat_id!r} expected {expected_label!r}, got {label!r}")


def test_r4_prose_guard() -> None:
    source_path = SCRIPT_DIR / "c6_longitudinal_report.py"
    source = source_path.read_text(encoding="utf-8")
    lines = source.splitlines()
    scan_chunks: list[str] = []
    skip_forbidden_tuple = False
    for line in lines:
        if line.strip().startswith("FORBIDDEN_PROSE"):
            skip_forbidden_tuple = True
            continue
        if skip_forbidden_tuple:
            if line.strip() == ")":
                skip_forbidden_tuple = False
            continue
        scan_chunks.append(line)
    scanned = "\n".join(scan_chunks).lower()
    for phrase in report.FORBIDDEN_PROSE:
        if phrase.lower() in scanned:
            raise TestFailure(f"R4: forbidden prose {phrase!r} found in template strings")


def test_r5_gate_aborted_regression(fixtures_dir: Path) -> None:
    cases = load_json(fixtures_dir / "gate_status_golden.json")["cases"]
    payloads = run_gate_golden_cases(fixtures_dir)
    spec = cases["aborted_regression"]["safety"]
    safety = payloads["aborted_regression"]["categories"]["safety"]
    for key, expected in spec.items():
        if safety.get(key) is not expected:
            raise TestFailure(f"R5: safety[{key}] expected {expected}, got {safety.get(key)}")


def test_r6_official_gates_green(fixtures_dir: Path) -> None:
    cases = load_json(fixtures_dir / "gate_status_golden.json")["cases"]
    payloads = run_gate_golden_cases(fixtures_dir)
    spec = cases["gates_green"]["safety"]
    safety = payloads["gates_green"]["categories"]["safety"]
    for key, expected in spec.items():
        if safety.get(key) is not expected:
            raise TestFailure(f"R6: safety[{key}] expected {expected}, got {safety.get(key)}")


def test_r7_missing_gate_evidence(fixtures_dir: Path) -> None:
    cases = load_json(fixtures_dir / "gate_status_golden.json")["cases"]
    payloads = run_gate_golden_cases(fixtures_dir)
    spec = cases["default_missing_gates"]
    payload = payloads["default_missing_gates"]
    safety_spec = spec["safety"]
    safety = payload["categories"]["safety"]
    for key, expected in safety_spec.items():
        if safety.get(key) is not expected:
            raise TestFailure(f"R7: safety[{key}] expected {expected}, got {safety.get(key)}")
    flags = payload.get("flags") or []
    for flag in spec.get("flags_contains", []):
        if flag not in flags:
            raise TestFailure(f"R7: expected flag {flag!r} in {flags}")


def test_r8_incomplete_plots_skipped(fixtures_dir: Path) -> None:
    payload = pin_payload(
        run_analysis(fixtures_dir, decision_trace=fixtures_dir / "missing_decision_trace.jsonl")
    )
    if payload["report_completeness"] != "incomplete":
        raise TestFailure("R8: expected incomplete fixture")
    analytical_flags = list(payload["flags"])

    with tempfile.TemporaryDirectory() as tmp:
        out = Path(tmp) / "output"
        plot_dir = Path(tmp) / "plots"
        plot_dir.mkdir(parents=True, exist_ok=True)
        orchestrate_payload(payload, out, plot_dir)

        if any(plot_dir.iterdir()):
            raise TestFailure(f"R8: plot_dir must remain empty, found {list(plot_dir.iterdir())}")
        for name in longitudinal_plots.PLOT_FILENAMES:
            if (plot_dir / name).exists():
                raise TestFailure(f"R8: plot file must not exist: {name}")

        summary = load_json(out / "cognitive_longitudinal_summary.json")
        if "plots_skipped:incomplete_report" not in summary.get("flags", []):
            raise TestFailure(f"R8: summary missing plots_skipped flag, got {summary.get('flags')}")

        jsonl_lines = [
            json.loads(line)
            for line in (out / "cognitive_longitudinal.jsonl").read_text(encoding="utf-8").splitlines()
            if line.strip()
        ]
        if jsonl_lines[0]["flags"] != analytical_flags:
            raise TestFailure("R8: JSONL flags must match analytical payload only")
        if any(flag.startswith("plots_skipped:") for flag in jsonl_lines[0]["flags"]):
            raise TestFailure("R8: JSONL must not contain plots_skipped flags")


def test_r9_required_artifacts(fixtures_dir: Path) -> None:
    payload = pin_payload(run_analysis(fixtures_dir))
    with tempfile.TemporaryDirectory() as tmp:
        out = Path(tmp) / "output"
        plot_dir = Path(tmp) / "plots"
        orchestrate_payload(payload, out, plot_dir)

        required = (
            "cognitive_longitudinal.jsonl",
            "cognitive_longitudinal_summary.json",
            "cognitive_longitudinal_report.md",
        )
        for name in required:
            path = out / name
            if not path.is_file() or path.stat().st_size == 0:
                raise TestFailure(f"R9: required artifact missing or empty: {name}")

        if longitudinal_plots._import_matplotlib() is not None:
            for name in longitudinal_plots.PLOT_FILENAMES:
                path = plot_dir / name
                if not path.is_file() or path.stat().st_size == 0:
                    raise TestFailure(f"R9: expected optional plot {name} when matplotlib available")


def run_all_tests(fixtures_dir: Path, verbose: bool = False) -> dict[str, bool]:
    tests = [
        ("r1_markdown_header", lambda: test_r1_markdown_header(fixtures_dir)),
        ("r1b_section_coverage", lambda: test_r1b_section_coverage(fixtures_dir)),
        ("r2_incomplete_banner", lambda: test_r2_incomplete_banner(fixtures_dir)),
        ("r3_threat_parity", lambda: test_r3_threat_parity(fixtures_dir)),
        ("r4_prose_guard", test_r4_prose_guard),
        ("r5_gate_aborted", lambda: test_r5_gate_aborted_regression(fixtures_dir)),
        ("r6_gates_green", lambda: test_r6_official_gates_green(fixtures_dir)),
        ("r7_missing_gate_evidence", lambda: test_r7_missing_gate_evidence(fixtures_dir)),
        ("r8_incomplete_plots", lambda: test_r8_incomplete_plots_skipped(fixtures_dir)),
        ("r9_required_artifacts", lambda: test_r9_required_artifacts(fixtures_dir)),
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
    parser = argparse.ArgumentParser(description="C6.3-03 reporting regression (RC v1.0 R1–R9)")
    parser.add_argument("--fixtures-dir", type=Path, default=None)
    parser.add_argument("--verbose", action="store_true")
    parser.add_argument(
        "--write-goldens",
        action="store_true",
        help="Regenerate header golden fixtures (maintainer only)",
    )
    return parser


def write_goldens(fixtures_dir: Path) -> None:
    complete = pin_payload(run_analysis(fixtures_dir))
    incomplete = pin_payload(
        run_analysis(fixtures_dir, decision_trace=fixtures_dir / "missing_decision_trace.jsonl")
    )
    (fixtures_dir / "report_header_golden.md").write_text(report.header_slice(complete), encoding="utf-8")
    (fixtures_dir / "report_header_incomplete_golden.md").write_text(
        report.header_slice(incomplete), encoding="utf-8"
    )
    print(f"Wrote goldens under {fixtures_dir}")


def main(argv: list[str] | None = None) -> int:
    parser = build_arg_parser()
    args = parser.parse_args(argv)

    try:
        fixtures_dir = resolve_fixtures_dir(args.fixtures_dir)
    except FileNotFoundError as exc:
        print(str(exc), file=sys.stderr)
        return 2

    if args.write_goldens:
        write_goldens(fixtures_dir)
        return 0

    print("C6.3-03 reporting regression (RC v1.0 R1–R9)")
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
