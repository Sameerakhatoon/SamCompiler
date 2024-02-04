#!/usr/bin/env bash
# Integer types and constants:
# constants have their C type (value, base and suffix), arithmetic runs
# in the type of the usual arithmetic conversions, and pointers subtract
# and compare as addresses. tests/diff/ (div_*, unsigned_*, mixed_*,
# literal_*, wide_literal, ptr_sub_cmp, ...) compare these with gcc on
# all targets; this checks the lexer diagnostics and one program.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

tmp=$(mktemp -d /tmp/sam_int_types.XXXXXX)
trap 'rm -rf "$tmp"' EXIT

expect_error(){
    printf '%s\n' "$2" > "$tmp/$1.c"
    local rc=0 err
    err=$(./main -target=x86_64 "$tmp/$1.c" "$tmp/$1" 2>&1 >/dev/null) || rc=$?
    [ $rc != 0 ] || fail "$1: compiled without an error"
    assert_contains "$err" "$3" "$1 diagnostic"
}
expect_error octal_digit  'int main(void){ return 08; }'    "Invalid digit '8' in the integer constant 08"
expect_error empty_hex    'int main(void){ return 0x; }'    "The integer constant 0x has no digits"
expect_error bad_suffix   'int main(void){ return 10q; }'   "Invalid suffix \"q\" on the integer constant 10q"
expect_error too_big      'int main(void){ return 99999999999999999999; }' "does not fit in 64 bits"

cat > "$tmp/t.c" <<'EOF'
int main(void){
    int a = -17;
    unsigned int u = 4000000000u;
    long big = 5000000000;
    int arr[5]; int* p = &arr[1]; int* q = &arr[4];
    int r = 0;
    if(a / 5 == -3) r = r + 1;          /* A4/A5: was unsigned division */
    if(u > 1) r = r + 2;                /* A4: was a signed compare */
    if(u % 7 == 4000000000u % 7) r = r + 4;
    if(big / 1000000000 == 5) r = r + 8;  /* A3: was cut to 32 bits */
    if(q - p == 3 && q > p) r = r + 16; /* A10 */
    if(010 + 0xFF + 0B11 == 8 + 255 + 3) r = r + 32;
    return r;                           /* 63 */
}
EOF
./main -target=x86_64 "$tmp/t.c" "$tmp/t" >/dev/null 2>&1 || fail "the program did not compile"
rc=0
"$tmp/t" || rc=$?
assert_eq 63 "$rc" "exit status"

pass
