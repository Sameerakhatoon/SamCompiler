#!/usr/bin/env bash
# every golden input and every sample, compiled for x86 (the
# default target), assembled with nasm -f elf32, linked by the driver
# (i686-linux-gnu-gcc -static, or gcc -m32) and run natively. The
# exit code must match `// EXPECTED EXIT: N`; `// EXPECTED OUTPUT:
# text` checks stdout. Since the struct-return fix no input is
# excluded.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1
need_tools nasm
need_x86_link

tmp=$(mktemp -d /tmp/sam_x86_run.XXXXXX)
trap 'rm -rf "$tmp"' EXIT

failures=""
count=0
for src in tests/golden/inputs/*.c samples/*.c; do
    base=$(basename "$src" .c)
    # lib.sh runs with set -e and pipefail: a grep that matches nothing
    # or a failed build must not end the loop.
    expected=$(grep -oP 'EXPECTED EXIT: \K[0-9]+' "$src" | head -1 || true)
    "$REPO_ROOT/main" "$src" "$tmp/$base" >"$tmp/$base.log" 2>&1 || true
    if [ ! -x "$tmp/$base" ]; then
        failures="$failures $base(build)"
        continue
    fi
    if [ "$count" -eq 0 ]; then
        file "$tmp/$base" | grep -Eq 'ELF 32-bit LSB (pie )?executable, Intel 80386' || fail "$base is not an i386 ELF: $(file "$tmp/$base")"
    fi
    out=$("$tmp/$base") && ec=0 || ec=$?
    [ "$ec" = "$expected" ] || failures="$failures $base(exit=$ec,want=$expected)"
    want_out=$(grep -oP 'EXPECTED OUTPUT: \K.*' "$src" | head -1 || true)
    if [ -n "$want_out" ] && [ "$out" != "$want_out" ]; then
        failures="$failures $base(output=[$out])"
    fi
    count=$((count + 1))
done

[ -z "$failures" ] || fail "x86 runs failed:$failures"
pass
