#!/usr/bin/env bash
# Declaration forms: declarator
# lists with initializers and their own `*` and `[N]`, bare `unsigned`,
# volatile / register / inline, unary +, and a prototype followed by its
# definition. tests/diff/ compares each with gcc on all targets.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

tmp=$(mktemp -d /tmp/sam_decls.XXXXXX)
trap 'rm -rf "$tmp"' EXIT

cat > "$tmp/d.c" <<'EOF'
static inline int twice(register int v);
int twice(register int v){ volatile int r = v * 2; return r; }
int main(void){
    int a = 1, b = 2, *p = &b;      /* crashed */
    int arr[4], c;                  /* c was an array */
    unsigned n = 3;                 /* rejected */
    c = 5; arr[0] = +4;
    return a + *p + c + arr[0] + n + twice(2);   /* 1+2+5+4+3+4 = 19 */
}
EOF
for t in x86 x86_64; do
    ./main -target=$t "$tmp/d.c" "$tmp/d_$t" >/dev/null 2>&1 || fail "$t: did not compile"
    rc=0
    "$tmp/d_$t" || rc=$?
    assert_eq 19 "$rc" "$t exit status"
done

pass
