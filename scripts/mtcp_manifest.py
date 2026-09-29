#!/usr/bin/env python3
"""Validate an MTCP condition manifest. Fail closed on any mismatch."""

from __future__ import annotations

import argparse
import hashlib
import json
import subprocess
import sys
from pathlib import Path

REQUIRED = (
    "protocol",
    "lifecycle",
    "seal_commit",
    "product_sha",
    "engine_sha",
    "engine_image",
    "llama_image_digest",
    "llama_build",
    "chat_model_path",
    "chat_model_sha256",
    "embedding_model_path",
    "embedding_model_sha256",
    "n_ctx",
    "embedding_context",
    "generation_ceiling",
    "temperature",
    "top_p",
    "top_k",
    "min_p",
    "seed_mode",
    "inference_backend",
    "inference_mode",
    "text_timeout_seconds",
    "embedding_timeout_seconds",
    "embedding_strict",
    "reflection_maximum",
    "retrieval_config_sha256",
    "corpus_set_sha256",
    "appendix_asset_sha256",
    "task_set_sha256",
    "s0_char_id",
    "s0_char_ok",
    "stage",
    "started_at",
)
SEALED = {
    "protocol": "MTCP v1.0",
    "lifecycle": "SEALED",
    "seal_commit": "a3690f7fc50da920992829bcbc39e19aa983894f",
    "n_ctx": 8192,
    "retrieval_config_sha256": "b4eab95e104f84de05ee7c1ac26e1117447104c983ce924942e987db073df396",
    "corpus_set_sha256": "155f9f0be3a5210730fd5042bcaf228ea1d4421d21b411ea1890e769d58a8921",
    "appendix_asset_sha256": "7d8a3b4bd07663737c50d3a56304dedaca37e2b6db9cddea8c6e0140a353ec39",
    "task_set_sha256": "a41031ff84308edbaf40804f4e96aae8b85395a0ff9e00fffc7afe402002ec90",
}


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def observe_repository(repo: Path) -> dict[str, str]:
    product = subprocess.check_output(
        ["git", "-C", str(repo), "rev-parse", "HEAD"], text=True
    ).strip()
    engine = subprocess.check_output(
        ["git", "-C", str(repo / "external" / "basic_agent"), "rev-parse", "HEAD"],
        text=True,
    ).strip()
    return {"product_sha": product, "engine_sha": engine}


def observe_model_bytes(chat_model: Path, embedding_model: Path) -> dict[str, str]:
    if not chat_model.is_file():
        raise FileNotFoundError(f"chat model bytes not found: {chat_model}")
    if not embedding_model.is_file():
        raise FileNotFoundError(f"embedding model bytes not found: {embedding_model}")
    return {
        "chat_model_sha256": sha256_file(chat_model),
        "embedding_model_sha256": sha256_file(embedding_model),
    }


def validate(document: dict, observed: dict | None = None) -> list[str]:
    errors = []
    for key in REQUIRED:
        if key not in document:
            errors.append(f"missing {key}")
    for key, expected in SEALED.items():
        if document.get(key) != expected:
            errors.append(f"{key} mismatch")
    if not observed:
        errors.append("runtime identity was not observed")
    else:
        for key in (
            "product_sha",
            "engine_sha",
            "chat_model_sha256",
            "embedding_model_sha256",
        ):
            actual = observed.get(key)
            if not actual:
                errors.append(f"{key} was not observed")
            elif document.get(key) != actual:
                errors.append(f"{key} does not match observed bytes")
    if document.get("s0_char_ok") is not True:
        errors.append("s0_char_ok is not true")
    if document.get("stage") == "B" and document.get("stage_b_predicate") is not True:
        errors.append("stage B without a true predicate")
    if document.get("seed_mode") == "deterministic" and document.get("seed") != 17001:
        errors.append("deterministic mode requires seed 17001")
    if document.get("seed_mode") == "fallback" and "seed" in document:
        errors.append("fallback mode must omit seed")
    return errors


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("manifest", type=Path)
    parser.add_argument("--repo", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--chat-model", type=Path, required=True)
    parser.add_argument("--embedding-model", type=Path, required=True)
    args = parser.parse_args()
    document = json.loads(args.manifest.read_text(encoding="utf-8"))
    observed = observe_repository(args.repo)
    observed.update(observe_model_bytes(args.chat_model, args.embedding_model))
    errors = validate(document, observed)
    if errors:
        print("\n".join(errors), file=sys.stderr)
        raise SystemExit(1)


if __name__ == "__main__":
    main()
