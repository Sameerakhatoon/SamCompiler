#!/usr/bin/env bash
# Initializers, static locals and
# goto labels. tests/diff/ runs the programs against gcc.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

tmp=$(mktemp -d /tmp/sam_init.XXXXXX)
trap 'rm -rf "$tmp"' EXIT

expect_error(){
    printf '%s\n' "$2" > "$tmp/$1.c"
    local rc=0 err
    err=$(./main -target=x86_64 "$tmp/$1.c" "$tmp/$1" 2>&1 >/dev/null) || rc=$?
    [ $rc != 0 ] || fail "$1: compiled without an error"
    assert_contains "$err" "$3" "$1 diagnostic"
}
expect_error too_many     'int main(void){ int a[2] = {1, 2, 3}; return a[0]; }' 'Too many initializers for the object'
expect_error bad_member   'struct p { int x; }; int main(void){ struct p v = { .y = 1 }; return v.x; }' 'No member named y to initialize'
expect_error bad_index    'int main(void){ int a[2] = { [5] = 1 }; return a[0]; }' 'Array designator index 5 is outside the array'
expect_error global_var   'int h; int g[2] = { 1, h }; int main(void){ return g[0]; }' 'The initializer of global g must be a constant expression'
expect_error no_label     'int main(void){ goto out; }' 'goto out: no such label in main'
expect_error label_twice  'int main(void){ a: a: return 0; }' 'Label a is defined twice'

# A static local is a global: two calls see the same object, and the
# assembly names it after its function.
printf 'int f(void){ static int n = 41; return ++n; }\nint main(void){ f(); return f(); }\n' > "$tmp/s.c"
./main -target=x86_64 "$tmp/s.c" "$tmp/s" >/dev/null 2>&1 || fail "static local did not compile"
rc=0
"$tmp/s" || rc=$?
assert_eq 43 "$rc" "static local keeps its value"

pass
