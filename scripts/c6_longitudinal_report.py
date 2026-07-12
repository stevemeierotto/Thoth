#!/usr/bin/env python3
"""
c6_longitudinal_report.py — C6.3-03 markdown renderer (RC v1.0).

Contract: docs/C6_phase3_reporting_contract.md
Payload in, markdown out. No join re-run, no artifact reads.
"""

from __future__ import annotations

from datetime import datetime, timezone
from typing import Any

REPORT_SECTIONS = [
    "title",
    "incomplete_banner",
    "reproducibility",
    "disclaimer",
    "evidence_summary",
    "performance",
    "categories",
    "safety",
    "segments",
    "threats",
    "flags",
]

THREAT_SCHEMA_VERSION = "1.0"

THREAT_LABELS = {
    "model_hash_drift": "Changing models",
    "corpus_fingerprint_drift": "Corpus growth",
    "runtime_environment_drift": "Runtime environment drift",
    "prompt_evolution": "Prompt / config evolution",
    "protocol_revision_mid_window": "Protocol revision mid-window",
    "small_sample": "Small sample",
    "tier_mixing": "Tier mixing",
    "incomplete_data": "Incomplete data",
    "selection_bias": "Selection bias risk",
    "env_hash_drift": "Environment hash drift",
    "fingerprint_mismatch": "Fingerprint mismatch on goals",
    "no_official_anchor": "No official window anchor",
    "benchmark_gate_failure": "Official benchmark gate failure",
    "gate_evidence_missing": "Missing gate terminal evidence",
}

INCOMPLETE_BANNER = (
    "> **INCOMPLETE — exploratory use only.** Required authoritative artifacts or "
    "validation rows are missing. Do not use for official longitudinal claims or "
    "F-series promotion."
)

DISCLAIMER = (
    "**Observational evidence only.** This report describes statistical trends in "
    "immutable benchmark artifacts over a preregistered window. It does **not** "
    "establish causal relationships between code changes, configuration changes, and "
    "observed performance. Correlation in a rolling window is not proof of causation. "
    "Official learning claims require controlled benchmarks (E2, E3, B1, C3/C5) in "
    "addition to longitudinal evidence."
)

FORBIDDEN_PROSE = (
    "because",
    "caused by",
    "proves that",
    "due to the refactor",
)


