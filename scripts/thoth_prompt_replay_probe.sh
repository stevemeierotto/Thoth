#!/usr/bin/env bash
# thoth_prompt_replay_probe.sh — Replay captured Thoth prompt directly to llama.cpp.
#
# Diagnostic only. Does not invoke Thoth or change production configuration.
#
# Usage:
#   ./scripts/thoth_prompt_replay_probe.sh [path/to/thoth_final_prompt.txt]
#
# Environment:
#   LLAMA_CONTAINER   — default thoth-llama-server-1
#   INFERENCE_MODEL   — default chat
#   MAX_TOKENS        — default 128 (diagnostic cap)
#   PROBE_TIMEOUT_SEC — default 180

set -euo pipefail

PROMPT_FILE="${1:-/tmp/thoth-inference-trace-1786297828/thoth_final_prompt.txt}"
LLAMA_CONTAINER="${LLAMA_CONTAINER:-thoth-llama-server-1}"
INFERENCE_MODEL="${INFERENCE_MODEL:-chat}"
TEMPERATURE="${TEMPERATURE:-0.7}"
TOP_P="${TOP_P:-1.0}"
MAX_TOKENS="${MAX_TOKENS:-128}"
PROBE_TIMEOUT_SEC="${PROBE_TIMEOUT_SEC:-180}"

TS="$(date +%s)"
WORKDIR="${WORKDIR:-/tmp/thoth-prompt-replay-${TS}}"
mkdir -p "$WORKDIR"

log() { echo "$1" | tee -a "$WORKDIR/replay.log"; }

if [ ! -f "$PROMPT_FILE" ]; then
  echo "FAIL: prompt file not found: $PROMPT_FILE" >&2
  exit 1
fi

cp "$PROMPT_FILE" "$WORKDIR/prompt_input.txt"

log "=== Thoth prompt replay (direct llama.cpp) ==="
log "workdir=${WORKDIR}"
log "prompt_file=${PROMPT_FILE} chars=$(wc -c <"$PROMPT_FILE")"
log "container=${LLAMA_CONTAINER} model=${INFERENCE_MODEL}"
log "endpoint=POST /v1/completions max_tokens=${MAX_TOKENS} temperature=${TEMPERATURE} top_p=${TOP_P}"

# --- Verify same container/model Thoth uses ---
{
  echo "container=$(docker inspect "$LLAMA_CONTAINER" --format '{{.Name}}')"
  echo "image=$(docker inspect "$LLAMA_CONTAINER" --format '{{.Config.Image}}')"
  echo "cmd=$(docker inspect "$LLAMA_CONTAINER" --format '{{json .Config.Cmd}}')"
  docker exec "$LLAMA_CONTAINER" ls -la /models/chat.gguf
  docker exec "$LLAMA_CONTAINER" curl -sf http://127.0.0.1:8080/v1/models
} >"$WORKDIR/llama_verify.txt" 2>&1

log "verification saved to llama_verify.txt"

# --- Build payload ---
python3 - "$INFERENCE_MODEL" "$PROMPT_FILE" "$MAX_TOKENS" "$TEMPERATURE" "$TOP_P" "$WORKDIR" <<'PY' >"$WORKDIR/request_payload_compact.json"
import json, sys
from pathlib import Path

model, prompt_path, max_tokens, temperature, top_p, work = sys.argv[1:7]
work = Path(work)
prompt = Path(prompt_path).read_text()
payload = {
    "model": model,
    "prompt": prompt,
    "max_tokens": int(max_tokens),
    "temperature": float(temperature),
    "top_p": float(top_p),
    "stream": False,
}
(work / "request_payload.json").write_text(json.dumps(payload, indent=2))
(work / "request_notes.txt").write_text(
    "Diagnostic replay of captured Thoth final_prompt.\n"
    f"Production Thoth uses max_tokens=512; this diagnostic uses max_tokens={max_tokens} only.\n"
    "No stop field sent (matches Thoth completions path).\n"
)
print(json.dumps(payload))
PY

# --- Send request ---
log "sending request (timeout=${PROBE_TIMEOUT_SEC}s)..."
t0=$(date +%s%3N 2>/dev/null || python3 -c 'import time; print(int(time.time()*1000))')

response=$(docker exec -i "$LLAMA_CONTAINER" curl -s -m "$PROBE_TIMEOUT_SEC" \
  -X POST http://127.0.0.1:8080/v1/completions \
  -H 'Content-Type: application/json' \
  -d @- \
  -w $'\n__HTTP_CODE__:%{http_code}' \
  < "$WORKDIR/request_payload_compact.json" \
  2>"$WORKDIR/curl_err.txt" || true)

t1=$(date +%s%3N 2>/dev/null || python3 -c 'import time; print(int(time.time()*1000))')
latency_ms=$((t1 - t0))

