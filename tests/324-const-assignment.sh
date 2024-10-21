#!/usr/bin/env bash
# An assignment to
# a const object is an error. It was silently dropped.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

tmp=$(mktemp -d /tmp/sam_const_assignment.XXXXXX)
trap 'rm -rf "$tmp"' EXIT

n=0
expect_error(){
    n=$((n + 1))
    printf '%s\n' "$1" > "$tmp/c$n.c"
    for target in x86 x86_64; do
        local rc=0 err
        err=$(./main -target=$target "$tmp/c$n.c" "$tmp/c$n.o" object 2>&1 >/dev/null) || rc=$?
        [ $rc != 0 ] || fail "'$1' ($target): compiled without an error"
        assert_contains "$err" "$2" "'$1' ($target)"
    done
}

expect_error 'int main(void){ const int x = 1; x = 2; return x; }'   "Cannot assign to x, it is const"
expect_error 'int main(void){ const int x = 1; x += 2; return x; }'  "Cannot assign to x, it is const"
expect_error 'int main(void){ const int x = 1; x++; return x; }'     "Cannot assign to x, it is const"
expect_error 'int main(void){ const int x = 1; --x; return x; }'     "Cannot assign to x, it is const"
expect_error 'const int g = 1; int main(void){ g = 2; return g; }'   "Cannot assign to g, it is const"
expect_error 'int main(void){ int v = 0; const int *q = &v; *q = 3; return v; }'  "Cannot assign to a const object"
expect_error 'int main(void){ int v = 0; const int *q = &v; q[0] = 3; return v; }' "Cannot assign to a const object"
expect_error 'int main(void){ const int a[2] = {1, 2}; a[1] = 5; return a[1]; }'  "Cannot assign to a const object"
expect_error 'struct s { int a; }; int main(void){ struct s v; const struct s *p = &v; p->a = 1; return 0; }' "Cannot assign to a const object"

# Pointers to const may change; const objects may be initialized.
cat > "$tmp/valid.c" <<'SRC'
const int limit = 5;
int main(void){
    const int base = 30;
    int values[2];
    const int *p = values;
    const char *s = "ab";
    values[0] = 3; values[1] = 4;
    p = p + 1;
    s++;
    return base + limit + *p + (*s == 'b');
}
SRC
for target in x86 x86_64; do
    ./main -target=$target "$tmp/valid.c" "$tmp/valid_$target" >/dev/null 2>&1 || fail "valid const use rejected ($target)"
done
rc=0
"$tmp/valid_x86_64" || rc=$?
assert_eq 40 "$rc" "exit status of the valid program"

pass
