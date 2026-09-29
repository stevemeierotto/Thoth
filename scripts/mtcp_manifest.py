#!/usr/bin/env python3
"""Validate an MTCP condition manifest. Fail closed on any mismatch."""

from __future__ import annotations

import argparse
import json
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


def validate(document: dict) -> list[str]:
    errors = []
    for key in REQUIRED:
        if key not in document:
            errors.append(f"missing {key}")
    for key, expected in SEALED.items():
        if document.get(key) != expected:
            errors.append(f"{key} mismatch")
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
    args = parser.parse_args()
    document = json.loads(args.manifest.read_text(encoding="utf-8"))
    errors = validate(document)
    if errors:
        print("\n".join(errors), file=sys.stderr)
        raise SystemExit(1)


if __name__ == "__main__":
    main()
