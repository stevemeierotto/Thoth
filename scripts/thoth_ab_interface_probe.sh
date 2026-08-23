#!/usr/bin/env bash
# thoth_ab_interface_probe.sh — Compare /v1/completions vs /v1/chat/completions
# using the same captured Thoth content in two representations.
#
# Diagnostic only. No production changes.
#
# Usage:
#   ./scripts/thoth_ab_interface_probe.sh [path/to/thoth_final_prompt.txt]

set -euo pipefail

PROMPT_FILE="${1:-/tmp/thoth-inference-trace-1786297828/thoth_final_prompt.txt}"
LLAMA_CONTAINER="${LLAMA_CONTAINER:-thoth-llama-server-1}"
INFERENCE_MODEL="${INFERENCE_MODEL:-chat}"
TEMPERATURE="${TEMPERATURE:-0.7}"
TOP_P="${TOP_P:-1.0}"
MAX_TOKENS="${MAX_TOKENS:-128}"
PROBE_TIMEOUT_SEC="${PROBE_TIMEOUT_SEC:-180}"

TS="$(date +%s)"
WORKDIR="${WORKDIR:-/tmp/thoth-ab-interface-${TS}}"
mkdir -p "$WORKDIR"

log() { echo "$1" | tee -a "$WORKDIR/probe.log"; }

if [ ! -f "$PROMPT_FILE" ]; then
  echo "FAIL: prompt file not found: $PROMPT_FILE" >&2
  exit 1
fi

cp "$PROMPT_FILE" "$WORKDIR/thoth_final_prompt.txt"

log "=== A/B interface probe: completions vs chat/completions ==="
log "workdir=${WORKDIR}"

# --- Chat template from llama.cpp /props ---
docker exec "$LLAMA_CONTAINER" curl -sf http://127.0.0.1:8080/props \
  >"$WORKDIR/llama_props.json"
python3 - "$WORKDIR" <<'PY'
import json, sys
from pathlib import Path
work = Path(sys.argv[1])
props = json.loads((work / "llama_props.json").read_text())
(work / "chat_template.txt").write_text(props.get("chat_template", ""))
meta = {
    "build_info": props.get("build_info"),
    "model_path": props.get("model_path"),
    "model_ftype": props.get("model_ftype"),
    "chat_format": props.get("default_generation_settings", {}).get("params", {}).get("chat_format"),
    "n_ctx": props.get("default_generation_settings", {}).get("n_ctx"),
}
(work / "llama_model_meta.json").write_text(json.dumps(meta, indent=2))
print("chat template saved")
PY

# --- Build Test A + Test B payloads from captured prompt ---
python3 - "$PROMPT_FILE" "$WORKDIR" "$INFERENCE_MODEL" "$MAX_TOKENS" "$TEMPERATURE" "$TOP_P" <<'PY'
import json, sys
from pathlib import Path

prompt_file, work, model, max_tokens, temperature, top_p = sys.argv[1:8]
work = Path(work)
raw = Path(prompt_file).read_text()

# Parse captured Thoth flat prompt into components (same bytes as captured).
rag_marker = "[RAG Context]\n"
user_query_marker = "[User Query]\n"
agent_marker = "[Agent]"

if not raw.startswith(rag_marker):
    raise SystemExit("unexpected prompt shape: missing [RAG Context]")

rest = raw[len(rag_marker):]
if user_query_marker not in rest:
    raise SystemExit("unexpected prompt shape: missing [User Query]")

rag_part, tail = rest.split(user_query_marker, 1)
# tail: query + rules + [Agent]
lines = tail.split("\n")
query = lines[0].strip()

rules_and_identity = tail[len(lines[0]):].strip()
if rules_and_identity.endswith(agent_marker):
    rules_and_identity = rules_and_identity[: -len(agent_marker)].rstrip()

system_content = rules_and_identity
user_content = (
    "Context:\n"
    + rag_part.rstrip()
    + "\n\nQuestion: "
    + query
)

(work / "test_b_system.txt").write_text(system_content)
(work / "test_b_user.txt").write_text(user_content)

