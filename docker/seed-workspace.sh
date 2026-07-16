#!/usr/bin/env bash
# Plan L L1 — seed engine Compose workspace RAG from docker/seed_rag/ (explicit only).
# Never auto-run from compose up. Never mirrors host agent_workspace/. Never touches memory.db.
#
# Usage (repo root):
#   ./docker/seed-workspace.sh
#   ./docker/seed-workspace.sh --dry-run
#   ./docker/seed-workspace.sh --no-restart
#
# Respects COMPOSE_PROJECT_NAME and the same compose files the operator uses.
# Workspace target is Compose-attached thoth-engine:/workspace (no hardcoded volume name).

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

SEED_DIR="${ROOT}/docker/seed_rag"
SERVICE="thoth-engine"
WORKSPACE_DEST="/workspace"
RAG_DEST="${WORKSPACE_DEST}/rag"

# Locked whitelist (Plan L). Extra files under SEED_DIR are ignored.
WHITELIST=(GRAG.md HOWTO.md AGENTS.md cognate.md)

DRY_RUN=0
NO_RESTART=0

log() { printf '[seed-workspace] %s\n' "$*"; }
fail() { printf '[seed-workspace] FAIL: %s\n' "$*" >&2; exit 1; }

usage() {
  cat <<'EOF'
Usage: ./docker/seed-workspace.sh [--dry-run] [--no-restart] [-h|--help]

  Seeds only the locked Plan L whitelist into thoth-engine:/workspace/rag,
  removes rag_index.bin so the engine can rebuild, and restarts thoth-engine
  unless --no-restart.

  --dry-run      Validate whitelist and print actions; no volume writes
  --no-restart   Seed + clear index only; do not restart the engine
  -h, --help     Show this help

Compose project: uses docker compose from the repo root (honors COMPOSE_PROJECT_NAME).
EOF
}

while [ $# -gt 0 ]; do
  case "$1" in
    --dry-run) DRY_RUN=1 ;;
    --no-restart) NO_RESTART=1 ;;
    -h|--help) usage; exit 0 ;;
    *) fail "unknown argument: $1 (try --help)" ;;
  esac
  shift
done

compose() {
  docker compose "$@"
}

require_whitelist() {
  [ -d "$SEED_DIR" ] || fail "missing seed directory: $SEED_DIR"
  local missing=0
  local f
  for f in "${WHITELIST[@]}"; do
    if [ ! -f "${SEED_DIR}/${f}" ]; then
      printf '[seed-workspace] missing whitelist file: %s/%s\n' "$SEED_DIR" "$f" >&2
      missing=1
    fi
  done
  if [ "$missing" -ne 0 ]; then
    fail "whitelist incomplete under docker/seed_rag/ (see Plan L)"
  fi
}

list_ignored_extras() {
  local f base
  # shellcheck disable=SC2045
  for f in "$SEED_DIR"/*; do
    [ -e "$f" ] || continue
    base="$(basename "$f")"
    case "$base" in
      README.md) continue ;;
    esac
    local listed=0
    local w
    for w in "${WHITELIST[@]}"; do
      if [ "$base" = "$w" ]; then
        listed=1
        break
      fi
    done
    if [ "$listed" -eq 0 ]; then
      log "ignoring non-whitelist path: $base"
    fi
  done
}

# Diagnostic only — never used as a hardcoded seed target.
log_workspace_volume_if_possible() {
  local cid mount_name
  cid="$(compose ps -q "$SERVICE" 2>/dev/null || true)"
  if [ -n "$cid" ]; then
    mount_name="$(docker inspect "$cid" --format '{{range .Mounts}}{{if eq .Destination "/workspace"}}{{.Name}}{{end}}{{end}}' 2>/dev/null || true)"
    if [ -n "$mount_name" ]; then
      log "diagnostics: $SERVICE container has /workspace volume name=${mount_name}"
      return 0
    fi
  fi
  log "diagnostics: $SERVICE not running — /workspace volume will be whatever Compose attaches on run"
}

build_seed_shell() {
  # Remote shell: copy only whitelist; clear index; chown for runtime user; never touch memory.db
  local parts=()
  local f
  parts+=('set -e')
  parts+=('mkdir -p /workspace/rag')
  for f in "${WHITELIST[@]}"; do
    parts+=("cp \"/seed/${f}\" \"/workspace/rag/${f}\"")
  done
  parts+=('rm -f /workspace/rag/rag_index.bin')
  parts+=('if command -v chown >/dev/null 2>&1; then chown -R thoth:thoth /workspace/rag 2>/dev/null || true; fi')
  parts+=('echo "[seed-workspace] /workspace/rag after seed:"')
  parts+=('ls -la /workspace/rag')
  parts+=('if [ -e /workspace/memory.db ]; then echo "[seed-workspace] memory.db present (untouched)"; fi')
  local IFS=';'
  printf '%s' "${parts[*]}"
}

require_whitelist
list_ignored_extras

PROJECT_NAME="${COMPOSE_PROJECT_NAME:-}"
if [ -z "$PROJECT_NAME" ]; then
  PROJECT_NAME="$(compose config 2>/dev/null | awk '/^name:/{print $2; exit}' || true)"
fi
PROJECT_NAME="${PROJECT_NAME:-thoth}"
log "compose project=${PROJECT_NAME} (COMPOSE_PROJECT_NAME=${COMPOSE_PROJECT_NAME:-<unset>})"
log "seed source=${SEED_DIR}"
log "whitelist=${WHITELIST[*]}"
log_workspace_volume_if_possible

if [ "$DRY_RUN" -eq 1 ]; then
  log "DRY-RUN: would copy whitelist → ${SERVICE}:${RAG_DEST}"
  log "DRY-RUN: would remove ${RAG_DEST}/rag_index.bin"
  if [ "$NO_RESTART" -eq 1 ]; then
    log "DRY-RUN: would skip restart (--no-restart)"
  else
    log "DRY-RUN: would: docker compose restart ${SERVICE}"
  fi
  log "DRY-RUN: OK"
  exit 0
fi

command -v docker >/dev/null 2>&1 || fail "docker not found"
compose version >/dev/null 2>&1 || fail "docker compose not available"

SEED_SHELL="$(build_seed_shell)"
log "seeding via compose run (Compose-managed ${SERVICE}:${WORKSPACE_DEST})"
compose run --rm --no-deps \
  -v "${SEED_DIR}:/seed:ro" \
  --entrypoint sh \
  "$SERVICE" \
  -c "$SEED_SHELL"

if [ "$NO_RESTART" -eq 1 ]; then
  log "seed complete; skipped restart (--no-restart)"
  log "restart when ready: docker compose restart ${SERVICE}"
else
  log "restarting ${SERVICE}"
  # restart is a no-op message if not running; try up -d for first-time
  if [ -n "$(compose ps -q "$SERVICE" 2>/dev/null || true)" ]; then
    compose restart "$SERVICE"
  else
    log "${SERVICE} not running; starting with compose up -d --no-deps ${SERVICE}"
    compose up -d --no-deps "$SERVICE" || log "note: up failed (e.g. image/deps) — seed files are on the volume; start the stack when ready"
  fi
fi

log "verify: docker compose exec ${SERVICE} ls -la ${RAG_DEST}"
log "after start, first request may rebuild rag_index.bin (embedding can be slow)"
log "done"
