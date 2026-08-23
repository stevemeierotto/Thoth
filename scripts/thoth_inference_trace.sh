#!/usr/bin/env bash
# thoth_inference_trace.sh — Forensic trace of ONE real Thoth chat turn.
#
# Diagnostic only. Does not modify Thoth source, prompts, sanitizer, or llama config.
# Uses existing THOTH_LOG_* hooks + a temporary HTTP proxy to capture wire payloads.
#
# Usage:
#   ./scripts/thoth_inference_trace.sh
#
# Environment:
#   TRACE_TURN_TIMEOUT   — client wait for turn HTTP (default 180)
#   TRACE_QUERY          — chat query (default: architectural_facts probe)
#   TRACE_DOC            — document to ingest (default: docs/architectural_facts.md)
#   THOTH_ENGINE_URL     — default http://127.0.0.1:8090

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

BASE="${THOTH_ENGINE_URL:-http://127.0.0.1:8090}"
TRACE_TURN_TIMEOUT="${TRACE_TURN_TIMEOUT:-180}"
INGEST_WAIT="${TRACE_INGEST_WAIT:-30}"
TS="$(date +%s)"
WORKDIR="${TRACE_WORKDIR:-/tmp/thoth-inference-trace-${TS}}"
PROXY_NAME="thoth-inference-trace-proxy-${TS}"
COMPOSE="docker compose -f ${ROOT}/docker-compose.yml"
NETWORK="thoth_thoth-net"

QUERY="${TRACE_QUERY:-What does architectural_facts.md say about the UI sidebar architecture?}"
DOC="${TRACE_DOC:-${ROOT}/docs/architectural_facts.md}"
SID="inference-trace-${TS}"

mkdir -p "$WORKDIR"
log() { echo "$1" | tee -a "$WORKDIR/trace.log"; }

cleanup() {
  log "cleanup: restoring thoth-engine and removing proxy"
  $COMPOSE up -d thoth-engine 2>/dev/null || true
  docker rm -f "$PROXY_NAME" 2>/dev/null || true
}
trap cleanup EXIT

log "=== Thoth inference forensic trace ==="
log "workdir=${WORKDIR} sid=${SID}"
log "query=${QUERY}"
log "turn_timeout=${TRACE_TURN_TIMEOUT}s"

# --- max_tokens note (production path has no per-request override) ---
cat >"$WORKDIR/max_tokens_note.txt" <<'NOTE'
Thoth chat path max_tokens observation (diagnostic note)
========================================================
Production Thoth sets opts.max_tokens = ChatPrompt::kChatMaxTokens (512) in
command_processor.cpp — hardcoded; no per-request API override exists.

The conversation/turns endpoint does not accept max_tokens.

Therefore this trace uses the ACTUAL Thoth production value: max_tokens=512.
A diagnostic cap of 128 is NOT available without source changes.

Client/trace HTTP timeout: TRACE_TURN_TIMEOUT (default 180s).
At ~2 tokens/sec observed on this host, a full 512-token completion may exceed
180s. If so, this is recorded as a performance/infrastructure observation.
NOTE

# --- Start temporary HTTP proxy on docker network ---
log "starting inference trace proxy container"

cat >"$WORKDIR/proxy.py" <<'PY'
import json
import os
import sys
import time
import urllib.error
import urllib.request
from datetime import datetime, timezone
from http.server import BaseHTTPRequestHandler, HTTPServer

UPSTREAM = os.environ.get("UPSTREAM", "http://llama-server:8080")
TRACE_DIR = os.environ.get("TRACE_DIR", "/trace")
os.makedirs(TRACE_DIR, exist_ok=True)

counter = 0

