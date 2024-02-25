#!/usr/bin/env bash
# Preprocessor items:
# #else/#elif, #pragma, #line, variadic macros, the self-reference rule,
# __FILE__, __STDC__, and the directive diagnostics. tests/diff/ runs the
# programs against gcc.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

tmp=$(mktemp -d /tmp/sam_pp.XXXXXX)
trap 'rm -rf "$tmp"' EXIT

expect_error(){
    printf '%s\n' "$2" > "$tmp/$1.c"
    local rc=0 err
    err=$(./main -target=x86_64 "$tmp/$1.c" "$tmp/$1" 2>&1 >/dev/null) || rc=$?
    [ $rc != 0 ] || fail "$1: compiled without an error"
    assert_contains "$err" "$3" "$1 diagnostic"
}
expect_error missing_endif $'#if 1\nint main(void){ return 0; }' 'Missing #endif'
expect_error two_else      $'#if 0\n#else\n#else\n#endif\nint main(void){ return 0; }' '#else'
expect_error elif_after    $'#if 0\n#else\n#elif 1\n#endif\nint main(void){ return 0; }' '#elif'
expect_error div_zero      $'#if 1 / 0\n#endif\nint main(void){ return 0; }' 'zero'
expect_error empty_if      $'#if\n#endif\nint main(void){ return 0; }' 'Expected an expression'
expect_error no_name       $'#define\nint main(void){ return 0; }' '#define needs a macro name'
expect_error few_args      $'#define F(a, b, ...) a\nint main(void){ return F(1); }' 'at least 2 arguments'
expect_error many_args     $'#define F(a) a\nint main(void){ return F(1, 2); }' 'takes 1 arguments, 2 given'

# E6: a file that includes itself stops with an error.
printf '#include "%s"\nint main(void){ return 0; }\n' "$tmp/self.c" > "$tmp/self.c"
rc=0
err=$(./main -target=x86_64 "$tmp/self.c" "$tmp/self" 2>&1 >/dev/null) || rc=$?
[ $rc != 0 ] || fail "self include: compiled without an error"
assert_contains "$err" "nested more than 200 deep" "self include diagnostic"

# #pragma once through two includes, on every target.
cat > "$tmp/once.h" <<'EOF'
#pragma once
int once_value = 3;
EOF
printf '#include "%s"\n#include "%s"\nint main(void){ return once_value; }\n' "$tmp/once.h" "$tmp/once.h" > "$tmp/once.c"
for t in x86 x86_64; do
    ./main -target=$t "$tmp/once.c" "$tmp/once_$t" >/dev/null 2>&1 || fail "$t: once.c did not compile"
    rc=0
    "$tmp/once_$t" || rc=$?
    assert_eq 3 "$rc" "$t #pragma once"
done

pass
