#!/usr/bin/env python3
"""Deterministic tests for prospective C6.4 environment identity. No inference."""

from __future__ import annotations

import copy
import sys
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

import c64_environment_identity as ident  # noqa: E402


class TestFailure(Exception):
    pass


def base_kwargs() -> dict:
    return {
        "evaluation_tier": "authoritative",
        "inference_backend_name": "llama_cpp",
        "thoth_git_sha": "product-sha-aaa",
        "basic_agent_git_sha": "engine-sha-bbb",
        "llm_model": "configured-llm",
        "embedding_model": "configured-embed",
        "embedding_method": "External",
        "embedding_dimension": 768,
        "corpus_fingerprint": "corpus-abc",
        "retrieval_weights": {
            "trajectory": -0.05,
            "query": 0.4,
            "keyword": 0.3,
            "direction": 0.4,
        },
        "cognitive_runtime": {
            "prune_batch_size": 10,
            "max_hot_age_days": 30,
            "max_hot_messages": 50,
        },
    }


def expect_error(label: str, **overrides) -> None:
    kwargs = base_kwargs()
    kwargs.update(overrides)
    try:
        ident.build_prospective_pin(**kwargs)
    except ident.C64IdentityError:
        return
    raise TestFailure(f"{label}: expected C64IdentityError")


def test_schema_and_independence() -> None:
    pin = ident.build_prospective_pin(**base_kwargs(), environment_hash="e1-hash-unchanged")
    if pin["environment_schema_version"] != "c64-env-1":
        raise TestFailure(f"schema {pin['environment_schema_version']}")
    if pin["evaluation_tier"] != "authoritative":
        raise TestFailure("evaluation tier was rewritten")
    if pin["inference"]["backend_name"] != "llama_cpp":
        raise TestFailure("provider was rewritten")
    mock_same_provider = ident.build_prospective_pin(**{**base_kwargs(), "evaluation_tier": "mock"})
    if mock_same_provider["evaluation_tier"] != "mock":
        raise TestFailure("mock tier not preserved beside llama_cpp")
    if mock_same_provider["inference"]["backend_name"] != "llama_cpp":
        raise TestFailure("changing evaluation tier changed provider")
    if mock_same_provider["c64_cohort_fingerprint"] == pin["c64_cohort_fingerprint"]:
        raise TestFailure("evaluation tier did not change the fingerprint")
    ollama = ident.build_prospective_pin(
        **{**base_kwargs(), "inference_backend_name": "ollama"}
    )
    if ollama["evaluation_tier"] != "authoritative":
        raise TestFailure("changing provider changed evaluation tier")
    if ollama["c64_cohort_fingerprint"] == pin["c64_cohort_fingerprint"]:
        raise TestFailure("provider did not change the fingerprint")
    if "window_id" in pin:
        raise TestFailure("pin opened a window id")


def test_stable_and_order_independent() -> None:
    first = ident.build_prospective_pin(**base_kwargs())
    second = ident.build_prospective_pin(**base_kwargs())
    if first["c64_cohort_fingerprint"] != second["c64_cohort_fingerprint"]:
        raise TestFailure("same environment produced two fingerprints")
    scrambled = base_kwargs()
    scrambled["retrieval_weights"] = {
        "keyword": 0.3,
        "direction": 0.4,
        "trajectory": -0.05,
        "query": 0.4,
    }
    scrambled["cognitive_runtime"] = {
        "max_hot_messages": 50,
        "prune_batch_size": 10,
        "max_hot_age_days": 30,
    }
    third = ident.build_prospective_pin(**scrambled)
    if third["c64_cohort_fingerprint"] != first["c64_cohort_fingerprint"]:
        raise TestFailure("map key order changed the fingerprint")
    identity = ident.cohort_identity(**base_kwargs())
    reversed_identity = {key: identity[key] for key in reversed(list(identity))}
    if ident.c64_cohort_fingerprint(identity) != ident.c64_cohort_fingerprint(reversed_identity):
        raise TestFailure("cohort object key order changed the fingerprint")


