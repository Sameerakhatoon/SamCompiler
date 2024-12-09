#!/usr/bin/env bash
# A21: float and double. tests/diff/float_*.c
# check the arithmetic against gcc on every target (test 250); here:
#   - printf of floats and doubles and sqrt/sqrtf from libm, by their
#     output, built by SamCompiler and linked with -lm on all four
#     targets, against the output of the same program built by gcc;
#   - what is rejected instead of miscompiled: % on a double or a long
#     double, va_arg of a float (it was promoted to double), a global
#     long double initialized with long double arithmetic (
#     long double itself works since then, tests 391 and diff/long_double_*);
#   - va_arg of a double on all four targets.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

tmp=$(mktemp -d /tmp/sam_fp.XXXXXX)
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

    $cc -w tests/float/print.c -o "$tmp/ref_$target" -lm || fail "$target: gcc rejected print.c"
    want=$($run "$tmp/ref_$target")
    ./main -target=$target tests/float/print.c "$tmp/print_$target.s" object >"$tmp/log" 2>&1 \
        || fail "$target: print.c did not compile: $(tail -2 "$tmp/log")"
    $cc "$tmp/print_$target.s.o" -o "$tmp/print_$target" -lm 2>"$tmp/log" \
        || fail "$target: link failed: $(tail -2 "$tmp/log")"
    got=$($run "$tmp/print_$target")
    assert_eq "$want" "$got" "$target: printf/sqrt output"
done

reject(){
    local target="$1" what="$2" src="$3"
    printf '%s\n' "$src" > "$tmp/bad.c"
    if ./main -target=$target "$tmp/bad.c" "$tmp/bad" >"$tmp/log" 2>&1; then
        fail "$target: $what was accepted"
    fi
    grep -q "$4" "$tmp/log" || fail "$target: $what: unexpected message: $(grep -v '^\s' "$tmp/log" | tail -2)"
}

for target in x86 x86_64 aarch64 riscv64; do
    reject $target "1.0L / 3 in a global" 'long double g = 1.0L / 3; int main(void){ return 0; }' "must be a constant expression"
    reject $target "% on a long double" 'int main(void){ long double x = 5; return x % 2; }' "does not take"
    reject $target "% on a double" 'int main(void){ double x = 5; return x % 2; }' "does not take floating operands"
done
vasrc='#include <stdarg.h>
double sum(int n, ...){ va_list ap; double s = 0; va_start(ap, n); for(int i = 0; i < n; i++) s += va_arg(ap, double); va_end(ap); return s; }
int main(void){ return sum(3, 1.5, 2.25, 0.25) == 4.0 ? 7 : 1; }'
fsrc='#include <stdarg.h>
float first(int n, ...){ va_list ap; va_start(ap, n); float f = va_arg(ap, float); va_end(ap); return f; }
int main(void){ return first(1, 1.5f) == 1.5f; }'

printf '%s\n' "$vasrc" > "$tmp/va.c"
for target in x86 x86_64 aarch64 riscv64; do
    # A float passed to `...` is a double: there is no float to read.
    reject $target "va_arg(ap, float)" "$fsrc" "va_arg of a float"
    case $target in
        aarch64) run="qemu-aarch64" ;;
        riscv64) run="qemu-riscv64" ;;
        *)       run="" ;;
    esac
    ./main -target=$target "$tmp/va.c" "$tmp/va_$target" >"$tmp/log" 2>&1 \
        || fail "$target: va_arg(ap, double) did not build: $(tail -2 "$tmp/log")"
    rc=0
    $run "$tmp/va_$target" || rc=$?
    assert_eq 7 "$rc" "$target: va_arg(ap, double)"
done

pass
