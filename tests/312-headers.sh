#!/usr/bin/env bash
# pc_includes declares the common C library, so an
# ordinary program (malloc, string.h, ctype.h, errno, limits, qsort,
# assert, stdio) compiles with SamCompiler and links against the
# target's glibc on all four targets.
. "$(dirname "$0")/lib.sh"

tmp=$(mktemp -d /tmp/sam_headers.XXXXXX)
trap 'rm -rf "$tmp"' EXIT
M="$REPO_ROOT/main"

cat > "$tmp/h.c" <<'EOF'
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <assert.h>

int compare(const void* a, const void* b){
    return *(const int*)a - *(const int*)b;
}

int main(){
    int* v = malloc(4 * sizeof(int));
    v[0] = 3; v[1] = 1; v[2] = 4; v[3] = 2;
    qsort(v, 4, sizeof(int), compare);
    char* s = calloc(32, 1);
    strcpy(s, "hello");
    strcat(s, " world");
    s[0] = toupper(s[0]);
    errno = 0;
    long n = strtol("123", NULL, 10);
    bool ok = errno == 0 && n == 123 && isdigit('7') && !isalpha('7');
    ok = ok && INT_MAX == 2147483647 && UINT8_MAX == 255 && CHAR_BIT == 8;
    ok = ok && sizeof(size_t) == sizeof(void*) && sizeof(ptrdiff_t) == sizeof(void*);
    ok = ok && strcmp(s, "Hello world") == 0 && strchr(s, 'w') == s + 6;
    assert(ok);
    char buf[32];
    snprintf(buf, sizeof(buf), "%d-%d", atoi("12"), (int)strlen(s));
    printf("%s %d%d%d%d %s\n", s, v[0], v[1], v[2], v[3], buf);
#ifdef __LP64__
    // stdin, stdout and stderr are extern variables, which the x86
    // backend does not support yet.
    fputs("stdout ok\n", stdout);
    fflush(stdout);
#endif
    errno = 0;
    FILE* f = fopen("/nonexistent/x", "r");
    ok = ok && f == NULL && errno == ENOENT;
    v = realloc(v, 8 * sizeof(int));
    free(v);
    free(s);
    if(!ok){
        exit(EXIT_FAILURE);
    }
    puts("done");
    return EXIT_SUCCESS;
}
EOF

targets="x86 x86_64"
command -v aarch64-linux-gnu-gcc >/dev/null && command -v qemu-aarch64 >/dev/null && targets="$targets aarch64"
command -v riscv64-linux-gnu-gcc >/dev/null && command -v qemu-riscv64 >/dev/null && targets="$targets riscv64"
for t in $targets; do
    "$M" -target=$t "$tmp/h.c" -o "$tmp/h_$t" >"$tmp/$t.log" 2>&1 || fail "$t: did not build: $(grep -v 'GNU-stack\|NOTE:' "$tmp/$t.log" | tail -3)"
    case $t in x86|x86_64) run="";; *) run="qemu-$t";; esac
    out=$(timeout 60 $run "$tmp/h_$t") || fail "$t: exit $?: $out"
    want="Hello world 1234 12-11"
    [ $t = x86 ] || want="$want
stdout ok"
    assert_eq "$want
done" "$out" "$t output"
done

# Every header compiles on its own, on every target, and twice over
# (include guards; assert.h is meant to be included again).
for h in stdio stdlib string ctype errno limits stdint stddef stdbool stdarg assert; do
    printf '#include <%s.h>\n#include <%s.h>\nint main(){ return 0; }\n' $h $h > "$tmp/one.c"
    for t in x86 x86_64 aarch64 riscv64; do
        "$M" -target=$t -S "$tmp/one.c" -o "$tmp/one.s" 2>"$tmp/one.log" || fail "$t: <$h.h> failed: $(tail -2 "$tmp/one.log")"
    done
done

# NDEBUG turns assert off.
printf '#include <assert.h>\nint main(){ assert(0); return 3; }\n' > "$tmp/nd.c"
"$M" -target=x86_64 -DNDEBUG "$tmp/nd.c" -o "$tmp/nd" 2>/dev/null || fail "NDEBUG build failed"
rc=0; "$tmp/nd" || rc=$?
assert_eq 3 "$rc" "assert under NDEBUG"
"$M" -target=x86_64 "$tmp/nd.c" -o "$tmp/nd2" 2>/dev/null || fail "assert build failed"
rc=0; "$tmp/nd2" 2>"$tmp/nd2.err" || rc=$?
[ "$rc" != 0 ] && [ "$rc" != 3 ] || fail "assert(0) did not abort (exit $rc)"
assert_contains "$(cat "$tmp/nd2.err")" "Assertion" "assert message"
pass
