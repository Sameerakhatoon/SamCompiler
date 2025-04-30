#!/usr/bin/env bash
# Struct items: layout,
# unions, typedef struct, "abc"[i], unary operands and casts.
# tests/diff/ runs the programs against gcc.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

tmp=$(mktemp -d /tmp/sam_structs.XXXXXX)
trap 'rm -rf "$tmp"' EXIT

expect_error(){
    printf '%s\n' "$2" > "$tmp/$1.c"
    local rc=0 err
    err=$(samcc -target=x86_64 "$tmp/$1.c" "$tmp/$1" 2>&1 >/dev/null) || rc=$?
    [ $rc != 0 ] || fail "$1: compiled without an error"
    assert_contains "$err" "$3" "$1 diagnostic"
}
expect_error no_member 'struct s { int a; }; int main(void){ struct s x; return x.b; }' 's has no member named b'

# Offsets and sizes by value: each member written and read back, plus
# sizeof, which differs between ILP32 and LP64.
cat > "$tmp/layout.c" <<'EOF'
struct inner { char c; long l; };
union num { int i; unsigned char b[4]; };
struct outer { char a; char name[5]; struct inner in; union num n; char* p; char z; };
int main(void){
    struct outer o;
    o.a = 1; o.name[4] = 2; o.in.c = 3; o.in.l = 4; o.n.i = 0x01020304; o.p = 0; o.z = 6;
    return o.a + o.name[4] + o.in.c + o.in.l + o.n.b[0] + (o.p == 0) + o.z + sizeof(struct outer);
}
EOF
# The members add up to 21.
# x86: in at 8 (8 bytes), n at 16, p at 20, z at 24, size 28.
# x86_64: in at 8 (16 bytes), n at 24, p at 32, z at 40, size 48.
for t in x86:49 x86_64:69; do
    target=${t%%:*}
    samcc -target=$target "$tmp/layout.c" "$tmp/layout_$target" >/dev/null 2>&1 || fail "$target: layout.c did not compile"
    rc=0
    "$tmp/layout_$target" || rc=$?
    assert_eq "${t##*:}" "$rc" "$target struct layout"
done

pass
