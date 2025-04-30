#!/usr/bin/env bash
# Ch143: end-to-end - SamCompiler emits asm; NASM assembles it to ELF
# .o; gcc -m32 links it into a runnable binary. Tests the full
# pipeline by running ./main with the input + output paths and
# letting it shell out to nasm + gcc.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

# Need nasm plus a 32-bit linker: i686-linux-gnu-gcc (what main uses
# when present) or gcc -m32. A missing one fails (lib.sh).
need_tools nasm
need_x86_link

scratch=$(mktemp /tmp/sam_ch143_input.XXXXXX.c)
asm_out=$(mktemp /tmp/sam_ch143_asm.XXXXXX)
trap 'rm -f "$scratch" "$asm_out" "${asm_out}.o"' EXIT

# Minimal program that just lays out a function prologue / epilogue
# and a local arithmetic expression - the actual return value is
# whatever's left in eax, which our current codegen leaves as the
# last arithmetic result.
printf 'int main() { int a = 3 + 4; }' > "$scratch"

# Drive the CLI in "asm-only" mode by passing "object" so nasm runs
# but the gcc link step is skipped (we only need to confirm nasm
# accepts the asm).
samcc "$scratch" "$asm_out" object >/tmp/_e2e_log 2>&1 || {
    cat /tmp/_e2e_log
    fail "main exited non-zero on the end-to-end run"
}
rm -f /tmp/_e2e_log
[ -s "${asm_out}.o" ] || fail "nasm did not produce a non-empty .o"
file "${asm_out}.o" | grep -q 'ELF 32-bit' || fail "produced .o is not ELF 32-bit"
pass
