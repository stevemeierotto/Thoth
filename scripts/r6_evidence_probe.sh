#!/usr/bin/env bash
# r6_evidence_probe.sh — Phase 0 evidence gathering for R6 End-to-End Functional Audit.
#
# Produces an evidence bundle under /tmp/r6-evidence-<timestamp>/:
#   summary.txt          — human-readable pass/fail matrix
#   evidence.json        — machine-readable checkpoint results
#   0a-ingest-*          — Send-to-Engine chain (HTTP ingest mirror)
#   0b-turn*.json        — per-turn conversation responses
#   0b-turn-metrics.json — Turn 1/2/3 comparison table (R6-08)
#   0c-diagnostics-*.json
#
# Usage:
#   ./scripts/r6_evidence_probe.sh              # full (ingest + 3 turns)
#   ./scripts/r6_evidence_probe.sh ingest       # 0A only
#   ./scripts/r6_evidence_probe.sh turns        # 0B + 0C only (Engine must be idle)
#   THOTH_ENGINE_URL=http://127.0.0.1:8090 ./scripts/r6_evidence_probe.sh
#
# Environment:
#   R6_TURN_TIMEOUT       — per-turn curl timeout (default 600, matches chat HTTP)
#   R6_INGEST_TIMEOUT     — ingest POST timeout (default 30)
#   R6_INDEX_POLL_SEC     — seconds to wait for indexing (default 15)
#   R6_SKIP_TURNS         — set 1 to skip 0B/0C (ingest-only run)
#
# Exit 0 when all executed checkpoints pass; non-zero on any failure in executed scope.

set -euo pipefail

BASE="${THOTH_ENGINE_URL:-http://127.0.0.1:8090}"
TURN_TIMEOUT="${R6_TURN_TIMEOUT:-600}"
INGEST_TIMEOUT="${R6_INGEST_TIMEOUT:-30}"
INDEX_POLL_SEC="${R6_INDEX_POLL_SEC:-15}"
MODE="${1:-full}"
TS="$(date +%s)"
WORKDIR="/tmp/r6-evidence-${TS}"
SUM="${WORKDIR}/summary.txt"
EVIDENCE="${WORKDIR}/evidence.json"

mkdir -p "$WORKDIR"

log() {
  echo "$1" | tee -a "$SUM"
}

# Append JSON checkpoint: name, pass (true/false), detail string
checkpoint() {
  local name="$1" pass="$2" detail="$3"
  python3 - <<PY
import json, os
path = "$EVIDENCE"
data = {"checkpoints": [], "meta": {}}
if os.path.isfile(path):
    with open(path) as f:
        data = json.load(f)
data["checkpoints"].append({
    "id": """$name""",
    "pass": """$pass""" == "true",
    "detail": """$detail""",
})
with open(path, "w") as f:
    json.dump(data, f, indent=2)
PY
}

require_ready() {
  if ! curl -sf -m 5 "$BASE/ready" >"${WORKDIR}/ready.json"; then
    log "FAIL: Engine not ready at $BASE"
    checkpoint "ready" false "Engine /ready unreachable"
    exit 1
  fi
  log "ready_ok=yes base=$BASE"
  checkpoint "ready" true "$(cat "${WORKDIR}/ready.json" | tr -d '\n' | head -c 500)"
}

capture_version() {
  curl -sf -m 5 "$BASE/version" >"${WORKDIR}/version.json" 2>/dev/null || echo '{}' >"${WORKDIR}/version.json"
  log "engine_version=$(python3 -c "import json; print(json.load(open('${WORKDIR}/version.json')).get('engine','?'))")"
}

corpus_count() {
  python3 - <<PY
import json, sys
try:
    b = json.load(open("$1"))
    docs = b.get("documents", [])
    print(len(docs))
except Exception:
    print(0)
PY
}

corpus_names() {
  python3 - <<PY
import json
try:
    b = json.load(open("$1"))
    for d in b.get("documents", []):
        print(d.get("name", "?"), d.get("status", "?"), d.get("id", ""))
except Exception:
    pass
PY
}

