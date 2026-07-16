#!/usr/bin/env bash
# Plan I operator smoke (8 steps) + Plan J CI mode (SMOKE_MODE=ci).
# Run from repository root.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

PORT="${THOTH_ENGINE_PUBLISH_PORT:-8090}"
BASE="http://127.0.0.1:${PORT}"
TIMEOUT_HEALTH="${SMOKE_HEALTH_TIMEOUT_SEC:-180}"
SKIP_INFERENCE="${SMOKE_SKIP_INFERENCE:-0}"
SMOKE_MODE="${SMOKE_MODE:-full}"

log() { printf '[smoke] %s\n' "$*"; }
fail() { printf '[smoke] FAIL: %s\n' "$*" >&2; exit 1; }

compose() {
  if [ "$SMOKE_MODE" = "ci" ]; then
    docker compose -f docker-compose.yml -f docker/compose.ci.yml "$@"
  else
    docker compose "$@"
  fi
}

wait_engine_healthy() {
  local deadline=$((SECONDS + TIMEOUT_HEALTH))
  while [ "$SECONDS" -lt "$deadline" ]; do
    local engine_status
    engine_status="$(compose ps --format '{{.Service}} {{.Health}}' | awk '$1=="thoth-engine"{print $2}')"
    if [ "$engine_status" = "healthy" ]; then
      compose ps
      return 0
    fi
    sleep 3
  done
  compose ps
  fail "timed out waiting for thoth-engine healthy (${TIMEOUT_HEALTH}s)"
}

wait_full_healthy() {
  local deadline=$((SECONDS + TIMEOUT_HEALTH))
  while [ "$SECONDS" -lt "$deadline" ]; do
    local llama_status engine_status
    llama_status="$(compose ps --format '{{.Service}} {{.Health}}' | awk '$1=="llama-server"{print $2}')"
    engine_status="$(compose ps --format '{{.Service}} {{.Health}}' | awk '$1=="thoth-engine"{print $2}')"
    if [ "$llama_status" = "healthy" ] && [ "$engine_status" = "healthy" ]; then
      compose ps
      return 0
    fi
    sleep 3
  done
  compose ps
  fail "timed out waiting for healthy services (${TIMEOUT_HEALTH}s)"
}

# --- CI mode (Plan J): no GGUF, engine-only packaging gate -----------------------------
if [ "$SMOKE_MODE" = "ci" ]; then
  log "mode=ci (Plan J packaging — no llama / no GGUF)"

  log "step 1: compose up -d --build thoth-engine (CI override, no llama)"
  compose up -d --build thoth-engine

  log "step 2: wait for thoth-engine healthy"
  wait_engine_healthy

  log "step 3: GET /health"
  curl -sf "${BASE}/health" >/dev/null

  log "step 4: GET /ready"
  ready_json="$(curl -sf "${BASE}/ready")"
  for cap in chat goals control events; do
    echo "$ready_json" | grep -q "\"${cap}\"" || fail "/ready missing capability: ${cap}"
  done

  log "step 5: POST /v1/chat (/help) [optional builtin — no inference]"
  curl -sf -X POST "${BASE}/v1/chat" \
    -H 'Content-Type: application/json' \
    -d '{"text":"/help","session_id":"ci-smoke"}' >/dev/null

  log "step 6: compose down"
  compose down

  log "PASS: CI packaging smoke completed"
  exit 0
fi

# --- Full operator smoke (Plan I) -------------------------------------------------------
log "mode=full (operator — llama + engine)"

log "step 1: docker compose up -d --build"
compose up -d --build

log "step 2: wait for healthy services"
wait_full_healthy

log "step 3: GET /health"
curl -sf "${BASE}/health" >/dev/null

log "step 4: GET /ready"
ready_json="$(curl -sf "${BASE}/ready")"
for cap in chat goals control events; do
  echo "$ready_json" | grep -q "\"${cap}\"" || fail "/ready missing capability: ${cap}"
done

log "step 5: POST /v1/chat (/help)"
curl -sf -X POST "${BASE}/v1/chat" \
  -H 'Content-Type: application/json' \
  -d '{"text":"/help","session_id":"smoke"}' >/dev/null

log "step 6: POST /v1/goals"
goals_http="$(curl -s -o /tmp/thoth_smoke_goals.json -w '%{http_code}' -X POST "${BASE}/v1/goals" \
  -H 'Content-Type: application/json' \
  -d '{"goal":"smoke test goal","session_id":"smoke"}')"
if [ "$goals_http" = "200" ]; then
  grep -q '"status"[[:space:]]*:[[:space:]]*"accepted"' /tmp/thoth_smoke_goals.json \
    || fail "/v1/goals 200 but status not accepted"
elif [ "$SKIP_INFERENCE" = "1" ]; then
  log "SKIP goals (SMOKE_SKIP_INFERENCE=1)"
else
  fail "POST /v1/goals returned HTTP ${goals_http} (set SMOKE_SKIP_INFERENCE=1 to skip when models unavailable)"
fi

log "step 7: GET /v1/events (SSE)"
if ! timeout 5 curl -Ns -H 'Accept: text/event-stream' "${BASE}/v1/events" | head -c 1 >/dev/null; then
  fail "no SSE data within 5s"
fi

log "step 8: docker compose down"
compose down

log "PASS: all smoke steps completed"
rm -f /tmp/thoth_smoke_goals.json
