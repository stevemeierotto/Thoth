#!/usr/bin/env bash
# inference_control_probe.sh — Direct llama.cpp /v1/completions control diagnostic.
#
# Bypasses Thoth RAG, prompt factory, sanitizer, and retry logic.
# Measurement only — no production or configuration changes.
#
# Usage:
#   ./scripts/inference_control_probe.sh
#
# Environment:
#   LLAMA_CONTAINER      — default thoth-llama-server-1
#   LLAMA_BASE           — default http://127.0.0.1:8080 (inside container)
#   INFERENCE_MODEL      — default chat (matches OLLAMA_MODEL)
#   TEMPERATURE          — default 0.7
#   TOP_P                — default 1.0
#   MAX_TOKENS           — default 512
#   PROBE_TIMEOUT_SEC    — default 300 per request

set -euo pipefail

LLAMA_CONTAINER="${LLAMA_CONTAINER:-thoth-llama-server-1}"
LLAMA_BASE="${LLAMA_BASE:-http://127.0.0.1:8080}"
INFERENCE_MODEL="${INFERENCE_MODEL:-chat}"
TEMPERATURE="${TEMPERATURE:-0.7}"
TOP_P="${TOP_P:-1.0}"
MAX_TOKENS="${MAX_TOKENS:-512}"
PROBE_TIMEOUT_SEC="${PROBE_TIMEOUT_SEC:-300}"

TS="$(date +%s)"
WORKDIR="${WORKDIR:-/tmp/inference-control-${TS}}"
mkdir -p "$WORKDIR"

log() { echo "$1" | tee -a "$WORKDIR/probe.log"; }

llama_curl() {
  local method="$1"
  local path="$2"
  local body="${3:-}"
  local out_file="$4"
  local status_file="$5"
  local timeout="${6:-30}"
  local response http_code

  if [ -n "$body" ]; then
    response=$(docker exec "$LLAMA_CONTAINER" curl -s -m "$timeout" \
      -X "$method" "${LLAMA_BASE}${path}" \
      -H 'Content-Type: application/json' \
      -d "$body" -w $'\n__HTTP_CODE__:%{http_code}' \
      2>"$WORKDIR/curl_err_${status_file##*/}.txt" || true)
  else
    response=$(docker exec "$LLAMA_CONTAINER" curl -s -m "$timeout" \
      -X "$method" "${LLAMA_BASE}${path}" \
      -w $'\n__HTTP_CODE__:%{http_code}' \
      2>"$WORKDIR/curl_err_${status_file##*/}.txt" || true)
  fi

  http_code=$(printf '%s' "$response" | awk -F: '/^__HTTP_CODE__:/ {print $2}' | tr -d '\r')
  printf '%s' "$response" | sed '/^__HTTP_CODE__:/d' >"$out_file"
  printf '%s' "${http_code:-000}" >"$status_file"
}

log "=== inference control probe ==="
log "workdir=${WORKDIR}"
log "container=${LLAMA_CONTAINER} base=${LLAMA_BASE}"
log "model=${INFERENCE_MODEL} temperature=${TEMPERATURE} top_p=${TOP_P} max_tokens=${MAX_TOKENS}"

# --- Phase 0: baseline ---------------------------------------------------------
log "phase 0: baseline capture"

llama_curl GET /health "" "$WORKDIR/baseline_health.json" "$WORKDIR/baseline_health.http"
llama_curl GET /props "" "$WORKDIR/baseline_props.json" "$WORKDIR/baseline_props.http"
llama_curl GET /v1/models "" "$WORKDIR/baseline_models.json" "$WORKDIR/baseline_models.http"
docker exec "$LLAMA_CONTAINER" ls -la /models/ >"$WORKDIR/baseline_models_ls.txt" 2>&1 || true
docker inspect "$LLAMA_CONTAINER" --format '{{json .Config.Cmd}}' >"$WORKDIR/baseline_container_cmd.json" 2>&1 || true
docker inspect "$LLAMA_CONTAINER" --format '{{index .Config.Image}}' >"$WORKDIR/baseline_container_image.txt" 2>&1 || true

