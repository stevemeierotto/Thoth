#!/usr/bin/env bash
# thoth_abc_interface_probe.sh — Test C: actual buildChatRolePrompt() shape via chat/completions.
# Compares with existing A/B artifacts when available.
#
# Diagnostic only. No production changes.
#
# Usage:
#   ./scripts/thoth_abc_interface_probe.sh
#   ./scripts/thoth_abc_interface_probe.sh /path/to/thoth_final_prompt.txt [/path/to/ab_workdir]

set -euo pipefail

PROMPT_FILE="${1:-/tmp/thoth-inference-trace-1786297828/thoth_final_prompt.txt}"
AB_WORKDIR="${2:-/tmp/thoth-ab-interface-1786300362}"
LLAMA_CONTAINER="${LLAMA_CONTAINER:-thoth-llama-server-1}"
INFERENCE_MODEL="${INFERENCE_MODEL:-chat}"
TEMPERATURE="${TEMPERATURE:-0.7}"
TOP_P="${TOP_P:-1.0}"
MAX_TOKENS="${MAX_TOKENS:-128}"
PROBE_TIMEOUT_SEC="${PROBE_TIMEOUT_SEC:-180}"

# Exact Thoth ChatML stops — read from source to avoid transcription errors
read -r CHATML_STOP_IM_END CHATML_STOP_IM_START < <(python3 - <<'PY'
import re
text = open("/home/steve/Thoth/external/basic_agent/include/chat_prompt_config.h").read()
end = re.search(r'kChatMlStopImEnd\s*=\s*"([^"]+)"', text).group(1)
start = re.search(r'kChatMlStopImStart\s*=\s*"([^"]+)"', text).group(1)
print(end, start)
PY
)

TS="$(date +%s)"
WORKDIR="${WORKDIR:-/tmp/thoth-abc-interface-${TS}}"
mkdir -p "$WORKDIR"

log() { echo "$1" | tee -a "$WORKDIR/probe.log"; }

log "=== Test C: Thoth buildChatRolePrompt() replay ==="
log "workdir=${WORKDIR}"
log "prompt_source=${PROMPT_FILE}"
log "ab_reference=${AB_WORKDIR}"
log "stops=[${CHATML_STOP_IM_END}, ${CHATML_STOP_IM_START}]"

# --- Build Test C messages (mirrors buildChatRolePrompt grounded layout) ---
python3 - "$PROMPT_FILE" "$WORKDIR" "$INFERENCE_MODEL" "$MAX_TOKENS" "$TEMPERATURE" "$TOP_P" "$CHATML_STOP_IM_END" "$CHATML_STOP_IM_START" <<'PY'
import json
import sys
from pathlib import Path

prompt_file, work, model, max_tokens, temperature, top_p, stop_end, stop_start = sys.argv[1:10]
work = Path(work)
raw = Path(prompt_file).read_text()

# Decompose flat Thoth prompt into buildChatRolePrompt() roles:
#   system_content = assembleConversationSections (rules + Thoth identity, no [Agent])
#   user_content   = [RAG Context] + rag + [User Query] + query
rag_marker = "[RAG Context]\n"
user_query_marker = "[User Query]\n"
agent_marker = "[Agent]"

if not raw.startswith(rag_marker):
    raise SystemExit("missing [RAG Context] header")
rest = raw[len(rag_marker):]
if user_query_marker not in rest:
    raise SystemExit("missing [User Query] header")
rag_part, tail = rest.split(user_query_marker, 1)
query_line = tail.split("\n", 1)[0]
rules_and_identity = tail[len(query_line):].strip()
if rules_and_identity.endswith(agent_marker):
    rules_and_identity = rules_and_identity[: -len(agent_marker)].rstrip()

system_content = rules_and_identity
user_content = rag_marker + rag_part.rstrip() + "\n" + user_query_marker + query_line + "\n"

(work / "test_c_system.txt").write_text(system_content)
(work / "test_c_user.txt").write_text(user_content)

payload = {
    "model": model,
    "messages": [
        {"role": "system", "content": system_content},
        {"role": "user", "content": user_content},
    ],
    "max_tokens": int(max_tokens),
    "temperature": float(temperature),
    "top_p": float(top_p),
    "stream": False,
    "stop": [stop_end, stop_start],
}

(work / "test_c_payload.json").write_text(json.dumps(payload, indent=2))
(work / "test_c_stops.json").write_text(json.dumps({
    "kChatMlStopImEnd": stop_end,
    "kChatMlStopImStart": stop_start,
    "source": "external/basic_agent/include/chat_prompt_config.h",
}, indent=2))

