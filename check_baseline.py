#!/usr/bin/env python3
"""
check_baseline.py — Validate TEST_SUITE log signals (TC-02–TC-07).

Usage:
    python3 check_baseline.py \\
        --trace agent_workspace/decision_trace.jsonl \\
        --bench agent_workspace/grag_benchmark.jsonl \\
        --applog agent_workspace/app_log.jsonl

Prefer headless run: ./build/debug/tests/run_test_suite
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
    return scoring_type in ("grag", "grag_hybrid", "cosine")


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


def main():
    parser = argparse.ArgumentParser(description="Check Thoth TEST_SUITE signals from logs.")
    parser.add_argument("--trace", default="agent_workspace/decision_trace.jsonl")
    parser.add_argument("--bench", default="agent_workspace/grag_benchmark.jsonl")
    parser.add_argument("--applog", default="agent_workspace/app_log.jsonl")
    args = parser.parse_args()

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
    p = result(
        "RETRIEVAL_DIAGNOSTICS in trace",
        "RETRIEVAL_DIAGNOSTICS" in event_names,
        "UI/diagnostics path not exercised — run headless run_test_suite or GUI TC-04",
    )
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
