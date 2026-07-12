#!/usr/bin/env python3
"""
generate_c6_official_longitudinal_fixtures.py — C6.3-06-3 maintainer tool.

Deterministically writes the official-longitudinal companion fixture corpus
(plan-olxx namespace). Not invoked by CTest or the seven-suite regression gate.

Contract: docs/C6_phase3_protocol.md § C6.3-06-3
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import Any

SCRIPT_DIR = Path(__file__).resolve().parent
REPO_ROOT = SCRIPT_DIR.parent
FIXTURES_DIR = REPO_ROOT / "tests" / "fixtures" / "cognitive_longitudinal"

GENERATOR_VERSION = "1.0.0"
CORPUS_NAME = "official_longitudinal_v1"
PROTOCOL_VERSION = "C6.3 v0.2.1"
ANALYZER_VERSION = "0.2.0"
METRIC_SCHEMA_VERSION = "1.0"

GOAL_COUNT = 32
MS_PER_DAY = 86_400_000
WINDOW_DAYS = 28
WINDOW_END_MS = 1_800_001_000_000
VALIDATE_AS_OF_MS = 1_800_002_000_000

CORPUS_FINGERPRINT = "corpus-fp-official-ol-v1"
MODEL_HASH = "model-hash-official-ol-v1"
EMBEDDING_MODEL = "nomic-embed-text"

ANCHOR_RUN_ID = "run-anchor-ol"
GATES_RUN_ID = "run-gates-ol"


def _env_block(*, env_hash: str, tier: str = "FULL") -> dict[str, Any]:
    return {
        "corpus": {"fingerprint": CORPUS_FINGERPRINT},
        "model": {"model_hash": MODEL_HASH, "embedding_model": EMBEDDING_MODEL},
        "prov": {"thoth_git_sha": "fixture-ol", "basic_agent_git_sha": "fixture-ol"},
        "runtime": {"tier": tier},
    }


def _benchmark_env_row(run_id: str, env_hash: str, *, tier: str = "FULL") -> dict[str, Any]:
    return {
        "event": "BENCHMARK_ENV",
        "run_id": run_id,
        "env_hash": env_hash,
        "env": _env_block(env_hash=env_hash, tier=tier),
    }


def _goal_started_at_ms(index: int) -> int:
    """Spread goals evenly inside the 28-day window (1-based index)."""
    window_start = WINDOW_END_MS - WINDOW_DAYS * MS_PER_DAY
    inner_start = window_start + MS_PER_DAY
    inner_end = WINDOW_END_MS - MS_PER_DAY
    if GOAL_COUNT <= 1:
        return inner_start
    step = (inner_end - inner_start) // (GOAL_COUNT - 1)
    return inner_start + (index - 1) * step


def build_metrics_rows() -> list[dict[str, Any]]:
    rows: list[dict[str, Any]] = []
    for i in range(1, GOAL_COUNT + 1):
        suffix = f"{i:02d}"
        rows.append(
            {
                "event": "GOAL_COGNITIVE_METRICS",
                "plan_id": f"plan-ol{suffix}",
                "session_id": f"session-ol{suffix}",
                "goal_started_at_ms": _goal_started_at_ms(i),
                "outcome": "completed",
                "run_id": f"run-ol{suffix}",
                "env_hash": f"env-hash-ol{suffix}",
                "plan_reused": False,
            }
        )
    return rows


def build_benchmark_env_rows() -> list[dict[str, Any]]:
    rows: list[dict[str, Any]] = []
    for i in range(1, GOAL_COUNT + 1):
        suffix = f"{i:02d}"
        rows.append(_benchmark_env_row(f"run-ol{suffix}", f"env-hash-ol{suffix}"))

    rows.append(_benchmark_env_row(ANCHOR_RUN_ID, "env-hash-anchor-ol", tier="full"))
    rows.append(
        {
            "event": "TEST_SUITE_COMPLETE",
            "run_id": ANCHOR_RUN_ID,
            "ts": WINDOW_END_MS,
            "payload": {"failed": 0, "tier": "full"},
        }
    )

    rows.append(_benchmark_env_row(GATES_RUN_ID, "env-hash-gates-ol", tier="full"))
    gate_ts = [
        WINDOW_END_MS - 10 * MS_PER_DAY,
        WINDOW_END_MS - 10 * MS_PER_DAY + 1_000_000,
        WINDOW_END_MS - 10 * MS_PER_DAY + 2_000_000,
    ]
    for event, ts in zip(
        ("REFLECTION_AB_COMPLETE", "ROBUSTNESS_COMPLETE", "EPISODIC_LEARNING_COMPLETE"),
        gate_ts,
        strict=True,
    ):
        rows.append({"event": event, "run_id": GATES_RUN_ID, "ts": ts, "payload": {"tier": "full"}})

    return rows


def write_jsonl(path: Path, rows: list[dict[str, Any]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    lines = [json.dumps(row, separators=(",", ":"), sort_keys=True) for row in rows]
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def fixture_paths() -> dict[str, Path]:
    return {
        "metrics": FIXTURES_DIR / "metrics_official_longitudinal.jsonl",
        "app_log": FIXTURES_DIR / "app_log_official_longitudinal.jsonl",
        "decision_trace": FIXTURES_DIR / "decision_trace_official_longitudinal.jsonl",
        "benchmark_env": FIXTURES_DIR / "benchmark_env_official_longitudinal.jsonl",
    }


def generate_fixtures() -> dict[str, Path]:
    paths = fixture_paths()
    write_jsonl(paths["metrics"], build_metrics_rows())
    write_jsonl(paths["benchmark_env"], build_benchmark_env_rows())
    write_jsonl(paths["app_log"], [])
    write_jsonl(paths["decision_trace"], [])
    return paths


def validate_fixtures(paths: dict[str, Path]) -> None:
    if str(SCRIPT_DIR) not in sys.path:
        sys.path.insert(0, str(SCRIPT_DIR))
    import analyze_cognitive_longitudinal as analyzer  # noqa: E402

    result = analyzer.analyze_longitudinal(
        paths["metrics"],
        paths["app_log"],
        paths["decision_trace"],
        paths["benchmark_env"],
        generated_at_ms=VALIDATE_AS_OF_MS,
    )
    payload = result.payload
    errors: list[str] = []

    if payload.get("report_completeness") != "complete":
        errors.append(f"report_completeness expected complete, got {payload.get('report_completeness')!r}")
    if payload.get("evidence_scope") != "official_longitudinal":
        errors.append(f"evidence_scope expected official_longitudinal, got {payload.get('evidence_scope')!r}")
    if payload.get("confidence_label") == "low":
        errors.append("confidence_label must not be low")
    safety = payload.get("categories", {}).get("safety", {})
    if not safety.get("reproducibility_ok"):
        errors.append("reproducibility_ok must be true")
    if safety.get("tier_contamination"):
        errors.append("tier_contamination must be false")
    if safety.get("benchmark_regression"):
        errors.append("benchmark_regression must be false")
    if not safety.get("official_gates_green"):
        errors.append("official_gates_green must be true")
    flags = payload.get("flags", [])
    if "fingerprint_mismatch" in flags:
        errors.append("fingerprint_mismatch flag must be absent")
    if any(str(f).startswith("gate_evidence_missing:") for f in flags):
        errors.append(f"gate evidence missing flags not allowed: {flags}")
    eligible = payload.get("categories", {}).get("performance", {}).get("eligible_count", 0)
    if eligible < analyzer.MIN_GOALS_LONGITUDINAL:
        errors.append(f"eligible_count {eligible} < MIN_GOALS_LONGITUDINAL {analyzer.MIN_GOALS_LONGITUDINAL}")

    if errors:
        print("Validation failed:", file=sys.stderr)
        for err in errors:
            print(f"  - {err}", file=sys.stderr)
        print(json.dumps(payload, indent=2, sort_keys=True), file=sys.stderr)
        raise SystemExit(1)

    print(
        f"Validation ok: evidence_scope=official_longitudinal eligible_count={eligible} "
        f"confidence_label={payload.get('confidence_label')}"
    )


def build_arg_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Generate C6.3-06-3 official longitudinal companion fixtures")
    parser.add_argument(
        "--validate",
        action="store_true",
        help="After writing fixtures, run analyzer smoke validation",
    )
    parser.add_argument(
        "--fixtures-dir",
        type=Path,
        default=FIXTURES_DIR,
        help="Output directory (default: tests/fixtures/cognitive_longitudinal)",
    )
    return parser


def main(argv: list[str] | None = None) -> int:
    args = build_arg_parser().parse_args(argv)
    global FIXTURES_DIR
    FIXTURES_DIR = args.fixtures_dir.resolve()

    paths = generate_fixtures()
    print(f"Wrote official longitudinal fixtures ({GOAL_COUNT} goals, {CORPUS_NAME}):")
    for key, path in paths.items():
        print(f"  {key}: {path.relative_to(REPO_ROOT)}")

    if args.validate:
        validate_fixtures(paths)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