# Optional Thoth-side reference (read-only)
docker exec thoth-thoth-engine-1 cat /workspace/config.json >"$WORKDIR/thoth_workspace_config.json" 2>/dev/null || echo '{}' >"$WORKDIR/thoth_workspace_config.json"
curl -sf -m 10 http://127.0.0.1:8090/ready >"$WORKDIR/thoth_engine_ready.json" 2>/dev/null || echo '{}' >"$WORKDIR/thoth_engine_ready.json"

# --- Test prompts --------------------------------------------------------------
read -r -d '' TEST1_PROMPT <<'EOF' || true
What are three common improvements you can make to a software project?
EOF

read -r -d '' TEST2_PROMPT <<'EOF' || true
Here are three completed improvements: improved indexing, faster retrieval, and better error handling. Summarize these improvements in three concise bullet points.
EOF

read -r -d '' TEST3_PROMPT <<'EOF' || true
Completed improvements include improved indexing, better attachment handling, faster retrieval, and improved chat response validation.

Based on the information above, summarize the completed improvements in 3 concise bullet points.
EOF

read -r -d '' TEST4_PROMPT <<'EOF' || true
Passage 1: A dedicated llama-embed-server was added for embeddings on port 8081, with startup embed probe and dimension 768 validation.

Passage 2: GUI Phase 10 shipped Engine-owned conversation and session authority.

Passage 3: Plan N delivered chat generation safety: sanitize, soft-empty handling, empty transcript stops, and source_span cleanup.

Passage 4: GRAG retrieval uses directional scoring with goal-relative adaptive graph retrieval.

Passage 5: Memory consolidation M4 added range restore and warm-tier archive policies.

Based on the passages above, summarize the completed improvements in 3–5 concise bullet points. Do not quote the passages unless necessary.
EOF

declare -a TEST_NAMES=("test1_basic" "test2_synthesis" "test3_single_passage" "test4_multi_passage")
declare -a TEST_PROMPTS=("$TEST1_PROMPT" "$TEST2_PROMPT" "$TEST3_PROMPT" "$TEST4_PROMPT")
declare -a TEST_LABELS=(
  "Test 1 — Basic question"
  "Test 2 — Simple synthesis"
  "Test 3 — One realistic retrieved passage"
  "Test 4 — Small multi-passage synthesis"
)

# --- Run tests -----------------------------------------------------------------
for i in "${!TEST_NAMES[@]}"; do
  name="${TEST_NAMES[$i]}"
  prompt="${TEST_PROMPTS[$i]}"
  log "running ${name}..."

  printf '%s' "$prompt" >"$WORKDIR/${name}_prompt.txt"

  payload=$(python3 - "$INFERENCE_MODEL" "$prompt" "$MAX_TOKENS" "$TEMPERATURE" "$TOP_P" <<'PY'
import json, sys
model, prompt, max_tokens, temperature, top_p = sys.argv[1:6]
print(json.dumps({
    "model": model,
    "prompt": prompt,
    "max_tokens": int(max_tokens),
    "temperature": float(temperature),
    "top_p": float(top_p),
    "stream": False,
}))
PY
)

  t0=$(date +%s%3N 2>/dev/null || python3 -c 'import time; print(int(time.time()*1000))')
  llama_curl POST /v1/completions "$payload" "$WORKDIR/${name}_raw.json" "$WORKDIR/${name}.http" "$PROBE_TIMEOUT_SEC"
  t1=$(date +%s%3N 2>/dev/null || python3 -c 'import time; print(int(time.time()*1000))')
  latency_ms=$((t1 - t0))

  echo "$latency_ms" >"$WORKDIR/${name}_latency_ms.txt"
  log "${name} http=$(cat "$WORKDIR/${name}.http") latency_ms=${latency_ms}"
done