class ProxyHandler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, fmt, *args):
        sys.stderr.write("%s - %s\n" % (self.address_string(), fmt % args))

    def _forward(self):
        global counter
        counter += 1
        seq = counter
        length = int(self.headers.get("Content-Length", "0") or 0)
        body = self.rfile.read(length) if length else b""
        ts = datetime.now(timezone.utc).isoformat()
        req_path = self.path

        req_meta = {
            "seq": seq,
            "timestamp": ts,
            "method": self.command,
            "path": req_path,
            "endpoint": req_path,
            "upstream": UPSTREAM + req_path,
            "content_length": len(body),
        }
        with open(os.path.join(TRACE_DIR, f"attempt_{seq}_request_meta.json"), "w") as f:
            json.dump(req_meta, f, indent=2)
        with open(os.path.join(TRACE_DIR, f"attempt_{seq}_request_body.json"), "wb") as f:
            f.write(body)
        with open(os.path.join(TRACE_DIR, f"attempt_{seq}_request_payload.txt"), "wb") as f:
            f.write(body)

        # Extract prompt field for separate artifact when completions API
        try:
            payload = json.loads(body.decode("utf-8"))
            with open(os.path.join(TRACE_DIR, f"attempt_{seq}_prompt.txt"), "w") as f:
                if "prompt" in payload:
                    f.write(payload["prompt"])
                elif "messages" in payload:
                    json.dump(payload["messages"], f, indent=2)
                else:
                    f.write(json.dumps(payload, indent=2))
            with open(os.path.join(TRACE_DIR, f"attempt_{seq}_generation_params.json"), "w") as f:
                gen = {k: payload.get(k) for k in (
                    "model", "temperature", "top_p", "max_tokens", "stop", "stream"
                )}
                gen["stop_present"] = "stop" in payload
                gen["stop_value"] = payload.get("stop")
                json.dump(gen, f, indent=2)
        except Exception as e:
            with open(os.path.join(TRACE_DIR, f"attempt_{seq}_parse_error.txt"), "w") as f:
                f.write(str(e))

        t0 = time.time()
        try:
            req = urllib.request.Request(
                UPSTREAM + req_path,
                data=body,
                headers={"Content-Type": self.headers.get("Content-Type", "application/json")},
                method=self.command,
            )
            with urllib.request.urlopen(req, timeout=600) as resp:
                resp_body = resp.read()
                latency_ms = int((time.time() - t0) * 1000)
                status = resp.status
        except urllib.error.HTTPError as e:
            resp_body = e.read()
            latency_ms = int((time.time() - t0) * 1000)
            status = e.code
        except Exception as e:
            latency_ms = int((time.time() - t0) * 1000)
            err = {"error": str(e), "latency_ms": latency_ms}
            with open(os.path.join(TRACE_DIR, f"attempt_{seq}_proxy_error.json"), "w") as f:
                json.dump(err, f, indent=2)
            self.send_response(502)
            self.send_header("Content-Type", "application/json")
            self.end_headers()
            self.wfile.write(json.dumps(err).encode())
            return

        with open(os.path.join(TRACE_DIR, f"attempt_{seq}_response_meta.json"), "w") as f:
            json.dump({"seq": seq, "http_status": status, "latency_ms": latency_ms, "body_bytes": len(resp_body)}, f, indent=2)
        with open(os.path.join(TRACE_DIR, f"attempt_{seq}_response_body.json"), "wb") as f:
            f.write(resp_body)

        try:
            parsed = json.loads(resp_body.decode("utf-8"))
            choices = parsed.get("choices") or []
            if choices:
                text = choices[0].get("text") or ""
                if not text and isinstance(choices[0].get("message"), dict):
                    text = choices[0]["message"].get("content") or ""
                with open(os.path.join(TRACE_DIR, f"attempt_{seq}_raw_completion.txt"), "w") as f:
                    f.write(text)
            with open(os.path.join(TRACE_DIR, f"attempt_{seq}_finish_reason.txt"), "w") as f:
                if choices and choices[0].get("finish_reason"):
                    f.write(str(choices[0]["finish_reason"]))
            usage = parsed.get("usage") or {}
            with open(os.path.join(TRACE_DIR, f"attempt_{seq}_token_counts.json"), "w") as f:
                json.dump(usage, f, indent=2)
            if parsed.get("timings"):
                with open(os.path.join(TRACE_DIR, f"attempt_{seq}_server_timings.json"), "w") as f:
                    json.dump(parsed["timings"], f, indent=2)
        except Exception as e:
            with open(os.path.join(TRACE_DIR, f"attempt_{seq}_response_parse_error.txt"), "w") as f:
                f.write(str(e))

        self.send_response(status)
        self.send_header("Content-Type", resp.headers.get("Content-Type", "application/json"))
        self.send_header("Content-Length", str(len(resp_body)))
        self.end_headers()
        self.wfile.write(resp_body)

    def do_GET(self):
        if self.path == "/health":
            try:
                req = urllib.request.Request(UPSTREAM + "/health", method="GET")
                with urllib.request.urlopen(req, timeout=10) as resp:
                    body = resp.read()
                self.send_response(resp.status)
                self.send_header("Content-Type", "application/json")
                self.send_header("Content-Length", str(len(body)))
                self.end_headers()
                self.wfile.write(body)
            except Exception:
                self.send_response(503)
                self.end_headers()
            return
        self._forward()

    def do_POST(self):
        self._forward()

