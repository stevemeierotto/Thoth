#!/usr/bin/env python3
"""
summarize_cognitive_metrics.py — C7 latency summary from logs/cognitive_metrics.jsonl.

Usage:
    python3 scripts/summarize_cognitive_metrics.py
    python3 scripts/summarize_cognitive_metrics.py --log logs/cognitive_metrics.jsonl --last 50
"""

import argparse
import json
import statistics
import sys
from pathlib import Path


def load_rows(path: Path):
    rows = []
    if not path.exists():
        return rows
    with path.open("r", encoding="utf-8") as handle:
        for line in handle:
            line = line.strip()
            if not line:
                continue
            try:
                row = json.loads(line)
            except json.JSONDecodeError:
                continue
            if row.get("event") == "GOAL_COGNITIVE_METRICS":
                rows.append(row)
    return rows


def percentile(values, pct):
    if not values:
        return 0.0
    ordered = sorted(values)
    idx = int(round((pct / 100.0) * (len(ordered) - 1)))
    return float(ordered[max(0, min(idx, len(ordered) - 1))])


def summarize(values):
    if not values:
        return {"count": 0, "mean": 0.0, "p50": 0.0, "p95": 0.0}
    return {
        "count": len(values),
        "mean": round(statistics.fmean(values), 1),
        "p50": round(percentile(values, 50), 1),
        "p95": round(percentile(values, 95), 1),
    }


def main():
    parser = argparse.ArgumentParser(description="Summarize per-goal cognitive latency metrics.")
    parser.add_argument("--log", default="logs/cognitive_metrics.jsonl")
    parser.add_argument("--last", type=int, default=0, help="Only analyze the last N goals")
    args = parser.parse_args()

    rows = load_rows(Path(args.log))
    if args.last > 0:
        rows = rows[-args.last :]

    if not rows:
        print(f"No GOAL_COGNITIVE_METRICS rows in {args.log}")
        sys.exit(0)

    fields = {
        "total_wall_clock_ms": [],
        "planning_time_ms": [],
        "retrieval_time_ms": [],
        "llm_synthesis_time_ms": [],
        "synthesis_prompt_chars": [],
    }
    truncated = 0
    completed = 0

    for row in rows:
        for key in fields:
            value = row.get(key)
            if isinstance(value, (int, float)):
                fields[key].append(float(value))
        if row.get("synthesis_context_truncated"):
            truncated += 1
        if row.get("outcome") == "completed":
            completed += 1

    print("\nCognitive metrics summary")
    print(f"  log: {args.log}")
    print(f"  goals: {len(rows)} (completed: {completed})")
    print(f"  synthesis_context_truncated: {truncated}")
    print("")
    for key, values in fields.items():
        stats = summarize(values)
        print(f"  {key}: count={stats['count']} mean={stats['mean']} p50={stats['p50']} p95={stats['p95']}")
    print("")


if __name__ == "__main__":
    main()