notes = f"""Test C message construction (diagnostic)
=========================================
Mirrors PromptFactory::buildChatRolePrompt() for grounded chat:
  - system_content = assembleConversationSections output (rules + Thoth identity)
  - user_content   = [RAG Context] + fitted RAG + [User Query] + user query
  - NO [Agent] completion cue (includeCompletionCue=false in chat path)
  - ChatML stops exactly as Thoth chat mode: chatStopSequences(Chat)

Stop strings verified from source (not rendered report):
  kChatMlStopImEnd   = {stop_end!r}
  kChatMlStopImStart = {stop_start!r}
"""
(work / "test_c_notes.txt").write_text(notes)
print("Test C payloads built")
PY

# --- Run Test C ---
log "running Test C -> POST /v1/chat/completions"
t0=$(date +%s%3N 2>/dev/null || python3 -c 'import time; print(int(time.time()*1000))')
response=$(docker exec -i "$LLAMA_CONTAINER" curl -s -m "$PROBE_TIMEOUT_SEC" \
  -X POST http://127.0.0.1:8080/v1/chat/completions \
  -H 'Content-Type: application/json' \
  -d @- \
  -w $'\n__HTTP_CODE__:%{http_code}' \
  < "$WORKDIR/test_c_payload.json" \
  2>"$WORKDIR/test_c_curl_err.txt" || true)
t1=$(date +%s%3N 2>/dev/null || python3 -c 'import time; print(int(time.time()*1000))')
latency_ms=$((t1 - t0))
http_code=$(printf '%s' "$response" | awk -F: '/^__HTTP_CODE__:/ {print $2}' | tr -d '\r')
printf '%s' "$response" | sed '/^__HTTP_CODE__:/d' >"$WORKDIR/test_c_response.json"
printf '%s' "${http_code:-000}" >"$WORKDIR/test_c_http.txt"
printf '%s' "$latency_ms" >"$WORKDIR/test_c_latency_ms.txt"
log "Test C http=${http_code:-000} latency_ms=${latency_ms}"

# --- A/B/C comparison report ---
python3 - "$WORKDIR" "$AB_WORKDIR" <<'PY'
import json
import re
import sys
from datetime import datetime, timezone
from pathlib import Path

work = Path(sys.argv[1])
ab = Path(sys.argv[2])

def load_json(p):
    try:
        return json.loads(p.read_text())
    except Exception:
        return {}

def analyze(prefix, workdir):
    http_path = workdir / f"{prefix}_http.txt"
    if not http_path.exists():
        http_path = workdir / f"{prefix}.http"
    http = http_path.read_text().strip() if http_path.exists() else "missing"
    lat_path = workdir / f"{prefix}_latency_ms.txt"
    latency = lat_path.read_text().strip() if lat_path.exists() else "?"
    resp_path = workdir / f"{prefix}_response.json"
    if not resp_path.exists():
        resp_path = workdir / f"{prefix}_raw_completion.txt"
    data = load_json(resp_path) if resp_path.suffix == ".json" else {}
    text = ""
    if resp_path.suffix == ".txt" and resp_path.exists():
        text = resp_path.read_text()
    elif data.get("choices"):
        c0 = data["choices"][0]
        text = c0.get("text") or ""
        if not text and isinstance(c0.get("message"), dict):
            text = c0["message"].get("content") or ""
    finish = ""
    if data.get("choices"):
        finish = data["choices"][0].get("finish_reason") or ""
    usage = data.get("usage") or {}

    flags = []
    for name, pat in [
        ("user_marker", r"\[User\]"),
        ("agent_marker", r"\[Agent\]"),
        ("rag_context", r"\[RAG Context\]"),
        ("document_label", r"Document:"),
        ("source_span", r"source_span="),
        ("chatml_im_end", r"<\|im_end\|>"),
        ("chatml_im_start", r"<\|im_start\|>"),
    ]:
        if re.search(pat, text):
            flags.append(name)

    lower = text.lower()
    on_topic = any(k in lower for k in (
        "sidebar", "collapsible", "wxscrolled", "aui", "leftsidebar", "rightsidebar", "pane"
    ))
    transcript_loop = text.count("[User]") >= 2 or bool(re.search(r"\[User\]\s*\d", text))
    plausible = (
        http == "200"
        and bool(text.strip())
        and on_topic
        and not transcript_loop
        and "user_marker" not in flags
    )

    return {
        "http": http,
        "latency_ms": latency,
        "finish_reason": finish,
        "usage": usage,
        "text": text,
        "flags": flags,
        "on_topic": on_topic,
        "transcript_loop": transcript_loop,
        "plausible": plausible,
        "completion_chars": len(text),
    }

