#!/usr/bin/env bash
# variable-length arrays. tests/diff/vla.c runs them against
# gcc on all four targets; this covers what is rejected.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

tmp=$(mktemp -d /tmp/sam_vla.XXXXXX)
trap 'rm -rf "$tmp"' EXIT

expect_error(){
    local name="$1" want="$2"
    for target in x86 x86_64; do
        local rc=0 err
        err=$(samcc -target=$target "$tmp/$name.c" "$tmp/$name" 2>&1 >/dev/null) || rc=$?
        [ $rc != 0 ] || fail "$target: $name.c compiled without an error"
        assert_contains "$err" "$want" "$target: $name.c"
    done
}

# More than one dimension is supported now (tests/381).
printf 'int main(void){ int n = 2; int a[n] = { 1 }; return 0; }\n' > "$tmp/init.c"
expect_error init "The variable-length array a cannot be initialized"

printf 'int n = 2;\nint g[n];\nint main(void){ return 0; }\n' > "$tmp/global.c"
expect_error global "An array size must be an integer constant expression"

printf 'int main(void){ int n = 2; static int s[n]; return 0; }\n' > "$tmp/static.c"
expect_error static "An array size must be an integer constant expression"

pass
