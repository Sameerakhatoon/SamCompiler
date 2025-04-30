#!/usr/bin/env bash
# a call through a function pointer checks its arguments
# against the parameters of the pointer's type, and a function may
# return a function pointer without a typedef. tests/diff/ runs
# fn_pointer_return.c and fn_pointer_params.c against gcc.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

tmp=$(mktemp -d /tmp/sam_fnptypes.XXXXXX)
trap 'rm -rf "$tmp"' EXIT

expect_error(){
    local name="$1" want="$2"
    for target in x86 x86_64 aarch64; do
        local rc=0 err
        err=$(samcc -target=$target "$tmp/$name.c" "$tmp/$name" 2>&1 >/dev/null) || rc=$?
        [ $rc != 0 ] || fail "$target: $name.c compiled without an error"
        assert_contains "$err" "$want" "$target: $name.c"
    done
}

printf 'int two(int a, int b){ return a + b; }\nint main(void){ int (*f)(int, int) = two; return f(1); }\n' > "$tmp/few.c"
expect_error few "passes 1 arguments, its type has 2 parameters"

printf 'int one(int a){ return a; }\nint main(void){ int (*f)(int) = one; return f(1, 2); }\n' > "$tmp/many.c"
expect_error many "passes 2 arguments, its type has 1 parameters"

printf 'int none(void){ return 4; }\nint main(void){ int (*f)(void) = none; return f(3); }\n' > "$tmp/void.c"
expect_error void "passes 1 arguments, its type has 0 parameters"

cat > "$tmp/ok.c" <<'EOF'
int three(int a, int b, int c){ return a + b + c; }
int sum(int n, ...){ return n; }
int (*choose(int which))(int, int, int);
int (*choose(int which))(int, int, int){ return three; }
int main(void){
    int (*any)() = three;
    int (*v)(int, ...) = sum;
    return any(1, 2, 3) + v(4, 5, 6) + choose(0)(1, 1, 1);
}
EOF
for target in x86 x86_64; do
    samcc -target=$target "$tmp/ok.c" "$tmp/ok_$target" >/dev/null 2>&1 || fail "$target: ok.c did not compile"
    rc=0
    "$tmp/ok_$target" || rc=$?
    assert_eq 13 "$rc" "$target: unspecified, variadic and returned function pointers"
done

pass
