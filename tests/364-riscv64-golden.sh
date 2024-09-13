#!/usr/bin/env bash
# Byte-for-byte golden output was locked for x86
# only. Lock the riscv64 asm too: every sample and golden input compiled
# with -target=riscv64 must match tests/golden/riscv64 exactly. Regenerate with
# `bash tests/golden/regen.sh riscv64` only when the change is intended.
. "$(dirname "$0")/lib.sh"
. tests/golden/check.sh

./build.sh >/dev/null 2>&1
golden_check riscv64
pass