# --- Analyze and write report --------------------------------------------------
python3 - "$WORKDIR" <<'PY'
import json
import re
import sys
from datetime import datetime, timezone
from pathlib import Path

work = Path(sys.argv[1])

def load_json(path):
    try:
        return json.loads(path.read_text())
    except Exception:
        return {}

def load_text(path, default=""):
    try:
        return path.read_text()
    except Exception:
        return default

def extract_response(data):
    choices = data.get("choices") or []
    if not choices:
        return "", ""
    c0 = choices[0]
    text = c0.get("text") or ""
    if not text and isinstance(c0.get("message"), dict):
        text = c0["message"].get("content") or ""
    finish = c0.get("finish_reason") or ""
    return text, finish

def usage_fields(data):
    u = data.get("usage") or {}
    return {
        "prompt_tokens": u.get("prompt_tokens"),
        "completion_tokens": u.get("completion_tokens"),
        "total_tokens": u.get("total_tokens"),
    }

def check_leakage(text):
    flags = []
    patterns = [
        ("chatml_im_start", r"<\|im_start\|>"),
        ("chatml_im_end", r"<\|im_end\|>"),
        ("user_marker", r"\[User\]"),
        ("agent_marker", r"\[Agent\]"),
        ("rag_context", r"\[RAG Context\]"),
        ("document_label", r"Document:"),
        ("source_span", r"source_span="),
        ("user_query_header", r"\[User Query\]"),
    ]
    for name, pat in patterns:
        if re.search(pat, text):
            flags.append(name)
    return flags

def check_repetition(text):
    lines = [ln.strip() for ln in text.splitlines() if ln.strip()]
    if len(lines) >= 2:
        from collections import Counter
        counts = Counter(lines)
        repeated = [ln for ln, n in counts.items() if n >= 2 and len(ln) > 20]
        if repeated:
            return True, repeated[:3]
    # repeated 4+ word ngrams
    words = text.split()
    if len(words) >= 12:
        for n in (4, 5, 6):
            grams = [" ".join(words[i:i+n]) for i in range(len(words)-n+1)]
            from collections import Counter
            c = Counter(grams)
            for g, cnt in c.most_common(3):
                if cnt >= 3 and len(g) > 15:
                    return True, [g]
    return False, []

def plausible_answer(test_id, text, finish, http_status, error):
    if http_status != 200 or error:
        return False, "http_or_parse_error"
    stripped = text.strip()
    if not stripped:
        return False, "empty_text"
    if len(stripped) < 10:
        return False, "too_short"
    lower = stripped.lower()
    if test_id == "test1_basic":
        # expect list-like or enumerated improvements
        if any(k in lower for k in ("improve", "refactor", "test", "document", "performance", "code", "quality", "maintain")):
            return True, "on_topic_keywords"
        if re.search(r"\b1[\).\:]|\-\s|\*", stripped):
            return True, "list_structure"
        return False, "off_topic_or_nonsense"
    if test_id in ("test2_synthesis", "test3_single_passage", "test4_multi_passage"):
        if any(k in lower for k in ("index", "retriev", "error", "handl", "attach", "valid", "embed", "grag", "memory", "gui", "chat", "consolidat", "improv")):
            return True, "synthesis_on_topic"
        if re.search(r"\b1[\).\:]|\-\s|\*", stripped):
            return True, "bullet_structure"
        return False, "synthesis_off_topic"
    return True, "generic_nonempty"

# Baseline
props = load_json(work / "baseline_props.json")
models = load_json(work / "baseline_models.json")
health = load_json(work / "baseline_health.json")
models_ls = load_text(work / "baseline_models_ls.txt")
container_cmd = load_text(work / "baseline_container_cmd.json").strip()
container_image = load_text(work / "baseline_container_image.txt").strip()

model_meta = {}
if models.get("data"):
    model_meta = models["data"][0].get("meta") or {}
