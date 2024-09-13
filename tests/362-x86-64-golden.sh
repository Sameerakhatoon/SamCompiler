#!/usr/bin/env bash
# Byte-for-byte golden output was locked for x86
# only. Lock the x86_64 asm too: every sample and golden input compiled
# with -target=x86_64 must match tests/golden/x86_64 exactly. Regenerate with
# `bash tests/golden/regen.sh x86_64` only when the change is intended.
. "$(dirname "$0")/lib.sh"
. tests/golden/check.sh

./build.sh >/dev/null 2>&1
golden_check x86_64
pass
