#!/usr/bin/env bash
# on x86_64 the va_list is a pointer to the psABI's
# struct __va_list_tag, so a va_list made by SamCompiler's va_start and
# va_copy goes to glibc's vprintf family. The program below hands lists
# with integer, string, double and stack arguments (more than the six
# integer and eight floating-point registers) to vsnprintf, vfprintf and
# a SamCompiler-compiled helper taking a va_list, and must print what
# the gcc-built one prints.
. "$(dirname "$0")/lib.sh"

need_tools nasm gcc
tmp=$(mktemp -d /tmp/sam_valist.XXXXXX)
trap 'rm -rf "$tmp"' EXIT

cat > "$tmp/v.c" <<'X'
#include <stdarg.h>
#include <stdio.h>
int vsnprintf(char* s, unsigned long n, const char* fmt, va_list ap);
int vfprintf(FILE* f, const char* fmt, va_list ap);

static long sum_rest(int n, va_list ap){
    long s = 0;
    for(int i = 0; i < n; i++){
        s = s * 3 + va_arg(ap, int);
    }
    return s;
}

static long sum(int n, ...){
    va_list ap;
    va_start(ap, n);
    long s = sum_rest(n, ap);
    va_end(ap);
    return s;
}

static int format(char* out, int size, const char* f, ...){
    va_list ap, again;
    va_start(ap, f);
    va_copy(again, ap);
    int k = vsnprintf(out, size, f, ap);
    vfprintf(stdout, f, again);
    va_end(again);
    va_end(ap);
    return k;
}

static void named(double a, int b, const char* f, ...){
    va_list ap;
    va_start(ap, f);
    vfprintf(stdout, f, ap);
    va_end(ap);
    printf(" | %g %d\n", a, b);
}

int main(void){
    char buf[256];
    int k = format(buf, sizeof(buf), "[%d %s %.2f %ld %c %g %g %g %g %g %g %g %g %d %d %d %d %d]\n",
                   42, "str", 3.14159, 1234567890123L, 'x', 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.5, 1, 2, 3, 4, 5);
    printf("%d %s", k, buf);
    named(1.5, 7, "%s %d %f", "abc", -3, 2.25);
    printf("%ld\n", sum(9, 1, 2, 3, 4, 5, 6, 7, 8, 9));
    return 0;
}
X
gcc -w -o "$tmp/gcc_v" "$tmp/v.c" || fail "gcc cannot build the program"
"$tmp/gcc_v" > "$tmp/expected"
./main -target=x86_64 "$tmp/v.c" -o "$tmp/sam_v" > "$tmp/log" 2>&1 || fail "SamCompiler: $(head -3 "$tmp/log")"
"$tmp/sam_v" > "$tmp/got" || fail "the SamCompiler-built program failed"
cmp -s "$tmp/expected" "$tmp/got" || fail "output differs: $(diff "$tmp/expected" "$tmp/got" | head -5)"
pass
