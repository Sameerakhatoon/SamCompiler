#!/usr/bin/env bash
# CRLF and "a" "b": escapes, strings and
# their diagnostics. tests/diff/ runs the programs against gcc.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

tmp=$(mktemp -d /tmp/sam_strings.XXXXXX)
trap 'rm -rf "$tmp"' EXIT

expect_error(){
    printf '%s\n' "$2" > "$tmp/$1.c"
    local rc=0 err
    err=$(./main -target=x86_64 "$tmp/$1.c" "$tmp/$1" 2>&1 >/dev/null) || rc=$?
    [ $rc != 0 ] || fail "$1: compiled without an error"
    assert_contains "$err" "$3" "$1 diagnostic"
}
expect_error unknown_escape 'int main(void){ return "\q"[0]; }' 'Unknown escape sequence \q'
expect_error empty_hex      'int main(void){ return "\x"[0]; }' '\x needs at least one hex digit'
expect_error unterminated   'int main(void){ return "abc; }'   'Missing the closing " of a string'

# A19: an apostrophe in a NASM string.
cat > "$tmp/q.c" <<'EOF'
int main(void){ const char* s = "it's"; return s[2] + s[3]; }
EOF
for t in x86 x86_64; do
    ./main -target=$t "$tmp/q.c" "$tmp/q_$t" >/dev/null 2>&1 || fail "$t: did not compile"
    rc=0
    "$tmp/q_$t" || rc=$?
    assert_eq 154 "$rc" "$t exit status (' + s)"
done

pass
