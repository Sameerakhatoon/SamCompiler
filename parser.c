#include "compiler.h"
#include "helpers/vector.h"

static int parse_next(void);

// Module-private: the compile_process the parser is currently chewing on.
static struct compile_process* current_process;

static int parse_next(void){
    // ch25 stub. Returns -1 to terminate the loop immediately, so we
    // don't infinite-loop pushing NULL into node_tree_vec. Real
    // dispatch arrives in ch26+.
    return -1;
}

int parse(struct compile_process* process){
    current_process = process;

    struct node* node = 0;
    vector_set_peek_pointer(process->token_vec, 0);
    while(parse_next() == 0){
        // node = node_peek();  // ch27 wires this up.
        vector_push(process->node_tree_vec, &node);
    }
    return PARSE_ALL_OK;
}
