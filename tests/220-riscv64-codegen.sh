#!/usr/bin/env bash
# RISC-V asm shape, one check per feature. Only the compiler
# runs here; 221-riscv64-run.sh assembles, links and runs under
# qemu-riscv64. The expressions come from exp64.c (address in
# a0, then the access).
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

tmp=$(mktemp -d /tmp/sam_rv64_asm.XXXXXX)
trap 'rm -rf "$tmp"' EXIT

asm_for(){
    "$REPO_ROOT/main" -target=riscv64 "$1" "$tmp/out.s" object >"$tmp/log" 2>&1 || true
    grep -q "everything compiled fine" "$tmp/log" || fail "$1 did not compile: $(tail -2 "$tmp/log")"
    cat "$tmp/out.s"
}

out=$(asm_for tests/golden/inputs/28_call_args.c)
assert_contains "$out" ".type main, @function" "function symbol type"
# prologue: home area, frame record, a0-a7 spilled above the record
assert_contains "$out" $'f:\naddi sp, sp, -64\naddi sp, sp, -16\nsd ra, 8(sp)\nsd s0, 0(sp)\nmv s0, sp\nsd a0, 16(s0)\nsd a1, 24(s0)' "prologue"
assert_contains "$out" "sd a7, 72(s0)" "last argument register spilled"
assert_contains "$out" $'mv sp, s0\nld ra, 8(sp)\nld s0, 0(sp)\naddi sp, sp, 16\naddi sp, sp, 64\nret' "epilogue"
# expression stack: 16-byte slots
assert_contains "$out" $'addi sp, sp, -16\nsd a0, 0(sp)' "push"
assert_contains "$out" $'ld a0, 0(sp)\naddi sp, sp, 16' "pop"
# parameters from the home area, int loads sign extend
assert_contains "$out" $'addi a0, s0, 16\nlw a0, 0(a0)' "first parameter"
assert_contains "$out" $'addi a0, s0, 32\nlw a0, 0(a0)' "third parameter"
assert_contains "$out" $'lla t0, f\njalr t0' "jalr through t0"
# int arithmetic with the *w forms
assert_contains "$out" "mulw a0, a0, a1" "32-bit multiply"

out=$(asm_for tests/golden/inputs/03_div_mod.c)
assert_contains "$out" "divw a0, a0, a1" "signed divide"
assert_contains "$out" "remw a0, a0, a1" "signed remainder"

out=$(asm_for tests/golden/inputs/04_compare.c)
# an if on a comparison: a short branch over a `j` to the false label
assert_contains "$out" $'blt a0, a1, .Ltrue_1\nj .Lif_' "if(a < b)"
assert_contains "$out" "blt a1, a0, .Ltrue_" "if(a > b) (operands swapped)"

out=$(asm_for tests/golden/inputs/05_logical.c)
assert_contains "$out" "bnez a0, .Lexp_" "|| short circuit"

out=$(asm_for tests/golden/inputs/14_switch.c)
assert_contains "$out" $'li a1, 1\nbeq a0, a1, .Lswitch_stmt_' "switch compare chain"

out=$(asm_for tests/golden/inputs/21_global_array.c)
assert_contains "$out" "lla a0, g" "global address with lla"
assert_contains "$out" "g: .zero 16" "global array"

out=$(asm_for tests/golden/inputs/32_char_short.c)
# Plain char is unsigned in the RISC-V psABI; it was loaded signed.
assert_contains "$out" "lbu a0, 0(a0)" "plain char load"
assert_contains "$out" "lh a0, 0(a0)" "short load"
assert_contains "$out" "sb a0, 0(a1)" "char store"

out=$(asm_for tests/golden/inputs/38_long.c)
assert_contains "$out" "sd a0, 0(a1)" "long store"
assert_contains "$out" "sub a0, a0, a1" "long arithmetic, full width"

out=$(asm_for tests/golden/inputs/27_string.c)
assert_contains "$out" '.asciz "hi"' "string literal"

out=$(asm_for tests/golden/inputs/25_struct_return.c)
assert_contains "$out" $'addi a0, sp, 0\njalr t0' "struct return pointer in a0"
assert_contains "$out" "ld a0, 16(s0)" "callee reads the result pointer"

out=$(asm_for tests/golden/inputs/31_varargs.c)
assert_contains "$out" $'addi a0, s0, 16\naddi sp, sp, -16\nsd a0, 0(sp)\naddi a0, s0, -8\nmv a1, a0\nld a0, 0(sp)\naddi sp, sp, 16\nsd a0, 0(a1)' "va_start stores an 8-byte pointer"
assert_contains "$out" $'ld a0, 0(a0)\naddi a0, a0, 8\nld a1, 0(sp)\naddi sp, sp, 16\nsd a0, 0(a1)\nld a0, 0(a0)' "va_arg steps one 8-byte slot"
pass
