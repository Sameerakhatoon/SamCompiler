#!/usr/bin/env bash
# Malformed input is
# an error with a message and a nonzero status, never a crash. Each of
# these used to segfault, assert, or compile with status 0.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

tmp=$(mktemp -d /tmp/sam_malformed.XXXXXX)
trap 'rm -rf "$tmp"' EXIT

n=0
# expect_error SOURCE MESSAGE: an error naming MESSAGE on x86 and x86_64.
expect_error(){
    n=$((n + 1))
    printf '%s\n' "$1" > "$tmp/m$n.c"
    for target in x86 x86_64; do
        local rc=0 err
        err=$(timeout 60 ./main -target=$target "$tmp/m$n.c" "$tmp/m$n.o" object 2>&1 >/dev/null) || rc=$?
        [ $rc != 0 ] || fail "'$1' ($target): compiled without an error"
        [ $rc -lt 128 ] || [ $rc = 255 ] || fail "'$1' ($target): died with status $rc: $err"
        assert_contains "$err" "$2" "'$1' ($target)"
    done
}

expect_error 'int main(void){ return 0;'                     "found the end of the file"
expect_error 'int main(void){ struct nope x; return 0; }'    "struct nope is never defined"
expect_error 'int main(void){ break; return 0; }'            "'break' is only valid inside a loop or switch"
expect_error 'int main(void){ continue; return 0; }'         "'continue' is only valid inside a loop"
expect_error 'int main(void){ switch(1){ default: continue; } return 0; }' "'continue' is only valid inside a loop"
expect_error 'int main(void){ switch(1){ default: ; } break; return 0; }'  "'break' is only valid"
expect_error 'int main(void){ return y; }'                   "'y' is not declared"
expect_error 'int main(void){ int a; a = y + 1; return a; }' "'y' is not declared"
expect_error 'int main(void){ if (y) return 1; return 0; }'  "'y' is not declared"
expect_error 'int main(void){ while (y) {} return 0; }'      "'y' is not declared"
expect_error 'int main(void){ int a[3]; a[z] = 1; return 0; }' "'z' is not declared"
expect_error 'int main(void){ do {} while (q); return 0; }'  "'q' is not declared"
expect_error 'int main(void){ switch (q) { default: break; } return 0; }' "'q' is not declared"
expect_error 'int main(void){ int x = q; return x; }'        "'q' is not declared"
expect_error 'int main(void){ return f(); }'                 "'f' is not declared"
expect_error 'int main(void){ f(); return 0; }'              "'f' is not declared"
expect_error 'int main(void){ return later(); } int later(void){ return 1; }' "'later' is not declared"
expect_error 'struct s { int a; }; int main(void){ struct s v; return v.b; }'  "struct s has no member named b"
expect_error 'struct s { int a; }; int main(void){ struct s *v = 0; return v->b; }' "struct s has no member named b"
expect_error 'int main(void){ int a; a.b.c = 1; return 0; }' "Member b of int"
expect_error '#if
int main(void){ return 0; }
#endif'                                                      "Expected an expression after #if"
expect_error '#define
int main(void){ return 0; }'                                 "#define needs a macro name"
expect_error '#undef
int main(void){ return 0; }'                                 "#undef needs a macro name"
expect_error 'int main(void){ int f(void){ return 1; } return 0; }' "Function f is declared inside function main"
expect_error 'int main(void){ int helper(void); return 0; }' "Function helper is declared inside function main"
expect_error 'int main(void){ return sizeof(; }'             "Expected an expression but found ';'"
expect_error 'int main(void){ foo x; return 0; }'            "after 'foo' but found 'x' (is the first an unknown type name?)"
expect_error 'int main(void){ int a; a b; return a; }'       "after 'a' but found 'b'"
expect_error 'int main(void){ return 1 2; }'                 "after the number 1 but found the number 2"
expect_error 'int main( { return 1; }'                       "Expecting symbol ) but found '{'"

pass
