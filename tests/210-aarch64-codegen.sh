#!/usr/bin/env bash
# AArch64 asm shape, one check per feature. Only the compiler
# runs here; 211-aarch64-run.sh assembles, links and runs under
# qemu-aarch64.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

tmp=$(mktemp -d /tmp/sam_a64_asm.XXXXXX)
trap 'rm -rf "$tmp"' EXIT

asm_for(){
    "$REPO_ROOT/main" -target=aarch64 "$1" "$tmp/out.s" object >"$tmp/log" 2>&1 || true
    grep -q "everything compiled fine" "$tmp/log" || fail "$1 did not compile: $(tail -2 "$tmp/log")"
    cat "$tmp/out.s"
}

out=$(asm_for tests/golden/inputs/28_call_args.c)
# sections and symbols in GNU as syntax
assert_contains "$out" $'.text\n.p2align 2' "text section"
assert_contains "$out" ".globl main" "global symbol"
# prologue: home area, frame record, spill x0-x7
assert_contains "$out" $'f:\nsub sp, sp, #64\nstp x29, x30, [sp, #-16]!\nmov x29, sp\nstp x0, x1, [x29, #16]\nstp x2, x3, [x29, #32]\nstp x4, x5, [x29, #48]\nstp x6, x7, [x29, #64]' "prologue"
assert_contains "$out" $'ldp x29, x30, [sp], #16\nadd sp, sp, #64\nret' "epilogue"
# 16-byte expression slots
assert_contains "$out" "str x0, [sp, #-16]!" "push"
assert_contains "$out" "ldr x0, [sp], #16" "pop"
# parameters from the home area, sign extended
assert_contains "$out" "ldursw x0, [x29, #16]" "first parameter"
assert_contains "$out" "ldursw x0, [x29, #32]" "third parameter"
# call through x9 with arguments in x0..x2
assert_contains "$out" $'ldr x0, [sp], #16\nldr x1, [sp], #16\nldr x2, [sp], #16\nldr x9, [sp], #16\nblr x9' "arguments into x0-x2, blr"
# int arithmetic on w registers
assert_contains "$out" $'mul w0, w0, w1\nsxtw x0, w0' "32-bit multiply"

out=$(asm_for tests/golden/inputs/03_div_mod.c)
assert_contains "$out" "sdiv w0, w0, w1" "signed divide"
assert_contains "$out" $'sdiv w2, w0, w1\nmsub w0, w2, w1, w0' "remainder with msub"

out=$(asm_for tests/golden/inputs/04_compare.c)
assert_contains "$out" $'cmp w0, w1\ncset w0, lt' "compare to 0/1"
assert_contains "$out" "cbz x0, .Lif_" "if with cbz"

out=$(asm_for tests/golden/inputs/05_logical.c)
assert_contains "$out" "cbnz x0, .Lendc_" "|| short circuit"

out=$(asm_for tests/golden/inputs/14_switch.c)
assert_contains "$out" $'cmp w0, w1\nb.eq .Lswitch_stmt_' "switch compare chain"

out=$(asm_for tests/golden/inputs/21_global_array.c)
# g[1]: the constant index is folded into the symbol offset
assert_contains "$out" $'adrp x11, g+4\nadd x11, x11, :lo12:g+4' "global address with adrp"
assert_contains "$out" "g: .zero 16" "global array"

out=$(asm_for tests/golden/inputs/32_char_short.c)
assert_contains "$out" "ldursb x0, [x29, #-" "signed char load"
assert_contains "$out" "ldursh x0, [x29, #-" "short load"
assert_contains "$out" "sturb w0, [x29, #-" "char store"

out=$(asm_for tests/golden/inputs/38_long.c)
assert_contains "$out" "stur x0, [x29, #-8]" "long store"
assert_contains "$out" "sub x0, x0, x1" "long arithmetic on x registers"

out=$(asm_for tests/golden/inputs/27_string.c)
assert_contains "$out" 'str_1: .asciz "hi"' "string literal"

out=$(asm_for tests/golden/inputs/25_struct_return.c)
assert_contains "$out" $'ldr x9, [sp], #16\nldr x8, [sp], #16\nblr x9' "struct return pointer in x8"
assert_contains "$out" "str x8, [x29, #16]" "callee spills x8 first"
assert_contains "$out" "ldr x10, [x29, #16]" "callee reads the result pointer"

out=$(asm_for tests/golden/inputs/31_varargs.c)
assert_contains "$out" "stur x9, [x29, #-8]" "va_start stores an 8-byte pointer"
assert_contains "$out" $'ldr x11, [x9]\nadd x11, x11, #8\nstr x11, [x9]' "va_arg steps one 8-byte slot"
pass
