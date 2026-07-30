#!/usr/bin/env bash
# ALP-E / ALP-G — EGAR.md operator lifecycle verification (G2a).
# Prefer: ./scripts/alp_g_verify.sh gate
# Proves the full chain that caused weeks of pain (not piece unit tests).
#
# Usage (from repo root):
#   chmod +x scripts/alp_egar_lifecycle_verify.sh
#   ./scripts/alp_egar_lifecycle_verify.sh
#
# Requires: release build at build/release/tests/thoth-core-tests
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

BIN="${THOTH_CORE_TESTS_BIN:-$ROOT/build/release/tests/thoth-core-tests}"
if [ ! -x "$BIN" ]; then
  echo "[egar-lifecycle] building thoth-core-tests…" >&2
  cmake --build build/release --target thoth-core-tests
fi

log() { printf '[egar-lifecycle] %s\n' "$*"; }

log "Running isolated EGAR operator lifecycle (temp workspace + ALP flags)…"
log "Scenario: import → send → delete local note → reconcile → re-import newer → send"
log "Checks: one UUID, new revision, rev1 superseded, no EGAR_1.md, picker correct"

export THOTH_ALP_EGAR_LIFECYCLE_ONLY=1
if "$BIN"; then
  log "PASS: EGAR operator lifecycle verified"
  exit 0
fi

log "FAIL: see stderr above"
exit 1
