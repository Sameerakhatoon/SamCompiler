#!/usr/bin/env bash
# Build script. Clears stale .o files so header edits don't desync object files,
# then runs make. Mirrors the gotcha workflow.
set -euo pipefail

cd "$(dirname "$0")"

make clean >/dev/null 2>&1 || true
mkdir -p build/helpers build/static-includes
# Every test calls this twice, so the build dominates the suite.
# Measured on 10 tests: 31 s per test serial, 12.7 s with -j.
make -j"$(nproc)" all
