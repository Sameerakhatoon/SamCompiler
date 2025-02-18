#!/usr/bin/env bash
# long double arithmetic in a global's initializer is folded
# at compile time in the target's format, rounded as gcc rounds it: the
# x87 80-bit format on x86 and x86_64, IEEE binary128 on aarch64 and
# riscv64. tests/ldinit/table.c (some fifty constant expressions:
# + - * / with long double, double and integer operands, signed zeros,
# overflow to infinity, subnormal results, ties, operands far apart) is
# built by SamCompiler and, renamed, by gcc; tests/ldinit/main.c
# compares the bytes and prints "ok".
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

tmp=$(mktemp -d /tmp/sam_ldinit.XXXXXX)
trap 'rm -rf "$tmp"' EXIT

for target in x86 x86_64 aarch64 riscv64; do
    case $target in
        x86)     cc="i686-linux-gnu-gcc -static";    run="" ;;
        x86_64)  cc="gcc -no-pie";                   run="" ;;
        aarch64) cc="aarch64-linux-gnu-gcc -static"; run="qemu-aarch64" ;;
        riscv64) cc="riscv64-linux-gnu-gcc -static"; run="qemu-riscv64" ;;
    esac
    command -v "${cc%% *}" >/dev/null || fail "$target: ${cc%% *} is missing"
    [ -z "$run" ] || command -v "$run" >/dev/null || fail "$target: $run is missing"

    ./main -target=$target tests/ldinit/table.c "$tmp/sam_$target.s" object >"$tmp/log" 2>&1 \
        || fail "$target: table.c did not compile: $(tail -2 "$tmp/log")"
    $cc -w -Dtable=gcc_table -Dtable_count=gcc_table_count -c tests/ldinit/table.c -o "$tmp/gcc_$target.o" \
        || fail "$target: gcc rejected table.c"
    $cc tests/ldinit/main.c "$tmp/gcc_$target.o" "$tmp/sam_$target.s.o" -o "$tmp/ldinit_$target" 2>"$tmp/log" \
        || fail "$target: link failed: $(tail -2 "$tmp/log")"
    out=$($run "$tmp/ldinit_$target" 2>&1)
    assert_eq "ok" "$out" "$target: table.c folded as gcc folds it"
done

pass