if __name__ == "__main__":
    port = int(os.environ.get("PORT", "8082"))
    HTTPServer(("0.0.0.0", port), ProxyHandler).serve_forever()
PY

docker rm -f "$PROXY_NAME" 2>/dev/null || true
docker run -d --name "$PROXY_NAME" \
  --network "$NETWORK" \
  -v "$WORKDIR:/trace" \
  -e UPSTREAM=http://llama-server:8080 \
  -e TRACE_DIR=/trace \
  -e PORT=8082 \
  python:3.12-slim \
  python /trace/proxy.py >/dev/null

sleep 2
if ! docker exec "$PROXY_NAME" python -c "import urllib.request; urllib.request.urlopen('http://127.0.0.1:8082/health', timeout=5)" 2>/dev/null; then
  log "FAIL: proxy health check failed"
  exit 1
fi
log "proxy ok name=${PROXY_NAME}"

# --- Restart engine with logging + proxy URL (temporary diagnostic env) ---
log "restarting thoth-engine with trace logging env (temporary)"

$COMPOSE stop thoth-engine
THOTH_INFERENCE_BASE_URL="http://${PROXY_NAME}:8082" \
THOTH_LOG_CHAT_PROMPT=1 \
THOTH_LOG_FULL_RAW_CHAT_COMPLETION=1 \
THOTH_LOG_RAW_CHAT_COMPLETION=1 \
  $COMPOSE up -d thoth-engine

deadline=$((SECONDS + 120))
while [ "$SECONDS" -lt "$deadline" ]; do
  if curl -sf -m 5 "$BASE/ready" >/dev/null 2>&1; then
    break
  fi
  sleep 3
done
curl -sf -m 10 "$BASE/ready" >"$WORKDIR/engine_ready.json" || {
  log "FAIL: engine not ready after restart"
  exit 1
}
log "engine ready"

# --- Reproduce one real Thoth chat turn ---
log "creating session sid=${SID}"
curl -sf -m 10 -X POST "$BASE/v1/conversation/sessions" \
  -H 'Content-Type: application/json' \
  -d "{\"session_id\":\"${SID}\"}" >"$WORKDIR/session.json"

if [ ! -f "$DOC" ]; then
  log "FAIL: doc not found: $DOC"
  exit 1
fi

log "ingesting doc $(basename "$DOC")"
CONTENT=$(python3 -c "import json; print(json.dumps(open('${DOC}').read()[:12000]))")
INGEST_HTTP=$(curl -s -m 120 -o "$WORKDIR/ingest.json" -w '%{http_code}' \
  -X POST "$BASE/v1/rag/documents" \
  -H 'Content-Type: application/json' \
  -d "{\"name\":\"$(basename "$DOC")\",\"content\":${CONTENT},\"session_id\":\"${SID}\"}")
