#!/usr/bin/env bash
# Ch220: finishing the typedef directive - part 2. Adds support
# for struct / union typedefs.
#
# New helpers: handle_typedef_body_for_brackets (recursive {}
# walker that copies tokens including the closing brace),
# handle_typedef_body_for_struct_or_union (consumes `struct`,
# optional name, optional `{` body via for_brackets; declaration-
# only `typedef struct X Y;` short-circuits after pushing the
# referenced name).
#
# handle_typedef_body now routes struct / union to the new
# struct/union handler. handle_typedef_token: when the typedef
# is structural, pushes the captured body tokens through to
# token_vec, then a synthetic `;`, then replaces token_vec with
# a fresh keyword + identifier pair (`struct Name`) which
# becomes the TYPEDEF definition's value.
#
# New helper: preprocessor_token_push_semicolon.
#
# Test: feed `typedef struct Point { int x ; } P ; P p ;`
# - the structure body lands in token_vec (struct Point { int x ; })
#   followed by a synthetic `;`, the typedef stores `struct Point`
#   as the value of P. Then `P p ;` expands to `struct Point p ;`.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

probe=$(mktemp /tmp/sam_ch220_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch220_bin.XXXXXX)
trap 'rm -f "$probe" "$bin"' EXIT

cat > "$probe" <<'EOF'
#include <stdio.h>
#include "compiler.h"
#include "helpers/vector.h"

int preprocessor_run(struct compile_process* compiler);

void push_sym(struct vector* v, char c){ struct token t = {0}; t.type = TOKEN_TYPE_SYMBOL; t.cval = c; vector_push(v, &t); }
void push_id (struct vector* v, const char* s){ struct token t = {0}; t.type = TOKEN_TYPE_IDENTIFIER; t.sval = s; vector_push(v, &t); }
void push_kw (struct vector* v, const char* s){ struct token t = {0}; t.type = TOKEN_TYPE_KEYWORD; t.sval = s; vector_push(v, &t); }
void push_nl (struct vector* v){ struct token t = {0}; t.type = TOKEN_TYPE_NEWLINE; vector_push(v, &t); }

int main(void){
    struct compile_process* cp = compile_process_create("/dev/null", NULL, 0, NULL);
    // typedef struct Point { int x ; } P ;
    push_kw (cp->token_vec_original, "typedef");
    push_kw (cp->token_vec_original, "struct");
    push_id (cp->token_vec_original, "Point");
    push_sym(cp->token_vec_original, '{');
    push_kw (cp->token_vec_original, "int");
    push_id (cp->token_vec_original, "x");
    push_sym(cp->token_vec_original, ';');
    push_sym(cp->token_vec_original, '}');
    push_id (cp->token_vec_original, "P");
    push_sym(cp->token_vec_original, ';');
    push_nl (cp->token_vec_original);
    // P p ;
    push_id (cp->token_vec_original, "P");
    push_id (cp->token_vec_original, "p");
    push_sym(cp->token_vec_original, ';');
    push_nl (cp->token_vec_original);

    preprocessor_run(cp);

    // ch227's __LINE__ native lives at index 0 so user defs land at >0.
    int n_defs = vector_count(cp->preprocessor->definitions) - 1;
    vector_set_peek_pointer(cp->preprocessor->definitions, 1);
    struct preprocessor_definition* d = vector_peek_ptr(cp->preprocessor->definitions);
    int name_ok = d && S_EQ(d->name, "P");
    int type_ok = d && d->type == PREPROCESSOR_DEFINITION_TYPEDEF;

    int n = vector_count(cp->token_vec);
    // Walk token_vec to confirm the body was emitted then synthetic ;
    // followed by the expanded `struct Point p ;`.
    int saw_struct_kw   = 0;
    int saw_point_ident = 0;
    int saw_brace_open  = 0;
    int saw_brace_close = 0;
    int saw_p_ident     = 0;
    int saw_P_ident     = 0;
    for (int i = 0; i < n; i++){
        struct token* t = vector_at(cp->token_vec, i);
        if (!t) continue;
        if (t->type == TOKEN_TYPE_KEYWORD    && t->sval && S_EQ(t->sval, "struct")) saw_struct_kw++;
        if (t->type == TOKEN_TYPE_IDENTIFIER && t->sval && S_EQ(t->sval, "Point"))  saw_point_ident++;
        if (t->type == TOKEN_TYPE_SYMBOL && t->cval == '{') saw_brace_open++;
        if (t->type == TOKEN_TYPE_SYMBOL && t->cval == '}') saw_brace_close++;
        if (t->type == TOKEN_TYPE_IDENTIFIER && t->sval && S_EQ(t->sval, "p")) saw_p_ident++;
        if (t->type == TOKEN_TYPE_IDENTIFIER && t->sval && S_EQ(t->sval, "P")) saw_P_ident++;
    }
    printf("defs=%d name=%d type=%d struct=%d Point=%d openB=%d closeB=%d p=%d P=%d\n",
        n_defs, name_ok, type_ok, saw_struct_kw, saw_point_ident,
        saw_brace_open, saw_brace_close, saw_p_ident, saw_P_ident);
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>/dev/null || fail "ch220 probe failed to compile"
got="$("$bin")"
# Expected:
#   - 1 def, name=P, type=TYPEDEF
#   - body emitted: `struct Point { int x ; }` (struct kw 1+1=2 because expansion adds another, Point 1+1=2, {1, }1)
#   - then `;` (synthetic)
#   - then expansion: `struct Point p ;` (adds another struct kw, another Point, no P remains)
#   - p ident appears once, P ident appears 0 times.
assert_contains "$got" "defs=1 name=1 type=1 struct=2 Point=2 openB=1 closeB=1 p=1 P=0" \
    "typedef struct Point { ... } P; emits the body then expands P to struct Point on use"
pass
