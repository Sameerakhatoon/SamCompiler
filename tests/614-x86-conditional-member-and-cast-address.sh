#!/usr/bin/env bash
# two i386 gaps. `(c ? p : q)->m = 7` crashed the backend,
# and the driver exited with status 1 and printed nothing; it works now
# (tests/diff/x86_conditional_lvalue.c compares it with gcc). `&((char)x)`
# was accepted and the program read through x's address at run time; a
# cast is not an lvalue, as the 64-bit targets already said.
. "$(dirname "$0")/lib.sh"
need_x86_link

tmp=$(mktemp -d /tmp/sam_x86cond.XXXXXX)
trap 'rm -rf "$tmp"' EXIT

cat > "$tmp/c.c" <<'C'
struct S { int m; };
int main(void) {
    struct S s = { 1 }, t = { 2 };
    struct S* p = &s;
    struct S* q = &t;
    int c = 1;
    (c ? p : q)->m = 7;
    return s.m;
}
C
samcc -target=x86 "$tmp/c.c" "$tmp/c" >"$tmp/log" 2>&1 || fail "x86 did not build (c ? p : q)->m = 7: $(head -3 "$tmp/log")"
rc=0; "$tmp/c" || rc=$?
assert_eq 7 "$rc" "(c ? p : q)->m = 7"

# A member of a conditional's struct value is not an lvalue (gcc says
# so too): an error with a message, not a crash.
cat > "$tmp/v.c" <<'C'
struct S { int m; };
int main(void) { struct S s = { 1 }, t = { 2 }; int c = 1; (c ? s : t).m = 7; return s.m; }
C
rc=0
samcc -target=x86 -S "$tmp/v.c" -o "$tmp/v.s" >"$tmp/log" 2>&1 || rc=$?
[ "$rc" != 0 ] || fail "x86 accepted (c ? s : t).m = 7"
grep -q 'not an lvalue' "$tmp/log" || fail "x86: no message for (c ? s : t).m = 7: $(head -2 "$tmp/log")"

for expr in '(char)x' '((char)x)' '(int)x' '(long)(x)'; do
    echo "int main(void){ int x = 65; char* p = (char*)&($expr); return *p; }" > "$tmp/a.c"
    for t in x86 x86_64 aarch64 riscv64; do
        rc=0
        samcc -target=$t -S "$tmp/a.c" -o "$tmp/a.s" >"$tmp/log" 2>&1 || rc=$?
        [ "$rc" != 0 ] || fail "$t accepted &($expr)"
        { [ "$rc" -lt 128 ] || [ "$rc" = 255 ]; } || fail "$t crashed on &($expr) (status $rc)"
        grep -qiE 'address|rvalue' "$tmp/log" || fail "$t: no message for &($expr): $(head -2 "$tmp/log")"
    done
done
pass
