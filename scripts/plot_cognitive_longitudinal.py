#!/usr/bin/env python3
"""
plot_cognitive_longitudinal.py — C6.3-03 longitudinal summary plots (RC v1.0 § Plot schema).

Payload-only: reads cognitive_longitudinal_summary.json (or an in-memory summary mapping).
Does not read cognitive_metrics.jsonl, benchmark_env.jsonl, or prior JSONL history.

success_rate_trend.png is current-window only (categories.performance). Prior-window markers
are not inferred from JSONL; they require an explicit future summary field (none in RC v1.0).

Contract: docs/C6_phase3_reporting_contract.md § Step 03-4 implementation lock
"""

from __future__ import annotations

import argparse
import json
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Mapping

DEFAULT_SUMMARY_PATH = Path("logs/cognitive_longitudinal_summary.json")
DEFAULT_PLOT_DIR = Path("logs/plots/longitudinal")

PLOT_FILENAMES = (
    "success_rate_trend.png",
    "segment_success.png",
    "efficiency_p50.png",
)

EMPTY_SEGMENTS_ANNOTATION = "no segment data"


@dataclass(frozen=True)
class PlotResult:
    written_paths: tuple[Path, ...]
    flags: tuple[str, ...]
    matplotlib_available: bool


def should_skip_all_plots(payload: Mapping[str, Any]) -> str | None:
    """Return plots_skipped:* flag if all plots must be skipped, else None."""
    if payload.get("report_completeness") == "incomplete":
        return "plots_skipped:incomplete_report"
    return None


def performance_ci_values(payload: Mapping[str, Any]) -> tuple[float, float, float]:
    performance = payload.get("categories", {}).get("performance", {})
    return (
        float(performance.get("success_rate", 0.0)),
        float(performance.get("ci_low", 0.0)),
        float(performance.get("ci_high", 1.0)),
    )


def segment_success_entries(payload: Mapping[str, Any]) -> list[tuple[str, float]]:
    segments = payload.get("segments") or {}
    if not isinstance(segments, Mapping):
        return []
    entries: list[tuple[str, float]] = []
    for key in sorted(segments):
        block = segments[key]
        if not isinstance(block, Mapping):
            continue
        rate = block.get("success_rate", 0.0)
        if isinstance(rate, (int, float)):
            entries.append((str(key), float(rate)))
    return entries


def efficiency_p50_values(payload: Mapping[str, Any]) -> tuple[float, float]:
    efficiency = payload.get("categories", {}).get("efficiency", {})
    return (
        float(efficiency.get("total_wall_clock_ms_p50", 0.0)),
        float(efficiency.get("total_tokens_p50", 0.0)),
    )


def _import_matplotlib():
    try:
        import matplotlib

        matplotlib.use("Agg")
        import matplotlib.pyplot as plt

        return plt
    except ImportError:
        return None


def _render_success_rate_trend(payload: Mapping[str, Any], plot_dir: Path, plt: Any) -> Path:
    rate, ci_low, ci_high = performance_ci_values(payload)
    yerr_low = max(rate - ci_low, 0.0)
    yerr_high = max(ci_high - rate, 0.0)

    fig, ax = plt.subplots(figsize=(6, 4))
    ax.errorbar(
        [0],
        [rate],
        yerr=[[yerr_low], [yerr_high]],
        fmt="o",
        capsize=8,
        color="#4c78a8",
    )
    ax.set_xticks([0])
    ax.set_xticklabels(["current window"])
    ax.set_ylim(0.0, 1.0)
    ax.set_ylabel("Success rate")
    ax.set_title("Longitudinal success rate (current window)")
    ax.grid(True, axis="y", alpha=0.3)

    path = plot_dir / PLOT_FILENAMES[0]
    fig.tight_layout()
    fig.savefig(path, dpi=144)
    plt.close(fig)
    return path


def _render_segment_success(payload: Mapping[str, Any], plot_dir: Path, plt: Any) -> Path:
    entries = segment_success_entries(payload)
    fig, ax = plt.subplots(figsize=(10, max(3, 0.5 * max(len(entries), 1) + 1)))

    if not entries:
        ax.text(0.5, 0.5, EMPTY_SEGMENTS_ANNOTATION, ha="center", va="center", transform=ax.transAxes)
        ax.set_axis_off()
        ax.set_title("Segment success rates")
    else:
        labels = [label for label, _ in entries]
        values = [value for _, value in entries]
        y_pos = list(range(len(labels)))
        ax.barh(y_pos, values, color="#54a24b")
        ax.set_yticks(y_pos)
        ax.set_yticklabels(labels)
        ax.set_xlim(0.0, 1.0)
        ax.set_xlabel("Success rate")
        ax.set_title("Segment success rates")
        ax.grid(True, axis="x", alpha=0.3)

    path = plot_dir / PLOT_FILENAMES[1]
    fig.tight_layout()
    fig.savefig(path, dpi=144)
    plt.close(fig)
    return path


def _render_efficiency_p50(payload: Mapping[str, Any], plot_dir: Path, plt: Any) -> Path:
    wall_clock_p50, tokens_p50 = efficiency_p50_values(payload)
    labels = ["wall_clock_ms_p50", "tokens_p50"]
    values = [wall_clock_p50, tokens_p50]

    fig, ax = plt.subplots(figsize=(6, 4))
    ax.bar(labels, values, color=["#f58518", "#4c78a8"])
    ax.set_ylabel("p50 value")
    ax.set_title("Efficiency p50 (current window)")
    ax.grid(True, axis="y", alpha=0.3)

    path = plot_dir / PLOT_FILENAMES[2]
    fig.tight_layout()
    fig.savefig(path, dpi=144)
    plt.close(fig)
    return path


def write_longitudinal_plots(payload: Mapping[str, Any], plot_dir: Path) -> PlotResult:
    """Write 0–3 PNGs per § Plot schema. Payload-only — no artifact reads."""
    skip_flag = should_skip_all_plots(payload)
    if skip_flag is not None:
        return PlotResult(written_paths=(), flags=(skip_flag,), matplotlib_available=True)

    plt = _import_matplotlib()
    if plt is None:
        return PlotResult(
            written_paths=(),
            flags=("plots_skipped:matplotlib_unavailable",),
            matplotlib_available=False,
        )

    plot_dir.mkdir(parents=True, exist_ok=True)
    written = (
        _render_success_rate_trend(payload, plot_dir, plt),
        _render_segment_success(payload, plot_dir, plt),
        _render_efficiency_p50(payload, plot_dir, plt),
    )
    return PlotResult(written_paths=written, flags=(), matplotlib_available=True)


def load_summary(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8") as handle:
        data = json.load(handle)
    if not isinstance(data, dict):
        raise ValueError(f"summary JSON must be an object: {path}")
    return data


def build_arg_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Plot cognitive longitudinal summary PNGs.")
    parser.add_argument("--summary", type=Path, default=DEFAULT_SUMMARY_PATH)
    parser.add_argument("--plot-dir", type=Path, default=DEFAULT_PLOT_DIR)
    return parser


def main(argv: list[str] | None = None) -> int:
    parser = build_arg_parser()
    args = parser.parse_args(argv)

    summary_path = args.summary
    if not summary_path.is_file():
        print(f"summary not found: {summary_path}", file=sys.stderr)
        return 2

    payload = load_summary(summary_path)
    result = write_longitudinal_plots(payload, args.plot_dir)

    if result.flags:
        for flag in result.flags:
            print(f"plots skipped: {flag}", file=sys.stderr)
        return 0

    for path in result.written_paths:
        print(f"  wrote {path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
