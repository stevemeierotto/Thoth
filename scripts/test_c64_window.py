#!/usr/bin/env python3
"""Phase 5 window attribution tests. Isolated temp files only. No inference."""

from __future__ import annotations

import sys
import tempfile
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

import c64_environment_identity as ident  # noqa: E402
import c64_window as window  # noqa: E402


class TestFailure(Exception):
    pass


START = 1_700_000_000_000
SPAN = window.WINDOW_SPAN_MS


def pin_kwargs(**overrides):
    base = {
        "evaluation_tier": "authoritative",
        "inference_backend_name": "llama_cpp",
        "thoth_git_sha": "product-sha",
        "basic_agent_git_sha": "engine-sha",
        "llm_model": "configured-llm",
        "embedding_model": "configured-embed",
        "embedding_method": "External",
        "embedding_dimension": 768,
        "corpus_fingerprint": "corpus-abc",
        "retrieval_weights": {"query": 0.4, "direction": 0.4, "trajectory": -0.05, "keyword": 0.3},
        "cognitive_runtime": {"max_hot_messages": 50, "max_hot_age_days": 30, "prune_batch_size": 10},
    }
    base.update(overrides)
    return base


def gates():
    common = {
        "passed": True,
        "evaluation_tier": "mock",
        "thoth_git_sha": "product-sha",
        "basic_agent_git_sha": "engine-sha",
    }
    return dict(common), dict(common)


def opened(directory: Path, **overrides):
    c3, c5 = gates()
    kwargs = pin_kwargs(**overrides)
    return window.open_window(directory / "window.json", opened_at_ms=START, c3_gate=c3, c5_gate=c5, **kwargs)


def test_boundaries_and_membership() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        record = opened(Path(tmp))
        fingerprint = record["c64_cohort_fingerprint"]
        assert record["window_end_ms"] - record["window_start_ms"] == SPAN
        cases = [
            ("before", START - 1, False),
            ("start", START, True),
            ("inside", START + 5, True),
            ("end", START + SPAN, True),
            ("after", START + SPAN + 1, False),
        ]
        for name, stamp, expect in cases:
            got = window.attribute_goal(
                record, goal_started_at_ms=stamp, current_fingerprint=fingerprint, plan_id="p1", outcome="completed"
            )
            if expect and (got is None or got["window_id"] != record["window_id"]):
                raise TestFailure(f"{name} was not inside the window")
            if not expect and got is not None:
                raise TestFailure(f"{name} was attributed")
        failed = window.attribute_goal(
            record, goal_started_at_ms=START + 5, current_fingerprint=fingerprint, plan_id="p1", outcome="failed"
        )
        aborted = window.attribute_goal(
            record, goal_started_at_ms=START + 5, current_fingerprint=fingerprint, plan_id="p1", outcome="aborted"
        )
        if failed["window_id"] != aborted["window_id"] or failed["goal_started_at_ms"] != aborted["goal_started_at_ms"]:
            raise TestFailure("outcome changed membership")
        other = window.attribute_goal(
            record, goal_started_at_ms=START + 5, current_fingerprint=fingerprint, plan_id="p2", outcome="failed"
        )
        if other["plan_id"] == failed["plan_id"]:
            raise TestFailure("retry plan_id was merged")
        if window.qualifies_as_observation("chat"):
            raise TestFailure("chat was treated as a goal observation")
        if failed["window_id"] != record["window_id"]:
            raise TestFailure("failed goal lost attribution")


def test_no_window_and_fail_closed() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        path = Path(tmp) / "window.json"
        try:
            opened(path.parent, evaluation_tier="mock")
            raise TestFailure("mock tier opened an official window")
        except window.C64WindowError:
            pass
        try:
            c3, c5 = gates()
            c3["passed"] = False
            window.open_window(path, opened_at_ms=START, c3_gate=c3, c5_gate=c5, **pin_kwargs())
            raise TestFailure("failed C3 gate opened a window")
        except window.C64WindowError:
            pass
        mock_gate = {"evaluation_id": "episodic_authoritative_v2", "evaluation_tier": "mock", "authoritative_valid": False}
        try:
            window.open_window(
                path, opened_at_ms=START, c3_gate=gates()[0], c5_gate=gates()[1], episodic_gate=mock_gate, **pin_kwargs()
            )
            raise TestFailure("mock episodic gate was accepted")
        except window.C64WindowError:
            pass
        try:
            window.open_window(
                path,
                opened_at_ms=START,
                c3_gate=gates()[0],
                c5_gate=gates()[1],
                **pin_kwargs(llm_model=""),
            )
            raise TestFailure("blank model opened a window")
        except ident.C64IdentityError:
            pass
        record = opened(Path(tmp))
        if record["official_report_eligible"] is not False:
            raise TestFailure("missing episodic gate was treated as official-report eligible")
        if record["episodic_gate"]["satisfied"] is not False:
            raise TestFailure("episodic gate was fabricated")


def test_cohort_is_frozen() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        record = opened(Path(tmp))
        other = ident.build_prospective_pin(**pin_kwargs(llm_model="other-llm"))
        assigned = window.attribute_goal(
            record,
            goal_started_at_ms=START + 1,
            current_fingerprint=other["c64_cohort_fingerprint"],
            plan_id="p-new",
        )
        if assigned is not None:
            raise TestFailure("mismatched fingerprint was attributed to the old window")
        if record["c64_cohort_fingerprint"] == other["c64_cohort_fingerprint"]:
            raise TestFailure("pin builder did not change fingerprint")
        closed = window.close_window(Path(tmp) / "window.json", closed_at_ms=START + 10)
        if closed["status"] != "closed" or closed["c64_cohort_fingerprint"] != record["c64_cohort_fingerprint"]:
            raise TestFailure("close mutated the cohort")
        if closed["window_id"] != record["window_id"]:
            raise TestFailure("close changed window_id")
        later = window.attribute_goal(
            closed,
            goal_started_at_ms=START + 1,
            current_fingerprint=record["c64_cohort_fingerprint"],
            plan_id="p-after",
        )
        if later is not None:
            raise TestFailure("closed window still accepted a goal")


def main() -> int:
    tests = [test_boundaries_and_membership, test_no_window_and_fail_closed, test_cohort_is_frozen]
    try:
        for test in tests:
            test()
    except TestFailure as exc:
        print(f"FAIL {exc}", file=sys.stderr)
        return 1
    print("c64 window attribution")
    for test in tests:
        print(f"  {test.__name__}: ok")
    print("  OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