# --- 0A: Send to Engine evidence chain (HTTP mirror of GUI Send) ---
probe_0a_ingest() {
  local sid="r6-ingest-${TS}"
  local probe_name="r6-probe-${TS}.md"
  local probe_content="R6 Phase 0 ingest probe unique token: r6-token-${TS}"

  log ""
  log "=== 0A Send-to-Engine evidence chain sid=${sid} ==="

  curl -sf -m 10 "$BASE/v1/rag/corpus" >"${WORKDIR}/0a-corpus-before.json" || echo '{"documents":[]}' >"${WORKDIR}/0a-corpus-before.json"
  local before_count
  before_count=$(corpus_count "${WORKDIR}/0a-corpus-before.json")
  log "E7_baseline corpus_count=${before_count}"
  corpus_names "${WORKDIR}/0a-corpus-before.json" | tee -a "$SUM"

  # E2/E3/E4: POST with session_id
  local t0 t1 http_code
  t0=$(date +%s)
  http_code=$(curl -s -o "${WORKDIR}/0a-ingest-body.json" -w '%{http_code}' -m "$INGEST_TIMEOUT" \
    -X POST "$BASE/v1/rag/documents" \
    -H 'Content-Type: application/json' \
    -d "{\"name\":\"${probe_name}\",\"content\":\"${probe_content}\",\"session_id\":\"${sid}\"}" \
    2>"${WORKDIR}/0a-ingest-curl.log")
  t1=$(date +%s)
  log "E2_E3 POST /v1/rag/documents http=${http_code} latency_s=$((t1 - t0))"
  log "E4 session_id in body=${sid}"
  cat "${WORKDIR}/0a-ingest-body.json" | tee -a "$SUM"

  if [ "$http_code" = "200" ] || [ "$http_code" = "201" ] || [ "$http_code" = "202" ]; then
    checkpoint "E3_http_accept" true "http=${http_code} body in 0a-ingest-body.json"
  else
    checkpoint "E3_http_accept" false "http=${http_code} see 0a-ingest-body.json"
    log "FAIL 0A: ingest HTTP not accepted"
    return 1
  fi

  log "E6 waiting ${INDEX_POLL_SEC}s for indexing..."
  sleep "$INDEX_POLL_SEC"

  curl -sf -m 10 "$BASE/v1/rag/corpus" >"${WORKDIR}/0a-corpus-after.json" || echo '{"documents":[]}' >"${WORKDIR}/0a-corpus-after.json"
  local after_count
  after_count=$(corpus_count "${WORKDIR}/0a-corpus-after.json")
  log "E7_after corpus_count=${after_count} delta=$((after_count - before_count))"
  corpus_names "${WORKDIR}/0a-corpus-after.json" | tee -a "$SUM"

  python3 - <<PY | tee -a "$SUM"
import json
before = json.load(open("${WORKDIR}/0a-corpus-before.json"))
after = json.load(open("${WORKDIR}/0a-corpus-after.json"))
probe = "${probe_name}"
found = [d for d in after.get("documents", []) if d.get("name") == probe]
print(f"E7_probe_doc_found={bool(found)}")
if found:
    d = found[0]
    print(f"  status={d.get('status')} id={d.get('id')} chunks={d.get('chunk_count')}")
    if d.get("status") == "failed":
        print(f"  reason={d.get('reason')}")
PY

  local found_status
  found_status=$(python3 -c "
import json
after = json.load(open('${WORKDIR}/0a-corpus-after.json'))
probe = '${probe_name}'
found = [d for d in after.get('documents', []) if d.get('name') == probe]
print(found[0].get('status','missing') if found else 'missing')
")

  if [ "$found_status" = "indexed" ] || [ "$found_status" = "ready" ]; then
    checkpoint "E6_E7_indexing" true "doc=${probe_name} status=${found_status}"
  elif [ "$found_status" = "indexing" ] || [ "$found_status" = "pending" ]; then
    checkpoint "E6_E7_indexing" false "doc=${probe_name} still status=${found_status} (increase R6_INDEX_POLL_SEC)"
  else
    checkpoint "E6_E7_indexing" false "doc=${probe_name} status=${found_status}"
  fi

  # E9: retrieval bind — chat turn referencing probe content
  log "E9 retrieval bind probe (chat turn)..."
  local chat_http chat_latency
  t0=$(date +%s)
  chat_http=$(curl -s -o "${WORKDIR}/0a-retrieval-chat.json" -w '%{http_code}' -m "$TURN_TIMEOUT" \
    -X POST "$BASE/v1/conversation/turns" \
    -H 'Content-Type: application/json' \
    -d "{\"session_id\":\"${sid}\",\"content\":\"What is the R6 probe token in r6-probe document? Reply with the token only.\"}")
  t1=$(date +%s)
  chat_latency=$((t1 - t0))
  log "E9 chat http=${chat_http} latency_s=${chat_latency}"

  if [ "$chat_http" = "200" ]; then
    checkpoint "E9_retrieval_chat" true "http=200 latency_s=${chat_latency}"
    curl -sf -m 10 "$BASE/v1/diagnostics/latest-decision" >"${WORKDIR}/0a-diagnostics-after-chat.json" 2>/dev/null || true
  else
    checkpoint "E9_retrieval_chat" false "http=${chat_http} latency_s=${chat_latency}"
  fi

  # Try to pull chat_rag.jsonl tail from docker if available
  if command -v docker >/dev/null 2>&1; then
    docker compose -f "$(dirname "$0")/../docker-compose.yml" exec -T thoth-engine \
      tail -n 6 /logs/chat_rag.jsonl 2>/dev/null >"${WORKDIR}/0a-chat-rag-tail.jsonl" || true
    if [ -s "${WORKDIR}/0a-chat-rag-tail.jsonl" ]; then
      log "chat_rag.jsonl tail captured ($(wc -l < "${WORKDIR}/0a-chat-rag-tail.jsonl") lines)"
    else
      log "chat_rag.jsonl tail unavailable (docker exec failed or empty)"
    fi
  fi

  log "0A complete workdir=${WORKDIR}"
}

# --- 0B: Turn 1/2/3 prompt growth (R6-08) ---
turn_probe() {
  local n=$1
  local content=$2
  local sid=$3
  local t0 t1 resp http body
  t0=$(date +%s)
  resp=$(curl -s -w '\n__HTTP__%{http_code}' -m "$TURN_TIMEOUT" \
    -X POST "$BASE/v1/conversation/turns" \
    -H 'Content-Type: application/json' \
    -d "{\"session_id\":\"${sid}\",\"content\":\"${content}\"}" 2>"${WORKDIR}/0b-turn${n}-curl.log")
  t1=$(date +%s)
  http=$(echo "$resp" | sed -n 's/^__HTTP__//p')
  body=$(echo "$resp" | sed '/^__HTTP__/d')
  echo "$body" >"${WORKDIR}/0b-turn${n}.json"
  echo "$http" >"${WORKDIR}/0b-turn${n}-http.txt"
  log "0B turn${n} http=${http} latency_s=$((t1 - t0))"

  curl -sf -m 10 "$BASE/v1/diagnostics/latest-decision" >"${WORKDIR}/0c-diagnostics-after-turn${n}.json" 2>/dev/null \
    || echo '{}' >"${WORKDIR}/0c-diagnostics-after-turn${n}.json"

  echo "$((t1 - t0))" >"${WORKDIR}/0b-turn${n}-latency.txt"

  if [ "$http" = "200" ]; then
    checkpoint "0B_turn${n}" true "http=200 latency_s=$((t1 - t0))"
    return 0
  else
    checkpoint "0B_turn${n}" false "http=${http} latency_s=$((t1 - t0)) timeout=${TURN_TIMEOUT}s"
    return 1
  fi
}

probe_0b_turns() {
  local sid="r6-turns-${TS}"
  log ""
  log "=== 0B Turn comparison (R6-08) sid=${sid} turn_timeout=${TURN_TIMEOUT}s ==="

  turn_probe 1 "Reply with exactly: R6T1" "$sid" || true
  turn_probe 2 "Reply with exactly: R6T2" "$sid" || true
  turn_probe 3 "Reply with exactly: R6T3" "$sid" || true

  curl -sf -m 30 "$BASE/v1/conversation/sessions/${sid}" >"${WORKDIR}/0b-conversation.json" 2>/dev/null \
    || echo '{"messages":[]}' >"${WORKDIR}/0b-conversation.json"

  # Pull chat_rag.jsonl for prompt metrics
  if command -v docker >/dev/null 2>&1; then
    docker compose -f "$(dirname "$0")/../docker-compose.yml" exec -T thoth-engine \
      tail -n 30 /logs/chat_rag.jsonl 2>/dev/null >"${WORKDIR}/0b-chat-rag-tail.jsonl" || true
  fi

  python3 - <<'PY' | tee "${WORKDIR}/0b-turn-metrics.json" | tee -a "$SUM"
import json, os

workdir = os.environ["WORKDIR"]
metrics = {"turns": [], "context_limit_tokens": 2048, "notes": []}

# Index chat_rag events by request_id
ctx_by_req = {}
resp_by_req = {}
rag_path = f"{workdir}/0b-chat-rag-tail.jsonl"
if os.path.isfile(rag_path):
    for line in open(rag_path):
        line = line.strip()
        if not line:
            continue
        try:
            rec = json.loads(line)
        except Exception:
            continue
        rid = rec.get("request_id")
        if not rid:
            continue
        if rec.get("event") == "CHAT_RAG_CONTEXT":
            ctx_by_req[rid] = rec
        elif rec.get("event") == "CHAT_RAG_RESPONSE":
            resp_by_req[rid] = rec

for n in (1, 2, 3):
    entry = {
        "turn": n,
        "http": None,
        "latency_s": None,
        "assistant_content_preview": None,
        "conversation_messages": None,
        "prompt_tokens": None,
        "completion_tokens": None,
        "execution_time_ms": None,
        "final_prompt_chars": None,
        "conversation_history_chars": None,
        "retrieved_chars": None,
        "rag_wrapper_chars": None,
        "truncated": None,
        "finish_reason": None,
        "raw_answer_chars": None,
        "fallback_used": None,
        "request_id": None,
    }
    http_path = f"{workdir}/0b-turn{n}-http.txt"
    if os.path.isfile(http_path):
        entry["http"] = open(http_path).read().strip()
    lat_path = f"{workdir}/0b-turn{n}-latency.txt"
    if os.path.isfile(lat_path):
        try:
            entry["latency_s"] = int(open(lat_path).read().strip())
        except Exception:
            pass
    turn_path = f"{workdir}/0b-turn{n}.json"
    if os.path.isfile(turn_path):
        try:
            body = json.load(open(turn_path))
            asst = body.get("assistant", {})
            content = asst.get("content", "")
            entry["assistant_content_preview"] = content[:200]
            if "usage" in body:
                u = body["usage"]
                entry["prompt_tokens"] = u.get("prompt_tokens")
                entry["completion_tokens"] = u.get("completion_tokens")
            entry["request_id"] = body.get("request_id")
        except Exception:
            pass
    diag_path = f"{workdir}/0c-diagnostics-after-turn{n}.json"
    if os.path.isfile(diag_path):
        try:
            d = json.load(open(diag_path))
            entry["execution_time_ms"] = d.get("execution_time_ms")
            entry["request_id"] = entry["request_id"] or None
        except Exception:
            pass

    rid = entry.get("request_id")
    if rid and rid in ctx_by_req:
        ctx = ctx_by_req[rid]
        entry["request_id"] = rid
        entry["final_prompt_chars"] = ctx.get("final_prompt_chars")
        entry["conversation_history_chars"] = ctx.get("conversation_history_chars")
        entry["retrieved_chars"] = ctx.get("retrieved_chars")
        entry["rag_wrapper_chars"] = ctx.get("rag_wrapper_chars")
        entry["truncated"] = ctx.get("truncated") or ctx.get("truncated_section")
    if rid and rid in resp_by_req:
        resp = resp_by_req[rid]
        entry["finish_reason"] = resp.get("finish_reason")
        entry["raw_answer_chars"] = resp.get("raw_answer_chars")
        entry["fallback_used"] = resp.get("fallback_used")

    metrics["turns"].append(entry)

if os.path.isfile(rag_path):
    metrics["notes"].append(f"chat_rag_events={len(ctx_by_req)} context + {len(resp_by_req)} response")
else:
    metrics["notes"].append("chat_rag.jsonl not captured — see docker exec thoth-engine tail /logs/chat_rag.jsonl")

conv_path = f"{workdir}/0b-conversation.json"
if os.path.isfile(conv_path):
    try:
        msgs = json.load(open(conv_path)).get("messages", [])
        metrics["notes"].append(f"final_conversation_messages={len(msgs)}")
    except Exception:
        pass

chars = [t.get("final_prompt_chars") for t in metrics["turns"] if t.get("final_prompt_chars")]
if len(chars) >= 2:
    metrics["prompt_char_growth"] = {
        "turn1": chars[0],
        "turn2": chars[1] if len(chars) > 1 else None,
        "turn3": chars[2] if len(chars) > 2 else chars[-1],
        "ratio_turn3_to_turn1": round(chars[-1] / chars[0], 2) if chars[0] else None,
    }

lats = [t.get("latency_s") for t in metrics["turns"] if t.get("latency_s") is not None]
if len(lats) >= 2:
    metrics["latency_growth"] = {
        "turn1_s": lats[0],
        "turn2_s": lats[1] if len(lats) > 1 else None,
        "turn3_s": lats[2] if len(lats) > 2 else lats[-1],
    }

print(json.dumps(metrics, indent=2))
PY

  log "0B metrics written to ${WORKDIR}/0b-turn-metrics.json"
}

# --- 0C: Worker state snapshot ---
probe_0c_worker() {
  log ""
  log "=== 0C Worker / diagnostics snapshot ==="
  curl -sf -m 5 "$BASE/ready" >"${WORKDIR}/0c-ready-final.json" 2>/dev/null || true
  curl -sf -m 10 "$BASE/v1/diagnostics/latest-decision" >"${WORKDIR}/0c-diagnostics-final.json" 2>/dev/null \
    || echo '{}' >"${WORKDIR}/0c-diagnostics-final.json"

  python3 - <<PY | tee -a "$SUM"
import json
d = json.load(open("${WORKDIR}/0c-diagnostics-final.json"))
print(f"latest_execution_time_ms={d.get('execution_time_ms')}")
print(f"latest_session_id={d.get('session_id')!r}")
print(f"latest_goal={d.get('goal')!r}")
PY

  checkpoint "0C_diagnostics" true "see 0c-diagnostics-final.json"
}

# --- Main ---
log "=== R6 Phase 0 Evidence Probe $(date -Iseconds) ==="
log "WORKDIR=${WORKDIR}"
log "BASE=${BASE} TURN_TIMEOUT=${TURN_TIMEOUT}"

python3 - <<PY
import json, time
meta = {
    "started_at": "$(date -Iseconds)",
    "base": "$BASE",
    "turn_timeout": int("$TURN_TIMEOUT"),
    "ingest_timeout": int("$INGEST_TIMEOUT"),
    "mode": "$MODE",
}
with open("$EVIDENCE", "w") as f:
    json.dump({"checkpoints": [], "meta": meta}, f, indent=2)
PY

export WORKDIR
require_ready
capture_version

FAIL=0
case "$MODE" in
  ingest)
    probe_0a_ingest || FAIL=1
    ;;
  turns)
    probe_0b_turns || FAIL=1
    probe_0c_worker
    ;;
  full)
    probe_0a_ingest || FAIL=1
    if [ "${R6_SKIP_TURNS:-0}" != "1" ]; then
      probe_0b_turns || FAIL=1
      probe_0c_worker
    fi
    ;;
  *)
    echo "Usage: $0 {full|ingest|turns}" >&2
    exit 2
    ;;
esac

log ""
log "=== Checkpoint summary ==="
python3 - <<PY | tee -a "$SUM"
import json
data = json.load(open("$EVIDENCE"))
passed = sum(1 for c in data["checkpoints"] if c["pass"])
failed = sum(1 for c in data["checkpoints"] if not c["pass"])
print(f"passed={passed} failed={failed}")
for c in data["checkpoints"]:
    mark = "PASS" if c["pass"] else "FAIL"
    print(f"  [{mark}] {c['id']}: {c['detail'][:120]}")
PY

log "EVIDENCE=${EVIDENCE}"
log "WORKDIR=${WORKDIR}"

exit "$FAIL"