log "ingest_http=${INGEST_HTTP}"
echo "$QUERY" >"$WORKDIR/chat_query.txt"
sleep "$INGEST_WAIT"

QUERY_JSON=$(python3 -c 'import json,sys; print(json.dumps(sys.argv[1]))' "$QUERY")

log "posting conversation turn (timeout=${TRACE_TURN_TIMEOUT}s)"
t0=$(date +%s)
TURN_HTTP=$(curl -s -m "$TRACE_TURN_TIMEOUT" -o "$WORKDIR/turn_response.json" -w '%{http_code}' \
  -X POST "$BASE/v1/conversation/turns" \
  -H 'Content-Type: application/json' \
  -d "{\"session_id\":\"${SID}\",\"content\":${QUERY_JSON}}" \
  2>"$WORKDIR/turn_curl_err.txt" || echo "000")
t1=$(date +%s)
TURN_LATENCY=$((t1 - t0))
log "turn_http=${TURN_HTTP} latency_s=${TURN_LATENCY}"

# --- Extract CHAT_RAG records ---
sleep 2
docker compose -f "$ROOT/docker-compose.yml" exec -T thoth-engine sh -c "grep '${SID}' /logs/chat_rag.jsonl" \
  >"$WORKDIR/chat_rag.jsonl" 2>/dev/null || true

# --- Build artifact bundle + TRACE_REPORT.md ---
python3 - "$WORKDIR" "$SID" "$TURN_HTTP" "$TURN_LATENCY" "$TRACE_TURN_TIMEOUT" "$QUERY" <<'PY'
import json
import re
import sys
from datetime import datetime, timezone
from pathlib import Path

work = Path(sys.argv[1])
sid = sys.argv[2]
turn_http = sys.argv[3]
turn_latency = int(sys.argv[4])
turn_timeout = int(sys.argv[5])
query = sys.argv[6]

def read(p):
    try:
        return p.read_text()
    except Exception:
        return ""

def read_json(p):
    try:
        return json.loads(p.read_text())
    except Exception:
        return {}

# CHAT_RAG extract
ctx = {}
resp = {}
for line in read(work / "chat_rag.jsonl").splitlines():
    if not line.strip():
        continue
    row = json.loads(line)
    if row.get("event") == "CHAT_RAG_CONTEXT":
        ctx = row
    elif row.get("event") == "CHAT_RAG_RESPONSE":
        resp = row

request_id = ctx.get("request_id") or resp.get("request_id") or "unknown"

# Thoth-side prompt artifact
if ctx.get("final_prompt"):
    (work / "thoth_final_prompt.txt").write_text(ctx["final_prompt"])

# Turn API final response
turn = read_json(work / "turn_response.json")
final_from_api = ""
if isinstance(turn, dict):
    if turn.get("assistant"):
        final_from_api = turn["assistant"]
    elif turn.get("content"):
        final_from_api = turn["content"]
    elif turn.get("message"):
        final_from_api = str(turn["message"])
(work / "final_response.txt").write_text(final_from_api or read(work / "turn_response.json"))

