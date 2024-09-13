#!/usr/bin/env bash
# Byte-for-byte golden output was locked for x86
# only. Lock the aarch64 asm too: every sample and golden input compiled
# with -target=aarch64 must match tests/golden/aarch64 exactly. Regenerate with
# `bash tests/golden/regen.sh aarch64` only when the change is intended.
. "$(dirname "$0")/lib.sh"
. tests/golden/check.sh

./build.sh >/dev/null 2>&1
golden_check aarch64
pass
