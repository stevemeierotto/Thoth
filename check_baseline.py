#!/usr/bin/env python3
"""
check_baseline.py
Thoth Basic Agent — Automated Log Checker

Usage:
    python3 check_baseline.py --trace decision_trace.jsonl --bench grag_benchmark.jsonl --applog app_log.jsonl

Checks the most recent goal execution against the verified baseline.
Prints PASS or FAIL for each test case with a reason.
"""

import json
import argparse
import sys
from pathlib import Path


def load_jsonl(path):
    """Load all valid JSON lines from a .jsonl file."""
    entries = []
    try:
        with open(path, "r") as f:
            for line in f:
                line = line.strip()
                if line:
                    try:
                        entries.append(json.loads(line))
                    except json.JSONDecodeError:
                        pass
    except FileNotFoundError:
        print(f"  [WARN] File not found: {path}")
    return entries


def extract_events(trace_entries):
    """Pull event names and normalized metadata from decision_trace entries."""
    events = []
    for entry in trace_entries:
        # Format 1: ExecutiveController top-level event_type
        if "event_type" in entry:
            etype = entry["event_type"]
            meta = entry.get("metadata", {})
            events.append({"name": etype, "metadata": meta})
        
        # Format 2: StructuredLogger stages
        stages = entry.get("stages", [])
        for stage in stages:
            name = stage.get("name", "")
            meta = stage.get("metadata", {})
            if name:
                # Map stage names to standard event names if needed
                events.append({"name": name, "metadata": meta})
    return events


def get_routing_decisions(app_log_entries):
    """Find routing_decision metadata entries in app_log.jsonl."""
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
    parser = argparse.ArgumentParser(description="Check Thoth baseline from logs.")
    parser.add_argument("--trace", default="decision_trace.jsonl", help="Path to decision_trace.jsonl")
    parser.add_argument("--bench", default="grag_benchmark.jsonl", help="Path to grag_benchmark.jsonl")
    parser.add_argument("--applog", default="app_log.jsonl", help="Path to app_log.jsonl")
    args = parser.parse_args()

    print("\n══════════════════════════════════════════════")
    print("  Thoth Basic Agent — Baseline Check")
    print("══════════════════════════════════════════════\n")

    trace = load_jsonl(args.trace)
    bench = load_jsonl(args.bench)
    applog = load_jsonl(args.applog)

    if not trace:
        print("  [ERROR] No trace entries found. Is the path correct?")
        sys.exit(1)

    events = extract_events(trace)
    event_names = [e["name"] for e in events]
    routing = get_routing_decisions(applog)

    all_passed = True

    # ── TC-02: Plan contains RETRIEVAL step ──────────────────────────────────
    print("TC-02: Plan contains RETRIEVAL step")
    
    # Check PLAN_CREATED has steps
    plan_created = [e for e in events if e["name"] == "PLAN_CREATED"]
    has_steps = False
    if plan_created:
        plan = plan_created[-1]["metadata"].get("plan", {})
        steps = plan.get("steps", [])
        has_steps = len(steps) >= 2
    
    result("PLAN_CREATED contains >= 2 steps", has_steps, "Plan is empty or missing steps")

    # In our logs, StepType::RETRIEVAL is 1, StepType::LLM is 2
    retrieval_starts = [
        e for e in events
        if e["name"] == "STEP_STARTED" and e["metadata"].get("step_type") == 1
    ]
    
    p = result(
        "STEP_STARTED for RETRIEVAL step exists",
        len(retrieval_starts) > 0,
        "RETRIEVAL step never started (metadata missing or type != 1)"
    )
    all_passed = all_passed and p

    # Check sequence
    ret_idx = next((i for i,e in enumerate(events) if e["name"] == "STEP_STARTED" and e["metadata"].get("step_type") == 1), 999)
    llm_idx = next((i for i,e in enumerate(events) if e["name"] == "STEP_STARTED" and e["metadata"].get("step_type") == 2), 999)
    
    p = result(
        "RETRIEVAL step starts before LLM step",
        ret_idx < llm_idx,
        f"LLM step ({llm_idx}) started before or without RETRIEVAL step ({ret_idx})"
    )
    all_passed = all_passed and p

    # ── TC-03: GRAG math ────────────────────────────────────────────────────
    print("\nTC-03: GRAG math (alpha > 0, magnitude > 0)")
    if not bench:
        p = result("grag_benchmark.jsonl has entries", False, "No benchmark entries found")
        all_passed = False
    else:
        grag_entries = [e for e in bench if e.get("scoring_type") == "grag"]
        p = result(
            "scoring_type=grag present",
            len(grag_entries) > 0,
            "Only cosine entries found — goal embedding not reaching RAGPipeline"
        )
        all_passed = all_passed and p

        if grag_entries:
            latest = grag_entries[-1]
            alpha = latest.get("alpha", 0.0)
            magnitude = latest.get("direction_magnitude", 0.0)

            p = result(
                f"alpha > 0 (got {alpha})",
                alpha > 0.0,
                "alpha=0 means direction vector collapsed or goal not set"
            )
            all_passed = all_passed and p

            p = result(
                f"direction_magnitude > 0 (got {magnitude})",
                magnitude > 0.0,
                "magnitude=0 means goal and state embeddings are identical"
            )
            all_passed = all_passed and p

    # ── TC-04: Chat during active goal uses PLAN_AWARE ──────────────────────
    print("\nTC-04: Chat during active goal uses PLAN_AWARE")
    plan_aware = [r for r in routing if r.get("routing_mode") == "PLAN_AWARE"]
    p = result(
        "At least one PLAN_AWARE routing decision found",
        len(plan_aware) > 0,
        "All routing decisions used CONVERSATIONAL — processQuery sync fix may not have landed"
    )
    all_passed = all_passed and p

    if plan_aware:
        goal_active_correct = all(r.get("goal_active") == True for r in plan_aware)
        p = result(
            "goal_active=true in all PLAN_AWARE decisions",
            goal_active_correct,
            "goal_active=false found in a PLAN_AWARE entry — inconsistent state"
        )
        all_passed = all_passed and p

    # ── TC-06: No tool hallucinations ────────────────────────────────────────
    print("\nTC-06: No tool hallucinations")
    tool_names_to_check = ["gmail_read_labels", "stock_quote", "http_request"]
    raw_trace_text = Path(args.trace).read_text() if Path(args.trace).exists() else ""
    found_hallucination = any(t in raw_trace_text for t in tool_names_to_check)
    p = result(
        "No unexpected tool calls in trace",
        not found_hallucination,
        f"Found one of {tool_names_to_check} in trace — check for hallucination"
    )
    all_passed = all_passed and p

    # ── TC-07: PLAN_COMPLETED in trace ───────────────────────────────────────
    print("\nTC-07: Goal lifecycle completes")
    p = result(
        "PLAN_COMPLETED event present",
        "PLAN_COMPLETED" in event_names,
        "Plan never completed — possible hang or missing steps"
    )
    all_passed = all_passed and p

    # ── Summary ──────────────────────────────────────────────────────────────
    print("\n══════════════════════════════════════════════")
    if all_passed:
        print("  RESULT: ALL CHECKS PASSED ✓")
        print("  Baseline is intact. Safe to proceed.")
    else:
        print("  RESULT: ONE OR MORE CHECKS FAILED ✗")
        print("  Do not merge changes until all pass.")
    print("══════════════════════════════════════════════\n")

    sys.exit(0 if all_passed else 1)


if __name__ == "__main__":
    main()
