#!/usr/bin/env bash
# TOKEN-CHAR helper. Validates or starts only Compose project thoth-char.
# Does not create C6-LIVE or EGAR-LAB state, and does not ingest a corpus.
#
# Usage:
#   ./docker/token-char.sh config
#   ./docker/token-char.sh up
#   ./docker/token-char.sh down
#   ./docker/token-char.sh ps

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

ACTION="${1:-}"
case "$ACTION" in
  config|up|down|ps) ;;
  *)
    echo "Usage: ./docker/token-char.sh <config|up|down|ps>" >&2
    exit 1
    ;;
esac

if [ "${COMPOSE_PROJECT_NAME:-}" = "thoth" ] || [ "${COMPOSE_PROJECT_NAME:-}" = "thoth-c6" ] || [ "${COMPOSE_PROJECT_NAME:-}" = "thoth-egar" ]; then
  echo "[token-char] refusing COMPOSE_PROJECT_NAME=${COMPOSE_PROJECT_NAME}" >&2
  exit 1
fi

export COMPOSE_PROJECT_NAME="thoth-char"
export THOTH_ENGINE_PUBLISH_PORT="${THOTH_ENGINE_PUBLISH_PORT:-8093}"
export THOTH_ENVIRONMENT_LABEL="TOKEN-CHAR"

FILES=(-f docker-compose.yml -f docker/compose.isolated.yml -f docker/compose.token-char.yml)

if [ "$ACTION" = "up" ]; then
  running="$(docker ps --format '{{.Names}}' | grep -E 'llama-server|llama-embed-server' || true)"
  if [ -n "$running" ]; then
    echo "[token-char] another llama.cpp server is running; stop it before up:" >&2
    echo "$running" >&2
    exit 1
  fi
fi

docker compose "${FILES[@]}" "$ACTION"
