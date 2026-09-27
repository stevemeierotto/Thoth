#!/usr/bin/env python3
"""Prospective C6.4 window records. Does not open a production window.

Membership matches C6.4 v1.0: goal_started_at_ms is inside
[window_start_ms, window_end_ms], inclusive. The span is 28 days.
The cohort fingerprint is only the value returned by
c64_environment_identity.build_prospective_pin.
"""

from __future__ import annotations

import json
from pathlib import Path
from typing import Any, Mapping

import c64_environment_identity as ident

WINDOW_SPAN_MS = 28 * 86_400_000
EVALUATION_ID = "episodic_authoritative_v2"


class C64WindowError(ValueError):
    """A prospective window cannot be opened or used."""


def goal_in_window(window_start_ms: int, window_end_ms: int, goal_started_at_ms: int) -> bool:
    return window_start_ms <= goal_started_at_ms <= window_end_ms


def qualifies_as_observation(kind: str) -> bool:
    """Chat without a plan is not a C6.4 goal observation."""
    return kind == "goal"


def _gate(record: Mapping[str, Any] | None, name: str, thoth: str, engine: str) -> dict[str, Any]:
    if not isinstance(record, Mapping) or record.get("passed") is not True:
        raise C64WindowError(f"missing passing {name} gate")
    if record.get("thoth_git_sha") != thoth or record.get("basic_agent_git_sha") != engine:
        raise C64WindowError(f"{name} gate does not match the window code identity")
    if record.get("evaluation_tier") != "mock":
        raise C64WindowError(f"{name} gate must stay evaluation_tier=mock")
    return {
        "passed": True,
        "evaluation_tier": "mock",
        "thoth_git_sha": thoth,
        "basic_agent_git_sha": engine,
    }


def _episodic_gate(record: Mapping[str, Any] | None, provider: str) -> dict[str, Any]:
    if record is None:
        return {"satisfied": False}
    if record.get("evaluation_tier") == "mock" or record.get("authoritative_valid") is not True:
        raise C64WindowError("mock episodic result cannot satisfy the authoritative gate")
    if record.get("evaluation_id") != EVALUATION_ID:
        raise C64WindowError("episodic gate is not episodic_authoritative_v2")
    if record.get("inference_backend_name") != provider:
        raise C64WindowError("episodic gate provider does not match the window")
    return {"satisfied": True, "evaluation_id": EVALUATION_ID, "inference_backend_name": provider}


def open_window(
    path: Path,
    *,
    opened_at_ms: int,
    evaluation_tier: str,
    inference_backend_name: str,
    thoth_git_sha: str,
    basic_agent_git_sha: str,
    llm_model: str,
    embedding_model: str,
    embedding_method: str,
    embedding_dimension: int,
    corpus_fingerprint: str,
    retrieval_weights: Mapping[str, Any],
    cognitive_runtime: Mapping[str, Any],
    c3_gate: Mapping[str, Any],
    c5_gate: Mapping[str, Any],
    episodic_gate: Mapping[str, Any] | None = None,
    environment_hash: str | None = None,
) -> dict[str, Any]:
    """Write a new open window. Refuses to replace an existing file."""
    if path.exists():
        raise C64WindowError("refusing to overwrite an existing window record")
    pin = ident.build_prospective_pin(
        evaluation_tier=evaluation_tier,
        inference_backend_name=inference_backend_name,
        thoth_git_sha=thoth_git_sha,
        basic_agent_git_sha=basic_agent_git_sha,
        llm_model=llm_model,
        embedding_model=embedding_model,
        embedding_method=embedding_method,
        embedding_dimension=embedding_dimension,
        corpus_fingerprint=corpus_fingerprint,
        retrieval_weights=retrieval_weights,
        cognitive_runtime=cognitive_runtime,
        environment_hash=environment_hash,
    )
    if pin["evaluation_tier"] != "authoritative":
        raise C64WindowError("only evaluation_tier=authoritative can open an official window")
    c3 = _gate(c3_gate, "C3", thoth_git_sha, basic_agent_git_sha)
    c5 = _gate(c5_gate, "C5", thoth_git_sha, basic_agent_git_sha)
    episodic = _episodic_gate(episodic_gate, inference_backend_name)
    fingerprint = pin["c64_cohort_fingerprint"]
    end_ms = opened_at_ms + WINDOW_SPAN_MS
    window_id = "c64w-" + ident.sha256_canonical(
        {
            "c64_cohort_fingerprint": fingerprint,
            "environment_schema_version": pin["environment_schema_version"],
            "metric_schema_version": pin["metric_schema_version"],
            "protocol_version": pin["protocol_version"],
            "window_start_ms": opened_at_ms,
        }
    )[:32]
    record = {
        "status": "open",
        "window_id": window_id,
        "protocol_version": pin["protocol_version"],
        "metric_schema_version": pin["metric_schema_version"],
        "environment_schema_version": pin["environment_schema_version"],
        "evaluation_tier": pin["evaluation_tier"],
        "window_start_ms": opened_at_ms,
        "window_end_ms": end_ms,
        "c64_cohort_fingerprint": fingerprint,
        "pin": pin,
        "c3_gate": c3,
        "c5_gate": c5,
        "episodic_gate": episodic,
        "official_report_eligible": episodic["satisfied"],
        "audit": [{"event": "opened", "at_ms": opened_at_ms}],
    }
    path.write_text(json.dumps(record, sort_keys=True) + "\n", encoding="utf-8")
    return record


def load_window(path: Path) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8"))


def attribute_goal(
    window: Mapping[str, Any],
    *,
    goal_started_at_ms: int,
    current_fingerprint: str,
    plan_id: str,
    outcome: str | None = None,
) -> dict[str, Any] | None:
    """Assign membership from the start timestamp. Outcome is not consulted."""
    del outcome
    if not qualifies_as_observation("goal"):
        return None
    if window.get("status") != "open":
        return None
    if window.get("c64_cohort_fingerprint") != current_fingerprint:
        return None
    if not goal_in_window(int(window["window_start_ms"]), int(window["window_end_ms"]), goal_started_at_ms):
        return None
    if not plan_id:
        raise C64WindowError("missing plan_id")
    return {
        "plan_id": plan_id,
        "window_id": window["window_id"],
        "goal_started_at_ms": goal_started_at_ms,
        "c64_cohort_fingerprint": window["c64_cohort_fingerprint"],
        "protocol_version": window["protocol_version"],
        "environment_schema_version": window["environment_schema_version"],
    }


def close_window(path: Path, *, closed_at_ms: int) -> dict[str, Any]:
    record = load_window(path)
    frozen = record["c64_cohort_fingerprint"]
    window_id = record["window_id"]
    end_ms = min(int(record["window_end_ms"]), closed_at_ms)
    record["status"] = "closed"
    record["window_end_ms"] = end_ms
    record["audit"].append({"event": "closed", "at_ms": closed_at_ms})
    if record["c64_cohort_fingerprint"] != frozen or record["window_id"] != window_id:
        raise C64WindowError("close mutated the frozen cohort")
    path.write_text(json.dumps(record, sort_keys=True) + "\n", encoding="utf-8")
    return record
