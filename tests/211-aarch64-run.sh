#!/usr/bin/env bash
# every golden input and every sample, compiled with
# -target=aarch64, assembled with aarch64-linux-gnu-as, linked
# statically and run under qemu-aarch64. The exit code must match
# `// EXPECTED EXIT lp64: N` if present, else `// EXPECTED EXIT: N`;
# `// EXPECTED OUTPUT: text` checks stdout. A missing cross
# toolchain or qemu fails the test (need_tools in lib.sh).
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1
need_tools aarch64-linux-gnu-as aarch64-linux-gnu-gcc qemu-aarch64

tmp=$(mktemp -d /tmp/sam_a64_run.XXXXXX)
trap 'rm -rf "$tmp"' EXIT

failures=""
count=0
for src in tests/golden/inputs/*.c samples/*.c; do
    base=$(basename "$src" .c)
    # lib.sh runs with set -e and pipefail: a grep that matches nothing
    # or a failed build must not end the loop.
    expected=$(grep -oP 'EXPECTED EXIT lp64: \K[0-9]+' "$src" | head -1 || true)
    [ -n "$expected" ] || expected=$(grep -oP 'EXPECTED EXIT: \K[0-9]+' "$src" | head -1 || true)
    samcc -target=aarch64 "$src" "$tmp/$base" >"$tmp/$base.log" 2>&1 || true
    if [ ! -x "$tmp/$base" ]; then
        failures="$failures $base(build)"
        continue
    fi
    if [ "$count" -eq 0 ]; then
        file "$tmp/$base" | grep -qE 'ELF 64-bit LSB (pie )?executable, ARM aarch64' || fail "$base is not an AArch64 ELF: $(file "$tmp/$base")"
    fi
    out=$(timeout 20 qemu-aarch64 "$tmp/$base") && ec=0 || ec=$?
    [ "$ec" = "$expected" ] || failures="$failures $base(exit=$ec,want=$expected)"
    want_out=$(grep -oP 'EXPECTED OUTPUT: \K.*' "$src" | head -1 || true)
    if [ -n "$want_out" ] && [ "$out" != "$want_out" ]; then
        failures="$failures $base(output=[$out])"
    fi
    count=$((count + 1))
done

[ -z "$failures" ] || fail "aarch64 runs failed:$failures"
pass
