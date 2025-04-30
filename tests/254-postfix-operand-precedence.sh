#!/usr/bin/env bash
# After `a[i]`, `f()`, `s.a` or `p->a` the rest of
# the expression must group by precedence. The reorder pass treated
# only leaves as complete operands, so `a[1] * 2 + i * 10` became
# a[1] * (2 + i * 10). tests/diff/prec_* compare these with gcc on all
# targets; this runs one combined program natively.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

tmp=$(mktemp -d /tmp/sam_postfix_prec.XXXXXX)
trap 'rm -rf "$tmp"' EXIT

cat > "$tmp/p.c" <<'EOF'
struct s { int a; int b; };
int f(void){ return 4; }
int main(void){
    int a[2]; int i = 2; a[0] = 10; a[1] = 4;
    struct s v; v.a = 7; v.b = 2;
    int x = a[1] * 2 + i * 10;      /* 28, was 4 * 22 = 88 */
    int y = a[0] - i - 1;           /* 7,  was 10 - (2 - 1) = 9 */
    int z = f() * 2 + 1 - f() - 1;  /* 4 */
    int w = v.a * 2 + v.b - 1;      /* 15 */
    return x + y + z + w;           /* 54 */
}
EOF
samcc -target=x86_64 "$tmp/p.c" "$tmp/p" >/dev/null 2>&1 || fail "the program did not compile"
rc=0
"$tmp/p" || rc=$?
assert_eq 54 "$rc" "exit status"

pass
