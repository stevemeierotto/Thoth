#!/usr/bin/env bash
# Print SHA-256 of each GGUF on the shared model volume. Read-only. Does not copy files.
# Run when an experiment is frozen. Filename identity is not provenance.
#
# Usage:
#   ./docker/record-gguf-sha256.sh

set -euo pipefail

VOLUME="${THOTH_MODELS_VOLUME:-thoth_llama-models}"
LLAMA_IMAGE="${LLAMA_HASH_IMAGE:-ghcr.io/ggml-org/llama.cpp:server@sha256:823b6f019cafbee8878dfdd0d4750eae4f81dfafb60dc1fbefb66794a59903c8}"

printf '[record-gguf] volume=%s mount=/models:ro\n' "$VOLUME"
docker volume inspect "$VOLUME" >/dev/null

docker run --rm \
  -v "${VOLUME}:/models:ro" \
  --entrypoint sh \
  "$LLAMA_IMAGE" \
  -c 'set -eu; found=0; for f in /models/*.gguf; do [ -f "$f" ] || continue; found=1; sha256sum "$f"; done; [ "$found" -eq 1 ]'
