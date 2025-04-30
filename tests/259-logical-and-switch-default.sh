#!/usr/bin/env bash
# Switch defaults and `return a || b;`: a logical result has a type
# (it asserted), `||` treats a negative operand as true on x86, and a
# switch jumps to its default label. tests/diff/ compares the programs
# with gcc on all targets.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

tmp=$(mktemp -d /tmp/sam_logical_switch.XXXXXX)
trap 'rm -rf "$tmp"' EXIT

cat > "$tmp/l.c" <<'EOF'
int any(int a, int b){ return a || b; }
int pick(int x){ switch(x){ case 0: return 1; default: return 2; case 5: return 3; } }
int main(void){ return any(-1, 0) + pick(9) * 10 + any(0, 0) * 100; }  /* 1 + 20 */
EOF
for t in x86 x86_64; do
    samcc -target=$t "$tmp/l.c" "$tmp/l_$t" >/dev/null 2>&1 || fail "$t: did not compile"
    rc=0
    "$tmp/l_$t" || rc=$?
    assert_eq 21 "$rc" "$t exit status"
done

printf 'int main(void){ default: return 0; }\n' > "$tmp/d.c"
err=$(samcc -target=x86_64 "$tmp/d.c" "$tmp/d" 2>&1 >/dev/null) && fail "default outside a switch compiled"
assert_contains "$err" "'default' is only allowed inside a switch" "default outside a switch"

pass
