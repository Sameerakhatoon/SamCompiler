#!/usr/bin/env bash
# every golden input and every sample, compiled with
# -target=x86_64, assembled with nasm -f elf64, linked with
# gcc -no-pie and run natively. The exit code must match the
# `// EXPECTED EXIT lp64: N` line if there is one, else
# `// EXPECTED EXIT: N`. `// EXPECTED OUTPUT: text` checks stdout.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1
command -v nasm >/dev/null || { echo "201-x86-64-run ... skip (no nasm)"; exit 0; }

tmp=$(mktemp -d /tmp/sam_x64_run.XXXXXX)
trap 'rm -rf "$tmp"' EXIT

failures=""
count=0
for src in tests/golden/inputs/*.c samples/*.c; do
    base=$(basename "$src" .c)
    # lib.sh runs with set -e and pipefail: a grep that matches nothing
    # or a failed build must not end the loop.
    expected=$(grep -oP 'EXPECTED EXIT lp64: \K[0-9]+' "$src" | head -1 || true)
    [ -n "$expected" ] || expected=$(grep -oP 'EXPECTED EXIT: \K[0-9]+' "$src" | head -1 || true)
    "$REPO_ROOT/main" -target=x86_64 "$src" "$tmp/$base" >"$tmp/$base.log" 2>&1 || true
    if [ ! -x "$tmp/$base" ]; then
        failures="$failures $base(build)"
        continue
    fi
    if [ "$count" -eq 0 ]; then
        file "$tmp/$base" | grep -q 'ELF 64-bit LSB executable, x86-64' || fail "$base is not an x86-64 ELF: $(file "$tmp/$base")"
    fi
    # set -e would end the test on the first non-zero exit code
    out=$("$tmp/$base") && ec=0 || ec=$?
    [ "$ec" = "$expected" ] || failures="$failures $base(exit=$ec,want=$expected)"
    want_out=$(grep -oP 'EXPECTED OUTPUT: \K.*' "$src" | head -1 || true)
    if [ -n "$want_out" ] && [ "$out" != "$want_out" ]; then
        failures="$failures $base(output=[$out])"
    fi
    count=$((count + 1))
done

[ -z "$failures" ] || fail "x86_64 runs failed:$failures"
pass
