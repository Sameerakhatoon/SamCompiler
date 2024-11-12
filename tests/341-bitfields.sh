#!/usr/bin/env bash
# bit-fields. tests/diff/bitfields*.c run against gcc on all
# four targets; this covers the diagnostics and 64-bit units, which
# the i386 targets cannot share with gcc (long long is 4 bytes here).
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

tmp=$(mktemp -d /tmp/sam_bitfields.XXXXXX)
trap 'rm -rf "$tmp"' EXIT

expect_error(){
    local name="$1" want="$2" rc=0 err
    err=$(./main -target=x86_64 "$tmp/$name.c" "$tmp/$name" 2>&1 >/dev/null) || rc=$?
    [ $rc != 0 ] || fail "$name.c compiled without an error"
    assert_contains "$err" "$want" "$name.c"
}

printf 'struct s { int a : 33; };\nint main(void){ return 0; }\n' > "$tmp/wide.c"
expect_error wide "The bit-field width 33 does not fit its type"

printf 'struct s { int a : 0; };\nint main(void){ return 0; }\n' > "$tmp/zero.c"
expect_error zero "A bit-field of width 0 must have no name"

printf 'struct s { int* p : 3; };\nint main(void){ return 0; }\n' > "$tmp/pointer.c"
expect_error pointer "A bit-field must have an integer type"

printf 'struct s { int a : 3; };\nint main(void){ struct s v; int* p = &v.a; return 0; }\n' > "$tmp/address.c"
expect_error address "Cannot take the address of the bit-field a"

cat > "$tmp/long.c" <<'EOF'
struct wide { unsigned long low : 40; long mid : 20; unsigned long top : 4; };
int main(void){
    struct wide w = { 0xffffffffffUL, -5, 9 };
    if(sizeof(struct wide) != 8) return 1;
    w.low += 1;
    if(w.low != 0 || w.mid != -5 || w.top != 9) return 2;
    w.mid = 524287;
    w.mid++;
    if(w.mid != -524288) return 3;
    return (int)(w.top + 30);
}
EOF
for target in x86_64 aarch64 riscv64; do
    command -v qemu-$target >/dev/null || [ $target = x86_64 ] || continue
    ./main -target=$target "$tmp/long.c" "$tmp/long_$target" >/dev/null 2>&1 || fail "$target: long.c did not compile"
    case $target in x86_64) run="";; *) run="qemu-$target";; esac
    rc=0
    $run "$tmp/long_$target" || rc=$?
    assert_eq 39 "$rc" "$target: 64-bit bit-field units"
done

pass
