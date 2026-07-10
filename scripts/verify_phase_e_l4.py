#!/usr/bin/env python3
"""
verify_phase_e_l4.py — Phase E Step 3 L4 *verification* package checker.

Verification ≠ reproduction.
  - This script checks frozen artifacts, protocol-doc hashes, env compare,
    scoped summary fields, and package fingerprint.
  - It MUST NOT invoke the episodic learning harness or any live LLM.

Usage:
  python3 scripts/verify_phase_e_l4.py           # verify only
  python3 scripts/verify_phase_e_l4.py --write   # recompute hashes, digest, status, then verify

Exit 0 on verification success; non-zero on failure.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import sys
from pathlib import Path
from typing import Any

ROOT = Path(__file__).resolve().parents[1]

CITED_RUN_IDS = ["run-1783639167839", "run-1783639378206"]

# Paths relative to repo root (POSIX).
CITED_ARTIFACTS = [
    "docs/baselines/artifacts/phase_e/run_01_summary.json",
    "docs/baselines/artifacts/phase_e/run_02_summary.json",
    "docs/baselines/artifacts/phase_e/run_01_benchmark_env.json",
    "docs/baselines/artifacts/phase_e/run_02_benchmark_env.json",
    "docs/baselines/artifacts/phase_e/episodic_learning_benchmark_snapshot.jsonl",
    "docs/baselines/artifacts/phase_e/run_01_l2_environment.json",
    "docs/baselines/artifacts/phase_e/run_02_l2_environment.json",
]

PROTOCOL_DOCS = [
    "docs/benchmark_results/phase_e_strict_v1.md",
    "docs/E2_PROTOCOL.md",
    "docs/phases/E_ANALYSIS_PLAN.md",
    "docs/baselines/PHASE_E_PROVENANCE.md",
    "docs/baselines/phase_e_l4_verification.md",
]

# Package digest members (plan-locked). Manifest digest field omitted from its own hash input.
PACKAGE_MEMBERS = CITED_ARTIFACTS + [
    "docs/baselines/phase_e_run_manifest.json",
    "docs/baselines/PHASE_E_PROVENANCE.md",
    "docs/baselines/phase_e_l4_verification.md",
    "docs/benchmark_results/phase_e_strict_v1.md",
    "docs/E2_PROTOCOL.md",
    "docs/phases/E_ANALYSIS_PLAN.md",
]

MANIFEST_PATH = "docs/baselines/phase_e_run_manifest.json"
STATUS_PATH = "docs/baselines/phase_e_l4_status.json"
VERIFICATION_DOC = "docs/baselines/phase_e_l4_verification.md"
PROVENANCE_DOC = "docs/baselines/PHASE_E_PROVENANCE.md"

EXPECTED_ENV_HASH = "155b66a41bbca1bdba441e301841baaa35fd08b6681c3ebc1d7158610a96f790"
EXPECTED_INDEX_HASH = "c150f0362342c32cefa53e3653f84fa812aa9545b180e2fb6d8982c0124c50f8"
EXPECTED_FP = "ddc5c865b7edbff73a2702ac1b1d2a00075baa6992f480d23d490fe2d551668e"


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def rel(path: str) -> Path:
    return ROOT / path


def load_json(path: Path) -> Any:
    with path.open(encoding="utf-8") as f:
        return json.load(f)


def dump_json(path: Path, obj: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(obj, indent=2, sort_keys=False) + "\n", encoding="utf-8")


def hash_map(paths: list[str]) -> dict[str, str]:
    out: dict[str, str] = {}
    for p in paths:
        fp = rel(p)
        if not fp.is_file():
            raise FileNotFoundError(f"missing required file: {p}")
        out[p] = sha256_file(fp)
    return out


def canonical_listing(members: dict[str, str]) -> bytes:
    lines = [f"{path}\t{digest}\n" for path, digest in sorted(members.items())]
    return "".join(lines).encode("utf-8")


def package_digest_from_files() -> tuple[str, dict[str, str]]:
    """Hash package members; for the manifest, hash a copy without phase_e_l4_package_sha256."""
    members: dict[str, str] = {}
    for p in PACKAGE_MEMBERS:
        fp = rel(p)
        if not fp.is_file():
            raise FileNotFoundError(f"missing package member: {p}")
        if p == MANIFEST_PATH:
            doc = load_json(fp)
            doc.pop("phase_e_l4_package_sha256", None)
            # Stable bytes for digest input
            blob = json.dumps(doc, indent=2, sort_keys=True) + "\n"
            members[p] = hashlib.sha256(blob.encode("utf-8")).hexdigest()
        else:
            members[p] = sha256_file(fp)
    digest = hashlib.sha256(canonical_listing(members)).hexdigest()
    return digest, members


def check_summaries() -> list[str]:
    errors: list[str] = []
    for label, path, run_id in [
        ("A", "docs/baselines/artifacts/phase_e/run_01_summary.json", CITED_RUN_IDS[0]),
        ("B", "docs/baselines/artifacts/phase_e/run_02_summary.json", CITED_RUN_IDS[1]),
    ]:
        doc = load_json(rel(path))
        if doc.get("run_id") != run_id:
            errors.append(f"{label}: run_id {doc.get('run_id')!r} != {run_id!r}")
        if doc.get("e2_outcome") != "FAILURE":
            errors.append(f"{label}: e2_outcome {doc.get('e2_outcome')!r} != FAILURE")
        if float(doc.get("mean_episodic_lift", -1)) != 0.0:
            errors.append(f"{label}: mean_episodic_lift {doc.get('mean_episodic_lift')!r} != 0.0")
        if doc.get("cases_passed") not in (1, "1", "1/3"):
            # summaries may store int 1
            if doc.get("cases_passed") != 1:
                errors.append(f"{label}: cases_passed {doc.get('cases_passed')!r} unexpected")
        fp = doc.get("evaluation_fingerprint")
        fp_hash = fp.get("fingerprint_hash") if isinstance(fp, dict) else fp
        if fp_hash != EXPECTED_FP:
            errors.append(f"{label}: fingerprint_hash mismatch")
        cases = doc.get("case_results") or []
        if len(cases) != 3:
            errors.append(f"{label}: expected 3 case_results, got {len(cases)}")
        for c in cases:
            if c.get("evaluation_resolution") != "SCORED_SUCCESS":
                errors.append(
                    f"{label}/{c.get('case_id')}: evaluation_resolution "
                    f"{c.get('evaluation_resolution')!r}"
                )
            for arm in ("cold", "warm"):
                a = c.get(arm) or {}
                if a.get("terminal_state") != "COMPLETED":
                    errors.append(
                        f"{label}/{c.get('case_id')}/{arm}: terminal_state "
                        f"{a.get('terminal_state')!r}"
                    )
                tok = a.get("total_tokens")
                if tok is None or int(tok) <= 0:
                    errors.append(f"{label}/{c.get('case_id')}/{arm}: total_tokens={tok}")
    return errors


def check_l2_env() -> list[str]:
    errors: list[str] = []
    envs = []
    for path, run_id in [
        ("docs/baselines/artifacts/phase_e/run_01_l2_environment.json", CITED_RUN_IDS[0]),
        ("docs/baselines/artifacts/phase_e/run_02_l2_environment.json", CITED_RUN_IDS[1]),
    ]:
        doc = load_json(rel(path))
        envs.append(doc)
        if doc.get("run_id") != run_id:
            errors.append(f"L2 {path}: run_id mismatch")
        if doc.get("environment_hash") != EXPECTED_ENV_HASH:
            errors.append(f"L2 {path}: environment_hash mismatch")
        if doc.get("index_hash") != EXPECTED_INDEX_HASH:
            errors.append(f"L2 {path}: index_hash mismatch")
    if envs[0].get("environment_hash") != envs[1].get("environment_hash"):
        errors.append("L2 A/B environment_hash differ")
    if envs[0].get("index_hash") != envs[1].get("index_hash"):
        errors.append("L2 A/B index_hash differ")
    if envs[0].get("run_id") == envs[1].get("run_id"):
        errors.append("L2 A/B run_id unexpectedly identical")
    return errors


def build_manifest(
    artifact_hashes: dict[str, str],
    protocol_hashes: dict[str, str],
    package_sha: str,
    superseded_hashes: dict[str, str],
) -> dict[str, Any]:
    return {
        "manifest_version": "phase_e_l4_v1",
        "package_version": 1,
        "l4_mode": "verification",
        "evidence_scope": "n=3_strict_trio",
        "protocol_revision": {
            "e_analysis_plan": "E-AP v1.1",
            "e2_protocol": "E2_PROTOCOL.md v1.2",
        },
        "ep015_complete": True,
        "e233_complete": True,
        "step2_seal_commit": "0a38f22bb7970356e81b5a4907cd0df863c26d1b",
        "step3_plan_lock_commit": "07491b4a25e47ceca359139591b8a7c4d373bbf7",
        "git_sha": {
            "thoth_at_step2_run": "0a38f22",
            "basic_agent": "77508c4c2335e801692648074e657e3d29d846d5",
            "step3_plan_lock": "07491b4a25e47ceca359139591b8a7c4d373bbf7",
        },
        "inference_tier": "authoritative",
        "inference_backend_identifier": "ollama",
        "harness_command": (
            "THOTH_E2_WIRING_STAGE=B "
            "./build/debug/external/basic_agent/run_episodic_learning_benchmark --authoritative"
        ),
        "reproduction_recipe": {
            "status": "DOCUMENTED_NOT_EXECUTED",
            "command": (
                "THOTH_E2_WIRING_STAGE=B "
                "./build/debug/external/basic_agent/run_episodic_learning_benchmark --authoritative"
            ),
            "note": "Step 3 verifies the sealed package only; reproduction is deferred.",
        },
        "benchmark_corpus_identifier": "v1.2_strict_trio",
        "e2_28_result": {
            "status": "PASS",
            "diagnosis_bucket": 0,
            "verification_doc": "docs/baselines/phase_e_baseline_verification.md",
        },
        "phase_e_l4_package_sha256": package_sha,
        "file_hashes": {
            "cited_artifacts": artifact_hashes,
            "protocol_documents": protocol_hashes,
            "superseded_archives_historical": superseded_hashes,
        },
        "runs": [
            {
                "label": "A",
                "run_id": CITED_RUN_IDS[0],
                "environment_hash": EXPECTED_ENV_HASH,
                "index_hash": EXPECTED_INDEX_HASH,
                "evaluation_fingerprint": EXPECTED_FP,
                "corpus_snapshot_id": EXPECTED_INDEX_HASH,
                "wiring_stage": "B",
                "scoring_tier": "STRICT",
                "official_scoring": True,
                "model_identifier": {
                    "llm_model": "qwen2.5:3b",
                    "embedding_model": "nomic-embed-text:v1.5",
                    "embedding_method": "External",
                    "model_version_or_weights_hash": "qwen2.5:3b",
                    "embedding_model_version": "External:2",
                },
                "e2_outcome": "FAILURE",
                "evaluation_resolution": "SCORED_FAILURE",
                "mean_episodic_lift": 0.0,
                "cases_passed": "1/3",
                "artifact_paths": {
                    "summary": "docs/baselines/artifacts/phase_e/run_01_summary.json",
                    "benchmark_env_completion_envelope": (
                        "docs/baselines/artifacts/phase_e/run_01_benchmark_env.json"
                    ),
                    "l2_environment": "docs/baselines/artifacts/phase_e/run_01_l2_environment.json",
                },
            },
            {
                "label": "B",
                "run_id": CITED_RUN_IDS[1],
                "environment_hash": EXPECTED_ENV_HASH,
                "index_hash": EXPECTED_INDEX_HASH,
                "evaluation_fingerprint": EXPECTED_FP,
                "corpus_snapshot_id": EXPECTED_INDEX_HASH,
                "wiring_stage": "B",
                "scoring_tier": "STRICT",
                "official_scoring": True,
                "model_identifier": {
                    "llm_model": "qwen2.5:3b",
                    "embedding_model": "nomic-embed-text:v1.5",
                    "embedding_method": "External",
                    "model_version_or_weights_hash": "qwen2.5:3b",
                    "embedding_model_version": "External:2",
                },
                "e2_outcome": "FAILURE",
                "evaluation_resolution": "SCORED_FAILURE",
                "mean_episodic_lift": 0.0,
                "cases_passed": "1/3",
                "artifact_paths": {
                    "summary": "docs/baselines/artifacts/phase_e/run_02_summary.json",
                    "benchmark_env_completion_envelope": (
                        "docs/baselines/artifacts/phase_e/run_02_benchmark_env.json"
                    ),
                    "l2_environment": "docs/baselines/artifacts/phase_e/run_02_l2_environment.json",
                },
            },
        ],
        "artifact_bundle": "docs/baselines/artifacts/phase_e/",
        "superseded_archives": [
            "docs/baselines/artifacts/phase_e/superseded_pre_ep015/",
            "docs/baselines/artifacts/phase_e/superseded_e228_fail_pre_fix/",
        ],
        "run_record": "docs/benchmark_results/phase_e_strict_v1.md",
        "combined_jsonl": "docs/baselines/artifacts/phase_e/episodic_learning_benchmark_snapshot.jsonl",
        "l4_verification_doc": VERIFICATION_DOC,
        "l4_provenance_doc": PROVENANCE_DOC,
        "l4_status_doc": STATUS_PATH,
        "verifier": "scripts/verify_phase_e_l4.py",
    }


def write_status(package_sha: str, verified: bool) -> dict[str, Any]:
    status = {
        "l4_status": "VERIFIED" if verified else "FAILED",
        "e_q2_verification": verified,
        "e_q2_reproduction": False,
        "e_q2_reproduction_status": "DEFERRED",
        "evidence_scope": "n=3_strict_trio",
        "package_version": 1,
        "phase_e_l4_package_sha256": package_sha,
        "cited_run_ids": CITED_RUN_IDS,
        "verifier": "scripts/verify_phase_e_l4.py",
        "l4_mode": "verification",
    }
    dump_json(rel(STATUS_PATH), status)
    return status


def collect_superseded_hashes() -> dict[str, str]:
    base = rel("docs/baselines/artifacts/phase_e")
    out: dict[str, str] = {}
    for sub in ("superseded_pre_ep015", "superseded_e228_fail_pre_fix"):
        d = base / sub
        if not d.is_dir():
            continue
        for f in sorted(d.rglob("*")):
            if f.is_file():
                rel_path = f.relative_to(ROOT).as_posix()
                out[rel_path] = sha256_file(f)
    return out


def verify_against_manifest(manifest: dict[str, Any]) -> list[str]:
    errors: list[str] = []
    stored = manifest.get("file_hashes") or {}
    cited = stored.get("cited_artifacts") or {}
    proto = stored.get("protocol_documents") or {}
    for p, expected in {**cited, **proto}.items():
        fp = rel(p)
        if not fp.is_file():
            errors.append(f"missing hashed file: {p}")
            continue
        actual = sha256_file(fp)
        if actual != expected:
            errors.append(f"hash mismatch: {p}")
    package_sha = manifest.get("phase_e_l4_package_sha256")
    recomputed, _ = package_digest_from_files()
    if package_sha != recomputed:
        errors.append(
            f"phase_e_l4_package_sha256 mismatch: manifest={package_sha} recomputed={recomputed}"
        )
    if manifest.get("reproduction_recipe", {}).get("status") != "DOCUMENTED_NOT_EXECUTED":
        errors.append("reproduction_recipe.status must be DOCUMENTED_NOT_EXECUTED")
    if manifest.get("l4_mode") != "verification":
        errors.append("l4_mode must be verification")
    errors.extend(check_summaries())
    errors.extend(check_l2_env())
    return errors


def seal() -> str:
    artifact_hashes = hash_map(CITED_ARTIFACTS)
    # Protocol docs must exist before hashing; provenance + verification already written.
    protocol_hashes = hash_map(PROTOCOL_DOCS)
    superseded = collect_superseded_hashes()

    # First write manifest without package sha so digest input is stable, then set sha.
    draft = build_manifest(artifact_hashes, protocol_hashes, package_sha="PENDING", superseded_hashes=superseded)
    draft.pop("phase_e_l4_package_sha256", None)
    dump_json(rel(MANIFEST_PATH), draft)

    package_sha, _ = package_digest_from_files()
    final = build_manifest(artifact_hashes, protocol_hashes, package_sha, superseded)
    dump_json(rel(MANIFEST_PATH), final)

    # Re-hash protocol docs that might be unchanged; verification doc already hashed.
    # Recompute digest once more after final manifest write (manifest content changed by adding sha field,
    # but digest input strips that field — should be stable).
    package_sha2, _ = package_digest_from_files()
    if package_sha2 != package_sha:
        final["phase_e_l4_package_sha256"] = package_sha2
        dump_json(rel(MANIFEST_PATH), final)
        package_sha = package_sha2
    return package_sha


def main() -> int:
    parser = argparse.ArgumentParser(description="Phase E L4 verification package checker")
    parser.add_argument(
        "--write",
        action="store_true",
        help="Recompute file hashes, package digest, and status JSON, then verify",
    )
    args = parser.parse_args()

    print("Phase E L4 verification (reproduction deferred — harness not invoked)")
    try:
        if args.write:
            print("  --write: sealing hashes + package digest…")
            package_sha = seal()
            print(f"  phase_e_l4_package_sha256={package_sha}")
        else:
            if not rel(MANIFEST_PATH).is_file():
                print("ERROR: manifest missing; run with --write first", file=sys.stderr)
                return 2
            package_sha = load_json(rel(MANIFEST_PATH)).get("phase_e_l4_package_sha256", "")

        manifest = load_json(rel(MANIFEST_PATH))
        errors = verify_against_manifest(manifest)
        verified = not errors
        status = write_status(package_sha or manifest.get("phase_e_l4_package_sha256", ""), verified)

        if errors:
            print("VERIFICATION FAILED:")
            for e in errors:
                print(f"  - {e}")
            print(json.dumps(status, indent=2))
            return 1

        print("VERIFICATION PASSED")
        print(f"  l4_status={status['l4_status']}")
        print(f"  e_q2_verification={status['e_q2_verification']}")
        print(f"  e_q2_reproduction={status['e_q2_reproduction']} ({status['e_q2_reproduction_status']})")
        print(f"  phase_e_l4_package_sha256={status['phase_e_l4_package_sha256']}")
        return 0
    except Exception as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        write_status("", False)
        return 2


if __name__ == "__main__":
    sys.exit(main())
