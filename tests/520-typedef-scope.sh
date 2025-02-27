#!/usr/bin/env bash
# typedef names are declarations with C's scopes, read by the
# parser. tests/diff/typedef_scope*.c check the values against gcc on
# every target; here:
#   - the same programs on x86_64, so a run of this test alone sees them;
#   - -E keeps a typedef as a declaration (it used to vanish, its name
#     replaced by its type everywhere, scope or not);
#   - a variable-length array typedef is rejected where it cannot be a
#     local;
#   - a typedef name hidden by a variable is an ordinary identifier.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

tmp=$(mktemp -d /tmp/sam_typedef_scope.XXXXXX)
trap 'rm -rf "$tmp"' EXIT

for name in typedef_scope typedef_scope_more typedef_scope_vla; do
    ./main -target=x86_64 -o "$tmp/$name" "tests/diff/$name.c" >"$tmp/log" 2>&1 \
        || fail "$name.c did not compile: $(tail -2 "$tmp/log")"
    set +e
    "$tmp/$name"
    rc=$?
    set -e
    [ "$rc" = 0 ] || fail "$name.c returned $rc"
done

# -E: the declaration stays, the uses keep the name.
cat > "$tmp/e.c" <<'EOF'
typedef unsigned int u32;
typedef struct { u32 x; } point;
int f(void){ typedef char u32; u32 c = 1; return c; }
EOF
./main -E "$tmp/e.c" > "$tmp/e.i" 2>"$tmp/log" || fail "-E failed: $(tail -2 "$tmp/log")"
grep -q 'typedef unsigned int u32' "$tmp/e.i" || fail "-E lost the typedef: $(cat "$tmp/e.i")"
grep -q 'typedef char u32' "$tmp/e.i" || fail "-E lost the block scope typedef: $(cat "$tmp/e.i")"
grep -q 'typedef struct __anonymous_[0-9]* point' "$tmp/e.i" || fail "-E: no typedef of the hoisted body: $(cat "$tmp/e.i")"
grep -q 'u32 c = 1' "$tmp/e.i" || fail "-E replaced a typedef name: $(cat "$tmp/e.i")"
# And it compiles again.
cp "$tmp/e.i" "$tmp/e2.c"
./main -target=x86_64 -c -o "$tmp/e2.o" "$tmp/e2.c" >"$tmp/log" 2>&1 || fail "-E output does not compile: $(tail -2 "$tmp/log")"

reject(){
    local what="$1" src="$2" message="$3"
    printf '%s\n' "$src" > "$tmp/bad.c"
    if ./main -target=x86_64 -c -o "$tmp/bad.o" "$tmp/bad.c" >"$tmp/log" 2>&1; then
        fail "$what was accepted"
    fi
    grep -q "$message" "$tmp/log" || fail "$what: unexpected message: $(grep -v '^\s' "$tmp/log" | tail -2)"
}

reject "a file scope VLA typedef" 'int n = 3; typedef int row[n]; row a;' "integer constant expression"
reject "a VLA typedef member" 'int f(int n){ typedef int row[n]; struct s { row r; }; return 0; }' \
    "only supported for a local variable"
reject "a static VLA typedef local" 'int f(int n){ typedef int row[n]; static row r; return 0; }' \
    "only supported for a local variable"
reject "a cast to a VLA typedef" 'int f(int n){ typedef int row[n]; long x = 0; return (int)sizeof((row)x); }' \
    "variable-length array type"
# Hidden by a variable, the name is no type: `T * x;` is a product.
reject "a hidden typedef used as a type" 'typedef int T; int f(void){ int T = 2; T x; return 0; }' \
    "unknown type name"

pass
