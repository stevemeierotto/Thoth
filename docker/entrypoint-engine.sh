#!/bin/sh
# Plan I — ensure workspace/log dirs exist and are writable by thoth (uid 1000).
set -e

mkdir -p /workspace /logs

if [ "$(id -u)" = "0" ]; then
    chown -R thoth:thoth /workspace /logs
    # Preserve all CLI args for thoth-engine (e.g. --version, --serve flags).
    exec runuser -u thoth -- /usr/local/bin/thoth-engine "$@"
fi

exec /usr/local/bin/thoth-engine "$@"
