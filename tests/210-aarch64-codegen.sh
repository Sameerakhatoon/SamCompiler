#!/usr/bin/env bash
# AArch64 asm shape, one check per feature. Only the compiler
# runs here; 211-aarch64-run.sh assembles, links and runs under
# qemu-aarch64. The expressions come from exp64.c (address in
# x0, then the access).
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

tmp=$(mktemp -d /tmp/sam_a64_asm.XXXXXX)
trap 'rm -rf "$tmp"' EXIT

asm_for(){
    samcc -target=aarch64 "$1" "$tmp/out.s" object >"$tmp/log" 2>&1 || true
    grep -q "everything compiled fine" "$tmp/log" || fail "$1 did not compile: $(tail -2 "$tmp/log")"
    cat "$tmp/out.s"
}

out=$(asm_for tests/golden/inputs/28_call_args.c)
assert_contains "$out" $'.text\n.p2align 2' "text section"
assert_contains "$out" ".globl main" "global symbol"
# prologue: home area, frame record, x0-x7 spilled above the record
assert_contains "$out" $'f:\nsub sp, sp, #64\nstp x29, x30, [sp, #-16]!\nmov x29, sp\nstp x0, x1, [x29, #16]\nstp x2, x3, [x29, #32]\nstp x4, x5, [x29, #48]\nstp x6, x7, [x29, #64]' "prologue"
assert_contains "$out" $'mov sp, x29\nldp x29, x30, [sp], #16\nadd sp, sp, #64\nret' "epilogue"
# expression stack: 16-byte slots
assert_contains "$out" "str x0, [sp, #-16]!" "push"
assert_contains "$out" "ldr x0, [sp], #16" "pop"
# parameters from the home area, int loads sign extend
assert_contains "$out" $'add x0, x29, #16\nldrsw x0, [x0]' "first parameter"
assert_contains "$out" $'add x0, x29, #32\nldrsw x0, [x0]' "third parameter"
# call: first argument on top, popped into x0 first; callee in x9
assert_contains "$out" $'ldr x0, [sp], #16\nldr x1, [sp], #16\nldr x2, [sp], #16\nadrp x9, f\nadd x9, x9, :lo12:f\nblr x9' "arguments into x0-x2, blr"
# int multiply on w registers, then sign extended back
assert_contains "$out" $'mul w0, w0, w1\nsxtw x0, w0' "32-bit multiply"

out=$(asm_for tests/golden/inputs/03_div_mod.c)
assert_contains "$out" "sdiv w0, w0, w1" "signed divide"
assert_contains "$out" $'sdiv w2, w0, w1\nmsub w0, w2, w1, w0' "remainder with msub"

out=$(asm_for tests/golden/inputs/04_compare.c)
# an if on a comparison branches on the opposite condition
assert_contains "$out" $'cmp x0, x1\nb.ge .Lif_' "if(a < b)"
assert_contains "$out" $'cmp x0, x1\nb.le .Lif_' "if(a > b)"

out=$(asm_for tests/golden/inputs/05_logical.c)
assert_contains "$out" "cbnz x0, .Lexp_" "|| short circuit"

out=$(asm_for tests/golden/inputs/14_switch.c)
assert_contains "$out" $'cmp w0, w1\nb.eq .Lswitch_stmt_' "switch compare chain"

out=$(asm_for tests/golden/inputs/21_global_array.c)
assert_contains "$out" $'adrp x0, g\nadd x0, x0, :lo12:g' "global address with adrp"
assert_contains "$out" "g: .zero 16" "global array"

out=$(asm_for tests/golden/inputs/32_char_short.c)
# Plain char is unsigned in AAPCS64; it was loaded signed.
assert_contains "$out" "ldrb w0, [x0]" "plain char load"
assert_contains "$out" "ldrsh x0, [x0]" "short load"
assert_contains "$out" "strb w0, [x1]" "char store"

out=$(asm_for tests/golden/inputs/38_long.c)
assert_contains "$out" "str x0, [x1]" "long store"
assert_contains "$out" "sub x0, x0, x1" "long arithmetic on x registers"

out=$(asm_for tests/golden/inputs/27_string.c)
assert_contains "$out" '.asciz "hi"' "string literal"

# B1: an 8-byte struct comes back in x0.
out=$(asm_for tests/golden/inputs/25_struct_return.c)
assert_contains "$out" $'blr x9\nstr x0, [sp]\nmov x0, sp' "caller stores x0 into the room"
assert_contains "$out" $'sub x0, x29, #8\nmov x11, x0\nldr x0, [x11, #0]\nmov sp, x29' "callee returns the struct in x0"

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
assert_contains "$out" $'add x8, sp, #0\nblr x9' "struct return pointer in x8"
assert_contains "$out" $'str x8, [x29, #16]\nstp x0, x1, [x29, #32]' "callee spills x8, then x0-x7 from x29+32"
assert_contains "$out" "ldr x0, [x29, #16]" "callee reads the result pointer"
assert_contains "$out" $'add x0, sp, #0\nldr x1, [sp, #80]' "big struct by reference to a copy, small one in x1"
assert_contains "$out" $'add x0, x29, #16\nldr x0, [x0]\nadd x0, x0, #16' "callee reaches the big struct through its pointer"

out=$(asm_for tests/golden/inputs/31_varargs.c)
assert_contains "$out" $'add x0, x29, #24\nstr x0, [sp, #-16]!\nsub x0, x29, #40\nmov x1, x0\nldr x0, [sp], #16\nstr x0, [x1]' "va_start points gp past the named argument"
assert_contains "$out" $'cmp x0, x1\ncset w0, hi\ncbnz x0, .Lexp_3' "va_arg goes to the stack when the value does not fit in the registers left"
assert_contains "$out" $'ldr x0, [x0]\nsub x0, x0, #8' "va_arg gives the slot it stepped past"
pass
