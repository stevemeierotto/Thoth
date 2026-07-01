#!/usr/bin/env python3
"""
compare_benchmark_env.py — Compare two benchmark environment sidecars or JSONL rows.

Usage:
    python3 scripts/compare_benchmark_env.py sidecar_a.json sidecar_b.json
    python3 scripts/compare_benchmark_env.py --jsonl logs/benchmark_env.jsonl --run-id run-123

Exit code 0 when environments match (or differences are informational only).
Exit code 1 with --strict when run_id, environment_hash, or index_hash differ.
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import Any


def load_sidecar(path: Path) -> dict[str, Any]:
    with open(path, encoding="utf-8") as f:
        return json.load(f)


def load_jsonl_row(path: Path, run_id: str | None) -> dict[str, Any]:
    rows = []
    with open(path, encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            try:
                row = json.loads(line)
            except json.JSONDecodeError:
                continue
            if row.get("event") == "BENCHMARK_ENV":
                rows.append(row)
    if not rows:
        raise ValueError(f"No BENCHMARK_ENV rows in {path}")
    if run_id:
        for row in rows:
            if row.get("run_id") == run_id:
                return row
        raise ValueError(f"No BENCHMARK_ENV row with run_id={run_id!r} in {path}")
    return rows[-1]


def sidecar_from_source(path: Path, run_id: str | None) -> dict[str, Any]:
    if path.suffix == ".jsonl" or path.name.endswith(".jsonl"):
        row = load_jsonl_row(path, run_id)
        env = row.get("env") or row.get("payload", {}).get("environment") or {}
        return {
            "run_id": row.get("run_id", ""),
            "environment_hash": row.get("env_hash", ""),
            "index_hash": env.get("index_hash", "") if isinstance(env, dict) else "",
            "environment": env if isinstance(env, dict) else {},
            "_source": str(path),
        }
    doc = load_sidecar(path)
    doc["_source"] = str(path)
    return doc


def nested_get(doc: dict[str, Any], *keys: str, default: Any = "") -> Any:
    cur: Any = doc
    for key in keys:
        if not isinstance(cur, dict):
            return default
        cur = cur.get(key, default)
    return cur


def compare_docs(a: dict[str, Any], b: dict[str, Any], strict: bool) -> bool:
    fields = [
        ("run_id", lambda d: d.get("run_id", "")),
        ("environment_hash", lambda d: d.get("environment_hash", "")),
        ("index_hash", lambda d: d.get("index_hash", "")),
    ]
    env_fields = [
        ("tier", lambda d: nested_get(d, "environment", "runtime", "tier")),
        ("llm_model", lambda d: nested_get(d, "environment", "model", "llm_model")),
        ("embedding_model", lambda d: nested_get(d, "environment", "model", "embedding_model")),
        ("corpus_fingerprint", lambda d: nested_get(d, "environment", "corpus", "fingerprint")),
        ("thoth_git_sha", lambda d: nested_get(d, "environment", "prov", "thoth_git_sha")),
        ("basic_agent_git_sha", lambda d: nested_get(d, "environment", "prov", "basic_agent_git_sha")),
    ]

    ok = True
    print(f"\nA: {a.get('_source', 'sidecar')}")
    print(f"B: {b.get('_source', 'sidecar')}\n")

    for label, getter in fields:
        va, vb = getter(a), getter(b)
        match = va == vb
        mark = "OK" if match else ("DIFF" if strict else "note")
        print(f"  [{mark}] {label}: {va!r} vs {vb!r}")
        if strict and not match:
            ok = False

    for label, getter in env_fields:
        va, vb = getter(a), getter(b)
        if va or vb:
            match = va == vb
            mark = "OK" if match else "note"
            print(f"  [{mark}] {label}: {va!r} vs {vb!r}")

    mismatch_a = nested_get(a, "environment", "index", "index_mismatch", default=None)
    mismatch_b = nested_get(b, "environment", "index", "index_mismatch", default=None)
    if mismatch_a or mismatch_b:
        print(f"  [note] index_mismatch A: {mismatch_a}")
        print(f"  [note] index_mismatch B: {mismatch_b}")

    return ok


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Compare two benchmark environment sidecars or JSONL BENCHMARK_ENV rows."
    )
    parser.add_argument("left", nargs="?", help="First sidecar JSON or benchmark_env.jsonl")
    parser.add_argument("right", nargs="?", help="Second sidecar JSON or benchmark_env.jsonl")
    parser.add_argument(
        "--jsonl",
        default="",
        help="Compare two run_id rows from the same JSONL (requires --run-id-a and --run-id-b)",
    )
    parser.add_argument("--run-id-a", default="", help="run_id for left JSONL row")
    parser.add_argument("--run-id-b", default="", help="run_id for right JSONL row")
    parser.add_argument(
        "--strict",
        action="store_true",
        help="Exit non-zero when run_id, environment_hash, or index_hash differ",
    )
    args = parser.parse_args()

    if args.jsonl:
        jsonl_path = Path(args.jsonl)
        if not args.run_id_a or not args.run_id_b:
            print("ERROR: --jsonl requires --run-id-a and --run-id-b", file=sys.stderr)
            return 2
        doc_a = sidecar_from_source(jsonl_path, args.run_id_a)
        doc_b = sidecar_from_source(jsonl_path, args.run_id_b)
    elif args.left and args.right:
        doc_a = sidecar_from_source(Path(args.left), None)
        doc_b = sidecar_from_source(Path(args.right), None)
    else:
        parser.print_help()
        return 2

    ok = compare_docs(doc_a, doc_b, args.strict)
    print()
    if ok:
        print("RESULT: compare complete" + (" — match" if args.strict else ""))
        return 0
    print("RESULT: strict mismatch detected")
    return 1


if __name__ == "__main__":
    sys.exit(main())
