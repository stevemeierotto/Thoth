#!/usr/bin/env python3
"""
test_c6_longitudinal_threats.py — C6.3-03 Step 03-2b threat engine regression.

Contract: docs/C6_phase3_reporting_contract.md § Step 03-2b implementation lock
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

import c6_longitudinal_threats as threats  # noqa: E402

REPO_ROOT = SCRIPT_DIR.parent
FIXTURES_DIR = REPO_ROOT / "tests" / "fixtures" / "cognitive_longitudinal"

# From analyzer_golden_summary.json benchmark_run_ids_consumed
FIXTURE_CONSUMED_RUN_IDS = (
    "run-c601",
    "run-c602",
    "run-c603",
    "run-c604",
    "run-c606",
    "run-c608",
    "run-c610",
    "run-c611",
)

FIXTURE_WINDOW = {
    "start_ms": 1_797_581_800_000,
    "end_ms": 1_800_001_000_000,
    "days": 28,
    "anchor": "official_benchmark_execution",
}


class TestFailure(Exception):
    pass


def load_jsonl(path: Path) -> list[dict[str, Any]]:
    rows: list[dict[str, Any]] = []
    with path.open("r", encoding="utf-8") as handle:
        for line in handle:
            line = line.strip()
            if not line:
                continue
            rows.append(json.loads(line))
    return rows


def base_inputs(**overrides: Any) -> threats.ThreatInputs:
    defaults = {
        "eligible_count": 8,
        "safety": {
            "reproducibility_ok": False,
            "tier_contamination": False,
            "benchmark_regression": False,
            "official_gates_green": False,
        },
        "flags": ("fingerprint_mismatch",),
        "used_official_anchor": True,
        "report_completeness": "complete",
        "env_rows": tuple(load_jsonl(FIXTURES_DIR / "benchmark_env.jsonl")),
        "benchmark_run_ids_consumed": FIXTURE_CONSUMED_RUN_IDS,
        "validation_total_invalid": 0,
        "window": FIXTURE_WINDOW,
        "prior_longitudinal_rows": None,
    }
    defaults.update(overrides)
    return threats.ThreatInputs(**defaults)


def assert_contains(threat_list: list[str], threat_id: str, label: str) -> None:
    if threat_id not in threat_list:
        raise TestFailure(f"{label}: expected {threat_id!r} in {threat_list}")


def assert_missing(threat_list: list[str], threat_id: str, label: str) -> None:
    if threat_id in threat_list:
        raise TestFailure(f"{label}: did not expect {threat_id!r} in {threat_list}")


def env_row(
    run_id: str,
    *,
    env_hash: str = "env-hash",
    model_hash: str = "model-hash-fixture-1",
    corpus_fp: str = "corpus-fp-fixture-1",
    thoth: str | None = None,
    basic_agent: str | None = None,
) -> dict[str, Any]:
    env: dict[str, Any] = {
        "corpus": {"fingerprint": corpus_fp},
        "model": {"model_hash": model_hash, "embedding_model": "nomic-embed-text"},
        "runtime": {"tier": "FULL"},
    }
    if thoth is not None or basic_agent is not None:
        prov: dict[str, Any] = {}
        if thoth is not None:
            prov["thoth_git_sha"] = thoth
        if basic_agent is not None:
            prov["basic_agent_git_sha"] = basic_agent
        env["prov"] = prov
    return {"event": "BENCHMARK_ENV", "run_id": run_id, "env_hash": env_hash, "env": env}


def test_h1_deterministic_sorted() -> None:
    inputs = base_inputs()
    first = threats.detect_threats(inputs)
    second = threats.detect_threats(inputs)
    if first != sorted(first):
        raise TestFailure("H1: output must be sorted")
    if first != second:
        raise TestFailure("H1: detect_threats must be deterministic")


def test_h2_runtime_environment_drift_fixture() -> None:
    result = threats.detect_threats(base_inputs())
    assert_contains(result, "runtime_environment_drift", "H2")


def test_h3_model_hash_drift() -> None:
    rows = (
        env_row("run-a", env_hash="env-a", model_hash="model-a"),
        env_row("run-b", env_hash="env-b", model_hash="model-b"),
    )
    inputs = base_inputs(
        env_rows=rows,
        benchmark_run_ids_consumed=("run-a", "run-b"),
        flags=(),
        safety={"reproducibility_ok": True, "tier_contamination": False},
        eligible_count=30,
    )
    result = threats.detect_threats(inputs)
    assert_contains(result, "model_hash_drift", "H3")


def test_h4_corpus_fingerprint_drift() -> None:
    rows = (
        env_row("run-a", corpus_fp="corpus-a"),
        env_row("run-b", corpus_fp="corpus-b"),
    )
    inputs = base_inputs(
        env_rows=rows,
        benchmark_run_ids_consumed=("run-a", "run-b"),
        flags=(),
        safety={"reproducibility_ok": True, "tier_contamination": False},
        eligible_count=30,
    )
    result = threats.detect_threats(inputs)
    assert_contains(result, "corpus_fingerprint_drift", "H4")


def test_h5_prompt_evolution_fixture() -> None:
    result = threats.detect_threats(base_inputs())
    assert_contains(result, "prompt_evolution", "H5")


def test_h6_protocol_revision_mid_window() -> None:
    prior_path = FIXTURES_DIR / "prior_longitudinal.jsonl"
    prior_rows = tuple(load_jsonl(prior_path))
    inputs = base_inputs(prior_longitudinal_rows=prior_rows, flags=(), eligible_count=30)
    result = threats.detect_threats(inputs)
    assert_contains(result, "protocol_revision_mid_window", "H6")


def test_h7_incomplete_data() -> None:
    inputs = base_inputs(report_completeness="incomplete", flags=("missing_artifact:decision_trace",))
    result = threats.detect_threats(inputs)
    assert_contains(result, "incomplete_data", "H7")


def test_h8_selection_bias() -> None:
    inputs_flags = base_inputs(flags=("validation_rows_dropped",))
    result_flags = threats.detect_threats(inputs_flags)
    assert_contains(result_flags, "selection_bias", "H8 flags")

    inputs_invalid = base_inputs(validation_total_invalid=2, flags=())
    result_invalid = threats.detect_threats(inputs_invalid)
    assert_contains(result_invalid, "selection_bias", "H8 invalid count")


def test_h_null_1_empty_run_ids() -> None:
    inputs = base_inputs(
        benchmark_run_ids_consumed=(),
        flags=(),
        safety={"reproducibility_ok": True, "tier_contamination": False},
        eligible_count=30,
    )
    result = threats.detect_threats(inputs)
    assert_missing(result, "model_hash_drift", "H-null-1")
    assert_missing(result, "corpus_fingerprint_drift", "H-null-1")
    assert_missing(result, "runtime_environment_drift", "H-null-1")
    assert_missing(result, "prompt_evolution", "H-null-1")


def test_h_null_2_all_null_env_fields() -> None:
    rows = (
        {"event": "BENCHMARK_ENV", "run_id": "run-a", "env_hash": "", "env": {}},
        {"event": "BENCHMARK_ENV", "run_id": "run-b", "env": {"model": {}, "corpus": {}, "runtime": {}}},
    )
    inputs = base_inputs(
        env_rows=rows,
        benchmark_run_ids_consumed=("run-a", "run-b"),
        flags=(),
        safety={"reproducibility_ok": True, "tier_contamination": False},
        eligible_count=30,
    )
    result = threats.detect_threats(inputs)
    assert_missing(result, "model_hash_drift", "H-null-2")
    assert_missing(result, "corpus_fingerprint_drift", "H-null-2")
    assert_missing(result, "runtime_environment_drift", "H-null-2")


def test_h_null_3_single_env_triple() -> None:
    rows = (
        env_row("run-a", env_hash="same-env"),
        env_row("run-b", env_hash="same-env"),
    )
    inputs = base_inputs(
        env_rows=rows,
        benchmark_run_ids_consumed=("run-a", "run-b"),
        flags=(),
        safety={"reproducibility_ok": True, "tier_contamination": False},
        eligible_count=30,
    )
    result = threats.detect_threats(inputs)
    assert_missing(result, "runtime_environment_drift", "H-null-3")


def test_h_null_4_no_prior_jsonl() -> None:
    inputs = base_inputs(prior_longitudinal_rows=None, flags=(), eligible_count=30)
    result = threats.detect_threats(inputs)
    assert_missing(result, "protocol_revision_mid_window", "H-null-4")


def test_h_null_5_registry_validation() -> None:
    result = threats.detect_threats(base_inputs())
    for threat_id in result:
        if threat_id not in threats.THREAT_IDS:
            raise TestFailure(f"H-null-5: unknown threat ID {threat_id!r}")


def test_h9_negative_runtime_drift() -> None:
    rows = (
        env_row("run-a", env_hash="env-a", model_hash="model-a"),
        env_row("run-b", env_hash="env-b", model_hash="model-b"),
    )
    inputs = base_inputs(
        env_rows=rows,
        benchmark_run_ids_consumed=("run-a", "run-b"),
        flags=(),
        safety={"reproducibility_ok": True, "tier_contamination": False},
        eligible_count=30,
    )
    result = threats.detect_threats(inputs)
    assert_contains(result, "model_hash_drift", "H9")
    assert_missing(result, "runtime_environment_drift", "H9")


def test_h10_benchmark_gate_failure() -> None:
    inputs = base_inputs(
        safety={
            "reproducibility_ok": True,
            "tier_contamination": False,
            "benchmark_regression": True,
            "official_gates_green": False,
        },
        flags=(),
        eligible_count=30,
    )
    result = threats.detect_threats(inputs)
    assert_contains(result, "benchmark_gate_failure", "H10")


def test_h11_gate_evidence_missing() -> None:
    inputs = base_inputs(
        flags=("gate_evidence_missing:reflection_ab",),
        safety={
            "reproducibility_ok": True,
            "tier_contamination": False,
            "benchmark_regression": False,
            "official_gates_green": False,
        },
        eligible_count=30,
    )
    result = threats.detect_threats(inputs)
    assert_contains(result, "gate_evidence_missing", "H11")


def run_all_tests(verbose: bool = False) -> dict[str, bool]:
    cases = [
        ("h1_deterministic_sorted", test_h1_deterministic_sorted),
        ("h2_runtime_environment_drift", test_h2_runtime_environment_drift_fixture),
        ("h3_model_hash_drift", test_h3_model_hash_drift),
        ("h4_corpus_fingerprint_drift", test_h4_corpus_fingerprint_drift),
        ("h5_prompt_evolution", test_h5_prompt_evolution_fixture),
        ("h6_protocol_revision", test_h6_protocol_revision_mid_window),
        ("h7_incomplete_data", test_h7_incomplete_data),
        ("h8_selection_bias", test_h8_selection_bias),
        ("h_null_1_empty_run_ids", test_h_null_1_empty_run_ids),
        ("h_null_2_all_null_env", test_h_null_2_all_null_env_fields),
        ("h_null_3_single_env_triple", test_h_null_3_single_env_triple),
        ("h_null_4_no_prior_jsonl", test_h_null_4_no_prior_jsonl),
        ("h_null_5_registry", test_h_null_5_registry_validation),
        ("h9_negative_runtime_drift", test_h9_negative_runtime_drift),
        ("h10_benchmark_gate_failure", test_h10_benchmark_gate_failure),
        ("h11_gate_evidence_missing", test_h11_gate_evidence_missing),
    ]
    results: dict[str, bool] = {}
    for name, fn in cases:
        fn()
        results[name] = True
        if verbose:
            print(f"  {name}: ok")
    return results


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="C6.3-03 Step 03-2b threat engine regression")
    parser.add_argument("--verbose", action="store_true")
    args = parser.parse_args(argv)

    print("C6.3-03 threat engine (RC § Step 03-2b / 03-3c)")
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