# Proxy attempts
attempt_files = sorted(work.glob("attempt_*_request_meta.json"))
attempts = []
for meta_path in attempt_files:
    seq = meta_path.name.split("_")[1]
    meta = read_json(meta_path)
    resp_meta = read_json(work / f"attempt_{seq}_response_meta.json")
    gen_params = read_json(work / f"attempt_{seq}_generation_params.json")
    tokens = read_json(work / f"attempt_{seq}_token_counts.json")
    raw_completion = read(work / f"attempt_{seq}_raw_completion.txt")
    finish = read(work / f"attempt_{seq}_finish_reason.txt").strip()
    prompt_txt = read(work / f"attempt_{seq}_prompt.txt")
    req_body = read(work / f"attempt_{seq}_request_body.json")
    resp_body = read(work / f"attempt_{seq}_response_body.json")

    if prompt_txt and not (work / f"wire_prompt_attempt_{seq}.txt").exists():
        (work / f"wire_prompt_attempt_{seq}.txt").write_text(prompt_txt)

    thoth_attempt = None
    for ta in resp.get("generation_attempts") or []:
        if ta.get("attempt") == int(seq):
            thoth_attempt = ta
            break
    if thoth_attempt is None and resp.get("generation_attempts"):
        idx = int(seq) - 1
        if 0 <= idx < len(resp["generation_attempts"]):
            thoth_attempt = resp["generation_attempts"][idx]

    sanitizer_in = thoth_attempt.get("raw_completion") if thoth_attempt else raw_completion
    sanitizer_out = thoth_attempt.get("sanitized_completion") if thoth_attempt else ""

    if sanitizer_in:
        (work / f"attempt_{seq}_sanitizer_input.txt").write_text(sanitizer_in)
    if sanitizer_out:
        (work / f"attempt_{seq}_sanitizer_output.txt").write_text(sanitizer_out)

    attempts.append({
        "seq": int(seq),
        "endpoint": meta.get("path"),
        "http_status": resp_meta.get("http_status"),
        "latency_ms": resp_meta.get("latency_ms"),
        "generation_params": gen_params,
        "prompt_chars": len(prompt_txt),
        "raw_completion_chars": len(raw_completion),
        "finish_reason": finish,
        "token_counts": tokens,
        "raw_completion": raw_completion,
        "sanitizer_input": sanitizer_in,
        "sanitizer_output": sanitizer_out,
        "thoth_attempt": thoth_attempt,
        "request_body": req_body,
        "response_body": resp_body,
    })

# Compare sanitizer for attempt 1 if present
if attempts:
    a1 = attempts[0]
    si = a1.get("sanitizer_input") or ""
    so = a1.get("sanitizer_output") or ""
    cmp_lines = []
    cmp_lines.append(f"sanitizer_input_chars={len(si)}")
    cmp_lines.append(f"sanitizer_output_chars={len(so)}")
    cmp_lines.append(f"changed={si != so}")
    cmp_lines.append(f"output_empty={not so.strip()}")
    for pat, name in [(r"\[User\]", "user_marker"), (r"\[Agent\]", "agent_marker"),
                      (r"Document:", "document"), (r"source_span=", "source_span"),
                      (r"<\|im_start\|>", "chatml_start"), (r"<\|im_end\|>", "chatml_end")]:
        cmp_lines.append(f"input_has_{name}={bool(re.search(pat, si))}")
        cmp_lines.append(f"output_has_{name}={bool(re.search(pat, so))}")
    (work / "sanitizer_comparison.txt").write_text("\n".join(cmp_lines) + "\n")

# Classification helpers
def has_loop(text):
    return text.count("[User]") >= 2 or text.count("[Agent]") >= 2

def has_repetition(text):
    lines = [ln.strip() for ln in text.splitlines() if ln.strip()]
    if len(lines) >= 2 and len(set(lines)) < len(lines) * 0.6:
        return True
    return False

# Performance / completeness
perf_timeout = turn_http in ("000", "") or int(turn_http or 0) >= 408
production_max_tokens = ctx.get("generation_max_tokens") or 512

# First incorrect stage classification
classification = "F"
classification_detail = "Evidence is insufficient"

if not attempts and not resp:
    classification = "F"
    classification_detail = "Evidence is insufficient — no proxy captures and no CHAT_RAG_RESPONSE"
elif perf_timeout and not resp:
    classification = "F"
    classification_detail = (
        "Evidence is insufficient — turn HTTP did not complete within "
        f"{turn_timeout}s and no CHAT_RAG_RESPONSE was captured"
    )
