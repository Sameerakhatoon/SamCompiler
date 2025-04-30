#!/usr/bin/env bash
# Constants, enum, _Bool and
# constant case labels. tests/diff/ runs the programs against gcc.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

tmp=$(mktemp -d /tmp/sam_const.XXXXXX)
trap 'rm -rf "$tmp"' EXIT

expect_error(){
    printf '%s\n' "$2" > "$tmp/$1.c"
    local rc=0 err
    err=$(samcc -target=x86_64 "$tmp/$1.c" "$tmp/$1" 2>&1 >/dev/null) || rc=$?
    [ $rc != 0 ] || fail "$1: compiled without an error"
    assert_contains "$err" "$3" "$1 diagnostic"
}
expect_error global_var   'int h = 1; int g = h; int main(void){ return g; }' 'The initializer of global g must be a constant expression'
expect_error dup_case     'int main(void){ switch (1) { case 2: case 1 + 1: return 0; } return 1; }' 'Duplicate case value 2'
expect_error stray_case   'int main(void){ case 1: return 0; }' 'case label outside a switch statement'
expect_error div_zero     'int g = 1 / 0; int main(void){ return g; }' 'Division by zero in a constant expression'
expect_error array_size   'int n = 3; int a[n]; int main(void){ return 0; }' 'An array size must be an integer constant expression'
expect_error enum_value   'int x; enum { A = x }; int main(void){ return A; }' 'The value of A must be an integer constant expression'

pass
