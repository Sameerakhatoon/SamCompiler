#!/usr/bin/env bash
# Function pointers and struct
# assignment through a pointer. tests/diff/ runs the programs against gcc.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

tmp=$(mktemp -d /tmp/sam_fnptr.XXXXXX)
trap 'rm -rf "$tmp"' EXIT

printf 'int main(void){ int x = 3; return x(1); }\n' > "$tmp/bad.c"
rc=0
err=$(samcc -target=x86_64 "$tmp/bad.c" "$tmp/bad" 2>&1 >/dev/null) || rc=$?
[ $rc != 0 ] || fail "calling an int compiled without an error"
assert_contains "$err" "Called object is not a function or function pointer" "call of a non-function"

cat > "$tmp/table.c" <<'EOF'
typedef int (*unary)(int);
int twice(int x){ return x * 2; }
int plus3(int x){ return x + 3; }
struct entry { unary f; int arg; };
int main(void){
    struct entry t[2] = { { twice, 5 }, { plus3, 4 } };
    struct entry copy;
    struct entry* p = &copy;
    *p = t[1];
    return t[0].f(t[0].arg) + p->f(p->arg) + (*t[0].f)(1);
}
EOF
for target in x86 x86_64; do
    samcc -target=$target "$tmp/table.c" "$tmp/table_$target" >/dev/null 2>&1 || fail "$target: table.c did not compile"
    rc=0
    "$tmp/table_$target" || rc=$?
    assert_eq 19 "$rc" "$target calls through a table of function pointers"
done

pass