def ms_to_iso_utc(ms: int) -> str:
    return datetime.fromtimestamp(ms / 1000.0, tz=timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


def format_benchmark_run_ids(run_ids: list[str]) -> str:
    if not run_ids:
        return "0"
    count = len(run_ids)
    if count <= 10:
        return f"{count} ({', '.join(run_ids)})"
    shown = ", ".join(run_ids[:5])
    return f"{count} ({shown}, … (+{count - 5} more))"


def format_rate(value: float | int | None) -> str:
    if value is None:
        return "n/a"
    return f"{float(value):.6f}"


def is_incomplete(payload: dict[str, Any]) -> bool:
    return payload.get("report_completeness") == "incomplete"


def section_title(_payload: dict[str, Any]) -> str:
    return "# Cognitive Longitudinal Report\n"


def section_incomplete_banner(payload: dict[str, Any]) -> str:
    if not is_incomplete(payload):
        return ""
    return f"{INCOMPLETE_BANNER}\n"


def section_reproducibility(payload: dict[str, Any]) -> str:
    window = payload.get("window") or {}
    repro = payload.get("reproducibility") or {}
    run_ids = repro.get("benchmark_run_ids_consumed") or []
    generated_ms = payload.get("generated_at_ms", 0)

    lines = [
        "## Reproducibility",
        "",
        f"- **protocol_version:** {payload.get('protocol_version', 'unknown')}",
        f"- **metric_schema_version:** {payload.get('metric_schema_version', 'unknown')}",
        f"- **analyzer_version:** {payload.get('analyzer_version', 'unknown')}",
        f"- **analyzer_commit_hash:** {payload.get('analyzer_commit_hash', 'unknown')}",
        f"- **generated_at_ms:** {generated_ms} ({ms_to_iso_utc(int(generated_ms))})",
        f"- **report_completeness:** {payload.get('report_completeness', 'unknown')}",
        f"- **evidence_scope:** {payload.get('evidence_scope', 'unknown')}",
        f"- **confidence_label:** {payload.get('confidence_label', 'unknown')}",
        f"- **window.start_ms:** {window.get('start_ms', 'unknown')}",
        f"- **window.end_ms:** {window.get('end_ms', 'unknown')}",
        f"- **window.days:** {window.get('days', 'unknown')}",
        f"- **window.anchor:** {window.get('anchor', 'unknown')}",
        f"- **reproducibility.input_log_path:** {repro.get('input_log_path', 'unknown')}",
        f"- **reproducibility.benchmark_run_ids_consumed:** {format_benchmark_run_ids(run_ids)}",
        f"- **threat_schema_version:** {THREAT_SCHEMA_VERSION}",
        "",
    ]
    return "\n".join(lines)


def section_disclaimer(_payload: dict[str, Any]) -> str:
    return f"{DISCLAIMER}\n"


def section_evidence_summary(payload: dict[str, Any]) -> str:
    window = payload.get("window") or {}
    lines = [
        "## Evidence Summary",
        "",
        f"- **evidence_scope:** {payload.get('evidence_scope', 'unknown')}",
        f"- **confidence_label:** {payload.get('confidence_label', 'unknown')}",
        f"- **window:** {window.get('start_ms')} – {window.get('end_ms')} ({window.get('days')} days, anchor: {window.get('anchor')})",
        "",
    ]
    return "\n".join(lines)


def section_performance(payload: dict[str, Any]) -> str:
    perf = (payload.get("categories") or {}).get("performance") or {}
    lines = [
        "## Performance",
        "",
        f"- **success_rate:** {format_rate(perf.get('success_rate'))}",
        f"- **ci_low:** {format_rate(perf.get('ci_low'))}",
        f"- **ci_high:** {format_rate(perf.get('ci_high'))}",
        f"- **trend:** {perf.get('trend', 'unknown')}",
        f"- **eligible_count:** {perf.get('eligible_count', 0)}",
        "",
    ]
    return "\n".join(lines)


def _format_category_block(name: str, data: dict[str, Any]) -> list[str]:
    if not data:
        return []
    lines = [f"### {name.capitalize()}", ""]
    for key in sorted(data.keys()):
        lines.append(f"- **{key}:** {data[key]}")
    lines.append("")
    return lines


def section_categories(payload: dict[str, Any]) -> str:
    categories = payload.get("categories") or {}
    lines = ["## Categories", ""]
    for subsection in ("efficiency", "memory", "planning"):
        lines.extend(_format_category_block(subsection, categories.get(subsection) or {}))
    if lines == ["## Categories", ""]:
        lines.append("(no additional category data)")
        lines.append("")
    return "\n".join(lines)


def section_safety(payload: dict[str, Any]) -> str:
    safety = (payload.get("categories") or {}).get("safety") or {}
    lines = [
        "## Safety",
        "",
        f"- **reproducibility_ok:** {safety.get('reproducibility_ok', 'unknown')}",
        f"- **tier_contamination:** {safety.get('tier_contamination', 'unknown')}",
        f"- **benchmark_regression:** {safety.get('benchmark_regression', 'unknown')}",
        f"- **official_gates_green:** {safety.get('official_gates_green', 'unknown')}",
        "",
    ]
    return "\n".join(lines)


def section_segments(payload: dict[str, Any]) -> str:
    segments = payload.get("segments") or {}
    lines = ["## Segments", ""]
    if not segments:
        lines.append("(no segment data)")
        lines.append("")
        return "\n".join(lines)

    lines.append("| segment | goal_count | success_rate | ci_low | ci_high | confidence | trend |")
    lines.append("|---------|------------|--------------|--------|---------|------------|-------|")
    for key in sorted(segments.keys()):
        seg = segments[key]
        lines.append(
            "| {key} | {goal_count} | {success_rate} | {ci_low} | {ci_high} | {confidence_label} | {trend} |".format(
                key=key,
                goal_count=seg.get("goal_count", 0),
                success_rate=format_rate(seg.get("success_rate")),
                ci_low=format_rate(seg.get("ci_low")),
                ci_high=format_rate(seg.get("ci_high")),
                confidence_label=seg.get("confidence_label", "unknown"),
                trend=seg.get("trend", "unknown"),
            )
        )
    lines.append("")
    return "\n".join(lines)


def threat_label(threat_id: str) -> str:
    return THREAT_LABELS.get(threat_id, f"(unlabeled: {threat_id})")


def section_threats(payload: dict[str, Any]) -> str:
    threats = sorted(payload.get("threats_disclosed") or [])
    lines = ["## Threats to Validity", ""]
    if not threats:
        lines.append("(none)")
        lines.append("")
        return "\n".join(lines)
    for threat_id in threats:
        lines.append(f"- `{threat_id}` — {threat_label(threat_id)}")
    lines.append("")
    return "\n".join(lines)


def section_flags(payload: dict[str, Any]) -> str:
    flags = sorted(payload.get("flags") or [])
    warnings = sorted(payload.get("warnings") or [])
    lines = ["## Flags and Warnings", ""]
    lines.append("**flags:**")
    if flags:
        for flag in flags:
            lines.append(f"- `{flag}`")
    else:
        lines.append("- (none)")
    lines.append("")
    lines.append("**warnings:**")
    if warnings:
        for warning in warnings:
            lines.append(f"- {warning}")
    else:
        lines.append("- (none)")
    lines.append("")
    return "\n".join(lines)


_SECTION_BUILDERS = {
    "title": section_title,
    "incomplete_banner": section_incomplete_banner,
    "reproducibility": section_reproducibility,
    "disclaimer": section_disclaimer,
    "evidence_summary": section_evidence_summary,
    "performance": section_performance,
    "categories": section_categories,
    "safety": section_safety,
    "segments": section_segments,
    "threats": section_threats,
    "flags": section_flags,
}


def render_report_sections(payload: dict[str, Any]) -> dict[str, str]:
    sections: dict[str, str] = {}
    for section_id in REPORT_SECTIONS:
        if section_id == "incomplete_banner" and not is_incomplete(payload):
            continue
        builder = _SECTION_BUILDERS[section_id]
        content = builder(payload)
        if content:
            sections[section_id] = content
    return sections


def render_markdown_report(payload: dict[str, Any]) -> str:
    sections = render_report_sections(payload)
    parts: list[str] = []
    for section_id in REPORT_SECTIONS:
        if section_id not in sections:
            continue
        parts.append(sections[section_id].rstrip("\n"))
    return "\n\n".join(parts) + "\n"


def header_slice(payload: dict[str, Any]) -> str:
    """Sections title through disclaimer — used for golden header tests."""
    sections = render_report_sections(payload)
    order = ["title", "incomplete_banner", "reproducibility", "disclaimer"]
    parts: list[str] = []
    for section_id in order:
        if section_id not in sections:
            continue
        parts.append(sections[section_id].rstrip("\n"))
    return "\n\n".join(parts) + "\n"