model_path = props.get("model_path") or model_meta.get("model_path") or "/models/chat.gguf"
model_alias = props.get("model_alias") or model_path
model_ftype = props.get("model_ftype") or model_meta.get("ftype") or "unknown"
n_params = model_meta.get("n_params")
build_info = props.get("build_info") or (load_json(work / "test1_basic_raw.json").get("system_fingerprint") or "unknown")
default_gen = (props.get("default_generation_settings") or {}).get("params") or {}

tests = [
    ("test1_basic", "Test 1 — Basic question"),
    ("test2_synthesis", "Test 2 — Simple synthesis"),
    ("test3_single_passage", "Test 3 — One realistic retrieved passage"),
    ("test4_multi_passage", "Test 4 — Small multi-passage synthesis"),
]

results = []
for tid, label in tests:
    http_status = load_text(work / f"{tid}.http").strip()
    raw_path = work / f"{tid}_raw.json"
    raw_text = load_text(raw_path)
    data = load_json(raw_path)
    text, finish = extract_response(data)
    usage = usage_fields(data)
    latency = load_text(work / f"{tid}_latency_ms.txt").strip()
    leakage = check_leakage(text)
    rep, rep_samples = check_repetition(text)
    ok, reason = plausible_answer(tid, text, finish, int(http_status) if http_status.isdigit() else 0, data.get("error"))
    results.append({
        "id": tid,
        "label": label,
        "http_status": http_status,
        "latency_ms": latency,
        "finish_reason": finish,
        "usage": usage,
        "response_text": text,
        "raw_json": data,
        "leakage_flags": leakage,
        "repetition_detected": rep,
        "repetition_samples": rep_samples,
        "plausible_answer": ok,
        "plausible_reason": reason,
        "truncated": finish == "length",
    })

# Conclusion A/B/C/D
infra_fail = any(r["http_status"] != "200" for r in results) or not health
if infra_fail:
    conclusion = "D"
    conclusion_text = "The results are inconclusive because of an infrastructure/configuration issue."
else:
    t1_ok = results[0]["plausible_answer"]
    t2_ok = results[1]["plausible_answer"]
    ctx_ok = all(r["plausible_answer"] for r in results[2:])
    if not t1_ok:
        conclusion = "B"
        conclusion_text = "The model/inference environment fails basic generation."
    elif t1_ok and t2_ok and ctx_ok:
        conclusion = "A"
        conclusion_text = "The model/inference environment passes the basic control tests."
    elif t1_ok and t2_ok and not ctx_ok:
        conclusion = "C"
        conclusion_text = "Basic generation works, but synthesis fails when context is introduced."
    else:
        conclusion = "B"
        conclusion_text = "The model/inference environment fails basic generation."

summary = {
    "generated_at": datetime.now(timezone.utc).isoformat(),
    "workdir": str(work),
    "endpoint": "/v1/completions",
    "model_name_sent": "chat",
    "generation_parameters": {
        "temperature": 0.7,
        "top_p": 1.0,
        "max_tokens": 512,
        "stop_sequences": [],
        "stream": False,
    },
    "model_identity": {
        "gguf_path": model_path,
        "model_alias": model_alias,
        "quantization": model_ftype,
        "n_params": n_params,
        "n_ctx": model_meta.get("n_ctx") or props.get("default_generation_settings", {}).get("params", {}).get("n_ctx"),
        "size_bytes": model_meta.get("size"),
        "build_info": build_info,
        "container_image": container_image,
        "container_cmd": container_cmd,
        "models_volume_listing": models_ls.strip(),
    },
    "server_default_generation_settings": {
        "temperature": default_gen.get("temperature"),
        "top_p": default_gen.get("top_p"),
        "top_k": default_gen.get("top_k"),
        "n_ctx": props.get("default_generation_settings", {}).get("n_ctx") if isinstance(props.get("default_generation_settings"), dict) else None,
    },
    "tests": results,
    "conclusion_letter": conclusion,
    "conclusion_text": conclusion_text,
}
(work / "summary.json").write_text(json.dumps(summary, indent=2))

