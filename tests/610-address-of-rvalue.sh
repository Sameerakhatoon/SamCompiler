#!/usr/bin/env bash
# `&` of an rvalue (`&(x == 1)`, `&(x + 1)`) is an error on
# every target. On x86 the resolver found no object under the `&` and
# crashed (the malformed fuzz, test 322, made one from cast_bind.c).
# Also: an _Atomic struct global of a size that is not a power of two
# keeps its type's alignment on x86_64 (NASM refused `align 24`).
. "$(dirname "$0")/lib.sh"

tmp=$(mktemp -d /tmp/sam_addr.XXXXXX)
trap 'rm -rf "$tmp"' EXIT

for expr in 'x == 1' 'x + 1' '!x'; do
    echo "int f(void){ int x = 1; return &($expr) != 0; }" > "$tmp/a.c"
    for t in x86 x86_64 aarch64 riscv64; do
        rc=0
        samcc -target=$t "$tmp/a.c" "$tmp/a.o" object >/dev/null 2>"$tmp/err" || rc=$?
        [ "$rc" != 0 ] || fail "$t accepted &($expr)"
        { [ "$rc" -lt 128 ] || [ "$rc" = 255 ]; } || fail "$t crashed on &($expr) (status $rc)"
    done
done

cat > "$tmp/s.c" <<'C'
struct s24 { long a, b, c; };
char pad;
_Atomic struct s24 g24;
int main(void){ return (int)((unsigned long)&g24 % 8); }
C
samcc -target=x86_64 -S "$tmp/s.c" -o "$tmp/s.s" >/dev/null 2>&1 || fail "x86_64 did not compile an _Atomic 24-byte struct global"
grep -q 'align 24' "$tmp/s.s" && fail "x86_64 still aligns a 24-byte _Atomic global to 24"
grep -q 'align 8' "$tmp/s.s" || fail "x86_64 did not align the 24-byte _Atomic struct global to 8"
pass
