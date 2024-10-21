#!/usr/bin/env bash
# Diagnostics: every error
# names its own file and line and the token at fault, the validator
# reports several errors and a count, warnings are marked as such.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

tmp=$(mktemp -d /tmp/sam_diagnostics.XXXXXX)
trap 'rm -rf "$tmp"' EXIT

# compile_err NAME TARGET: compile $tmp/NAME.c, which must fail; its
# stderr in $err.
compile_err(){
    local rc=0
    err=$(./main -target="$2" "$tmp/$1.c" "$tmp/$1.o" object 2>&1 >/dev/null) || rc=$?
    [ $rc != 0 ] || fail "$1 ($2): compiled without an error"
    [ $rc -lt 128 ] || [ $rc = 255 ] || fail "$1 ($2): died with status $rc: $err"
}

# A validator error: the right line, a real file name, no "(null)".
cat > "$tmp/validator.c" <<'SRC'
int main(void){
    int value;
    int value;
    return 0;
}
SRC
compile_err validator x86_64
assert_contains "$err" "already defined the variable value" "validator message"
assert_contains "$err" "on line 3," "validator line"
assert_contains "$err" "in file $tmp/validator.c" "validator file"
case "$err" in *"(null)"*|*"line 0,"*) fail "validator error without a location: $err" ;; esac

# Preprocessor errors name the directive's line, not the end of the file.
printf '#if\nint main(void){ return 0; }\n#endif\n\n\n' > "$tmp/pp_if.c"
compile_err pp_if x86
assert_contains "$err" "on line 1," "#if line"
printf 'int a;\n#define F(x, y) x\nint main(void){\n    return F(1);\n}\n\n\n' > "$tmp/pp_macro.c"
compile_err pp_macro x86_64
assert_contains "$err" "on line 4," "macro argument count line"

# An error found by the backend names the statement's line.
cat > "$tmp/backend.c" <<'SRC'
struct s { int a; };
int main(void){
    struct s v;
    v++;
    return 0;
}


SRC
compile_err backend x86_64
assert_contains "$err" "on line 4," "backend error line"

# Several errors in one compile, and the count.
cat > "$tmp/several.c" <<'SRC'
int main(void){
    int a = missing_one;
    a = missing_two + 1;
    return missing_three;
}
SRC
for target in x86 x86_64; do
    compile_err several $target
    assert_contains "$err" "'missing_one' is not declared on line 2," "first error ($target)"
    assert_contains "$err" "'missing_two' is not declared on line 3," "second error ($target)"
    assert_contains "$err" "'missing_three' is not declared on line 4," "third error ($target)"
    assert_contains "$err" "3 errors generated." "error count ($target)"
done

# One error per function too: an error in f does not hide one in main.
printf 'int f(void){ return nope1; }\nint main(void){ return nope2; }\n' > "$tmp/two_functions.c"
compile_err two_functions x86
assert_contains "$err" "'nope1' is not declared" "error in f"
assert_contains "$err" "'nope2' is not declared" "error in main"
assert_contains "$err" "2 errors generated." "count over functions"

# The parser names what it found; one message per line.
printf 'int main(void){ return 1 }\n' > "$tmp/parser.c"
compile_err parser x86
assert_contains "$err" "Expecting symbol ; but found '}' on line 1," "parser message"
assert_contains "$err" "1 error generated." "parser count"

# A warning says so and does not fail the compile.
printf 'int main(void){ int unused; return 0; }\n' > "$tmp/warn.c"
err=$(./main -target=x86_64 "$tmp/warn.c" "$tmp/warn.o" object 2>&1 >/dev/null) || fail "a warning failed the compile: $err"
assert_contains "$err" "warning: Unused variable unused on line 1," "warning format"

pass
