#!/usr/bin/env python3
"""Prospective C6.4 environment identity (Phase 3).

Builds a c64-env-1 pin and c64_cohort_fingerprint. Does not read or write
benchmark logs, does not open a window, and does not select a provider,
model, or embedding backend.

Canonical cohort object keys are the sealed C6.4 v1.0 input set. Strategy
promotion and plan-reuse thresholds are compile-time constants in the Engine,
so they are covered by basic_agent_git_sha and are not part of
cognitive_runtime. Runtime-editable memory policy from config.json is.
"""

from __future__ import annotations

import hashlib
import json
from typing import Any, Mapping

PROTOCOL_VERSION = "C6.4 v1.0"
METRIC_SCHEMA_VERSION = "1.0"
ENVIRONMENT_SCHEMA_VERSION = "c64-env-1"
EVALUATION_TIERS = frozenset({"mock", "dev", "authoritative"})

# config.json → memory. Not compile-time kMinStrategySimilarity or plan-reuse limits.
COGNITIVE_RUNTIME_KEYS = (
    "max_hot_messages",
    "max_hot_age_days",
    "prune_batch_size",
)

# retrieval_config.json → retrieval_weights, as loaded by Config::loadRetrievalConfig.
RETRIEVAL_WEIGHT_KEYS = (
    "query",
    "direction",
    "trajectory",
    "keyword",
)


class C64IdentityError(ValueError):
    """Required prospective identity is missing or not usable. Fail closed."""


def canonical_json(value: Any) -> str:
    """UTF-8 JSON with sorted keys and no insignificant whitespace."""
    return json.dumps(
        value,
        sort_keys=True,
        separators=(",", ":"),
        ensure_ascii=False,
        allow_nan=False,
    )


def sha256_canonical(value: Any) -> str:
    return hashlib.sha256(canonical_json(value).encode("utf-8")).hexdigest()


def _require_text(name: str, value: Any) -> str:
    if not isinstance(value, str) or not value.strip():
        raise C64IdentityError(f"missing required identity: {name}")
    if value != value.strip():
        raise C64IdentityError(f"identity {name} must not have surrounding whitespace")
    return value


def _require_int(name: str, value: Any) -> int:
    if isinstance(value, bool) or not isinstance(value, int):
        raise C64IdentityError(f"identity {name} must be an integer")
    return value


def _require_number(name: str, value: Any) -> int | float:
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise C64IdentityError(f"identity {name} must be a number")
    return value


def _require_exact_map(name: str, value: Mapping[str, Any] | None, keys: tuple[str, ...]) -> dict[str, Any]:
    if not isinstance(value, Mapping):
        raise C64IdentityError(f"missing required identity: {name}")
    unknown = sorted(set(value) - set(keys))
    if unknown:
        raise C64IdentityError(f"{name} has undeclared keys: {', '.join(unknown)}")
    missing = [key for key in keys if key not in value]
    if missing:
        raise C64IdentityError(f"{name} missing keys: {', '.join(missing)}")
    return {key: value[key] for key in keys}


def cognitive_runtime_digest(settings: Mapping[str, Any]) -> str:
    """SHA-256 of the runtime memory policy. Compile-time planner thresholds are excluded."""
    exact = _require_exact_map("cognitive_runtime", settings, COGNITIVE_RUNTIME_KEYS)
    normalized = {key: _require_int(f"cognitive_runtime.{key}", exact[key]) for key in COGNITIVE_RUNTIME_KEYS}
    return sha256_canonical(normalized)


def retrieval_weights_digest(weights: Mapping[str, Any]) -> str:
    """SHA-256 of the loaded retrieval_weights object."""
    exact = _require_exact_map("retrieval_weights", weights, RETRIEVAL_WEIGHT_KEYS)
    normalized = {key: _require_number(f"retrieval_weights.{key}", exact[key]) for key in RETRIEVAL_WEIGHT_KEYS}
    return sha256_canonical(normalized)


def cohort_identity(
    *,
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
) -> dict[str, Any]:
    """The object that is hashed. Field order is irrelevant after canonicalization."""
    tier = _require_text("evaluation_tier", evaluation_tier)
    if tier not in EVALUATION_TIERS:
        raise C64IdentityError(
            "evaluation_tier must be mock, dev, or authoritative; "
            "historical BenchmarkTier values are not evaluation tiers"
        )
    dimension = _require_int("model.embedding_dimension", embedding_dimension)
    if dimension <= 0:
        raise C64IdentityError("model.embedding_dimension must be positive")
    return {
        "environment_schema_version": ENVIRONMENT_SCHEMA_VERSION,
        "protocol_version": PROTOCOL_VERSION,
        "metric_schema_version": METRIC_SCHEMA_VERSION,
        "evaluation_tier": tier,
        "inference": {"backend_name": _require_text("inference.backend_name", inference_backend_name)},
        "prov": {
            "thoth_git_sha": _require_text("prov.thoth_git_sha", thoth_git_sha),
            "basic_agent_git_sha": _require_text("prov.basic_agent_git_sha", basic_agent_git_sha),
        },
        "model": {
            "llm_model": _require_text("model.llm_model", llm_model),
            "embedding_model": _require_text("model.embedding_model", embedding_model),
            "embedding_method": _require_text("model.embedding_method", embedding_method),
            "embedding_dimension": dimension,
        },
        "corpus": {"fingerprint": _require_text("corpus.fingerprint", corpus_fingerprint)},
        "retrieval_weights_sha256": retrieval_weights_digest(retrieval_weights),
        "cognitive_runtime_sha256": cognitive_runtime_digest(cognitive_runtime),
    }


def c64_cohort_fingerprint(identity: Mapping[str, Any]) -> str:
    return sha256_canonical(identity)


def build_prospective_pin(
    *,
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
    environment_hash: str | None = None,
) -> dict[str, Any]:
    """Return a c64-env-1 pin. Does not open a window or rewrite historical rows.

    environment_hash, when supplied, is stored beside the fingerprint and is not hashed.
    """
    identity = cohort_identity(
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
    )
    pin = {
        "environment_schema_version": identity["environment_schema_version"],
        "protocol_version": identity["protocol_version"],
        "metric_schema_version": identity["metric_schema_version"],
        "evaluation_tier": identity["evaluation_tier"],
        "inference": dict(identity["inference"]),
        "prov": dict(identity["prov"]),
        "model": dict(identity["model"]),
        "corpus": dict(identity["corpus"]),
        "retrieval_weights_sha256": identity["retrieval_weights_sha256"],
        "cognitive_runtime_sha256": identity["cognitive_runtime_sha256"],
        "c64_cohort_fingerprint": c64_cohort_fingerprint(identity),
    }
    if environment_hash is not None:
        pin["environment_hash"] = _require_text("environment_hash", environment_hash)
    if "window_id" in pin:
        raise C64IdentityError("phase 3 pins must not carry window_id")
    return pin
