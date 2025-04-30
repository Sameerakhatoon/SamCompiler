#!/usr/bin/env bash
# va_arg of a struct made of long doubles, against gcc on all
# four targets. tests/ldabi/va_sam.c is built by SamCompiler and,
# renamed sam_ -> gcc_, by gcc; tests/ldabi/va_gcc.c (gcc) calls both
# with structs of one to four long doubles (homogeneous aggregates in q
# registers on aarch64, in memory on x86_64, by the integer rules on
# riscv64), more of them than registers, mixed with doubles, ints and
# long doubles, and a struct of a long double and an int.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

tmp=$(mktemp -d /tmp/sam_ldva.XXXXXX)
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

    samcc -target=$target tests/ldabi/va_sam.c "$tmp/sam_$target.s" object >"$tmp/log" 2>&1 \
        || fail "$target: va_sam.c did not compile: $(tail -2 "$tmp/log")"
    sed 's/sam_/gcc_/g' tests/ldabi/va_sam.c > "$tmp/va_ref.c"
    $cc -I tests/ldabi -c "$tmp/va_ref.c" -o "$tmp/ref_$target.o" || fail "$target: gcc rejected va_sam.c"
    $cc tests/ldabi/va_gcc.c "$tmp/ref_$target.o" "$tmp/sam_$target.s.o" -o "$tmp/ldva_$target" 2>"$tmp/log" \
        || fail "$target: link failed: $(tail -2 "$tmp/log")"
    out=$($run "$tmp/ldva_$target" 2>&1)
    assert_eq "ok" "$out" "$target: va_arg of structs of long doubles"
done

pass
