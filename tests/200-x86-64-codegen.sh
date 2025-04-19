#!/usr/bin/env bash
# x86_64 asm shape, one check per feature. Only the compiler
# runs here; 201-x86-64-run.sh assembles, links and runs. The
# expressions come from exp64.c (address in rax, then the access).
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
assert_contains "$out" $'$f:\npop r11\npush r9\npush r8\npush rcx\npush rdx\npush rsi\npush rdi\npush r11\npush rbp\nmov rbp, rsp' "prologue spills rdi..r9"
assert_contains "$out" "ret 48" "epilogue drops the home area"
# parameters read from the home area at rbp+16+8*i, int loads sign extend
assert_contains "$out" $'lea rax, [rbp+16]\nmovsxd rax, dword [rax]' "first parameter"
assert_contains "$out" $'lea rax, [rbp+24]\nmovsxd rax, dword [rax]' "second parameter"
assert_contains "$out" $'lea rax, [rbp+32]\nmovsxd rax, dword [rax]' "third parameter"
# call: arguments popped into registers in order, callee in r11
assert_contains "$out" $'pop rdi\npop rsi\npop rdx\nlea r11, [$f]' "arguments into rdi, rsi, rdx"
assert_contains "$out" $'xor eax, eax\ncall r11' "al cleared before every call"
# int arithmetic on 32-bit registers, then back to 64-bit form
assert_contains "$out" $'imul eax, ecx\nmovsxd rax, eax' "32-bit int multiply"

out=$(asm_for tests/golden/inputs/38_long.c)
assert_contains "$out" "mov qword [rcx], rax" "long stored as 8 bytes"
assert_contains "$out" "sub rax, rcx" "long arithmetic on 64-bit registers"

out=$(asm_for tests/golden/inputs/32_char_short.c)
assert_contains "$out" "movsx rax, byte [rax]" "signed char load"
assert_contains "$out" "movsx rax, word [rax]" "short load"
assert_contains "$out" "mov byte [rcx], al" "char store"

out=$(asm_for tests/golden/inputs/27_string.c)
assert_contains "$out" "lea rax, [str_" "string address with lea"

# B1: an 8-byte struct comes back in rax: no hidden pointer, the
# arguments start at rdi.
out=$(asm_for tests/golden/inputs/25_struct_return.c)
assert_contains "$out" $'pop rdi\npop rsi\nsub rsp, 8\nlea r11, [$make]' "small struct result: arguments from rdi"
assert_contains "$out" $'call r11\nadd rsp, 8\nmov [rsp], rax\nmov rax, rsp' "caller stores rax into the room"
assert_contains "$out" $'lea rax, [rbp-8]\nmov r11, rax\nmov rax, qword [r11+0]\nmov rsp, rbp' "callee returns the struct in rax"
assert_contains "$out" $'lea rax, [rbp+24]\nmovsxd rax, dword [rax]' "second parameter in the second slot"

# B1: a struct result of more than 16 bytes still goes through the
# hidden pointer; a 24-byte struct argument next to an 8-byte one.
cat > "$tmp/big.c" <<'EOF'
struct big { long a, b, c; };
struct pt { int x, y; };
struct big make(long x, long y){ struct big r; r.a = x; r.b = y; r.c = x + y; return r; }
long take(struct big b, struct pt p){ return b.c + p.x; }
int main(void){ struct big b = make(2, 7); struct pt p = { 1, 2 }; return take(b, p); }
EOF
out=$(asm_for "$tmp/big.c")
assert_contains "$out" $'pop rsi\npop rdx' "visible arguments from rsi on"
assert_contains "$out" "lea rdi, [rsp+8]" "hidden struct pointer in rdi"
assert_contains "$out" "mov rax, [rbp+16]" "callee reads the hidden pointer"
assert_contains "$out" $'lea rax, [rbp+24]\nmov rax, qword [rax]' "visible arguments shift one slot"
assert_contains "$out" $'mov r11, [rsp+32]\nmov [rsp+0], r11' "MEMORY struct copied to the outgoing area"
assert_contains "$out" "mov rdi, [rsp+56]" "small struct argument in rdi"
assert_contains "$out" $'lea rax, [rbp+64]\nadd rax, 16' "callee finds the stack struct above the home area"

out=$(asm_for tests/golden/inputs/31_varargs.c)
# the psABI va_list, a pointer to its state in the frame. The
# prologue copies the argument registers into the register save area.
assert_contains "$out" $'lea rax, [rbp-224]\nmov rcx, rax\nlea rax, [rbp+16]\nmov rax, qword [rax]\nmov qword [rcx], rax' "the save area gets rdi"
assert_contains "$out" $'mov rax, 8\npush rax\nlea rax, [rbp-48]\nmov rcx, rax\npop rax\nmov dword [rcx], eax' "va_start sets gp_offset past the named argument"
assert_contains "$out" $'lea rax, [rbp+64]\npush rax\nlea rax, [rbp-40]\nmov rcx, rax\npop rax\nmov qword [rcx], rax' "va_start points overflow_arg_area at the stack arguments"
assert_contains "$out" $'cmp rax, rcx\nseta al' "va_arg goes to the stack when the registers run out"
assert_contains "$out" $'mov rax, qword [rax]\nadd rax, -8' "va_arg gives the slot it stepped past"

out=$(asm_for tests/golden/inputs/39_pointer_size.c)
# sizeof is folded per operand at parse time: 8 for int* and for long
assert_contains "$out" $'mov rax, 8\nmov rcx, 8' "sizeof(int*) and sizeof(long) are 8"
pass