# Test C fresh
c = analyze("test_c", work)
(work / "test_c_raw_completion.txt").write_text(c["text"])

# Test A/B from prior run
a = analyze("test_a", ab)
b = analyze("test_b", ab)

# Decision tree
if c["plausible"]:
    decision = (
        "C-GOOD: Actual Thoth buildChatRolePrompt() messages via /v1/chat/completions produce a "
        "coherent answer. Evidence supports switching conversational inference to the chat interface "
        "WITHOUT rewriting buildChatRolePrompt() content."
    )
    letter = "interface-only"
elif c["http"] != "200" or not c["text"].strip():
    decision = (
        "C-INCONCLUSIVE: Test C did not return usable output (timeout/error/empty). Cannot distinguish "
        "interface-only fix from prompt rewrite."
    )
    letter = "inconclusive"
else:
    decision = (
        "C-BAD: Thoth buildChatRolePrompt() messages still fail via /v1/chat/completions. Evidence supports "
        "both switching to chat interface AND revising role-message construction."
    )
    letter = "interface-and-prompt"

lines = []
lines.append("# A / B / C Interface Comparison Report")
lines.append("")
lines.append(f"Generated: {datetime.now(timezone.utc).isoformat()}")
lines.append(f"Work directory: `{work}`")
lines.append(f"A/B reference: `{ab}`")
lines.append("")
lines.append("## Decision tree")
lines.append("")
lines.append("```")
lines.append("A: /v1/completions + flat Thoth prompt          -> BAD")
lines.append("B: /v1/chat/completions + clean diag messages  -> GOOD-ish")
lines.append("C: /v1/chat/completions + Thoth role messages  -> see below")
lines.append("```")
lines.append("")
stops = load_json(work / "test_c_stops.json")
lines.append("## Test C stop strings (source-verified)")
lines.append("")
lines.append(f"- `kChatMlStopImEnd`: `{stops.get('kChatMlStopImEnd')}`")
lines.append(f"- `kChatMlStopImStart`: `{stops.get('kChatMlStopImStart')}`")
lines.append("")

for label, key, r, endpoint, extra in (
    ("Test A", "A", a, "/v1/completions", "flat Thoth prompt"),
    ("Test B", "B", b, "/v1/chat/completions", "clean diagnostic messages, no stops"),
    ("Test C", "C", c, "/v1/chat/completions", "buildChatRolePrompt() shape + Thoth ChatML stops"),
):
    lines.append(f"## {label} — `{endpoint}` ({extra})")
    lines.append("")
    lines.append(f"| Field | Value |")
    lines.append(f"|-------|-------|")
    lines.append(f"| HTTP | {r['http']} |")
    lines.append(f"| Latency (ms) | {r['latency_ms']} |")
    lines.append(f"| finish_reason | `{r['finish_reason']}` |")
    lines.append(f"| prompt_tokens | {r['usage'].get('prompt_tokens')} |")
    lines.append(f"| completion_tokens | {r['usage'].get('completion_tokens')} |")
    lines.append(f"| On-topic | {r['on_topic']} |")
    lines.append(f"| Transcript loop | {r['transcript_loop']} |")
    lines.append(f"| Plausible answer | {r['plausible']} |")
    lines.append(f"| Leakage flags | {', '.join(r['flags']) if r['flags'] else 'none'} |")
    lines.append("")
    lines.append("**Raw completion:**")
    lines.append("")
    lines.append("```")
    lines.append(r["text"] or "(empty)")
    lines.append("```")
    lines.append("")

lines.append("## Conclusion")
lines.append("")
lines.append(f"**{letter}.** {decision}")
lines.append("")

(work / "ABC_REPORT.md").write_text("\n".join(lines))
summary = {
    "test_a": {k: v for k, v in a.items() if k != "text"},
    "test_b": {k: v for k, v in b.items() if k != "text"},
    "test_c": {k: v for k, v in c.items() if k != "text"},
    "decision": letter,
    "decision_text": decision,
    "stops": stops,
}
(work / "abc_summary.json").write_text(json.dumps(summary, indent=2))
print(f"ABC_REPORT.md -> {work / 'ABC_REPORT.md'}")
print(f"DECISION: {letter}")
print(decision)
PY

log "done workdir=${WORKDIR}"
echo ""
echo "Work directory: ${WORKDIR}"
echo "Report: ${WORKDIR}/ABC_REPORT.md"
ls -la "$WORKDIR"
