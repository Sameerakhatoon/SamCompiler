#!/usr/bin/env bash
# Plain char is unsigned on aarch64 and riscv64
# (AAPCS64, RISC-V psABI) and signed on x86 and x86_64. It was signed
# everywhere. tests/diff/plain_char*.c run the programs against gcc.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

tmp=$(mktemp -d /tmp/sam_plain_char.XXXXXX)
trap 'rm -rf "$tmp"' EXIT

cat > "$tmp/c.c" <<'EOF'
char c = 200;
int main(void){ return c > 0; }
EOF
for t in x86_64 aarch64 riscv64; do
    samcc -target=$t -S "$tmp/c.c" "$tmp/c_$t.s" >/dev/null 2>&1 \
        || samcc -target=$t "$tmp/c.c" "$tmp/c_$t.s" object >/dev/null 2>&1 \
        || fail "$t: did not compile"
done
assert_contains "$(cat "$tmp/c_x86_64.s")"  "movsx"  "x86_64 loads a plain char signed"
# exp64 loads through the address in x0: ldrb, not ldursb.
assert_contains "$(cat "$tmp/c_aarch64.s")" "ldrb w0" "aarch64 loads a plain char unsigned"
assert_contains "$(cat "$tmp/c_riscv64.s")" "lbu"    "riscv64 loads a plain char unsigned"

pass
