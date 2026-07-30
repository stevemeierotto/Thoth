#!/usr/bin/env bash
# Launch thoth-control-panel with variables from repo-root .env (GUI + optional overrides).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

if [[ -f "${ROOT}/.env" ]]; then
  set -a
  # shellcheck disable=SC1091
  source "${ROOT}/.env"
  set +a
fi

if [[ -n "${THOTH_GUI_BIN:-}" ]]; then
  BIN="${THOTH_GUI_BIN}"
elif [[ -x "${ROOT}/build/debug/thoth-control-panel" ]]; then
  BIN="${ROOT}/build/debug/thoth-control-panel"
elif [[ -x "${ROOT}/build/release/thoth-control-panel" ]]; then
  BIN="${ROOT}/build/release/thoth-control-panel"
elif [[ -x "${ROOT}/build/thoth-control-panel" ]]; then
  BIN="${ROOT}/build/thoth-control-panel"
else
  echo "thoth-gui.sh: thoth-control-panel not found (build with cmake first)" >&2
  exit 1
fi

exec "${BIN}" "$@"
