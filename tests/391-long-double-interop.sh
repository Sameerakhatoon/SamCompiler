#!/usr/bin/env bash
# long double across the ABI boundary on all four targets: the
# x87 80-bit format on x86 (12 bytes) and x86_64 (16), IEEE binary128
# on aarch64 and riscv64 through the libgcc soft-float routines.
# tests/ldabi/gcc.c (built by gcc, has main) and tests/ldabi/sam.c
# (built by SamCompiler) call each other both ways: long double
# arguments and results, more of them than there are registers, mixed
# with ints, doubles and floats, one split between a7 and the stack on
# riscv64, variadic callees reading them with va_arg, structs of one
# and two long doubles and of a long double and an int by value,
# globals of each side, and printf("%Lf") checked through sprintf. Each
# side prints a FAIL line per wrong result; gcc.c's main prints "ok".
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

tmp=$(mktemp -d /tmp/sam_ldabi.XXXXXX)
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

    ./main -target=$target tests/ldabi/sam.c "$tmp/sam_$target.s" object >"$tmp/log" 2>&1 \
        || fail "$target: sam.c did not compile: $(tail -2 "$tmp/log")"
    $cc -c tests/ldabi/gcc.c -o "$tmp/gcc_$target.o" || fail "$target: gcc rejected gcc.c"
    $cc "$tmp/gcc_$target.o" "$tmp/sam_$target.s.o" -o "$tmp/interop_$target" 2>"$tmp/log" \
        || fail "$target: link failed: $(tail -2 "$tmp/log")"
    out=$($run "$tmp/interop_$target" 2>&1)
    assert_eq "ok" "$out" "$target: gcc.c <-> sam.c"
done

pass