elif attempts:
    raw = attempts[0].get("raw_completion") or attempts[0].get("sanitizer_input") or ""
    si = attempts[0].get("sanitizer_input") or raw
    so = attempts[0].get("sanitizer_output") or ""
    final = final_from_api or resp.get("final_answer") or so

    raw_bad = (
        not raw.strip()
        or has_loop(raw)
        or has_repetition(raw)
        or "Document:" in raw
        or "source_span=" in raw
        or bool(re.search(r"<\|im_start\|>", raw))
    )

    pre_sanitize_change = raw != si and si
    sanitizer_worse = raw.strip() and not so.strip()
    sanitizer_changed = si != so

    if raw_bad:
        classification = "A"
        classification_detail = "Raw model output is already incorrect"
    elif pre_sanitize_change and not sanitizer_changed:
        classification = "B"
        classification_detail = "Thoth modifies otherwise-good model output before sanitization"
    elif sanitizer_worse or (sanitizer_changed and not so.strip()):
        classification = "C"
        classification_detail = "Sanitizer damages otherwise-good output"
    elif len(attempts) > 1 and attempts[0].get("raw_completion") != attempts[-1].get("raw_completion"):
        classification = "D"
        classification_detail = "Retry/fallback path introduces the failure"
    elif final and final != so and final != raw:
        classification = "E"
        classification_detail = "Final response assembly introduces the failure"
    elif not raw.strip() and perf_timeout:
        classification = "F"
        classification_detail = "Evidence is insufficient — generation likely exceeded diagnostic timeout"
    else:
        classification = "F"
        classification_detail = "Evidence is insufficient — captured data does not clearly localize failure"

# TRACE_REPORT.md
lines = []
lines.append("# Thoth Inference Forensic Trace Report")
lines.append("")
lines.append(f"Generated: {datetime.now(timezone.utc).isoformat()}")
lines.append(f"Work directory: `{work}`")
lines.append(f"Session ID: `{sid}`")
lines.append(f"Request ID: `{request_id}`")
lines.append("")
lines.append("## Diagnostic constraints")
lines.append("")
lines.append("- Real Thoth chat path only (`POST /v1/conversation/turns`)")
lines.append(f"- Production `max_tokens` on Thoth path: **{production_max_tokens}** (no per-request 128 override available)")
lines.append("- Temporary env for this run only: `THOTH_LOG_CHAT_PROMPT=1`, `THOTH_LOG_FULL_RAW_CHAT_COMPLETION=1`")
lines.append("- Temporary HTTP proxy captured wire request/response to llama.cpp")
lines.append(f"- Client turn timeout: **{turn_timeout}s** (turn HTTP={turn_http}, latency={turn_latency}s)")
lines.append("")
lines.append("## Query")
lines.append("")
lines.append("```")
lines.append(query)
lines.append("```")
lines.append("")
lines.append("## Timeline")
lines.append("")
lines.append("| Step | Detail |")
lines.append("|------|--------|")
lines.append(f"| Session | `{sid}` |")
lines.append(f"| Request ID | `{request_id}` |")
lines.append(f"| Turn HTTP | {turn_http} |")
lines.append(f"| Turn latency | {turn_latency}s |")
lines.append(f"| Inference mode (Thoth) | {ctx.get('inference_mode', 'unknown')} |")
lines.append(f"| Grounding mode | {ctx.get('grounding_mode', 'unknown')} |")
lines.append(f"| Generation max tokens (Thoth) | {production_max_tokens} |")
lines.append(f"| Stop sequence count (Thoth) | {ctx.get('chat_stop_sequence_count', 'unknown')} |")
for a in attempts:
    lines.append(f"| Attempt {a['seq']} endpoint | `{a.get('endpoint')}` HTTP {a.get('http_status')} latency {a.get('latency_ms')}ms |")
    lines.append(f"| Attempt {a['seq']} finish_reason | `{a.get('finish_reason')}` |")
    gp = a.get("generation_params") or {}
    lines.append(f"| Attempt {a['seq']} wire params | model={gp.get('model')} temp={gp.get('temperature')} top_p={gp.get('top_p')} max_tokens={gp.get('max_tokens')} stop_present={gp.get('stop_present')} |")
    tc = a.get("token_counts") or {}
    lines.append(f"| Attempt {a['seq']} tokens | prompt={tc.get('prompt_tokens')} completion={tc.get('completion_tokens')} |")
    lines.append(f"| Attempt {a['seq']} raw chars | {a.get('raw_completion_chars')} |")
    ta = a.get("thoth_attempt") or {}
    lines.append(f"| Attempt {a['seq']} retry flags | retried_without_stops={resp.get('retried_without_stops')} retry_due_to_regurgitation={resp.get('retry_due_to_regurgitation')} |")
    lines.append(f"| Attempt {a['seq']} sanitizer | changed={(a.get('sanitizer_input') or '') != (a.get('sanitizer_output') or '')} |")

