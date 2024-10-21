#!/usr/bin/env bash
# "Wrong success": invalid
# programs that compiled with status 0 are errors on every target, and
# valid uses of the same constructs still compile and run.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

tmp=$(mktemp -d /tmp/sam_invalid_programs.XXXXXX)
trap 'rm -rf "$tmp"' EXIT

n=0
expect_error(){
    n=$((n + 1))
    printf '%s\n' "$1" > "$tmp/e$n.c"
    for target in x86 x86_64; do
        local rc=0 err
        err=$(./main -target=$target "$tmp/e$n.c" "$tmp/e$n.o" object 2>&1 >/dev/null) || rc=$?
        [ $rc != 0 ] || fail "'$1' ($target): compiled without an error"
        [ $rc -lt 128 ] || [ $rc = 255 ] || fail "'$1' ($target): died with status $rc: $err"
        assert_contains "$err" "$2" "'$1' ($target)"
    done
}

expect_error 'int f(int a){ return a; } int main(void){ return f(1, 2); }' "Too many arguments in the call of f: it takes 1, 2 given"
expect_error 'int f(int a){ return a; } int main(void){ return f(); }'     "Too few arguments in the call of f: it takes 1, 0 given"
expect_error 'int f(void); int main(void){ return f(3); }'                 "Too many arguments in the call of f: it takes 0, 1 given"
expect_error 'int f(int a, ...); int main(void){ return f(); }'            "it takes at least 1, 0 given"
expect_error 'int main(void){ int x = 0; return x.a; }'                    "Member a of int, which is not a struct or union"
expect_error 'int main(void){ int x = 0; return x(); }'                    "Called object is not a function or function pointer (it is int)"
expect_error 'int main(void){ int x = 0; *x = 1; return 0; }'              "Indirection through int, which is not a pointer"
expect_error 'int main(void){ int x = 0; return x[0]; }'                   "Subscripted value of type int is not an array or pointer"
expect_error 'int main(void){ int x = "str"; return x; }'                  "a string literal cannot be stored in int"
expect_error 'int main(void){ char c; c = "s"; return c; }'                "a string literal cannot be stored in char"
expect_error 'int g(void){ return "s"; } int main(void){ return g(); }'    "a string literal cannot be stored in int"
expect_error 'int main(void){ return; }'                                   "'return;' with no value in function main"
expect_error 'void f(void){ return 1; } int main(void){ f(); return 0; }'  "returning a value in a function f"
expect_error 'struct s { int a; }; int main(void){ struct s v; struct s *p = &v; return p.a; }' "'.a' on struct s*, which is not a struct"
expect_error 'struct s { int a; }; int main(void){ struct s v; v.a = 1; return v->a; }'       "'->a' on struct s, which is not a pointer"
expect_error 'struct s { int a; }; int main(void){ struct s v; int x = v; return x; }'         "incompatible types"
expect_error 'struct s { int a; }; int main(void){ struct s v[2]; struct s *p = v; return *(p + 1) + 1; }' "Invalid operands to '+' (struct s and int)"
expect_error 'struct s { int a; }; int main(void){ struct s v; return !v; }' "Invalid operand to '!' (struct s)"

# Valid programs with the same constructs: compiled and run on x86_64.
cat > "$tmp/valid.c" <<'SRC'
struct pair { int a; int b; };
static int twice(int x){ return 2 * x; }
static int sum(int n, ...){ return n; }
static int none(void){ return 4; }
static struct pair make(int a){ struct pair p; p.a = a; p.b = a + 1; return p; }
int main(void){
    int (*fp)(int) = twice;
    struct pair p = make(3);
    struct pair *pp = &p;
    int arr[3];
    int *q = arr;
    char *s = "xyz";
    const char *cs = s;
    int size = sizeof(p);
    arr[0] = 1; arr[1] = 2; arr[2] = 3;
    if (size != 2 * sizeof(int)) return 100;
    if (fp(5) != 10) return 101;
    if ((*fp)(1) != 2) return 102;
    if (pp->b != 4 || make(7).a != 7) return 103;
    if (q[2] != 3 || *(q + 1) != 2) return 104;
    if (s[1] != 'y' || cs[2] != 'z') return 105;
    if (sum(2, 7, 8) != 2 || none() != 4) return 106;
    return 42;
}
SRC
./main -target=x86_64 "$tmp/valid.c" "$tmp/valid" >"$tmp/valid.log" 2>&1 || { cat "$tmp/valid.log" >&2; fail "the valid program was rejected"; }
# (The linker's own .note.GNU-stack warning, B8, is not ours.)
grep -v GNU-stack "$tmp/valid.log" | grep -q "^warning:" && { cat "$tmp/valid.log" >&2; fail "the valid program gave a warning"; }
rc=0
"$tmp/valid" || rc=$?
assert_eq 42 "$rc" "exit status of the valid program"

pass