# Test A — exact Thoth completions prompt (includes trailing [Agent] cue)
payload_a = {
    "model": model,
    "prompt": raw,
    "max_tokens": int(max_tokens),
    "temperature": float(temperature),
    "top_p": float(top_p),
    "stream": False,
}

# Test B — same information as ChatML messages; server applies chat template
payload_b = {
    "model": model,
    "messages": [
        {"role": "system", "content": system_content},
        {"role": "user", "content": user_content},
    ],
    "max_tokens": int(max_tokens),
    "temperature": float(temperature),
    "top_p": float(top_p),
    "stream": False,
}

(work / "test_a_payload.json").write_text(json.dumps(payload_a, indent=2))
(work / "test_b_payload.json").write_text(json.dumps(payload_b, indent=2))
(work / "test_a_prompt.txt").write_text(raw)
print("payloads built")
PY

run_test() {
  local name="$1"
  local endpoint="$2"
  local payload_file="$3"
  local out_prefix="$4"

  log "running ${name} -> POST ${endpoint} (max_tokens=${MAX_TOKENS}, timeout=${PROBE_TIMEOUT_SEC}s)"
  local t0 t1 response http_code latency_ms
  t0=$(date +%s%3N 2>/dev/null || python3 -c 'import time; print(int(time.time()*1000))')

  response=$(docker exec -i "$LLAMA_CONTAINER" curl -s -m "$PROBE_TIMEOUT_SEC" \
    -X POST "http://127.0.0.1:8080${endpoint}" \
    -H 'Content-Type: application/json' \
    -d @- \
    -w $'\n__HTTP_CODE__:%{http_code}' \
    < "$payload_file" \
    2>"$WORKDIR/${out_prefix}_curl_err.txt" || true)

  t1=$(date +%s%3N 2>/dev/null || python3 -c 'import time; print(int(time.time()*1000))')
  latency_ms=$((t1 - t0))

  http_code=$(printf '%s' "$response" | awk -F: '/^__HTTP_CODE__:/ {print $2}' | tr -d '\r')
  printf '%s' "$response" | sed '/^__HTTP_CODE__:/d' >"$WORKDIR/${out_prefix}_response.json"
  printf '%s' "${http_code:-000}" >"$WORKDIR/${out_prefix}_http.txt"
  printf '%s' "$latency_ms" >"$WORKDIR/${out_prefix}_latency_ms.txt"
  log "${name} http=${http_code:-000} latency_ms=${latency_ms}"
}

run_test "Test A (completions / Thoth flat prompt)" "/v1/completions" "$WORKDIR/test_a_payload.json" "test_a"
run_test "Test B (chat/completions / messages + template)" "/v1/chat/completions" "$WORKDIR/test_b_payload.json" "test_b"

python3 - "$WORKDIR" <<'PY'
import json
import re
import sys
from datetime import datetime, timezone
from pathlib import Path

work = Path(sys.argv[1])

def analyze(prefix):
    http = (work / f"{prefix}_http.txt").read_text().strip()
    latency = (work / f"{prefix}_latency_ms.txt").read_text().strip()
    data = json.loads((work / f"{prefix}_response.json").read_text() or "{}")
    text = ""
    finish = ""
    if data.get("choices"):
        c0 = data["choices"][0]
        text = c0.get("text") or ""
        if not text and isinstance(c0.get("message"), dict):
            text = c0["message"].get("content") or ""
        finish = c0.get("finish_reason") or ""
    usage = data.get("usage") or {}
    timings = data.get("timings") or {}

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
    repeated = len(lines) != len(set(lines)) if len(lines) >= 2 else False

    lower = text.lower()
    on_topic = any(k in lower for k in (
        "sidebar", "collapsible", "wxscrolled", "aui", "leftsidebar", "rightsidebar", "pane"
    ))
    plausible = bool(text.strip()) and on_topic and "user_marker" not in flags and "rag_context" not in flags

    (work / f"{prefix}_raw_completion.txt").write_text(text)
    return {
        "http": http,
        "latency_ms": latency,
        "finish_reason": finish,
        "usage": usage,
        "timings": timings,
        "text": text,
        "flags": flags,
        "repeated": repeated,
        "on_topic": on_topic,
        "plausible": plausible,
        "error": data.get("error"),
    }

