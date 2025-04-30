#!/usr/bin/env bash
# multi-dimensional variable-length arrays, pointers to arrays
# and VLA parameters. tests/diff/vla_multi.c and tests/diff/vla_goto.c
# run them against gcc on all four targets; this covers what is
# rejected, and a name that hides a variable-length array.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

tmp=$(mktemp -d /tmp/sam_vla_types.XXXXXX)
trap 'rm -rf "$tmp"' EXIT

n=0
expect_error(){
    n=$((n + 1))
    printf '%s\n' "$1" > "$tmp/c$n.c"
    for target in x86 x86_64; do
        local rc=0 err
        err=$(samcc -target=$target "$tmp/c$n.c" "$tmp/c$n.o" object 2>&1 >/dev/null) || rc=$?
        [ $rc != 0 ] || fail "'$1' ($target): compiled without an error"
        assert_contains "$err" "$2" "'$1' ($target)"
    done
}

expect_error 'int main(void){ int n = 2; int m[n][n] = { 0 }; return 0; }' \
    "The variable-length array m cannot be initialized"
expect_error 'int main(void){ int n = 2; int m[][n]; return 0; }' \
    "The variable-length array m needs a size"
expect_error 'int main(void){ int n = 2; int m[n][]; return 0; }' \
    "Only the first dimension of an array may be left out"
expect_error 'int (*g)[4]; int main(void){ return 0; }' \
    "A pointer to an array is only supported as a local or a parameter"
expect_error 'struct s { int (*rows)[4]; }; int main(void){ return 0; }' \
    "A pointer to an array is only supported as a local or a parameter"
expect_error 'int main(void){ int n = 2; int x[4]; int* q = (int (*)[n])x; return 0; }' \
    "A cast to a pointer to an array is not supported"
expect_error 'int main(void){ int n = 2; int (**pp)[n]; return 0; }' \
    "Only a pointer to an array"

# A name in an inner block hides the array: its own sizeof and
# subscripts; a goto that leaves no array block changes nothing.
cat > "$tmp/valid.c" <<'SRC'
int main(void){
    int n = 3;
    int a[n][n];
    a[1][2] = 6;
    int total = 0;
    {
        int a = 5;
        total += a + (int)sizeof(a);
    }
    {
        int a[4];
        a[3] = 2;
        total += a[3] + (int)sizeof(a);
    }
    total += a[1][2] + (int)sizeof(a);
    if(total > 0){
        goto done;
    }
    total = 0;
done:
    return total;
}
SRC
gcc -w -o "$tmp/valid_gcc" "$tmp/valid.c" || fail "gcc rejects the valid program"
want=0
"$tmp/valid_gcc" || want=$?
for target in x86 x86_64; do
    samcc -target=$target "$tmp/valid.c" "$tmp/valid_$target" >/dev/null 2>&1 || fail "valid program rejected ($target)"
    rc=0
    "$tmp/valid_$target" || rc=$?
    assert_eq "$want" "$rc" "exit status of the valid program ($target)"
done

pass
