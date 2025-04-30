#!/usr/bin/env bash
# A21: float and double across the ABI boundary, on all four targets.
# tests/float/gcc.c (built by gcc, has main) and tests/float/sam.c
# (built by SamCompiler) call each other both ways: float and double
# arguments and results, more of them than there are floating-point
# argument registers, mixed with integer ones, and a variadic gcc
# callee reading doubles (float arguments promoted). sam.c also checks
# printf("%f") output with sprintf. The exit status is a bit mask of
# the failed checks (see gcc.c).
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

tmp=$(mktemp -d /tmp/sam_float.XXXXXX)
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

    # object mode: the asm goes to the named file, the object next to it.
    samcc -target=$target tests/float/sam.c "$tmp/sam_$target.s" object >"$tmp/log" 2>&1 \
        || fail "$target: sam.c did not compile: $(tail -2 "$tmp/log")"
    $cc -c tests/float/gcc.c -o "$tmp/gcc_$target.o" || fail "$target: gcc rejected gcc.c"
    $cc "$tmp/gcc_$target.o" "$tmp/sam_$target.s.o" -o "$tmp/interop_$target" 2>"$tmp/log" \
        || fail "$target: link failed: $(tail -2 "$tmp/log")"
    rc=0
    out=$($run "$tmp/interop_$target") || rc=$?
    assert_eq 0 "$rc" "$target: failed check bits of gcc.c <-> sam.c"
    assert_eq "1 3.25 2 2.5 0.3 1.234500e+03 1.000 2.000 3.000 4.000 5.000 6.000 7.000 8.000 9" "$out" "$target: printf output"
done

pass