a = analyze("test_a")
b = analyze("test_b")

if b["plausible"] and not a["plausible"]:
    conclusion = (
        "Test B produced a more normal answer than Test A. Strong evidence that the issue "
        "is the interface between Thoth's flat /v1/completions prompt construction and the "
        "model's expected ChatML conversational format."
    )
    letter = "B-better"
elif a["plausible"] and not b["plausible"]:
    conclusion = "Test A outperformed Test B — unlikely template mismatch; investigate elsewhere."
    letter = "A-better"
elif not a["plausible"] and not b["plausible"]:
    conclusion = "Both tests produced abnormal output — issue may not be completions-vs-chat alone."
    letter = "both-bad"
else:
    conclusion = "Both tests produced plausible on-topic answers."
    letter = "both-good"

lines = []
lines.append("# A/B Interface Probe Report")
lines.append("")
lines.append(f"Generated: {datetime.now(timezone.utc).isoformat()}")
lines.append(f"Work directory: `{work}`")
lines.append("")
lines.append("## llama.cpp chat template (from `/props`)")
lines.append("")
lines.append("```")
lines.append((work / "chat_template.txt").read_text().strip())
lines.append("```")
lines.append("")
meta = json.loads((work / "llama_model_meta.json").read_text())
lines.append(f"- **model:** `{meta.get('model_path')}`")
lines.append(f"- **build:** `{meta.get('build_info')}`")
lines.append(f"- **quantization:** `{meta.get('model_ftype')}`")
lines.append("")
lines.append("## Shared generation parameters")
lines.append("")
lines.append("- `model=chat`, `temperature=0.7`, `top_p=1.0`, `max_tokens=128`, no `stop` field")
lines.append("- Same factual content; different API representation")
lines.append("")
for label, prefix, endpoint in (
    ("Test A — /v1/completions (Thoth flat prompt)", "test_a", "/v1/completions"),
    ("Test B — /v1/chat/completions (system/user messages)", "test_b", "/v1/chat/completions"),
):
    r = a if prefix == "test_a" else b
    lines.append(f"## {label}")
    lines.append("")
    lines.append(f"- **Endpoint:** `{endpoint}`")
    lines.append(f"- **HTTP:** {r['http']}")
    lines.append(f"- **Latency (ms):** {r['latency_ms']}")
    lines.append(f"- **finish_reason:** `{r['finish_reason']}`")
    lines.append(f"- **prompt_tokens:** {r['usage'].get('prompt_tokens')}")
    lines.append(f"- **completion_tokens:** {r['usage'].get('completion_tokens')}")
    lines.append(f"- **Leakage flags:** {', '.join(r['flags']) if r['flags'] else 'none'}")
    lines.append(f"- **Repetition:** {'yes' if r['repeated'] else 'no'}")
    lines.append(f"- **On-topic keywords:** {'yes' if r['on_topic'] else 'no'}")
    lines.append(f"- **Plausible normal answer:** {'yes' if r['plausible'] else 'no'}")
    if r.get("error"):
        lines.append(f"- **Error:** `{r['error']}`")
    lines.append("")
    lines.append("**Raw completion:**")
    lines.append("")
    lines.append("```")
    lines.append(r["text"] or "(empty)")
    lines.append("```")
    lines.append("")

lines.append("## Conclusion")
lines.append("")
lines.append(f"**{letter}.** {conclusion}")
lines.append("")

(work / "REPORT.md").write_text("\n".join(lines))
summary = {"test_a": {k: v for k, v in a.items() if k != "text"}, "test_b": {k: v for k, v in b.items() if k != "text"}, "conclusion": letter, "conclusion_text": conclusion}
(work / "summary.json").write_text(json.dumps(summary, indent=2))
print(f"REPORT.md -> {work / 'REPORT.md'}")
print(f"CONCLUSION: {letter} — {conclusion}")
PY

log "done workdir=${WORKDIR}"
echo ""
echo "Work directory: ${WORKDIR}"
echo "Report: ${WORKDIR}/REPORT.md"
ls -la "$WORKDIR"
