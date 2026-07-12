#!/usr/bin/env python3
"""
c6_longitudinal_provenance.py — C6.3-03 provenance pin helpers (RC v1.0 § Provenance fields).

Contract: docs/C6_phase3_reporting_contract.md § Provenance fields (locked)
"""

from __future__ import annotations

from typing import Any

ProvenancePin = tuple[str | None, str | None]


def normalize_pin_value(value: Any) -> str | None:
    if not isinstance(value, str):
        return None
    stripped = value.strip()
    if not stripped or stripped.lower() == "unknown":
        return None
    return stripped


def extract_provenance_pin(env_row: dict[str, Any]) -> ProvenancePin | None:
    """Return composite pin for a BENCHMARK_ENV row, or None if both components excluded."""
    env = env_row.get("env") if isinstance(env_row.get("env"), dict) else {}
    prov = env.get("prov") if isinstance(env.get("prov"), dict) else {}

    thoth = normalize_pin_value(prov.get("thoth_git_sha"))
    basic_agent = normalize_pin_value(prov.get("basic_agent_git_sha"))

    if thoth is None and basic_agent is None:
        return None
    return (thoth, basic_agent)


def collect_benchmark_env_rows(env_rows: list[dict[str, Any]]) -> list[dict[str, Any]]:
    return [row for row in env_rows if row.get("event") == "BENCHMARK_ENV"]


def env_rows_for_run_ids(env_rows: list[dict[str, Any]], run_ids: set[str]) -> list[dict[str, Any]]:
    if not run_ids:
        return []
    return [
        row
        for row in collect_benchmark_env_rows(env_rows)
        if isinstance(row.get("run_id"), str) and row["run_id"] in run_ids
    ]


def distinct_provenance_pins(env_rows: list[dict[str, Any]], run_ids: set[str]) -> set[ProvenancePin]:
    pins: set[ProvenancePin] = set()
    for row in env_rows_for_run_ids(env_rows, run_ids):
        pin = extract_provenance_pin(row)
        if pin is not None:
            pins.add(pin)
    return pins


def prompt_evolution_detected(env_rows: list[dict[str, Any]], run_ids: set[str]) -> bool:
    """True when consumed env rows have >1 distinct non-excluded provenance_pin."""
    return len(distinct_provenance_pins(env_rows, run_ids)) > 1
