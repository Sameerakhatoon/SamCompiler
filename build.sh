#!/usr/bin/env bash
# Build script. Clears stale .o files so header edits don't desync object files,
# then runs make. Mirrors the gotcha workflow from G01.
set -euo pipefail

cd "$(dirname "$0")"

make clean >/dev/null 2>&1 || true
mkdir -p build/helpers
make all
