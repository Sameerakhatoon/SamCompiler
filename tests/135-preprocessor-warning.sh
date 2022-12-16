#!/usr/bin/env bash
# Ch205: implementing #warning. preprocessor_token_is_warning gates
# preprocessor_handle_warning_token which builds a buffer of the
# rest-of-line tokens via preprocessor_multi_value_string and hands
# it to preprocessor_execute_warning (which prepends `#warning ` and
# calls compiler_warning).
#
# Test: feed `#warning hello \n` and confirm compiler_warning is
# invoked (stderr contains `#warning hello`).
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

probe=$(mktemp /tmp/sam_ch205_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch205_bin.XXXXXX)
trap 'rm -f "$probe" "$bin"' EXIT

cat > "$probe" <<'EOF'
#include <stdio.h>
#include "compiler.h"
#include "helpers/vector.h"

int preprocessor_run(struct compile_process* compiler);

int main(void){
    struct compile_process* cp = compile_process_create("/dev/null", NULL, 0, NULL);
    if (!cp){ printf("create=null\n"); return 0; }
    // Init pos so the warning emitter has something safe to print.
    cp->pos.line = 1;
    cp->pos.col  = 1;
    cp->pos.filename = "test";

    struct token tk;
    tk.type = TOKEN_TYPE_SYMBOL;     tk.cval = '#';      vector_push(cp->token_vec_original, &tk);
    tk.type = TOKEN_TYPE_IDENTIFIER; tk.sval = "warning";vector_push(cp->token_vec_original, &tk);
    tk.type = TOKEN_TYPE_IDENTIFIER; tk.sval = "hello";  vector_push(cp->token_vec_original, &tk);
    tk.type = TOKEN_TYPE_NEWLINE;                         vector_push(cp->token_vec_original, &tk);

    preprocessor_run(cp);
    printf("run_done\n");
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>/dev/null || fail "ch205 probe failed to compile"
combined="$("$bin" 2>&1)"
assert_contains "$combined" "#warning hello" "preprocessor #warning emits the message via compiler_warning"
assert_contains "$combined" "run_done"         "preprocessor_run returns cleanly after handling #warning"
pass
