#!/usr/bin/env bash
# _Alignas. `_Alignas(16) char buf[4];` in a function made the
# function vanish and buf a global, with exit status 0. The alignments
# themselves are compared with gcc by tests/diff/alignas.c; this test
# checks a sample program, the data directives and every placement
# that is an error (as in gcc), each a message and never a crash.
. "$(dirname "$0")/lib.sh"
need_x86_link

tmp=$(mktemp -d /tmp/sam_alignas.XXXXXX)
trap 'rm -rf "$tmp"' EXIT

cat > "$tmp/a.c" <<'C'
int main(void) {
    char c = 1;
    _Alignas(16) char buf[4];
    buf[0] = c;
    return (int)((unsigned long)buf % 16) + buf[0];
}
C
for t in x86 x86_64; do
    samcc -target=$t "$tmp/a.c" "$tmp/a-$t" >"$tmp/log" 2>&1 || fail "$t did not build _Alignas: $(head -3 "$tmp/log")"
    rc=0; "$tmp/a-$t" || rc=$?
    assert_eq 1 "$rc" "$t: a 16-aligned local"
done
for t in aarch64 riscv64; do
    samcc -target=$t -S "$tmp/a.c" -o "$tmp/a-$t.s" >"$tmp/log" 2>&1 || fail "$t did not compile _Alignas: $(head -3 "$tmp/log")"
    grep -q 'main' "$tmp/a-$t.s" || fail "$t: main vanished"
done

# A global asks the assembler for the alignment.
cat > "$tmp/g.c" <<'C'
char pad;
_Alignas(32) char g[3];
int main(void) { return g[0]; }
C
samcc -target=x86_64 -S "$tmp/g.c" -o "$tmp/g.s" >/dev/null 2>&1 || fail "x86_64 did not compile an _Alignas global"
grep -q 'align 32' "$tmp/g.s" || fail "x86_64: no align 32 for an _Alignas(32) global"
samcc -target=aarch64 -S "$tmp/g.c" -o "$tmp/g.s" >/dev/null 2>&1 || fail "aarch64 did not compile an _Alignas global"
grep -q '\.balign 32' "$tmp/g.s" || fail "aarch64: no .balign 32 for an _Alignas(32) global"

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
check_error 'int main(void){ _Alignas(3) int x = 0; return x; }' 'power of two'
check_error 'int main(void){ _Alignas(32) char b[4]; b[0] = 0; return b[0]; }' 'local variable is not supported'
check_error 'int f(_Alignas(8) int x){ return x; }' 'parameter'
check_error 'typedef _Alignas(8) int T; T t;' 'typedef'
check_error 'struct s { _Alignas(8) int b : 3; }; struct s v;' 'bit-field'
check_error 'int main(void){ int x = 1; return (_Alignas(8) int)x; }' 'cast'
check_error 'int main(void){ return sizeof(_Alignas(8) int); }' 'type name'
check_error '_Alignas(1) int x;' 'less aligned'
check_error '_Alignas(16) int f(void){ return 0; }' 'function'
check_error 'int main(void){ int n = 2; _Alignas(16) char v[n]; return 0; }' 'variable-length'
check_error 'int n = 2; _Alignas(n) int x;' 'constant expression'
pass
