#!/usr/bin/env bash
# Arrays decay to pointers, a runtime index
# reads the element, and a dereferenced value is not scaled as a pointer.
# tests/diff/arr_* and ptr_* compare many more cases with gcc on all
# targets; this runs one combined program on x86 and x86_64.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

tmp=$(mktemp -d /tmp/sam_arrays.XXXXXX)
trap 'rm -rf "$tmp"' EXIT

cat > "$tmp/a.c" <<'EOF'
int sum(int* p, int n){ int s = 0; int i; for(i = 0; i < n; i++) s = s + p[i]; return s; }
int main(void){
    int a[4]; int i;
    for(i = 0; i < 4; i++) a[i] = i + 1;   /* 1 2 3 4 */
    int* p = a;                            /* A7: was a[0] */
    int* q = &a[2];                        /* was (&a)[2] */
    int x = a[i - 1];                      /* A8: was the address; 4 */
    int y = *p + 1;                        /* A9: was 1 + 4 */
    int z = -a[1];                         /* crashed; -2 */
    return sum(a, 4) + x + y + *q + z;     /* 10 + 4 + 2 + 3 - 2 = 17 */
}
EOF
for t in x86 x86_64; do
    samcc -target=$t "$tmp/a.c" "$tmp/a_$t" >/dev/null 2>&1 || fail "$t: the program did not compile"
    rc=0
    "$tmp/a_$t" || rc=$?
    assert_eq 17 "$rc" "$t exit status"
done

pass
