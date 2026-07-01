#!/usr/bin/env python3
"""
check_baseline.py — Validate TEST_SUITE log signals (TC-02–TC-07).

Usage:
    python3 check_baseline.py \\
        --trace agent_workspace/decision_trace.jsonl \\
        --bench agent_workspace/grag_benchmark.jsonl \\
        --applog agent_workspace/app_log.jsonl

    python3 check_baseline.py --require-env
        Opt-in: require logs/benchmark_env.latest.json with run_id, environment_hash,
        index_hash; optionally cross-check terminal harness event in benchmark_env.jsonl.

Prefer headless run:
  ./build/debug/tests/run_test_suite --dev   # fast (~10s)
  ./build/debug/tests/run_test_suite --full  # Ollama regression
"""

import argparse
import json
import sys
from pathlib import Path


def load_jsonl(path):
    entries = []
    try:
        with open(path, "r", encoding="utf-8") as f:
            for line in f:
                line = line.strip()
                if not line:
                    continue
                try:
                    entries.append(json.loads(line))
                except json.JSONDecodeError:
                    pass
    except FileNotFoundError:
        print(f"  [WARN] File not found: {path}")
    return entries


def bench_row(entry):
    if "diagnostics" in entry and isinstance(entry["diagnostics"], dict):
        return entry["diagnostics"]
    return entry


def is_grag_scoring(scoring_type):
    return scoring_type in ("grag", "grag_hybrid", "rag_hybrid", "grag_blended_hybrid")


def diagnostics_rows(bench_entries):
    rows = []
    for row in bench_entries:
        d = bench_row(row)
        if d.get("scoring_type"):
            rows.append(d)
    return rows


def chat_retrieval_diagnostics(bench_entries):
    """Rows that prove goal-aware retrieval produced scored chunks (TC-05 GUI fallback)."""
    matches = []
    for d in diagnostics_rows(bench_entries):
        st = d.get("scoring_type", "")
        if st not in ("grag", "grag_hybrid", "rag_hybrid", "grag_blended_hybrid"):
            continue
        if float(d.get("alpha", 0.0)) <= 0.0:
            continue
        breakdowns = d.get("breakdowns", [])
        if not isinstance(breakdowns, list) or not breakdowns:
            continue
        scores = [
            float(b.get("final_score", 0.0))
            for b in breakdowns
            if isinstance(b, dict)
        ]
        if scores and max(scores) > 0.0:
            matches.append(d)
    return matches


def extract_events(trace_entries):
    events = []
    for entry in trace_entries:
        if "event_type" in entry:
            events.append({
                "name": entry["event_type"],
                "metadata": entry.get("metadata", {}),
            })
        for stage in entry.get("stages", []):
            name = stage.get("name", "")
            if name:
                events.append({"name": name, "metadata": stage.get("metadata", {})})
    return events


def get_routing_decisions(app_log_entries):
    decisions = []
    for entry in app_log_entries:
        if entry.get("event_name") == "routing_decision":
            decisions.append(entry.get("metadata", {}))
    return decisions


def result(name, passed, reason=""):
    status = "PASS ✓" if passed else "FAIL ✗"
    line = f"  {status}  {name}"
    if not passed and reason:
        line += f"\n         → {reason}"
    print(line)
    return passed


