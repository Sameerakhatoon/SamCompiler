#!/usr/bin/env bash
# A for loop that declares its variable.
# tests/diff/for_decl*.c compare with gcc on all targets.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

tmp=$(mktemp -d /tmp/sam_for_decl.XXXXXX)
trap 'rm -rf "$tmp"' EXIT

cat > "$tmp/f.c" <<'EOF'
int main(void){
    int i = 40; int s = 0;
    for(int i = 0; i < 4; i++) s += i;               /* 6 */
    for(int j = 0, k = 2; j < 3; j++){ s += k; }     /* 6 */
    return s + i;                                    /* 52: the outer i */
}
EOF
for t in x86 x86_64; do
    ./main -target=$t "$tmp/f.c" "$tmp/f_$t" >/dev/null 2>&1 || fail "$t: did not compile"
    rc=0
    "$tmp/f_$t" || rc=$?
    assert_eq 52 "$rc" "$t exit status"
done

pass
