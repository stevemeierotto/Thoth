#!/usr/bin/env bash
# Start or stop one isolated experimental Compose project.
# Does not ingest a corpus, open a C6 window, run goals, or touch project "thoth".
#
# Usage:
#   ./docker/isolated-up.sh c6 config
#   ./docker/isolated-up.sh c6 up          # engine only (no second llama.cpp)
#   ./docker/isolated-up.sh egar up --with-inference
#   ./docker/isolated-up.sh c6 down
#   ./docker/isolated-up.sh egar ps
#
# --with-inference starts llama-server and llama-embed-server as well.
# This laptop should run only one inference stack at a time. The script
# refuses --with-inference while any llama-server container is already running.

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

usage() {
  cat <<'EOF'
Usage: ./docker/isolated-up.sh <c6|egar> <config|up|down|ps> [--with-inference]

  c6    Compose project thoth-c6,  Engine port 8091, label C6-LIVE
  egar  Compose project thoth-egar, Engine port 8092, label EGAR-LAB

DEV is project thoth on port 8090 and is not controlled by this script.
EOF
}

fail() { printf '[isolated-up] FAIL: %s\n' "$*" >&2; exit 1; }

TARGET="${1:-}"
ACTION="${2:-}"
WITH_INFERENCE=0
if [ "${3:-}" = "--with-inference" ]; then
  WITH_INFERENCE=1
elif [ -n "${3:-}" ]; then
  fail "unknown argument: $3"
fi

case "$TARGET" in
  c6)
    PROJECT="thoth-c6"
    PORT="8091"
    LABEL="C6-LIVE"
    EXTRA=(-f docker/compose.c6.yml)
    ;;
  egar)
    PROJECT="thoth-egar"
    PORT="8092"
    LABEL="EGAR-LAB"
    EXTRA=()
    ;;
  dev|thoth)
    fail "refusing to operate on DEV project thoth"
    ;;
  *)
    usage
    exit 1
    ;;
esac

case "$ACTION" in
  config|up|down|ps) ;;
  *)
    usage
    exit 1
    ;;
esac

if [ -n "${COMPOSE_PROJECT_NAME:-}" ] && [ "$COMPOSE_PROJECT_NAME" = "thoth" ]; then
  fail "COMPOSE_PROJECT_NAME=thoth would target DEV"
fi

RETRIEVAL="$ROOT/docker/experiment/retrieval_config.json"
[ -f "$RETRIEVAL" ] || fail "missing $RETRIEVAL"

export COMPOSE_PROJECT_NAME="$PROJECT"
export THOTH_ENGINE_PUBLISH_PORT="$PORT"
export THOTH_ENVIRONMENT_LABEL="$LABEL"

FILES=(-f docker-compose.yml -f docker/compose.isolated.yml "${EXTRA[@]}")

compose() {
  docker compose "${FILES[@]}" "$@"
}

if [ "$ACTION" = "config" ]; then
  compose config
  exit 0
fi

if [ "$ACTION" = "ps" ]; then
  compose ps
  exit 0
fi

if [ "$ACTION" = "down" ]; then
  # Keep volumes. Stopping is how an environment retains state on this machine.
  compose stop
  exit 0
fi

if [ "$WITH_INFERENCE" -eq 1 ]; then
  running="$(docker ps --format '{{.Names}}' | grep -E 'llama-server|llama-embed-server' || true)"
  if [ -n "$running" ]; then
    fail "another llama.cpp server is running; stop that environment before --with-inference:
$running"
  fi
fi

IMAGE="${THOTH_ENGINE_IMAGE:-thoth-engine:local}"
printf '[isolated-up] project=%s port=%s label=%s image=%s inference=%s\n' \
  "$PROJECT" "$PORT" "$LABEL" "$IMAGE" "$WITH_INFERENCE"
docker image inspect "$IMAGE" --format '[isolated-up] image_id={{.Id}}' \
  || fail "image not present locally: $IMAGE (do not build from this script; pin or build DEV first)"

install_retrieval_config() {
  local volume="${PROJECT}_thoth-workspace"
  docker run --rm --entrypoint sh \
    -v "${volume}:/workspace" \
    -v "${RETRIEVAL}:/seed/retrieval_config.json:ro" \
    "$IMAGE" \
    -c 'set -eu
      dest=/workspace/retrieval_config.json
      if [ -f "$dest" ] && [ ! -s "$dest" ]; then
        rm -f "$dest"
      fi
      if [ -f "$dest" ] && ! cmp -s /seed/retrieval_config.json "$dest"; then
        echo "[isolated-up] existing retrieval_config.json does not match the seed; refusing to overwrite" >&2
        exit 2
      fi
      if [ ! -f "$dest" ]; then
        cp /seed/retrieval_config.json "$dest"
      fi
      chown 1000:1000 "$dest"
      chmod 444 "$dest"
      echo "[isolated-up] retrieval_config installed"'
}

if [ "$WITH_INFERENCE" -eq 1 ]; then
  compose create --no-build
else
  compose create --no-build --no-deps thoth-engine
fi

install_retrieval_config

if [ "$WITH_INFERENCE" -eq 1 ]; then
  compose start
else
  compose start thoth-engine
fi