def check_benchmark_env(sidecar_path: Path, jsonl_path: Path) -> bool:
    """Opt-in E1 gate: sidecar must carry run_id, environment_hash, index_hash."""
    print("\nENV: Benchmark environment sidecar (--require-env)")
    all_passed = True

    if not sidecar_path.is_file():
        p = result("benchmark_env.latest.json exists", False, str(sidecar_path))
        return False

    try:
        sidecar = json.loads(sidecar_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        result("sidecar parses as JSON", False, str(exc))
        return False

    run_id = sidecar.get("run_id", "")
    env_hash = sidecar.get("environment_hash", "")
    index_hash = sidecar.get("index_hash", "")

    p = result("run_id non-empty", bool(run_id), "missing run_id")
    all_passed = all_passed and p
    p = result("environment_hash non-empty", bool(env_hash), "missing environment_hash")
    all_passed = all_passed and p
    p = result("index_hash non-empty", bool(index_hash), "missing index_hash")
    all_passed = all_passed and p

    if jsonl_path.is_file() and run_id:
        terminal_events = {
            "TEST_SUITE_COMPLETE",
            "TEST_SUITE_ABORTED",
            "REFLECTION_AB_COMPLETE",
            "REFLECTION_AB_ABORTED",
            "ROBUSTNESS_COMPLETE",
            "ROBUSTNESS_ABORTED",
            "CHAT_RAG_BENCHMARK_COMPLETE",
            "CHAT_RAG_BENCHMARK_ABORTED",
            "GRAG_BENCHMARK_COMPLETE",
            "GRAG_BENCHMARK_ABORTED",
        }
        matched_terminal = False
        env_hash_match = False
        for line in jsonl_path.read_text(encoding="utf-8").splitlines():
            line = line.strip()
            if not line:
                continue
            try:
                row = json.loads(line)
            except json.JSONDecodeError:
                continue
            if row.get("run_id") != run_id:
                continue
            if row.get("env_hash") == env_hash:
                env_hash_match = True
            if row.get("event") in terminal_events:
                matched_terminal = True

        p = result(
            "JSONL row shares env_hash for run_id",
            env_hash_match,
            f"no benchmark_env.jsonl row with run_id={run_id!r} and matching env_hash",
        )
        all_passed = all_passed and p
        p = result(
            "terminal harness event for run_id (optional cross-check)",
            matched_terminal,
            "no COMPLETE/ABORTED terminal event — run may have crashed or sidecar is from partial run",
        )
        all_passed = all_passed and p

    return all_passed


def main():
    parser = argparse.ArgumentParser(description="Check Thoth TEST_SUITE signals from logs.")
    parser.add_argument("--trace", default="agent_workspace/decision_trace.jsonl")
    parser.add_argument("--bench", default="agent_workspace/grag_benchmark.jsonl")
    parser.add_argument("--applog", default="agent_workspace/app_log.jsonl")
    parser.add_argument(
        "--require-env",
        action="store_true",
        help="Opt-in: require logs/benchmark_env.latest.json with run_id, environment_hash, "
        "index_hash; cross-check benchmark_env.jsonl when present (off by default)",
    )
    parser.add_argument(
        "--env-sidecar",
        default="logs/benchmark_env.latest.json",
        help="Sidecar path for --require-env (default: logs/benchmark_env.latest.json)",
    )
    parser.add_argument(
        "--env-jsonl",
        default="logs/benchmark_env.jsonl",
        help="JSONL path for --require-env cross-check (default: logs/benchmark_env.jsonl)",
    )
    args = parser.parse_args()

    if args.require_env:
        ok = check_benchmark_env(Path(args.env_sidecar), Path(args.env_jsonl))
        sys.exit(0 if ok else 1)

    print("\n══════════════════════════════════════════════")
    print("  Thoth TEST_SUITE — Log Checker")
    print("══════════════════════════════════════════════\n")

    trace = load_jsonl(args.trace)
    bench = load_jsonl(args.bench)
    applog = load_jsonl(args.applog)

    if not trace and not applog:
        print("  [ERROR] No trace or app_log entries found.")
        sys.exit(1)

    events = extract_events(trace)
    event_names = [e["name"] for e in events]
    routing = get_routing_decisions(applog)

    all_passed = True

    # TC-01
    print("TC-01: Plain chat, no goal")
    convo = [r for r in routing if r.get("routing_mode") == "CONVERSATIONAL"]
    p = result(
        "CONVERSATIONAL routing present",
        len(convo) > 0,
        "No CONVERSATIONAL routing_decision in app_log.jsonl",
    )
    all_passed = all_passed and p
    if convo:
        p = result(
            "goal_active=false on latest CONVERSATIONAL turn",
            convo[-1].get("goal_active") is False,
            "goal_active true without a goal — sync bug",
        )
        all_passed = all_passed and p

    # TC-02
    print("\nTC-02: Plan contains RETRIEVAL step")
    plan_created = [e for e in events if e["name"] == "PLAN_CREATED"]
    chosen = None
    for e in reversed(plan_created):
        plan = e["metadata"].get("plan", {})
        if len(plan.get("steps", [])) >= 2:
            chosen = plan
            break
    p = result(
        "PLAN_CREATED with >= 2 steps",
        chosen is not None,
        "No multi-step plan in trace (LLM planner may have emitted single-step plan)",
    )
    all_passed = all_passed and p

    retrieval_starts = [
        e for e in events
        if e["name"] == "STEP_STARTED" and e["metadata"].get("step_type") == 1
    ]
    p = result(
        "STEP_STARTED for RETRIEVAL exists",
        len(retrieval_starts) > 0,
        "RETRIEVAL step never started (step_type != 1)",
    )
    all_passed = all_passed and p

    ret_idx = next(
        (i for i, e in enumerate(events)
         if e["name"] == "STEP_STARTED" and e["metadata"].get("step_type") == 1),
        999,
    )
    llm_idx = next(
        (i for i, e in enumerate(events)
         if e["name"] == "STEP_STARTED" and e["metadata"].get("step_type") == 2),
        999,
    )
    p = result(
        "RETRIEVAL starts before LLM",
        ret_idx < llm_idx,
        f"Order wrong (retrieval={ret_idx}, llm={llm_idx})",
    )
    all_passed = all_passed and p

    # TC-03
    print("\nTC-03: GRAG math")
    grag_entries = []
    for row in bench:
        d = bench_row(row)
        st = d.get("scoring_type", "")
        if st in ("grag", "grag_hybrid"):
            grag_entries.append(d)
    p = result(
        "grag/grag_hybrid scoring present",
        len(grag_entries) > 0,
        "No directional scoring entries in grag_benchmark.jsonl",
    )
    all_passed = all_passed and p
    if grag_entries:
        latest = grag_entries[-1]
        alpha = float(latest.get("alpha", 0.0))
        magnitude = float(latest.get("direction_magnitude", 0.0))
        p = result(f"alpha > 0 (got {alpha})", alpha > 0.0, "alpha=0 — goal not reaching RAG")
        all_passed = all_passed and p
        p = result(
            f"direction_magnitude > 0 (got {magnitude})",
            magnitude > 0.0,
            "magnitude=0 — G and C embeddings identical",
        )
        all_passed = all_passed and p

    # TC-04
    print("\nTC-04: Chat during active goal")
    plan_aware = [r for r in routing if r.get("routing_mode") == "PLAN_AWARE"]
    p = result(
        "PLAN_AWARE routing present",
        len(plan_aware) > 0,
        "No PLAN_AWARE routing — processQuery may ignore active goal",
    )
    all_passed = all_passed and p
    if plan_aware:
        p = result(
            "goal_active=true for PLAN_AWARE entries",
            all(r.get("goal_active") is True for r in plan_aware),
            "Inconsistent goal_active on PLAN_AWARE routing",
        )
        all_passed = all_passed and p

    # TC-05
    print("\nTC-05: Retrieval diagnostics event")
    has_trace_diag = "RETRIEVAL_DIAGNOSTICS" in event_names
    bench_diag = chat_retrieval_diagnostics(bench)
    p = result(
        "Retrieval diagnostics signal present",
        has_trace_diag or len(bench_diag) > 0,
        "No RETRIEVAL_DIAGNOSTICS in trace and no scored GRAG rows in grag_benchmark.jsonl",
    )
    all_passed = all_passed and p
    if bench_diag and not has_trace_diag:
        latest = bench_diag[-1]
        scores = [
            float(b.get("final_score", 0.0))
            for b in latest.get("breakdowns", [])
            if isinstance(b, dict)
        ]
        top = max(scores) if scores else 0.0
        p = result(
            f"Non-zero chunk scores in benchmark (top={top:.4f})",
            top > 0.0,
            "GRAG benchmark row has empty or zero breakdown scores",
        )
        all_passed = all_passed and p
    elif has_trace_diag:
        p = result("RETRIEVAL_DIAGNOSTICS in trace", True)
        all_passed = all_passed and p

    # TC-06
    print("\nTC-06: No tool hallucinations")
    raw_trace = Path(args.trace).read_text(encoding="utf-8") if Path(args.trace).exists() else ""
    bad = any(t in raw_trace for t in ("gmail_read_labels", "stock_quote", "http_request"))
    p = result("No suspicious tool names in trace", not bad, "Possible tool hallucination in trace")
    all_passed = all_passed and p

    # TC-07
    print("\nTC-07: Goal lifecycle")
    p = result("PLAN_COMPLETED present", "PLAN_COMPLETED" in event_names, "Plan never completed")
    all_passed = all_passed and p

    print("\n══════════════════════════════════════════════")
    if all_passed:
        print("  RESULT: ALL CHECKS PASSED ✓")
    else:
        print("  RESULT: ONE OR MORE CHECKS FAILED ✗")
    print("══════════════════════════════════════════════\n")

    sys.exit(0 if all_passed else 1)


if __name__ == "__main__":
    main()
