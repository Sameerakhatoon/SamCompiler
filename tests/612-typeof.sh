#!/usr/bin/env bash
# typeof, and a statement the parser does not know. `typeof(x)
# y;` in a function parsed as two expressions and left one node too many
# on the node stack: the function vanished from the output and the
# compile succeeded (only the linker noticed main was gone). typeof works
# now (tests/diff/typeof.c compares it with gcc); typeof_unqual, which
# gcc 13 does not have, is checked here, and so is the diagnostic for a
# statement that still does not parse as one.
. "$(dirname "$0")/lib.sh"
need_x86_link

tmp=$(mktemp -d /tmp/sam_typeof.XXXXXX)
trap 'rm -rf "$tmp"' EXIT

# The book's reproduction: main is there, and returns 6.
cat > "$tmp/t.c" <<'C'
int main(void) {
    int x = 5;
    typeof(x) y = x + 1;
    return y;
}
C
for t in x86 x86_64 aarch64 riscv64; do
    samcc -target=$t -S "$tmp/t.c" -o "$tmp/t-$t.s" >"$tmp/log" 2>&1 || fail "$t did not compile typeof: $(head -3 "$tmp/log")"
    grep -q 'main' "$tmp/t-$t.s" || fail "$t: main vanished from the output of typeof"
done
samcc -target=x86_64 "$tmp/t.c" "$tmp/t" >/dev/null 2>&1 || fail "x86_64 did not build typeof"
rc=0; "$tmp/t" || rc=$?
assert_eq 6 "$rc" "typeof(x) y = x + 1"

# typeof_unqual and its gcc spellings drop the qualifiers of the type
# itself, not those under a pointer.
cat > "$tmp/u.c" <<'C'
int main(void) {
    const int cx = 9;
    typeof_unqual(cx) a = 1; a++;
    __typeof_unqual__(const int) b = 2; b++;
    __typeof_unqual(volatile int) c = 3; c++;
    const char* const s = "xy";
    typeof_unqual(s) t = s; t++;
    _Atomic int at = 4;
    typeof_unqual(at) plain = at; plain++;
    typeof(cx) keeps = 5;
    int r = a + b + c + *t + plain + keeps;
    return r - 'y';
}
C
for t in x86 x86_64; do
    samcc -target=$t "$tmp/u.c" "$tmp/u-$t" >"$tmp/log" 2>&1 || fail "$t did not build typeof_unqual: $(head -3 "$tmp/log")"
    rc=0; "$tmp/u-$t" || rc=$?
    assert_eq 19 "$rc" "$t typeof_unqual"
done

# typeof keeps a const: assigning to the variable is an error.
echo 'int f(void){ const int c = 1; typeof(c) d = 2; d = 3; return d; }' > "$tmp/c.c"
rc=0; samcc -target=x86_64 -S "$tmp/c.c" -o "$tmp/c.s" >"$tmp/log" 2>&1 || rc=$?
[ "$rc" != 0 ] || fail "assigning to a typeof(const int) variable was accepted"

# Clear errors, never a crash or a silent success.
check_error(){
    local src="$1" want="$2"
    printf '%s\n' "$src" > "$tmp/e.c"
    for t in x86 x86_64 aarch64 riscv64; do
        rc=0
        samcc -target=$t -S "$tmp/e.c" -o "$tmp/e.s" >"$tmp/log" 2>&1 || rc=$?
        [ "$rc" != 0 ] || fail "$t accepted: $src"
        { [ "$rc" -lt 128 ] || [ "$rc" = 255 ]; } || fail "$t crashed (status $rc) on: $src"
        grep -q "$want" "$tmp/log" || fail "$t: no '$want' for: $src; got: $(head -2 "$tmp/log")"
    done
}
check_error 'int main(void){ int n = 3; int v[n]; typeof(v) w; return 0; }' 'variable-length array'
check_error 'int main(void){ int a[2]; typeof(&a) p = 0; return p != 0; }' 'pointer to an array'
check_error 'int main(void){ typeof(nosuch) y = 1; return y; }' 'not declared'
check_error 'int main(void){ int x = 1; foo(x) y = 2; return y; }' 'not valid C'
check_error 'int f(int); int main(void){ int x = 1; f(x) 3; return x; }' 'not valid C'
check_error 'int main(void){ int x = 1; if(x) bar(x) y; return 0; }' 'not valid C'
pass
