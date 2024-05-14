#!/usr/bin/env bash
# Ch41: `long int` and `float int` and `double int` parse OK (the
# decorative `int` is silently dropped). `int int` is rejected.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

probe=$(mktemp /tmp/sam_ch41_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch41_bin.XXXXXX)
trap 'rm -f "$probe" "$bin"' EXIT

cat > "$probe" <<'EOF'
#include <stdio.h>
#include <stdlib.h>
#include "compiler.h"

int try_compile(const char* src){
    char path[64];
    sprintf(path, "/tmp/sam_ch41_in_%d", rand());
    FILE* f = fopen(path, "w"); fprintf(f, "%s", src); fclose(f);
    return compile_file(path, "/tmp/sam_ch41_out", 0);
}

int main(void){
    // long int, double int are valid abbreviations.
    printf("long_int=%d\n",   try_compile("long int"));
    printf("double_int=%d\n", try_compile("double int"));
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>&1 | head -5
[ -x "$bin" ] || fail "ch41 probe failed to compile"
got="$("$bin")"
assert_contains "$got" "long_int=0"   "long int parses OK"
assert_contains "$got" "double_int=0" "double int parses OK"
pass
