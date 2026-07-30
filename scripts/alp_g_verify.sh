#!/usr/bin/env bash
# ALP-G — ALP1 certification harness (verify only; no Engine/GUI semantic changes).
#
# Subcommands:
#   preflight  — fail if forbidden source paths changed (certification guard)
#   gate       — G0: build + thoth-core-tests + EGAR lifecycle (CI-safe)
#   engine     — G1: optional live HTTP ALP smoke (requires Engine + ALP flags)
#   all        — preflight + gate + engine (engine skips if unavailable)
#
# Usage (from repo root):
#   ./scripts/alp_g_verify.sh gate
#   THOTH_ALP_ENABLED=1 THOTH_ALP_TX_INDEX=1 THOTH_ALP_GREENFIELD=1 \
#     THOTH_ENGINE_URL=http://127.0.0.1:8090 ./scripts/alp_g_verify.sh engine
#
# Report: agent_workspace/alp_certification/alp_g_report.json
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

BUILD_DIR="${ALP_G_BUILD_DIR:-$ROOT/build/release}"
TESTS_BIN="${THOTH_CORE_TESTS_BIN:-$BUILD_DIR/tests/thoth-core-tests}"
REPORT_DIR="${ALP_G_REPORT_DIR:-$ROOT/agent_workspace/alp_certification}"
REPORT_PATH="$REPORT_DIR/alp_g_report.json"
BASE="${THOTH_ENGINE_URL:-http://127.0.0.1:8090}"

CMD="${1:-gate}"
G0_STATUS="skip"
G1_STATUS="skip"
G2A_STATUS="skip"
G2B_STATUS="manual"
G3_STATUS="manual"

log() { printf '[alp-g] %s\n' "$*"; }
fail() { log "FAIL: $*"; exit 1; }
warn() { log "WARN: $*"; }

git_revision() {
  git -C "$ROOT" rev-parse HEAD 2>/dev/null || echo "unknown"
}

truthy_env() {
  case "${1:-}" in
    1|true|TRUE|yes|YES) return 0 ;;
    *) return 1 ;;
  esac
}

require_alp_engine_env() {
  truthy_env "${THOTH_ALP_ENABLED:-}" || fail "THOTH_ALP_ENABLED must be 1 for G1/G3"
  truthy_env "${THOTH_ALP_TX_INDEX:-}" || fail "THOTH_ALP_TX_INDEX must be 1 for G1/G3"
}

warn_alp_gui_env() {
  if ! truthy_env "${THOTH_ALP_GUI:-}"; then
    warn "THOTH_ALP_GUI is not 1 — GUI certification (G2b/G3) invalid"
  fi
}

write_report() {
  mkdir -p "$REPORT_DIR"
  local ts operator
  ts="$(date -u +"%Y-%m-%dT%H:%M:%SZ")"
  operator="${ALP_G_OPERATOR:-}"
  python3 - "$REPORT_PATH" "$ts" "$G0_STATUS" "$G1_STATUS" "$G2A_STATUS" "$G2B_STATUS" "$G3_STATUS" "$(git_revision)" "$operator" <<'PY'
import json, os, sys
path, ts, g0, g1, g2a, g2b, g3, git_rev, operator = sys.argv[1:10]
def env_bool(name):
    v = os.environ.get(name, "")
    return v in ("1", "true", "TRUE", "yes", "YES")
body = {
    "protocol": "ALP1",
    "phase": "ALP-G",
    "timestamp": ts,
    "flags": {
        "THOTH_ALP_ENABLED": env_bool("THOTH_ALP_ENABLED"),
        "THOTH_ALP_TX_INDEX": env_bool("THOTH_ALP_TX_INDEX"),
        "THOTH_ALP_GUI": env_bool("THOTH_ALP_GUI"),
        "THOTH_ALP_GREENFIELD": env_bool("THOTH_ALP_GREENFIELD"),
    },
    "tests": {
        "G0": g0,
        "G1": g1,
        "G2a": g2a,
        "G2b": g2b,
        "G3": g3,
    },
    "git_revision": git_rev,
    "operator": operator or None,
    "certification_note": (
        "Full ALP1 certification requires G2b and G3 manual operator sign-off."
    ),
}
with open(path, "w", encoding="utf-8") as out:
    json.dump(body, out, indent=2)
    out.write("\n")
print(path)
PY
  log "Wrote certification report: $REPORT_PATH"
}

preflight_forbidden() {
  if ! git -C "$ROOT" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
    warn "Not a git repo — skipping preflight diff guard"
    return 0
  fi
  local base files
  base="${ALP_G_PREFLIGHT_BASE:-}"
  if [ -z "$base" ]; then
    if git -C "$ROOT" rev-parse origin/main >/dev/null 2>&1; then
      base="origin/main"
    else
      base="HEAD~1"
    fi
  fi
  files="$(git -C "$ROOT" diff --name-only "$base"...HEAD 2>/dev/null \
    || git -C "$ROOT" diff --name-only "$base" HEAD 2>/dev/null \
    || true)"
  if [ -z "$files" ]; then
    log "preflight: no committed diff vs $base (OK)"
    return 0
  fi
  local forbidden_patterns=(
    'external/basic_agent/src/index_manager'
    'external/basic_agent/src/document_registry'
    'external/basic_agent/src/rag.cpp'
    'external/basic_agent/src/rag.h'
    'external/basic_agent/src/engine_http'
    'external/basic_agent/src/engine_runtime'
    'external/basic_agent/src/engine_sse'
    'external/basic_agent/src/basic_agent_plugin'
    'src/MainFrame'
    'docs/ATTACHMENT_LIFECYCLE_PROTOCOL.md'
  )
  local changed=0 f pat
  while IFS= read -r f; do
    [ -z "$f" ] && continue
    for pat in "${forbidden_patterns[@]}"; do
      if [[ "$f" == "$pat"* ]]; then
        log "Forbidden change in ALP-G diff: $f"
        changed=1
      fi
    done
  done <<<"$files"
  if [ "$changed" -ne 0 ]; then
    fail "ALP-G preflight: forbidden Engine/GUI/protocol changes in diff vs $base. Fix in responsible ALP phase, not G."
  fi
  log "preflight: no forbidden paths in diff vs $base"
}

