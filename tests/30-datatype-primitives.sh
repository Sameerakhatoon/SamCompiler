#!/usr/bin/env bash
# Ch35: parser handles `int`, `char`, `long int`, `short` cleanly via
# the new parser_datatype_init_type_and_size_for_primitive machinery.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

for input in 'int' 'char' 'short' 'long int' 'float'; do
    scratch=$(mktemp /tmp/sam_ch35_input.XXXXXX)
    printf '%s' "$input" > "$scratch"
    out="$( (./main >/dev/null 2>&1; printf '') ; ./main_with_input "$scratch" 2>&1 || true)"
    rm -f "$scratch"
done

# Smoke check via probe: feed each primitive and check parse returns OK.
probe=$(mktemp /tmp/sam_ch35_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch35_bin.XXXXXX)
trap 'rm -f "$probe" "$bin"' EXIT

cat > "$probe" <<'EOF'
#include <stdio.h>
#include <stdlib.h>
#include "compiler.h"

int try_compile(const char* src){
    char path[64];
    sprintf(path, "/tmp/sam_ch35_in_%d", rand());
    FILE* f = fopen(path, "w");
    fprintf(f, "%s", src);
    fclose(f);
    int r = compile_file(path, "/tmp/sam_ch35_out", 0);
    return r;
}

int main(void){
    const char* inputs[] = { "int", "char", "short", "long int", "float", "double", 0 };
    for(int i = 0; inputs[i]; i++){
        printf("%-12s = %d\n", inputs[i], try_compile(inputs[i]));
    }
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>&1 | head -5
[ -x "$bin" ] || fail "ch35 probe failed to compile"
got="$("$bin")"
assert_contains "$got" "int          = 0" "int parses"
assert_contains "$got" "char         = 0" "char parses"
assert_contains "$got" "short        = 0" "short parses"
assert_contains "$got" "long int     = 0" "long int parses with secondary type"
assert_contains "$got" "float        = 0" "float parses"
pass
