#!/usr/bin/env bash
# Assignment operators and the `a = b = 3` crash: every compound
# operator stores its result, ++/-- work as statements and as values,
# and an assignment used as an operand has a value. The run-time
# results are checked against gcc by tests/250 (tests/diff/inc_*,
# compound_*, chain_assign, assign_value*). This test pins the shapes
# that must not change.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

tmp=$(mktemp -d /tmp/sam_assign_ops.XXXXXX)
trap 'rm -rf "$tmp"' EXIT

# All ten compound operators lex as one token each.
cat > "$tmp/ops.c" <<'EOF'
int main(void){ int a = 100; a -= 3; a *= 2; a /= 2; a %= 50; a <<= 2; a >>= 1; a &= 255; a |= 1; a ^= 2; a += 1; return a; }
EOF
./main -target=x86_64 -S "$tmp/ops.c" "$tmp/ops.s" >/dev/null 2>&1 || ./main -target=x86_64 "$tmp/ops.c" "$tmp/ops.s" object >/dev/null 2>&1 \
    || fail "compound operators did not compile"
./main -target=x86_64 "$tmp/ops.c" "$tmp/ops" >/dev/null 2>&1 || fail "compound operators did not link"
rc=0
"$tmp/ops" || rc=$?
# 100-3=97 *2=194 /2=97 %50=47 <<2=188 >>1=94 &255=94 |1=95 ^2=93 +1=94
assert_eq 94 "$rc" "exit status after ten compound assignments"

# A statement `x += n` keeps its single add-to-memory instruction on x86,
# and a statement assignment pushes no value (x86 golden output).
cat > "$tmp/stmt.c" <<'EOF'
int main(void){ int a = 1; a += 2; a = 5; return a; }
EOF
./main -target=x86 "$tmp/stmt.c" "$tmp/stmt.asm" object >/dev/null 2>&1 || fail "x86 statement assignment did not compile"
asm=$(cat "$tmp/stmt.asm")
assert_contains "$asm" "add dword [ebp-4], eax" "x86 += statement"

# The value of `c = 300` is the stored char.
cat > "$tmp/value.c" <<'EOF'
int main(void){ signed char c; int d = (c = 300); return d; }
EOF
for t in x86 x86_64; do
    ./main -target=$t "$tmp/value.c" "$tmp/value_$t" >/dev/null 2>&1 || fail "$t: assignment value did not compile"
    rc=0
    "$tmp/value_$t" || rc=$?
    assert_eq 44 "$rc" "$t: value of c = 300"
done

pass