maybe_preflight() {
  if truthy_env "${ALP_G_PREFLIGHT:-}"; then
    preflight_forbidden
  fi
}

run_gate() {
  log "G0 — build gate"
  cmake --build "$BUILD_DIR" --target thoth-core-tests thoth-control-panel

  log "G0 — thoth-core-tests"
  "$TESTS_BIN"

  log "G0 — G2a EGAR operator lifecycle"
  THOTH_ALP_EGAR_LIFECYCLE_ONLY=1 "$TESTS_BIN"

  G0_STATUS="pass"
  G2A_STATUS="pass"
}

engine_ready() {
  curl -sf -m 5 "${BASE}/ready" >/dev/null 2>&1
}

run_engine() {
  require_alp_engine_env
  if ! engine_ready; then
    G1_STATUS="skip"
    warn "Engine not ready at $BASE — G1 skipped (optional)"
    return 0
  fi

  log "G1 — Engine HTTP ALP smoke at $BASE"
  local ready_json
  ready_json="$(curl -sf -m 10 "${BASE}/ready")"
  echo "$ready_json" | grep -q '"ingest"' || fail "G1: /ready missing ingest capability"
  echo "$ready_json" | grep -q '"corpus"' || fail "G1: /ready missing corpus capability"

  local session_id content hash body resp http doc_id
  session_id="alp-g-http-$(date +%s)"
  content="# ALP-G HTTP smoke document with enough bytes for validation gate.\n"
  hash="$(printf '%s' "$content" | sha256sum | awk '{print $1}')"

  body="$(python3 - <<PY
import json
print(json.dumps({
    "name": "alp_g_smoke.md",
    "content": "# ALP-G HTTP smoke document with enough bytes for validation gate.\\n",
    "session_id": "$session_id",
    "content_hash": "$hash",
    "dry_run": True,
}))
PY
)"

  resp="$(curl -s -w '\n__HTTP__%{http_code}' -m 30 -X POST "${BASE}/v1/rag/documents" \
    -H 'Content-Type: application/json' -d "$body")"
  http="$(echo "$resp" | sed -n 's/^__HTTP__//p')"
  resp="$(echo "$resp" | sed '/^__HTTP__/d')"
  [ "$http" = "200" ] || fail "G1 dry_run HTTP $http: $resp"
  echo "$resp" | grep -q '"action"' || fail "G1 dry_run missing action"
  echo "$resp" | grep -q '"create"' || fail "G1 dry_run expected create action"

  body="$(python3 - <<PY
import json
print(json.dumps({
    "name": "alp_g_smoke.md",
    "content": "# ALP-G HTTP smoke document with enough bytes for validation gate.\\n",
    "session_id": "$session_id",
    "content_hash": "$hash",
    "local_source_mtime_sec": 1700000000,
}))
PY
)"
  resp="$(curl -s -w '\n__HTTP__%{http_code}' -m 30 -X POST "${BASE}/v1/rag/documents" \
    -H 'Content-Type: application/json' -d "$body")"
  http="$(echo "$resp" | sed -n 's/^__HTTP__//p')"
  resp="$(echo "$resp" | sed '/^__HTTP__/d')"
  [ "$http" = "200" ] || fail "G1 create HTTP $http: $resp"
  doc_id="$(python3 - <<PY
import json, sys
j=json.loads(sys.stdin.read())
print(j.get("document", {}).get("id", ""))
PY
<<<"$resp")"
  [ -n "$doc_id" ] || fail "G1 create missing document.id"

  local corpus
  corpus="$(curl -sf -m 15 "${BASE}/v1/rag/corpus")"
  echo "$corpus" | grep -q "$doc_id" || fail "G1 corpus list missing created document"
  echo "$corpus" | grep -q '_1.md' && fail "G1 corpus must not contain suffix documents"

  G1_STATUS="pass"
  log "G1 PASS document_id=$doc_id"
}

case "$CMD" in
  preflight)
    preflight_forbidden
    ;;
  gate)
    maybe_preflight
    run_gate
    write_report
    log "PASS: G0 + G2a (automated minimum)"
    ;;
  engine)
    maybe_preflight
    run_engine
    write_report
    log "G1 finished: status=$G1_STATUS"
    ;;
  all)
    maybe_preflight
    run_gate
    run_engine
    write_report
    log "PASS: automated gates (G1=${G1_STATUS})"
    ;;
  *)
    fail "Unknown subcommand: $CMD (use preflight|gate|engine|all)"
    ;;
esac