def test_each_cohort_input_changes_fingerprint() -> None:
    baseline = ident.build_prospective_pin(**base_kwargs())["c64_cohort_fingerprint"]
    cases = {
        "thoth_git_sha": {"thoth_git_sha": "product-sha-zzz"},
        "basic_agent_git_sha": {"basic_agent_git_sha": "engine-sha-zzz"},
        "inference_backend_name": {"inference_backend_name": "ollama"},
        "llm_model": {"llm_model": "other-llm"},
        "embedding_model": {"embedding_model": "other-embed"},
        "embedding_method": {"embedding_method": "TfIdf"},
        "embedding_dimension": {"embedding_dimension": 10000},
        "corpus_fingerprint": {"corpus_fingerprint": "corpus-other"},
        "evaluation_tier": {"evaluation_tier": "dev"},
        "retrieval_weights": {
            "retrieval_weights": {
                "query": 0.4,
                "direction": 0.4,
                "trajectory": 0.0,
                "keyword": 0.3,
            }
        },
        "cognitive_runtime": {
            "cognitive_runtime": {
                "max_hot_messages": 40,
                "max_hot_age_days": 30,
                "prune_batch_size": 10,
            }
        },
    }
    for name, overrides in cases.items():
        kwargs = base_kwargs()
        kwargs.update(overrides)
        fingerprint = ident.build_prospective_pin(**kwargs)["c64_cohort_fingerprint"]
        if fingerprint == baseline:
            raise TestFailure(f"{name} did not change c64_cohort_fingerprint")


def test_volatile_values_do_not_change_fingerprint() -> None:
    left = ident.build_prospective_pin(**base_kwargs(), environment_hash="hash-a")
    right = ident.build_prospective_pin(**base_kwargs(), environment_hash="hash-b")
    if left["c64_cohort_fingerprint"] != right["c64_cohort_fingerprint"]:
        raise TestFailure("environment_hash changed the cohort fingerprint")
    if left["environment_hash"] != "hash-a" or right["environment_hash"] != "hash-b":
        raise TestFailure("environment_hash was not stored beside the fingerprint")
    identity = ident.cohort_identity(**base_kwargs())
    if "environment_hash" in identity or "window_id" in identity or "run_id" in identity:
        raise TestFailure("volatile fields entered the hashed object")


def test_fail_closed() -> None:
    expect_error("blank model", llm_model="  ")
    expect_error("missing backend", inference_backend_name="")
    expect_error("historical FULL is not a tier", evaluation_tier="FULL")
    expect_error("historical OLLAMA is not a tier", evaluation_tier="OLLAMA")
    expect_error("historical MOCK is not a tier", evaluation_tier="MOCK")
    weights = dict(base_kwargs()["retrieval_weights"])
    del weights["keyword"]
    expect_error("missing retrieval key", retrieval_weights=weights)
    extra = dict(base_kwargs()["retrieval_weights"])
    extra["graph"] = 0.3
    expect_error("undeclared retrieval key", retrieval_weights=extra)
    cognitive = dict(base_kwargs()["cognitive_runtime"])
    del cognitive["prune_batch_size"]
    expect_error("missing cognitive key", cognitive_runtime=cognitive)
    expect_error("bad dimension", embedding_dimension=True)
    historical = {
        "event": "BENCHMARK_ENV",
        "env": {"runtime": {"tier": "OLLAMA"}, "environment_hash": "old"},
    }
    snapshot = copy.deepcopy(historical)
    try:
        ident.build_prospective_pin(
            evaluation_tier=historical["env"]["runtime"]["tier"],
            **{k: v for k, v in base_kwargs().items() if k != "evaluation_tier"},
        )
        raise TestFailure("accepted historical runtime.tier as evaluation_tier")
    except ident.C64IdentityError:
        pass
    if historical != snapshot:
        raise TestFailure("historical environment record was mutated")


def test_code_identity_is_two_shas() -> None:
    pin = ident.build_prospective_pin(**base_kwargs())
    if pin["prov"]["thoth_git_sha"] == pin["prov"]["basic_agent_git_sha"]:
        raise TestFailure("fixture accidentally used one SHA for both repositories")
    product = ident.build_prospective_pin(**{**base_kwargs(), "thoth_git_sha": "only-product"})
    engine = ident.build_prospective_pin(**{**base_kwargs(), "basic_agent_git_sha": "only-engine"})
    base = pin["c64_cohort_fingerprint"]
    if len({base, product["c64_cohort_fingerprint"], engine["c64_cohort_fingerprint"]}) != 3:
        raise TestFailure("product SHA and engine SHA are not independent cohort inputs")


def main() -> int:
    tests = [
        test_schema_and_independence,
        test_stable_and_order_independent,
        test_each_cohort_input_changes_fingerprint,
        test_volatile_values_do_not_change_fingerprint,
        test_fail_closed,
        test_code_identity_is_two_shas,
    ]
    try:
        for test in tests:
            test()
    except TestFailure as exc:
        print(f"FAIL {exc}", file=sys.stderr)
        return 1
    print("c64 environment identity")
    for test in tests:
        print(f"  {test.__name__}: ok")
    print("  OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
