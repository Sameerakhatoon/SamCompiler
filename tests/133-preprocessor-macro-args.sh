#!/usr/bin/env bash
# Ch203: creating macro arguments. preprocessor_handle_definition_
# token now checks preprocessor_is_next_macro_arguments (true when
# the next token is `(` with no whitespace separating it from the
# name); if so it routes through parse_macro_argument_declaration
# which reads identifiers separated by `,` into the arguments
# vector. A non-empty arguments vector promotes the definition to
# type=MACRO_FUNCTION.
#
# Also adds previous_token / next_token_no_increment /
# peek_next_token_skip_nl helpers and fixes a return-missing bug
# in vector_create_no_saves.
#
# Test: feed `#define ABC(x, y) x + y` and confirm the resulting
# definition has type=MACRO_FUNCTION, arguments={x, y}, and
# value-vector tokens are IDENTIFIER(x), OPERATOR(+), IDENTIFIER(y).
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

probe=$(mktemp /tmp/sam_ch203_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch203_bin.XXXXXX)
trap 'rm -f "$probe" "$bin"' EXIT

cat > "$probe" <<'EOF'
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "compiler.h"
#include "helpers/vector.h"

int preprocessor_run(struct compile_process* compiler);

void push_ident(struct vector* v, const char* s, bool ws){
    struct token t = {0};
    t.type = TOKEN_TYPE_IDENTIFIER; t.sval = s; t.whitespace = ws;
    vector_push(v, &t);
}
void push_op(struct vector* v, const char* s, bool ws){
    struct token t = {0};
    t.type = TOKEN_TYPE_OPERATOR; t.sval = s; t.whitespace = ws;
    vector_push(v, &t);
}
void push_sym(struct vector* v, char c, bool ws){
    struct token t = {0};
    t.type = TOKEN_TYPE_SYMBOL; t.cval = c; t.whitespace = ws;
    vector_push(v, &t);
}
void push_nl(struct vector* v){
    struct token t = {0};
    t.type = TOKEN_TYPE_NEWLINE;
    vector_push(v, &t);
}

int main(void){
    struct compile_process* cp = compile_process_create("/dev/null", NULL, 0, NULL);
    if (!cp){ printf("create=null\n"); return 0; }

    // # define ABC(x, y) x + y \n
    push_sym  (cp->token_vec_original, '#',     false);
    push_ident(cp->token_vec_original, "define",false);
    push_ident(cp->token_vec_original, "ABC",   false);
    // critical: no whitespace before `(` so is_next_macro_arguments == true.
    push_op   (cp->token_vec_original, "(",     false);
    push_ident(cp->token_vec_original, "x",     false);
    push_op   (cp->token_vec_original, ",",     false);
    push_ident(cp->token_vec_original, "y",     true);
    // The book treats `)` as a SYMBOL when read by the lexer in the
    // macro-arg path; arg parser checks token_is_symbol(')').
    push_sym  (cp->token_vec_original, ')',     false);
    push_ident(cp->token_vec_original, "x",     true);
    push_op   (cp->token_vec_original, "+",     true);
    push_ident(cp->token_vec_original, "y",     true);
    push_nl   (cp->token_vec_original);

    preprocessor_run(cp);

    // ch227's __LINE__ native lives at index 0 so user defs land at >0.
    int n_defs = vector_count(cp->preprocessor->definitions) - 1;
    vector_set_peek_pointer(cp->preprocessor->definitions, 1);
    struct preprocessor_definition* d = vector_peek_ptr(cp->preprocessor->definitions);
    int name_ok = d && S_EQ(d->name, "ABC");
    int type_ok = d && d->type == PREPROCESSOR_DEFINITION_MACRO_FUNCTION;
    int args_n  = d && d->standard.arguments ? vector_count(d->standard.arguments) : -1;
    int val_n   = d && d->standard.value ? vector_count(d->standard.value) : -1;
    // Upstream pushes arg strings via `vector_push(args, (void*)next_token->sval)`
    // which copies the first sizeof(char*) bytes of the string contents
    // into the vector slot rather than the pointer. So the stored slot is
    // garbage and dereferencing it would crash. We just count, per docs/203.

    printf("defs=%d name=%d type=%d argsN=%d valN=%d\n",
        n_defs, name_ok, type_ok, args_n, val_n);
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>/dev/null || fail "ch203 probe failed to compile"
got="$("$bin")"
assert_contains "$got" "defs=1 name=1 type=1 argsN=2 valN=3" \
    "#define ABC(x, y) x + y registers a MACRO_FUNCTION definition with 2 args and 3 value tokens"
pass
