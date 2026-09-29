#!/usr/bin/env python3
"""S0-CHAR snapshot, restore, and contamination checks. No live Engine required."""

from __future__ import annotations

import argparse
import hashlib
import json
import shutil
import tarfile
from pathlib import Path

CORPUS_FILES = (
    "mtcp_configuration.md",
    "mtcp_recovery_order.md",
    "mtcp_service_failure.md",
)
ALLOWED_HASHES = {
    "mtcp_configuration.md": "4f5fdb9bb63268e8c5df5fcc483992af55d255745a64c4e4294b6ed3f387c722",
    "mtcp_recovery_order.md": "fc838d2681f114347de8e5de2cccd92d7fb79b30baa837d57343a628000c4000",
    "mtcp_service_failure.md": "522f62f3eb652d2023e0764ef92d56209a813502829254e0f60535eb770ac9b0",
}
RETRIEVAL_SHA = "b4eab95e104f84de05ee7c1ac26e1117447104c983ce924942e987db073df396"
CORPUS_SET = "155f9f0be3a5210730fd5042bcaf228ea1d4421d21b411ea1890e769d58a8921"


def sha256_file(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def fail(message: str) -> None:
    raise SystemExit(message)


def corpus_set_fingerprint(directory: Path) -> str:
    lines = []
    for name in sorted(CORPUS_FILES):
        lines.append(f"{name} {sha256_file(directory / name)}\n")
    return hashlib.sha256("".join(lines).encode()).hexdigest()


def workspace_listing(workspace: Path) -> dict:
    rows = []
    for path in sorted(p for p in workspace.rglob("*") if p.is_file()):
        rel = path.relative_to(workspace).as_posix()
        if rel.endswith("-wal") or rel.endswith("-shm"):
            fail(f"refusing snapshot while SQLite sidecar exists: {rel}")
        rows.append({"path": rel, "sha256": sha256_file(path)})
    return {"files": rows, "corpus_set": corpus_set_fingerprint(workspace)}


def verify_hashes(workspace: Path) -> None:
    for name, expected in ALLOWED_HASHES.items():
        path = workspace / name
        if not path.is_file():
            fail(f"missing corpus file {name}")
        if sha256_file(path) != expected:
            fail(f"corpus hash mismatch {name}")
    retrieval = workspace / "retrieval_config.json"
    if not retrieval.is_file() or sha256_file(retrieval) != RETRIEVAL_SHA:
        fail("retrieval config hash mismatch")
    if corpus_set_fingerprint(workspace) != CORPUS_SET:
        fail("corpus-set fingerprint mismatch")


def snapshot(workspace: Path, destination: Path) -> None:
    verify_hashes(workspace)
    listing = workspace_listing(workspace)
    destination.parent.mkdir(parents=True, exist_ok=True)
    manifest = destination.with_suffix(".json")
    manifest.write_text(json.dumps(listing, sort_keys=True), encoding="utf-8")
    with tarfile.open(destination, "w") as archive:
        for row in listing["files"]:
            archive.add(workspace / row["path"], arcname=row["path"])


def restore(workspace: Path, source: Path) -> None:
    if workspace.exists():
        shutil.rmtree(workspace)
    workspace.mkdir(parents=True)
    with tarfile.open(source, "r") as archive:
        archive.extractall(workspace, filter="data")
    verify(workspace, source.with_suffix(".json"))


def verify(workspace: Path, manifest_path: Path) -> None:
    verify_hashes(workspace)
    if any(workspace.rglob("*.db-wal")) or any(workspace.rglob("*.db-shm")):
        fail("SQLite WAL sidecar present")
    expected = json.loads(manifest_path.read_text(encoding="utf-8"))
    current = workspace_listing(workspace)
    if current != expected:
        fail("S0-CHAR workspace does not match the snapshot")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("action", choices=("snapshot", "restore", "verify"))
    parser.add_argument("--workspace", required=True, type=Path)
    parser.add_argument("--archive", required=True, type=Path)
    args = parser.parse_args()
    if args.action == "snapshot":
        snapshot(args.workspace, args.archive)
    elif args.action == "restore":
        restore(args.workspace, args.archive)
    else:
        verify(args.workspace, args.archive.with_suffix(".json"))


if __name__ == "__main__":
    main()
