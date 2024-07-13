#!/usr/bin/env bash
# the constructs SamOS's userland needed, one case per fix in
# tests/st2/*.c. Each is compiled for x86_64, aarch64 and riscv64 (the
# cross targets when their tools are installed), run (natively or
# under qemu-<arch>), and its stdout compared with its `// EXPECT:`
# line. The i386 backend is not covered: the samos targets are 64-bit.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

tmp=$(mktemp -d /tmp/sam_st2.XXXXXX)
trap 'rm -rf "$tmp"' EXIT

targets="x86_64"
command -v aarch64-linux-gnu-gcc >/dev/null && command -v qemu-aarch64 >/dev/null && targets="$targets aarch64"
command -v riscv64-linux-gnu-gcc >/dev/null && command -v qemu-riscv64 >/dev/null && targets="$targets riscv64"

failures=""
count=0
for src in tests/st2/*.c; do
    base=$(basename "$src" .c)
    want=$(grep -oP '^// EXPECT: \K.*' "$src" | head -1 || true)
    [ -n "$want" ] || fail "$src has no // EXPECT: line"
    for t in $targets; do
        bin="$tmp/$base.$t"
        "$REPO_ROOT/main" -target=$t "$src" "$bin" >"$bin.log" 2>&1 || true
        if [ ! -x "$bin" ]; then
            failures="$failures $base/$t(build: $(grep -v '^\(push\|pop\|mov\)' "$bin.log" | tail -1))"
            continue
        fi
        case $t in x86_64) run="";; *) run="qemu-$t";; esac
        out=$(timeout 20 $run "$bin" 2>&1) && ec=0 || ec=$?
        [ "$ec" = 0 ] || failures="$failures $base/$t(exit=$ec)"
        [ "$out" = "$want" ] || failures="$failures $base/$t(output=[$out])"
        count=$((count + 1))
    done
done

[ -z "$failures" ] || fail "$failures"
[ "$count" -ge 3 ] || fail "only $count cases ran"
pass
