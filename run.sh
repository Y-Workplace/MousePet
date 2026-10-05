#!/usr/bin/env bash
set -euo pipefail

DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
BIN="$DIR/build/mousepet"

if [[ ! -f "$BIN" ]]; then
    echo "[mousepet] Binary not found. Building with make..."
    make -C "$DIR" -j"$(nproc)"
fi

LOADER="/lib64/ld-linux-x86-64.so.2"
if [[ -f "$LOADER" ]]; then
    exec "$LOADER" "$BIN" "$@"
else
    exec "$BIN" "$@"
fi
