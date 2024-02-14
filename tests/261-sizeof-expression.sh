#!/usr/bin/env bash
# Sizeof of an expression. Only
# sizeof(type) parsed. tests/diff/sizeof_*.c compare with gcc on all
# targets; this checks the x86_64 sizes directly.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

tmp=$(mktemp -d /tmp/sam_sizeof.XXXXXX)
trap 'rm -rf "$tmp"' EXIT

cat > "$tmp/s.c" <<'EOF'
int main(void){
    int a = 1; long b; char buf[10]; int m[2][3]; int* p = &a;
    int r = 0;
    if(sizeof a == 4) r = r + 1;
    if(sizeof(b) == 8) r = r + 2;
    if(sizeof buf == 10 && sizeof m == 24 && sizeof m[1] == 12) r = r + 4;
    if(sizeof *p == 4 && sizeof(p) == 8) r = r + 8;
    if(sizeof "abc" == 4 && sizeof(a + 1L) == 8) r = r + 16;
    if(sizeof(a++) == 4 && a == 1) r = r + 32;
    return r;   /* 63 */
}
EOF
./main -target=x86_64 "$tmp/s.c" "$tmp/s" >/dev/null 2>&1 || fail "did not compile"
rc=0
"$tmp/s" || rc=$?
assert_eq 63 "$rc" "exit status"

pass
