#!/usr/bin/env bash
# Ch227: creating native definitions - part 1. Adds the
# native-callback machinery so preprocessor definitions can hand
# off evaluation to C code instead of a token vector.
#
# preprocessor_initialize now calls preprocessor_create_definitions
# which (over in preprocessor/native.c) registers `__LINE__` via
# preprocessor_definition_create_native (NATIVE_CALLBACK type,
# evaluate + value callbacks).
#
# Test: confirm a fresh preprocessor has at least one definition
# (the native __LINE__) registered after preprocessor_create.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

probe=$(mktemp /tmp/sam_ch227_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch227_bin.XXXXXX)
trap 'rm -f "$probe" "$bin"' EXIT

cat > "$probe" <<'EOF'
#include <stdio.h>
#include "compiler.h"
#include "helpers/vector.h"

int main(void){
    struct compile_process* cp = compile_process_create("/dev/null", NULL, 0, NULL);
    if (!cp){ printf("create=null\n"); return 0; }
    int n = vector_count(cp->preprocessor->definitions);
    int saw_line = 0;
    int saw_native = 0;
    vector_set_peek_pointer(cp->preprocessor->definitions, 0);
    struct preprocessor_definition* d = vector_peek_ptr(cp->preprocessor->definitions);
    while (d){
        if (d->type == PREPROCESSOR_DEFINITION_NATIVE_CALLBACK) saw_native = 1;
        if (d->name && S_EQ(d->name, "__LINE__")) saw_line = 1;
        d = vector_peek_ptr(cp->preprocessor->definitions);
    }
    printf("n=%d line=%d native=%d\n", n, saw_line, saw_native);
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>/dev/null || fail "ch227 probe failed to compile"
got="$("$bin")"
assert_contains "$got" "n=1 line=1 native=1" "preprocessor_create registers __LINE__ as a NATIVE_CALLBACK definition"
pass
