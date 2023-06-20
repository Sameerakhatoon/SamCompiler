#!/usr/bin/env bash
# every golden input and every sample, compiled with
# -target=riscv64, assembled with riscv64-linux-gnu-as, linked
# statically and run under qemu-riscv64. The exit code must match
# `// EXPECTED EXIT lp64: N` if present, else `// EXPECTED EXIT: N`;
# `// EXPECTED OUTPUT: text` checks stdout. Skips cleanly when the
# cross toolchain or qemu is missing.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1
for tool in riscv64-linux-gnu-as riscv64-linux-gnu-gcc qemu-riscv64; do
    command -v "$tool" >/dev/null || { echo "221-riscv64-run ... skip (no $tool)"; exit 0; }
done

tmp=$(mktemp -d /tmp/sam_rv_run.XXXXXX)
trap 'rm -rf "$tmp"' EXIT

failures=""
count=0
for src in tests/golden/inputs/*.c samples/*.c; do
    base=$(basename "$src" .c)
    # lib.sh runs with set -e and pipefail: a grep that matches nothing
    # or a failed build must not end the loop.
    expected=$(grep -oP 'EXPECTED EXIT lp64: \K[0-9]+' "$src" | head -1 || true)
    [ -n "$expected" ] || expected=$(grep -oP 'EXPECTED EXIT: \K[0-9]+' "$src" | head -1 || true)
    "$REPO_ROOT/main" -target=riscv64 "$src" "$tmp/$base" >"$tmp/$base.log" 2>&1 || true
    if [ ! -x "$tmp/$base" ]; then
        failures="$failures $base(build)"
        continue
    fi
    if [ "$count" -eq 0 ]; then
        file "$tmp/$base" | grep -q 'ELF 64-bit LSB executable, UCB RISC-V' || fail "$base is not a RISC-V ELF: $(file "$tmp/$base")"
    fi
    out=$(timeout 20 qemu-riscv64 "$tmp/$base") && ec=0 || ec=$?
    [ "$ec" = "$expected" ] || failures="$failures $base(exit=$ec,want=$expected)"
    want_out=$(grep -oP 'EXPECTED OUTPUT: \K.*' "$src" | head -1 || true)
    if [ -n "$want_out" ] && [ "$out" != "$want_out" ]; then
        failures="$failures $base(output=[$out])"
    fi
    count=$((count + 1))
done

[ -z "$failures" ] || fail "riscv64 runs failed:$failures"
pass
