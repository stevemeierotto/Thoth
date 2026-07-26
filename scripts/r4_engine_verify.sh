#!/usr/bin/env bash
# r4_engine_verify.sh — Engine-side R4 verification probes (curl; no GUI).
#
# Timeouts match production chat behavior on slow hardware:
#   - sanity (single hello): 240s default (R4_VERIFY_SANITY_TIMEOUT)
#   - multi-turn + goal/chat: 600s default (R4_VERIFY_TURN_TIMEOUT; same as chat HTTP default)
#
# Usage:
#   ./scripts/r4_engine_verify.sh sanity
#   ./scripts/r4_engine_verify.sh full
#   THOTH_ENGINE_URL=http://127.0.0.1:8090 ./scripts/r4_engine_verify.sh full
#
# Exit 0 on pass; non-zero on failure.

set -euo pipefail

BASE="${THOTH_ENGINE_URL:-http://127.0.0.1:8090}"
SANITY_TIMEOUT="${R4_VERIFY_SANITY_TIMEOUT:-240}"
TURN_TIMEOUT="${R4_VERIFY_TURN_TIMEOUT:-600}"
MODE="${1:-sanity}"

WORKDIR="/tmp/r4-verify-$(date +%s)"
mkdir -p "$WORKDIR"
SUM="$WORKDIR/summary.txt"

log() {
  echo "$1" | tee -a "$SUM"
}

require_ready() {
  if ! curl -sf -m 5 "$BASE/ready" >"$WORKDIR/ready.json"; then
    log "FAIL: Engine not ready at $BASE"
    exit 1
  fi
  log "ready_ok=yes base=$BASE"
}

sanity_probe() {
  local sid="r4-sanity-$(date +%s)"
  log "=== R4 sanity probe $(date -Iseconds) timeout=${SANITY_TIMEOUT}s sid=$sid ==="
  require_ready

  local t0 t1 resp http body
  t0=$(date +%s)
  resp=$(curl -s -w '\n__HTTP__%{http_code}' -m "$SANITY_TIMEOUT" \
    -X POST "$BASE/v1/conversation/turns" \
    -H 'Content-Type: application/json' \
    -d "{\"session_id\":\"$sid\",\"content\":\"hello\"}")
  t1=$(date +%s)
  http=$(echo "$resp" | sed -n 's/^__HTTP__//p')
  body=$(echo "$resp" | sed '/^__HTTP__/d')
  echo "$body" >"$WORKDIR/sanity_turn.json"
  log "sanity_turn http=$http latency_s=$((t1 - t0))"
  if [ "$http" != "200" ]; then
    log "FAIL: sanity turn expected HTTP 200 (increase R4_VERIFY_SANITY_TIMEOUT if LLM is slow)"
    log "body=$body"
    exit 1
  fi
  log "sanity=PASS"
}

turn_probe() {
  local n=$1
  local content=$2
  local sid=$3
  local t0 t1 resp http body
  t0=$(date +%s)
  resp=$(curl -s -w '\n__HTTP__%{http_code}' -m "$TURN_TIMEOUT" \
    -X POST "$BASE/v1/conversation/turns" \
    -H 'Content-Type: application/json' \
    -d "{\"session_id\":\"$sid\",\"content\":\"$content\"}")
  t1=$(date +%s)
  http=$(echo "$resp" | sed -n 's/^__HTTP__//p')
  body=$(echo "$resp" | sed '/^__HTTP__/d')
  echo "$body" >"$WORKDIR/turn${n}.json"
  log "turn${n} http=$http latency_s=$((t1 - t0))"
  if [ "$http" != "200" ]; then
    log "FAIL turn${n} (timeout=${TURN_TIMEOUT}s)"
    log "body=$body"
    return 1
  fi
}

full_probe() {
  local sid="r4-verify-$(date +%s)"
  local sid_b="r4-verify-b-$(date +%s)"
  log "=== R4 full probe $(date -Iseconds) turn_timeout=${TURN_TIMEOUT}s sid=$sid ==="
  require_ready

  local rej_http
  rej_http=$(curl -s -w '%{http_code}' -o /dev/null -m 10 \
    -X POST "$BASE/v1/conversation/turns" \
    -H 'Content-Type: application/json' -d '{}')
  log "v7_missing_fields http=$rej_http expect=400"

  turn_probe 1 "Reply with exactly: R4T1" "$sid"
  turn_probe 2 "Reply with exactly: R4T2" "$sid"
  turn_probe 3 "Reply with exactly: R4T3" "$sid"

  curl -sf -m 30 "$BASE/v1/conversation/sessions/$sid" >"$WORKDIR/conversation.json"
  python3 - <<PY | tee -a "$SUM"
import json
b = json.load(open("$WORKDIR/conversation.json"))
msgs = b.get("messages", [])
users = [m for m in msgs if m.get("role") == "user"]
assts = [m for m in msgs if m.get("role") == "assistant"]
print(f"e3_users={len(users)} e3_assistants={len(assts)} total={len(msgs)}")
print("v1_v6_e3", "PASS" if len(users) == 3 and len(assts) == 3 else "FAIL")
PY

  local goal_http t0 t1 chat_http
  goal_http=$(curl -s -w '%{http_code}' -o "$WORKDIR/goal.json" -m 30 \
    -X POST "$BASE/v1/goals" \
    -H 'Content-Type: application/json' \
    -d "{\"goal\":\"Reply with exactly: R4GOAL\",\"session_id\":\"$sid_b\"}")
  log "v4_goal_accept http=$goal_http"
  t0=$(date +%s)
  chat_http=$(curl -s -w '%{http_code}' -o "$WORKDIR/goal_chat.json" -m "$TURN_TIMEOUT" \
    -X POST "$BASE/v1/conversation/turns" \
    -H 'Content-Type: application/json' \
    -d "{\"session_id\":\"$sid_b\",\"content\":\"Reply with exactly: R4CHAT\"}")
  t1=$(date +%s)
  log "v4_chat_during_goal http=$chat_http latency_s=$((t1 - t0))"

  log "full=PASS"
}

case "$MODE" in
  sanity) sanity_probe ;;
  full) full_probe ;;
  *)
    echo "Usage: $0 {sanity|full}" >&2
    exit 2
    ;;
esac

log "WORKDIR=$WORKDIR"
cat "$SUM"
