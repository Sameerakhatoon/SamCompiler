#!/usr/bin/env bash
# A statement or operand the parser cannot read is
# a compile error. It used to pop an earlier node instead, so the whole
# enclosing function vanished while ./main still said "everything
# compiled fine".
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

tmp=$(mktemp -d /tmp/sam_parser_progress.XXXXXX)
trap 'rm -rf "$tmp"' EXIT

# expect_error NAME SOURCE MESSAGE
expect_error(){
    printf '%s\n' "$2" > "$tmp/$1.c"
    local rc=0 err
    err=$(./main -target=x86_64 "$tmp/$1.c" "$tmp/$1" 2>&1 >/dev/null) || rc=$?
    [ $rc != 0 ] || fail "$1: compiled without an error"
    assert_contains "$err" "$3" "$1 diagnostic"
}

expect_error missing_brace  'int main(void){ return 0;'          "Expected a statement or '}' but found the end of the file"
expect_error empty_init     'int main(void){ int x = ; return 0; }' "Expected an expression but found ';'"
expect_error dangling_op    'int main(void){ return 1 +; }'      "Expected an operand after '+' but found ';'"
expect_error stray_brace    'int x; } int main(void){ return 0; }' "Unexpected '}'"

# The function before an unreadable statement must not disappear: the
# good function still compiles and runs when it stands alone.
printf 'int main(void){ int a = 2; return a + 3; }\n' > "$tmp/ok.c"
./main -target=x86_64 "$tmp/ok.c" "$tmp/ok" >/dev/null 2>&1 || fail "a valid program was rejected"
rc=0
"$tmp/ok" || rc=$?
assert_eq 5 "$rc" "exit status of the valid program"

pass
