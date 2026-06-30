#!/usr/bin/env python3
"""
plot_cognitive_metrics.py — C6 Phase 2 charts from logs/cognitive_metrics.jsonl.

Requires matplotlib (pip install matplotlib).

Usage:
    python3 scripts/plot_cognitive_metrics.py
    python3 scripts/plot_cognitive_metrics.py --log logs/cognitive_metrics.jsonl --output-dir logs/plots
"""

import argparse
import json
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


def pick_series(rows, key):
    values = []
    for row in rows:
        value = row.get(key)
        if isinstance(value, (int, float)):
            values.append(float(value))
    return values


def main():
    parser = argparse.ArgumentParser(description="Plot per-goal cognitive metrics.")
    parser.add_argument("--log", default="logs/cognitive_metrics.jsonl")
    parser.add_argument("--output-dir", default="logs/plots")
    parser.add_argument("--last", type=int, default=0, help="Only plot the last N goals")
    args = parser.parse_args()

    try:
        import matplotlib

        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
    except ImportError:
        print("matplotlib is required: pip install matplotlib", file=sys.stderr)
        sys.exit(1)

    rows = load_rows(Path(args.log))
    if args.last > 0:
        rows = rows[-args.last :]

    if not rows:
        print(f"No GOAL_COGNITIVE_METRICS rows in {args.log}")
        sys.exit(0)

    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    indices = list(range(1, len(rows) + 1))
    outcomes = [row.get("outcome", "?") for row in rows]

    def save_latency_chart():
        fig, ax = plt.subplots(figsize=(10, 5))
        ax.plot(indices, pick_series(rows, "total_wall_clock_ms"), marker="o", label="total")
        ax.plot(indices, pick_series(rows, "planning_time_ms"), marker=".", label="planning")
        ax.plot(indices, pick_series(rows, "retrieval_time_ms"), marker=".", label="retrieval")
        ax.plot(indices, pick_series(rows, "llm_synthesis_time_ms"), marker=".", label="synthesis")
        ax.set_xlabel("Goal index")
        ax.set_ylabel("Latency (ms)")
        ax.set_title("Per-goal latency breakdown")
        ax.legend()
        ax.grid(True, alpha=0.3)
        for i, outcome in enumerate(outcomes):
            if outcome != "completed":
                ax.axvline(i + 1, color="red", alpha=0.08, linewidth=6)
        path = output_dir / "cognitive_latency.png"
        fig.tight_layout()
        fig.savefig(path, dpi=144)
        plt.close(fig)
        return path

    def save_token_chart():
        totals = pick_series(rows, "total_tokens")
        if not totals or max(totals) <= 0:
            return None
        fig, ax = plt.subplots(figsize=(10, 4))
        ax.bar(indices, totals, color="#4c78a8", label="total")
        planning = pick_series(rows, "planning_tokens")
        synthesis = pick_series(rows, "synthesis_tokens")
        if planning and max(planning) > 0:
            ax.bar(indices, planning, color="#f58518", alpha=0.8, label="planning")
        if synthesis and max(synthesis) > 0:
            ax.bar(indices, synthesis, bottom=planning or [0] * len(indices),
                   color="#54a24b", alpha=0.8, label="synthesis")
        ax.set_xlabel("Goal index")
        ax.set_ylabel("Tokens")
        ax.set_title("Per-goal token usage")
        ax.legend()
        ax.grid(True, axis="y", alpha=0.3)
        path = output_dir / "cognitive_tokens.png"
        fig.tight_layout()
        fig.savefig(path, dpi=144)
        plt.close(fig)
        return path

    def save_quality_chart():
        fig, ax1 = plt.subplots(figsize=(10, 4))
        alpha = pick_series(rows, "grag_alpha")
        chunks = pick_series(rows, "retrieved_chunk_count")
        if alpha:
            ax1.plot(indices, alpha, color="#72b7b2", marker="o", label="grag_alpha")
            ax1.set_ylabel("grag_alpha")
        ax2 = ax1.twinx()
        if chunks:
            ax2.bar(indices, chunks, alpha=0.25, color="#b279a2", label="chunks")
            ax2.set_ylabel("retrieved chunks")
        ax1.set_xlabel("Goal index")
        ax1.set_title("Retrieval quality signals")
        ax1.grid(True, alpha=0.3)
        path = output_dir / "cognitive_retrieval.png"
        fig.tight_layout()
        fig.savefig(path, dpi=144)
        plt.close(fig)
        return path

    written = [save_latency_chart(), save_quality_chart()]
    token_path = save_token_chart()
    if token_path:
        written.append(token_path)

    print(f"Plotted {len(rows)} goals from {args.log}")
    for path in written:
        if path:
            print(f"  wrote {path}")


if __name__ == "__main__":
    main()
