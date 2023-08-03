#!/usr/bin/env bash
# Build script. The Makefile tracks header dependencies (-MMD), so a plain
# incremental make is safe after a header edit. Pass "clean" to force
# a rebuild from scratch.
set -euo pipefail

cd "$(dirname "$0")"

if [ "${1:-}" = clean ]; then
    make clean >/dev/null 2>&1 || true
fi
mkdir -p build/helpers build/static-includes
make -j"$(nproc)" all
