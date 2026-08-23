#!/usr/bin/env bash
# transcript_loop_probe.sh — multi-run capture for transcript-loop investigation.
#
# Usage:
#   THOTH_LOG_CHAT_PROMPT=1 THOTH_LOG_FULL_RAW_CHAT_COMPLETION=1 ./scripts/transcript_loop_probe.sh [runs]
#
# Default 5 runs; same session + grounded doc, identical query each turn.
# Extracts CHAT_RAG_* records into WORKDIR.

set -euo pipefail

BASE="${THOTH_ENGINE_URL:-http://127.0.0.1:8090}"
RUNS="${1:-5}"
TURN_TIMEOUT="${TRANSCRIPT_PROBE_TURN_TIMEOUT:-900}"
INGEST_WAIT="${TRANSCRIPT_PROBE_INGEST_WAIT:-30}"
QUERY="Tell me about some completed improvements."
DOC="/home/steve/Thoth/docs/completed_improvements_log.md"
TS="$(date +%s)"
SID="transcript-probe-${TS}"
WORKDIR="/tmp/transcript-probe-${TS}"
COMPOSE="-f /home/steve/Thoth/docker-compose.yml"

mkdir -p "$WORKDIR"
log() { echo "$1" | tee -a "$WORKDIR/probe.log"; }

log "=== transcript loop probe runs=${RUNS} sid=${SID} ==="
log "base=${BASE} workdir=${WORKDIR}"

curl -sf -m 10 "$BASE/ready" >"$WORKDIR/ready.json"
curl -sf -m 10 -X POST "$BASE/v1/conversation/sessions" \
  -H 'Content-Type: application/json' \
  -d "{\"session_id\":\"${SID}\"}" >"$WORKDIR/session.json"

CONTENT=$(python3 -c "import json; print(json.dumps(open('${DOC}').read()[:12000]))")
INGEST_HTTP=$(curl -s -m 120 -o "$WORKDIR/ingest.json" -w '%{http_code}' \
  -X POST "$BASE/v1/rag/documents" \
  -H 'Content-Type: application/json' \
  -d "{\"name\":\"completed_improvements_log.md\",\"content\":${CONTENT},\"session_id\":\"${SID}\"}")
log "ingest_http=${INGEST_HTTP}"
sleep "$INGEST_WAIT"

for i in $(seq 1 "$RUNS"); do
  log "run ${i}/${RUNS} starting..."
  t0=$(date +%s)
  TURN_HTTP=$(curl -s -m "$TURN_TIMEOUT" -o "$WORKDIR/turn_${i}.json" -w '%{http_code}' \
    -X POST "$BASE/v1/conversation/turns" \
    -H 'Content-Type: application/json' \
    -d "{\"session_id\":\"${SID}\",\"content\":\"${QUERY}\"}")
  t1=$(date +%s)
  log "run ${i} http=${TURN_HTTP} latency_s=$((t1 - t0))"
done

docker compose $COMPOSE exec -T thoth-engine sh -c "grep '${SID}' /logs/chat_rag.jsonl" \
  >"$WORKDIR/chat_rag.jsonl" 2>&1 || true

python3 - "$WORKDIR" "$SID" <<'PY'
import json, sys, pathlib
work = pathlib.Path(sys.argv[1])
sid = sys.argv[2]
lines = (work / "chat_rag.jsonl").read_text().splitlines()
runs = []
ctx_by_req = {}
for line in lines:
    if not line.strip():
        continue
    row = json.loads(line)
    if row.get("event") == "CHAT_RAG_CONTEXT":
        ctx_by_req[row["request_id"]] = row
    elif row.get("event") == "CHAT_RAG_RESPONSE":
        req = row["request_id"]
        ctx = ctx_by_req.get(req, {})
        runs.append({"context": ctx, "response": row})

summary = work / "summary.json"
summary.write_text(json.dumps({"session_id": sid, "run_count": len(runs), "runs": runs}, indent=2))
print(f"extracted {len(runs)} run pairs -> {summary}")

# Per-run compact table
table = work / "summary_table.txt"
with table.open("w") as out:
    out.write("run\trequest_id\tattempts\tfinish\tfallback\tvalid\tfinal_chars\traw1_chars\tu1\ta1\traw2_chars\tu2\ta2\n")
    for i, r in enumerate(runs, 1):
        resp = r["response"]
        attempts = resp.get("generation_attempts", [])
        a1 = attempts[0] if len(attempts) > 0 else {}
        a2 = attempts[1] if len(attempts) > 1 else {}
        out.write(
            f"{i}\t{resp.get('request_id','')}\t{resp.get('generation_attempt_count',0)}\t"
            f"{resp.get('finish_reason','')}\t{resp.get('fallback_used')}\t"
            f"{resp.get('response_valid')}\t{resp.get('answer_chars',0)}\t"
            f"{a1.get('raw_answer_chars',0)}\t{a1.get('transcript_user_marker_count',0)}\t"
            f"{a1.get('transcript_agent_marker_count',0)}\t"
            f"{a2.get('raw_answer_chars',0)}\t{a2.get('transcript_user_marker_count',0)}\t"
            f"{a2.get('transcript_agent_marker_count',0)}\n"
        )
print(f"table -> {table}")
PY

log "done workdir=${WORKDIR}"
