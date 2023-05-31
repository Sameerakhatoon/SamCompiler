#!/usr/bin/env bash
# x86_64 asm shape, one check per feature. Only the compiler
# runs here; 201-x86-64-run.sh assembles, links and runs.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

tmp=$(mktemp -d /tmp/sam_x64_asm.XXXXXX)
trap 'rm -rf "$tmp"' EXIT

asm_for(){
    "$REPO_ROOT/main" -target=x86_64 "$1" "$tmp/out.asm" object >"$tmp/log" 2>&1 || true
    grep -q "everything compiled fine" "$tmp/log" || fail "$1 did not compile: $(tail -2 "$tmp/log")"
    cat "$tmp/out.asm"
}

out=$(asm_for tests/golden/inputs/28_call_args.c)
assert_contains "$out" "default rel" "rip-relative globals"
# prologue: home area for the six argument registers, then the frame
assert_contains "$out" $'f:\npop r11\npush r9\npush r8\npush rcx\npush rdx\npush rsi\npush rdi\npush r11\npush rbp\nmov rbp, rsp' "prologue spills rdi..r9"
assert_contains "$out" "ret 48" "epilogue drops the home area"
# parameters read from the home area at rbp+16+8*i, int loads sign extend
assert_contains "$out" "movsxd rax, dword [rbp+16]" "first parameter"
assert_contains "$out" "movsxd rax, dword [rbp+24]" "second parameter"
assert_contains "$out" "movsxd rax, dword [rbp+32]" "third parameter"
# call: arguments popped into registers in order, callee in r11
assert_contains "$out" $'pop rdi\npop rsi\npop rdx\npop r11' "arguments into rdi, rsi, rdx"
assert_contains "$out" $'xor eax, eax\ncall r11' "al cleared before every call"
# int arithmetic on 32-bit registers, then back to 64-bit form
assert_contains "$out" $'imul eax, ecx\nmovsxd rax, eax' "32-bit int multiply"

out=$(asm_for tests/golden/inputs/38_long.c)
assert_contains "$out" "mov qword [rbp-8], rax" "long stored as 8 bytes"
assert_contains "$out" "sub rax, rcx" "long arithmetic on 64-bit registers"

out=$(asm_for tests/golden/inputs/32_char_short.c)
assert_contains "$out" "movsx rax, byte [rbp-" "signed char load"
assert_contains "$out" "movsx rax, word [rbp-" "short load"

out=$(asm_for tests/golden/inputs/27_string.c)
assert_contains "$out" "lea rax, [str_" "string address with lea"

out=$(asm_for tests/golden/inputs/25_struct_return.c)
assert_contains "$out" $'pop r11\npop rdi' "hidden struct pointer in rdi"
assert_contains "$out" "mov rdx, [rbp+16]" "callee reads the hidden pointer"
assert_contains "$out" "movsxd rax, dword [rbp+24]" "visible arguments shift one slot"

out=$(asm_for tests/golden/inputs/31_varargs.c)
assert_contains "$out" "mov qword [rbp-8], r11" "va_start stores an 8-byte pointer"
assert_contains "$out" "add qword [r11], 8" "va_arg steps one 8-byte slot"

out=$(asm_for tests/golden/inputs/39_pointer_size.c)
# sizeof is folded per operand at parse time: 8 for int* and for long
assert_contains "$out" $'push qword 8\npush qword 8' "sizeof(int*) and sizeof(long) are 8"
pass