# REPORT.md
lines = []
lines.append("# Inference Control Diagnostic Report")
lines.append("")
lines.append(f"Generated: {summary['generated_at']}")
lines.append(f"Work directory: `{work}`")
lines.append("")
lines.append("## Key question")
lines.append("")
lines.append("Can the actual llama.cpp model currently running in Docker produce coherent, on-topic answers and simple synthesis when accessed directly through the same `/v1/completions` endpoint and generation parameters used by Thoth?")
lines.append("")
lines.append("## 1. Model identity")
lines.append("")
lines.append(f"| Field | Value |")
lines.append(f"|-------|-------|")
lines.append(f"| GGUF path | `{model_path}` |")
lines.append(f"| Model alias | `{model_alias}` |")
lines.append(f"| Parameter count | `{n_params}` |")
lines.append(f"| Quantization | `{model_ftype}` |")
lines.append(f"| Context (n_ctx) | `{model_meta.get('n_ctx', 'unknown')}` |")
lines.append(f"| Size (bytes) | `{model_meta.get('size', 'unknown')}` |")
lines.append(f"| llama.cpp build | `{build_info}` |")
lines.append(f"| Container image | `{container_image}` |")
lines.append(f"| Container command | `{container_cmd}` |")
lines.append("")
lines.append("Models volume listing:")
lines.append("```")
lines.append(models_ls.strip())
lines.append("```")
lines.append("")
lines.append("## 2. Endpoint and generation parameters")
lines.append("")
lines.append("- **Endpoint:** `POST /v1/completions` (Thoth current path; no `/v1/chat/completions` test in this run)")
lines.append("- **Model name sent:** `chat`")
lines.append("- **Parameters sent:** `temperature=0.7`, `top_p=1.0`, `max_tokens=512`, no `stop` field")
lines.append("")
lines.append("Server default generation settings (from `/props`, for reference only — not used when explicit params are sent):")
lines.append("")
lines.append(f"- temperature: `{default_gen.get('temperature')}`")
lines.append(f"- top_p: `{default_gen.get('top_p')}`")
lines.append(f"- top_k: `{default_gen.get('top_k')}`")
lines.append("")
lines.append("## 3. Test results")
lines.append("")

for r in results:
    lines.append(f"### {r['label']}")
    lines.append("")
    lines.append(f"- **HTTP status:** {r['http_status']}")
    lines.append(f"- **Latency (ms):** {r['latency_ms']}")
    lines.append(f"- **finish_reason:** `{r['finish_reason']}`")
    lines.append(f"- **prompt_tokens:** {r['usage'].get('prompt_tokens')}")
    lines.append(f"- **completion_tokens:** {r['usage'].get('completion_tokens')}")
    lines.append(f"- **Plausible on-topic answer:** {r['plausible_answer']} ({r['plausible_reason']})")
    lines.append(f"- **Truncated (finish_reason=length):** {r['truncated']}")
    if r['leakage_flags']:
        lines.append(f"- **Leakage flags:** {', '.join(r['leakage_flags'])}")
    else:
        lines.append("- **Leakage flags:** none detected")
    if r['repetition_detected']:
        lines.append(f"- **Repetition detected:** yes — samples: {r['repetition_samples']}")
    else:
        lines.append("- **Repetition detected:** no")
    lines.append("")
    lines.append("**Raw response text:**")
    lines.append("")
    lines.append("```")
    lines.append(r['response_text'] or "(empty)")
    lines.append("```")
    lines.append("")

lines.append("## 4. Conclusion")
lines.append("")
lines.append(f"**{conclusion}.** {conclusion_text}")
lines.append("")

(work / "REPORT.md").write_text("\n".join(lines))
print(f"REPORT.md -> {work / 'REPORT.md'}")
print(f"summary.json -> {work / 'summary.json'}")
print(f"CONCLUSION: {conclusion} — {conclusion_text}")
PY

log "done workdir=${WORKDIR}"
echo ""
echo "Work directory: ${WORKDIR}"
echo "Report: ${WORKDIR}/REPORT.md"