http_code=$(printf '%s' "$response" | awk -F: '/^__HTTP_CODE__:/ {print $2}' | tr -d '\r')
printf '%s' "$response" | sed '/^__HTTP_CODE__:/d' >"$WORKDIR/response_raw.json"
printf '%s' "${http_code:-000}" >"$WORKDIR/http_status.txt"
printf '%s' "$latency_ms" >"$WORKDIR/latency_ms.txt"

log "http=${http_code:-000} latency_ms=${latency_ms}"

# --- Analyze ---
python3 - "$WORKDIR" <<'PY'
import json
import re
import sys
from datetime import datetime, timezone
from pathlib import Path

work = Path(sys.argv[1])

def load_json(p):
    try:
        return json.loads(p.read_text())
    except Exception:
        return {}

http = work.joinpath("http_status.txt").read_text().strip()
latency = work.joinpath("latency_ms.txt").read_text().strip()
data = load_json(work / "response_raw.json")

text = ""
finish = ""
usage = {}
timings = {}
if data.get("choices"):
    c0 = data["choices"][0]
    text = c0.get("text") or ""
    finish = c0.get("finish_reason") or ""
usage = data.get("usage") or {}
timings = data.get("timings") or {}

(work / "raw_completion.txt").write_text(text)

flags = []
for name, pat in [
    ("user_marker", r"\[User\]"),
    ("agent_marker", r"\[Agent\]"),
    ("rag_context", r"\[RAG Context\]"),
    ("document_label", r"Document:"),
    ("source_span", r"source_span="),
    ("chatml_im_start", r"<\|im_start\|>"),
    ("chatml_im_end", r"<\|im_end\|>"),
]:
    if re.search(pat, text):
        flags.append(name)

lines = [ln.strip() for ln in text.splitlines() if ln.strip()]
repeated = len(lines) != len(set(lines)) if lines else False

lines_out = []
lines_out.append("# Thoth Prompt Replay Report")
lines_out.append("")
lines_out.append(f"Generated: {datetime.now(timezone.utc).isoformat()}")
lines_out.append(f"Work directory: `{work}`")
lines_out.append("")
lines_out.append("## Verification")
lines_out.append("")
lines_out.append("Same container/model as Thoth (`THOTH_INFERENCE_BASE_URL=http://llama-server:8080`, `OLLAMA_MODEL=chat`).")
lines_out.append("See `llama_verify.txt`.")
lines_out.append("")
lines_out.append("## Request")
lines_out.append("")
lines_out.append("- **Source prompt:** captured `thoth_final_prompt.txt`")
lines_out.append("- **Endpoint:** `POST /v1/completions`")
lines_out.append("- **model:** `chat`")
lines_out.append("- **temperature:** 0.7")
lines_out.append("- **top_p:** 1.0")
lines_out.append("- **max_tokens:** 128 (diagnostic cap; Thoth production uses 512)")
lines_out.append("- **stop:** absent")
lines_out.append("")
lines_out.append("## Response")
lines_out.append("")
lines_out.append(f"- **HTTP status:** {http}")
lines_out.append(f"- **Latency (ms):** {latency}")
lines_out.append(f"- **finish_reason:** `{finish}`")
lines_out.append(f"- **prompt_tokens:** {usage.get('prompt_tokens')}")
lines_out.append(f"- **completion_tokens:** {usage.get('completion_tokens')}")
lines_out.append(f"- **completion_chars:** {len(text)}")
lines_out.append(f"- **Leakage flags:** {', '.join(flags) if flags else 'none'}")
lines_out.append(f"- **Line repetition:** {'yes' if repeated else 'no'}")
if timings:
    lines_out.append(f"- **Server timings:** `{json.dumps(timings)}`")
lines_out.append("")
lines_out.append("## Raw completion (`choices[0].text`)")
lines_out.append("")
lines_out.append("```")
lines_out.append(text or "(empty)")
lines_out.append("```")
lines_out.append("")

(work / "REPORT.md").write_text("\n".join(lines_out))

summary = {
    "http_status": http,
    "latency_ms": latency,
    "finish_reason": finish,
    "usage": usage,
    "completion_chars": len(text),
    "leakage_flags": flags,
    "line_repetition": repeated,
}
(work / "summary.json").write_text(json.dumps(summary, indent=2))

print(f"REPORT.md -> {work / 'REPORT.md'}")
print(f"raw_completion.txt -> {work / 'raw_completion.txt'} ({len(text)} chars)")
print(f"http={http} finish_reason={finish} completion_tokens={usage.get('completion_tokens')}")
PY

log "done workdir=${WORKDIR}"
echo ""
echo "Work directory: ${WORKDIR}"
echo "Report: ${WORKDIR}/REPORT.md"
