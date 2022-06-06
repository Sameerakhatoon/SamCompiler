#!/usr/bin/env bash
# Ch25: compile_file now invokes parse() which is a stub returning OK.
# node_vec / node_tree_vec exist on compile_process.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1
out="$(./main)"
assert_contains "$out" "everything compiled fine" "main still succeeds with parser stub"

probe=$(mktemp /tmp/sam_ch25_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch25_bin.XXXXXX)
trap 'rm -f "$probe" "$bin"' EXIT

cat > "$probe" <<EOF
#include <stdio.h>
#include "compiler.h"
#include "helpers/vector.h"
int main(void){
    struct compile_process* cp = compile_process_create("./test.c", "/tmp/sam_ch25_out", 0);
    if(!cp){ printf("FAIL cp\n"); return 1; }
    if(!cp->node_vec){ printf("FAIL: node_vec NULL\n"); return 1; }
    if(!cp->node_tree_vec){ printf("FAIL: node_tree_vec NULL\n"); return 1; }
    printf("nv=%d ntv=%d\n",
        vector_count(cp->node_vec), vector_count(cp->node_tree_vec));
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" \
    "$REPO_ROOT"/build/compiler.o "$REPO_ROOT"/build/cprocess.o \
    "$REPO_ROOT"/build/lexer.o "$REPO_ROOT"/build/lex_process.o \
    "$REPO_ROOT"/build/token.o "$REPO_ROOT"/build/parser.o \
    "$REPO_ROOT"/build/helpers/buffer.o "$REPO_ROOT"/build/helpers/vector.o \
    -o "$bin" 2>&1 | head -5
[ -x "$bin" ] || fail "ch25 probe failed to compile"
got="$("$bin")"
assert_eq "nv=0 ntv=0" "$got" "fresh compile_process: both vectors empty"
pass
