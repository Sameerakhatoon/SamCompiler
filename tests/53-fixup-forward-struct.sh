#!/usr/bin/env bash
# Ch97: a variable referencing a not-yet-declared struct registers a
# parser fixup. After parsing the full TU (which later defines the
# struct), the fixup resolves and the variable's datatype is patched
# with a real size + struct_node.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

scratch=$(mktemp /tmp/sam_ch97_input.XXXXXX)
printf 'struct foo* p; struct foo { int a; int b; };' > "$scratch"

probe=$(mktemp /tmp/sam_ch97_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch97_bin.XXXXXX)
trap 'rm -f "$probe" "$bin" "$scratch"' EXIT

cat > "$probe" <<EOF
#include <stdio.h>
#include "compiler.h"
#include "helpers/vector.h"
extern struct lex_process_functions compiler_lex_functions;
int main(void){
    struct compile_process* cp = compile_process_create("${scratch}", "/tmp/sam_ch97_out", 0, NULL);
    struct lex_process* lp = lex_process_create(cp, &compiler_lex_functions, 0);
    lex(lp);
    cp->token_vec = lex_process_tokens(lp);
    parse(cp);
    struct node** pp = vector_at(cp->node_tree_vec, 0);
    struct node* var = *pp;
    printf("type=%d struct_node=%p\n",
        var->var.type.type,
        (void*)var->var.type.struct_node);
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>&1 | head -5
[ -x "$bin" ] || fail "ch97 probe failed to compile"
got="$("$bin")"
# DATA_TYPE_STRUCT == 7 in our enum; we just check the struct_node is
# no longer NULL after fixups resolve.
case "$got" in
    *"struct_node=0x"*) ;;
    *"struct_node=(nil)"*) fail "fixup did not resolve: $got" ;;
    *) fail "unexpected output: $got" ;;
esac
pass
