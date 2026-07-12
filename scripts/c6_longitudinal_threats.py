#!/usr/bin/env python3
"""
c6_longitudinal_threats.py — C6.3-03 threat detection engine (RC v1.0 § Step 03-2b).

Contract: docs/C6_phase3_reporting_contract.md § Threat detection · § Step 03-2b
"""

from __future__ import annotations

from collections.abc import Iterable, Mapping
from dataclasses import dataclass
from typing import Any

import c6_longitudinal_provenance as provenance

MIN_GOALS_LONGITUDINAL = 30

THREAT_IDS: frozenset[str] = frozenset(
    {
        "model_hash_drift",
        "corpus_fingerprint_drift",
        "runtime_environment_drift",
        "prompt_evolution",
        "protocol_revision_mid_window",
        "small_sample",
        "tier_mixing",
        "incomplete_data",
        "selection_bias",
        "env_hash_drift",
        "fingerprint_mismatch",
        "no_official_anchor",
        "benchmark_gate_failure",
        "gate_evidence_missing",
    }
)


@dataclass(frozen=True)
class ThreatInputs:
    eligible_count: int
    safety: Mapping[str, Any]
    flags: tuple[str, ...]
    used_official_anchor: bool
    report_completeness: str
    env_rows: tuple[dict[str, Any], ...]
    benchmark_run_ids_consumed: tuple[str, ...]
    validation_total_invalid: int
    window: Mapping[str, Any]
    prior_longitudinal_rows: tuple[dict[str, Any], ...] | None = None


def distinct_non_null(values: Iterable[Any]) -> set[str]:
    distinct: set[str] = set()
    for value in values:
        if value is None:
            continue
        if not isinstance(value, str):
            continue
        stripped = value.strip()
        if stripped:
            distinct.add(stripped)
    return distinct


def extract_env_fields_for_threats(env_row: dict[str, Any]) -> tuple[str | None, str | None, str | None, str | None]:
    """Parity: c6_longitudinal_join._extract_env_fields (JC v1.0). Do not diverge."""
    env = env_row.get("env") if isinstance(env_row.get("env"), dict) else {}
    corpus = env.get("corpus") if isinstance(env.get("corpus"), dict) else {}
    model = env.get("model") if isinstance(env.get("model"), dict) else {}
    runtime = env.get("runtime") if isinstance(env.get("runtime"), dict) else {}

    env_hash = env_row.get("env_hash")
    env_hash = env_hash if isinstance(env_hash, str) and env_hash else None

    corpus_fp = corpus.get("fingerprint")
    corpus_fp = corpus_fp if isinstance(corpus_fp, str) and corpus_fp else None

    model_hash = model.get("model_hash")
    if not isinstance(model_hash, str) or not model_hash:
        embedding_model = model.get("embedding_model")
        model_hash = embedding_model if isinstance(embedding_model, str) and embedding_model else None

    tier = runtime.get("tier")
    tier = tier.upper() if isinstance(tier, str) and tier else None

    return env_hash, model_hash, corpus_fp, tier


def _consumed_env_rows(inputs: ThreatInputs) -> list[dict[str, Any]]:
    run_ids = set(inputs.benchmark_run_ids_consumed)
    return provenance.env_rows_for_run_ids(list(inputs.env_rows), run_ids)


def _distinct_env_field(inputs: ThreatInputs, index: int) -> set[str]:
    return distinct_non_null(extract_env_fields_for_threats(row)[index] for row in _consumed_env_rows(inputs))


def _detect_env_drift_threats(inputs: ThreatInputs) -> set[str]:
    rows = _consumed_env_rows(inputs)
    if not rows:
        return set()

    env_hashes = _distinct_env_field(inputs, 0)
    model_hashes = _distinct_env_field(inputs, 1)
    corpus_fps = _distinct_env_field(inputs, 2)

    threats: set[str] = set()
    if len(model_hashes) > 1:
        threats.add("model_hash_drift")
    if len(corpus_fps) > 1:
        threats.add("corpus_fingerprint_drift")
    if len(env_hashes) > 1 and len(model_hashes) == 1 and len(corpus_fps) == 1:
        threats.add("runtime_environment_drift")
    return threats


def _detect_protocol_revision(inputs: ThreatInputs) -> bool:
    prior_rows = inputs.prior_longitudinal_rows
    if not prior_rows:
        return False

    start_ms = inputs.window.get("start_ms")
    end_ms = inputs.window.get("end_ms")
    if not isinstance(start_ms, int) or not isinstance(end_ms, int):
        return False

    versions: set[str] = set()
    for row in prior_rows:
        if row.get("event") != "COGNITIVE_LONGITUDINAL_SUMMARY":
            continue
        window = row.get("window") if isinstance(row.get("window"), dict) else {}
        row_end = window.get("end_ms")
        if not isinstance(row_end, int):
            continue
        if start_ms <= row_end <= end_ms:
            version = row.get("metric_schema_version")
            if isinstance(version, str) and version.strip():
                versions.add(version.strip())

    return len(versions) > 1


def detect_threats(inputs: ThreatInputs) -> list[str]:
    """Pure: ThreatInputs → sorted threat IDs. Does not mutate inputs."""
    threats: set[str] = set()

    if inputs.safety.get("tier_contamination"):
        threats.add("tier_mixing")
    if not inputs.safety.get("reproducibility_ok", True):
        threats.add("env_hash_drift")
    if inputs.eligible_count < MIN_GOALS_LONGITUDINAL:
        threats.add("small_sample")
    if not inputs.used_official_anchor or "no_official_anchor" in inputs.flags:
        threats.add("no_official_anchor")
    if "fingerprint_mismatch" in inputs.flags:
        threats.add("fingerprint_mismatch")
    if inputs.report_completeness == "incomplete" or any(
        flag.startswith("missing_artifact:") for flag in inputs.flags
    ):
        threats.add("incomplete_data")
    if "validation_rows_dropped" in inputs.flags or inputs.validation_total_invalid > 0:
        threats.add("selection_bias")

    threats.update(_detect_env_drift_threats(inputs))

    run_ids = set(inputs.benchmark_run_ids_consumed)
    if provenance.prompt_evolution_detected(list(inputs.env_rows), run_ids):
        threats.add("prompt_evolution")

    if _detect_protocol_revision(inputs):
        threats.add("protocol_revision_mid_window")

    if inputs.safety.get("benchmark_regression") is True:
        threats.add("benchmark_gate_failure")
    if any(flag.startswith("gate_evidence_missing:") for flag in inputs.flags):
        threats.add("gate_evidence_missing")

    unknown = threats - THREAT_IDS
    if unknown:
        raise ValueError(f"detect_threats emitted unknown threat IDs: {sorted(unknown)}")

    return sorted(threats)