lines.append("")
lines.append("## Evidence questions")
lines.append("")

raw0 = (attempts[0].get("raw_completion") if attempts else "") or ""
si0 = (attempts[0].get("sanitizer_input") if attempts else "") or raw0
so0 = (attempts[0].get("sanitizer_output") if attempts else "") or ""

qas = [
    ("Was the raw model output already bad?", "yes" if classification == "A" else ("partial/timeout" if not raw0.strip() else "see artifacts")),
    ("Did the raw model output contain [User]/[Agent] loops?", "yes" if has_loop(raw0) else "no"),
    ("Did the raw model output contain Document: or source_span=?", "yes" if ("Document:" in raw0 or "source_span=" in raw0) else "no"),
    ("Did the raw model output contain ChatML markers?", "yes" if re.search(r"<\|im_start\|>|<\|im_end\|>", raw0) else "no"),
    ("Did the raw model output repeat itself?", "yes" if has_repetition(raw0) else "no"),
    ("Did the raw model output hit max_tokens?", "yes" if (attempts and attempts[0].get("finish_reason") == "length") else "no/unknown"),
    ("Did Thoth change the output before sanitization?", "yes" if (raw0 != si0 and si0) else "no"),
    ("Did the sanitizer change good content into bad content?", "yes" if classification == "C" else "see sanitizer_comparison.txt"),
    ("Did a retry alter the behavior?", "yes" if len(attempts) > 1 else str(resp.get("retried_without_stops"))),
    ("At what exact stage does the response first become incorrect?", classification_detail),
]
for i, (q, a) in enumerate(qas, 1):
    lines.append(f"{i}. **{q}** {a}")

lines.append("")
lines.append("## Classification")
lines.append("")
lines.append(f"**{classification}.** {classification_detail}")
lines.append("")
lines.append("## Artifact index")
lines.append("")
for p in sorted(work.iterdir()):
    if p.is_file():
        lines.append(f"- `{p.name}` ({p.stat().st_size} bytes)")

(work / "TRACE_REPORT.md").write_text("\n".join(lines))

summary = {
    "request_id": request_id,
    "session_id": sid,
    "turn_http": turn_http,
    "turn_latency_s": turn_latency,
    "production_max_tokens": production_max_tokens,
    "attempt_count": len(attempts),
    "classification": classification,
    "classification_detail": classification_detail,
    "attempts": [{
        "seq": a["seq"],
        "http_status": a.get("http_status"),
        "finish_reason": a.get("finish_reason"),
        "max_tokens": (a.get("generation_params") or {}).get("max_tokens"),
        "raw_completion_chars": a.get("raw_completion_chars"),
    } for a in attempts],
}
(work / "trace_summary.json").write_text(json.dumps(summary, indent=2))
print(f"TRACE_REPORT.md -> {work / 'TRACE_REPORT.md'}")
print(f"CLASSIFICATION: {classification} — {classification_detail}")
PY

log "trace complete workdir=${WORKDIR}"
echo ""
echo "Work directory: ${WORKDIR}"
echo "Report: ${WORKDIR}/TRACE_REPORT.md"
ls -la "$WORKDIR"
