#!/usr/bin/env bash
# a qualifier before a pointer typedef qualifies the pointer.
# `const str s` with `typedef char* str;` is `char* const s`: stores
# through s are allowed, s itself is read-only. The typedef's own
# qualifiers stay on what it points to. tests/diff/typedef_const_pointer.c
# runs the valid forms against gcc.
. "$(dirname "$0")/lib.sh"

tmp=$(mktemp -d /tmp/sam_typedef_qual.XXXXXX)
trap 'rm -rf "$tmp"' EXIT

# expect_error NAME SOURCE MESSAGE
expect_error(){
    printf '%s\n' "$2" > "$tmp/$1.c"
    if samcc -target=x86_64 -S "$tmp/$1.c" -o "$tmp/$1.s" > "$tmp/$1.log" 2>&1; then
        fail "$1 compiled; expected: $3"
    fi
    assert_contains "$(cat "$tmp/$1.log")" "$3" "$1"
}

expect_error ptr_assign 'typedef char* str; int main(void){ char a[2], b[2]; const str s = a; s = b; return 0; }' \
    'Cannot assign to s, it is const'
expect_error ptr_assign_after 'typedef char* str; int main(void){ char a[2], b[2]; str const s = a; s = b; return 0; }' \
    'Cannot assign to s, it is const'
expect_error pointee 'typedef const char* cstr; int main(void){ char a[2]; cstr p = a; *p = 1; return 0; }' \
    'Cannot assign to a const object'
expect_error both 'typedef const char* cstr; int main(void){ char a[2], b[2]; const cstr q = a; q = b; return 0; }' \
    'Cannot assign to q, it is const'
expect_error two_levels 'typedef char* str; int main(void){ char* a = 0; char* b = 0; const str* pp = &a; *pp = b; return 0; }' \
    'Cannot assign to a const object'
expect_error not_pointer 'typedef int myint; int main(void){ const myint x = 1; x = 2; return 0; }' \
    'Cannot assign to x, it is const'

# The stores C allows compile, on every target, without a diagnostic.
cat > "$tmp/ok.c" <<'EOF'
typedef char* str;
typedef int* ip;
static void put(const str s){ s[1] = 2; }
int main(void){
    char a[4];
    const str s = a;
    s[0] = 1;
    *s = 3;
    int v = 0;
    const ip p = &v;
    *p = 4;
    put(a);
    return a[0] + v;
}
EOF
for target in x86 x86_64 aarch64 riscv64; do
    samcc -target=$target -S "$tmp/ok.c" -o "$tmp/ok.s" > "$tmp/ok.log" 2>&1 || fail "ok.c rejected on $target: $(tail -2 "$tmp/ok.log")"
    if grep -qiE 'error|warning' "$tmp/ok.log"; then
        fail "ok.c gave a diagnostic on $target: $(cat "$tmp/ok.log")"
    fi
done
pass
