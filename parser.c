#include "compiler.h"
#include "helpers/vector.h"

static void          parser_ignore_nl_or_comment(struct token* token);
static struct token* token_next(void);
static struct token* token_peek_next(void);
static void          parse_single_token_to_node(void);
static int           parse_next(void);

static struct compile_process* current_process;
static struct token*           parser_last_token;

// Advance the parser's peek pointer past newlines / comments / line-
// continuation symbols, which the parser never cares about.
static void parser_ignore_nl_or_comment(struct token* token){
    while(token && token_is_nl_or_comment_or_newline_seperator(token)){
        vector_peek(current_process->token_vec);
        token = vector_peek_no_increment(current_process->token_vec);
    }
}

static struct token* token_next(void){
    struct token* next_token = vector_peek_no_increment(current_process->token_vec);
    parser_ignore_nl_or_comment(next_token);
    if(next_token){
        current_process->pos = next_token->pos;
    }
    parser_last_token = next_token;
    return vector_peek(current_process->token_vec);
}

static struct token* token_peek_next(void){
    struct token* next_token = vector_peek_no_increment(current_process->token_vec);
    parser_ignore_nl_or_comment(next_token);
    return vector_peek_no_increment(current_process->token_vec);
}

// Trivial 1-token-to-1-node conversion. NUMBER / IDENTIFIER / STRING
// each become their matching NODE_TYPE_* with the value copied over.
static void parse_single_token_to_node(void){
    struct token* token = token_next();
    switch(token->type){
        case TOKEN_TYPE_NUMBER:
            node_create(&(struct node){ .type = NODE_TYPE_NUMBER,     .llnum = token->llnum });
            break;
        case TOKEN_TYPE_IDENTIFIER:
            node_create(&(struct node){ .type = NODE_TYPE_IDENTIFIER, .sval  = token->sval  });
            break;
        case TOKEN_TYPE_STRING:
            node_create(&(struct node){ .type = NODE_TYPE_STRING,     .sval  = token->sval  });
            break;
        default:
            compiler_error(current_process, "This is not a single token that can be converted to a node");
    }
}

static int parse_next(void){
    struct token* token = token_peek_next();
    if(!token){
        return -1;
    }

    int res = 0;
    switch(token->type){
        case TOKEN_TYPE_NUMBER:
        case TOKEN_TYPE_IDENTIFIER:
        case TOKEN_TYPE_STRING:
            parse_single_token_to_node();
            break;
        default:
            // ch28+ will dispatch operators, symbols, keywords etc.
            // For now, anything else stops the loop cleanly.
            return -1;
    }
    return res;
}

int parse(struct compile_process* process){
    current_process   = process;
    parser_last_token = 0;
    node_set_vector(process->node_vec, process->node_tree_vec);

    struct node* node = 0;
    vector_set_peek_pointer(process->token_vec, 0);
    while(parse_next() == 0){
        node = node_peek();
        vector_push(process->node_tree_vec, &node);
    }
    return PARSE_ALL_OK;
}
