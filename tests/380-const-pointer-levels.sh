#!/usr/bin/env bash
# const per pointer level. `int* const
# p` may not be reassigned but `*p` may change, `const int* p` the
# reverse; storing a pointer to const in a pointer to non-const warns.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

tmp=$(mktemp -d /tmp/sam_const_levels.XXXXXX)
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

expect_warning(){
    n=$((n + 1))
    printf '%s\n' "$1" > "$tmp/c$n.c"
    for target in x86 x86_64; do
        local rc=0 err
        err=$(samcc -target=$target "$tmp/c$n.c" "$tmp/c$n.o" object 2>&1 >/dev/null) || rc=$?
        [ $rc = 0 ] || fail "'$1' ($target): rejected: $err"
        assert_contains "$err" "$2" "'$1' ($target)"
    done
}

expect_error 'int main(void){ int v = 3, u = 4; int* const p = &v; p = &u; return *p; }'        "Cannot assign to p, it is const"
expect_error 'int main(void){ int v = 3; int* const p = &v; p++; return *p; }'                   "Cannot assign to p, it is const"
expect_error 'int main(void){ int v = 3; int* const p = &v; p += 1; return *p; }'                "Cannot assign to p, it is const"
expect_error 'int main(void){ int v = 3, u = 4; const int* const p = &v; p = &u; return *p; }'  "Cannot assign to p, it is const"
expect_error 'int main(void){ int v = 3; const int* const p = &v; *p = 1; return *p; }'          "Cannot assign to a const object"
expect_error 'int main(void){ int v = 3; int const* p = &v; *p = 1; return *p; }'                "Cannot assign to a const object"
expect_error 'int main(void){ char* x = "a"; char* const* pp = &x; *pp = "b"; return 0; }'      "Cannot assign to a const object"
expect_error 'int main(void){ int v; int* const a = &v, * const b = &v; b = a; return 0; }'      "Cannot assign to b, it is const"
expect_error 'int* const g; int v; int main(void){ g = &v; return 0; }'                          "Cannot assign to g, it is const"
expect_error 'int f(int x){ return x; } int main(void){ int (* const fp)(int) = f; fp = f; return 0; }' "Cannot assign to fp, it is const"
expect_error 'struct s { int* p; }; int main(void){ int v; struct s x; const struct s* cp = &x; cp->p = &v; return 0; }' "Cannot assign to a const object on line 1"
expect_error 'struct s { int a[2]; }; int main(void){ struct s x; const struct s* cp = &x; cp->a[1] = 3; return 0; }' "Cannot assign to a const object"
expect_error 'int main(void){ int v; int* const a[2] = { &v, &v }; a[1] = 0; return 0; }'        "Cannot assign to a const object"

expect_warning 'int main(void){ const char* s = "hi"; char* t = s; return t[0]; }' \
    "Initialization discards the 'const' qualifier from the pointer target type"
expect_warning 'int main(void){ const char* s = "hi"; char* t; t = s; return t[0]; }' \
    "Assignment discards the 'const' qualifier from the pointer target type"
expect_warning 'int f(char* p){ return *p; } int main(void){ const char* s = "hi"; return f(s); }' \
    "Argument 1 of f discards the 'const' qualifier from the pointer target type"
expect_warning 'char* f(const char* s){ return s; } int main(void){ return *f("x"); }' \
    "Return discards the 'const' qualifier from the pointer target type"
expect_warning 'int main(void){ const int a[2] = { 1, 2 }; void* p = a; return p != 0; }' \
    "Initialization discards the 'const' qualifier from the pointer target type"

# Valid programs: every combination that is allowed compiles without a
# warning and runs as with gcc. A cast that drops const does not warn
# (gcc warns only with -Wcast-qual).
cat > "$tmp/valid.c" <<'SRC'
struct s { int* p; const char* name; int a[2]; };
static int sum(char const* const* names, int count){
    int total = 0;
    for(int i = 0; i < count; i++){
        total += names[i][0];
    }
    return total;
}
int main(void){
    int v = 3, u = 5;
    const int* cp = &v;
    int* const pc = &u;
    const int* const cpc = &v;
    int const* also = &u;
    int* volatile vp = &v;
    int* restrict rp = &u;
    cp = &u;
    *pc = 7;
    also = cp;
    vp = rp;
    const int table[3] = { 1, 2, 3 };
    int* const pointers[2] = { &v, &u };
    *pointers[0] = 4;
    char* words[2];
    words[0] = "a";
    words[1] = "b";
    struct s x;
    x.p = &v;
    x.name = "q";
    x.a[0] = 1;
    const struct s* sp = &x;
    struct s* const ps = &x;
    *sp->p = 9;
    ps->a[1] = 2;
    int* drop = (int*)cp;
    const void* cv = cp;
    void* plain = (void*)cv;
    return *cp + *pc + *cpc + *also + *vp + table[2] + *drop + (plain != 0)
        + sum((char const* const*)words, 2) + sp->name[0] + ps->a[1] + v - 300;
}
SRC
gcc -w -o "$tmp/valid_gcc" "$tmp/valid.c" || fail "gcc rejects the valid program"
want=0
"$tmp/valid_gcc" || want=$?
for target in x86 x86_64; do
    err=$(samcc -target=$target "$tmp/valid.c" "$tmp/valid_$target" 2>&1 >/dev/null) || fail "valid const use rejected ($target): $err"
    case "$err" in *warning*) fail "valid const use warned ($target): $err";; esac
    rc=0
    "$tmp/valid_$target" || rc=$?
    assert_eq "$want" "$rc" "exit status of the valid program ($target)"
done

pass
