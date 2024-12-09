#!/usr/bin/env bash
# structs with float or double members by value across the ABI
# boundary, on all four targets. tests/fpabi/gcc.c (built by gcc, has
# main) and tests/fpabi/sam.c (built by SamCompiler) call each other
# both ways with eighteen struct shapes: one to four floats or doubles,
# arrays and nested structs of them, a double or float next to an
# integer either way round, a union inside, and a 32-byte aggregate
#. Each shape is
# returned, passed, passed and returned, passed after seven doubles or
# six longs (the registers run out), and read with va_arg. Each side
# prints a FAIL line per wrong result; gcc.c's main prints "ok".
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

tmp=$(mktemp -d /tmp/sam_fpabi.XXXXXX)
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

    ./main -target=$target tests/fpabi/sam.c "$tmp/sam_$target.s" object >"$tmp/log" 2>&1 \
        || fail "$target: sam.c did not compile: $(tail -2 "$tmp/log")"
    $cc -c tests/fpabi/gcc.c -o "$tmp/gcc_$target.o" || fail "$target: gcc rejected gcc.c"
    $cc "$tmp/gcc_$target.o" "$tmp/sam_$target.s.o" -o "$tmp/interop_$target" 2>"$tmp/log" \
        || fail "$target: link failed: $(tail -2 "$tmp/log")"
    out=$($run "$tmp/interop_$target" 2>&1)
    assert_eq "ok" "$out" "$target: gcc.c <-> sam.c"
done

pass
