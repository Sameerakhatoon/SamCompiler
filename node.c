#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "compiler.h"
#include "helpers/vector.h"

static void node_set_vector_internal(struct vector* vec, struct vector* root_vec);

// Module-private: the two vectors the node helpers operate on. Set once
// per parse() pass via node_set_vector. Owned by compile_process.
static struct vector* node_vector      = 0;
static struct vector* node_vector_root = 0;

void node_set_vector(struct vector* vec, struct vector* root_vec){
    node_set_vector_internal(vec, root_vec);
}

static void node_set_vector_internal(struct vector* vec, struct vector* root_vec){
    node_vector      = vec;
    node_vector_root = root_vec;
}

void node_push(struct node* node){
    vector_push(node_vector, &node);
}

struct node* node_peek_or_null(void){
    return vector_back_ptr_or_null(node_vector);
}

struct node* node_peek(void){
    return *(struct node**)(vector_back(node_vector));
}

// Pop from the scratch vector. If the popped node also happens to sit
// at the top of node_vector_root (the AST-roots vector), pop it from
// there too - keeps the two vectors consistent when the parser walks
// back over a finished top-level node.
struct node* node_pop(void){
    struct node* last_node      = vector_back_ptr(node_vector);
    struct node* last_node_root = vector_empty(node_vector_root) ? 0 : vector_back_ptr_or_null(node_vector_root);

    vector_pop(node_vector);

    if(last_node == last_node_root){
        vector_pop(node_vector_root);
    }
    return last_node;
}

bool node_is_expressionable(struct node* node){
    return node
        && (node->type == NODE_TYPE_EXPRESSION
         || node->type == NODE_TYPE_EXPRESSION_PARENTHESES
         || node->type == NODE_TYPE_UNARY
         || node->type == NODE_TYPE_IDENTIFIER
         || node->type == NODE_TYPE_NUMBER
         || node->type == NODE_TYPE_STRING);
}

struct node* node_peek_expressionable_or_null(void){
    struct node* last_node = node_peek_or_null();
    return node_is_expressionable(last_node) ? last_node : 0;
}

void make_exp_node(struct node* left_node, struct node* right_node, const char* op){
    assert(left_node);
    assert(right_node);
    node_create(&(struct node){
        .type      = NODE_TYPE_EXPRESSION,
        .exp.left  = left_node,
        .exp.right = right_node,
        .exp.op    = op,
    });
}

void make_bracket_node(struct node* inner){
    node_create(&(struct node){
        .type          = NODE_TYPE_BRACKET,
        .bracket.inner = inner,
    });
}

// ch49: the currently-being-parsed body node. The parser writes to it
// while consuming statements, and binded.owner restores the parent on
// exit. Exported (not static) because parser.c walks it directly.
struct node* parser_current_body     = 0;
// ch72: same idea for "which function are we inside".
struct node* parser_current_function = 0;

void make_body_node(struct vector* body_vec, size_t size, bool padded, struct node* largest_var_node){
    node_create(&(struct node){
        .type                  = NODE_TYPE_BODY,
        .body.statements       = body_vec,
        .body.size             = size,
        .body.padded           = padded,
        .body.largest_var_node = largest_var_node,
    });
}

void make_struct_node(const char* name, struct node* body_node){
    int flags = 0;
    if(!body_node){
        flags |= NODE_FLAG_IS_FORWARD_DECLARATION;
    }
    node_create(&(struct node){
        .type           = NODE_TYPE_STRUCT,
        ._struct.body_n = body_node,
        ._struct.name   = name,
        .flags          = flags,
    });
}

// ch99: NODE_TYPE_UNION mirror of make_struct_node.
void make_union_node(const char* name, struct node* body_node){
    int flags = 0;
    if(!body_node){
        flags |= NODE_FLAG_IS_FORWARD_DECLARATION;
    }
    node_create(&(struct node){
        .type          = NODE_TYPE_UNION,
        ._union.body_n = body_node,
        ._union.name   = name,
        .flags         = flags,
    });
}

// ch65: symbol -> node accessors. Lets the parser resolve `struct foo`
// references against previously-registered struct definitions.
struct node* node_from_sym(struct symbol* sym){
    if(sym->type != SYMBOL_TYPE_NODE){
        return 0;
    }
    return sym->data;
}

struct node* node_from_symbol(struct compile_process* process, const char* name){
    struct symbol* sym = symresolver_get_symbol(process, name);
    if(!sym){
        return 0;
    }
    return node_from_sym(sym);
}

struct node* struct_node_for_name(struct compile_process* process, const char* name){
    struct node* node = node_from_symbol(process, name);
    if(!node){
        return 0;
    }
    if(node->type != NODE_TYPE_STRUCT){
        return 0;
    }
    return node;
}

struct node* union_node_for_name(struct compile_process* process, const char* name){
    struct node* node = node_from_symbol(process, name);
    if(!node){
        return 0;
    }
    if(node->type != NODE_TYPE_UNION){
        return 0;
    }
    return node;
}

// ch72: build a NODE_TYPE_FUNCTION. args_vector / body_node may be
// NULL; the caller fills them in once parsed.
struct node* make_function_node(struct datatype* ret_type, const char* name,
                                struct vector* arguments, struct node* body_node){
    struct node* func_node = node_create(&(struct node){
        .type                  = NODE_TYPE_FUNCTION,
        .func.name             = name,
        .func.args.vector      = arguments,
        .func.body_n           = body_node,
        .func.rtype            = *ret_type,
        // Default stack_addition = sizeof(void*) for the return EIP.
        .func.args.stack_addition = DATA_SIZE_DDWORD,
    });
    return func_node;
}

// Copy the caller's stack-allocated node onto the heap, push onto the
// scratch stack, and return the heap pointer. TODO: set binded.owner
// and binded.function when the parser starts threading the AST.
struct node* node_create(struct node* _node){
    struct node* node = malloc(sizeof(struct node));
    memcpy(node, _node, sizeof(struct node));
    // ch72: stamp binded.owner / binded.function so every node knows
    // where it sits in the tree without the parser threading args.
    node->binded.owner    = parser_current_body;
    node->binded.function = parser_current_function;
    node_push(node);
    return node;
}

bool node_is_struct_or_union_variable(struct node* node){
    if(node->type != NODE_TYPE_VARIABLE){
        return false;
    }
    return datatype_is_struct_or_union(&node->var.type);
}

// ch124: structural test used by struct_offset.
bool node_is_struct_or_union(struct node* node){
    return node->type == NODE_TYPE_STRUCT || node->type == NODE_TYPE_UNION;
}

// Get the variable node behind a struct / union / variable. For
// NODE_TYPE_VARIABLE the node itself; for NODE_TYPE_STRUCT, the
// attached var pointer; UNION not yet implemented.
struct node* variable_node(struct node* node){
    switch(node->type){
        case NODE_TYPE_VARIABLE: return node;
        case NODE_TYPE_STRUCT:   return node->_struct.var;
        case NODE_TYPE_UNION:    return node->_union.var;
    }
    return 0;
}

bool variable_node_is_primitive(struct node* node){
    assert(node->type == NODE_TYPE_VARIABLE);
    return datatype_is_primitive(&node->var.type);
}

// VARIABLE_LIST passes straight through; everything else unwraps via
// variable_node.
struct node* variable_node_or_list(struct node* node){
    if(node->type == NODE_TYPE_VARIABLE_LIST){
        return node;
    }
    return variable_node(node);
}

// ch74: stack_addition is the byte gap between EBP and arg 0. Default
// from make_function_node is DWORD; +DWORD if return type is
// struct/union by value.
size_t function_node_argument_stack_addition(struct node* node){
    assert(node->type == NODE_TYPE_FUNCTION);
    return node->func.args.stack_addition;
}

void make_exp_parentheses_node(struct node* exp_node){
    node_create(&(struct node){
        .type            = NODE_TYPE_EXPRESSION_PARENTHESES,
        .parenthesis.exp = exp_node,
    });
}

void make_if_node(struct node* cond_node, struct node* body_node, struct node* next_node){
    node_create(&(struct node){
        .type                   = NODE_TYPE_STATEMENT_IF,
        .stmt.if_stmt.cond_node = cond_node,
        .stmt.if_stmt.body_node = body_node,
        .stmt.if_stmt.next      = next_node,
    });
}

void make_else_node(struct node* body_node){
    node_create(&(struct node){
        .type                     = NODE_TYPE_STATEMENT_ELSE,
        .stmt.else_stmt.body_node = body_node,
    });
}

void make_return_node(struct node* exp_node){
    node_create(&(struct node){
        .type                 = NODE_TYPE_STATEMENT_RETURN,
        .stmt.return_stmt.exp = exp_node,
    });
}

void make_for_node(struct node* init_node, struct node* cond_node,
                   struct node* loop_node, struct node* body_node){
    node_create(&(struct node){
        .type                    = NODE_TYPE_STATEMENT_FOR,
        .stmt.for_stmt.init_node = init_node,
        .stmt.for_stmt.cond_node = cond_node,
        .stmt.for_stmt.loop_node = loop_node,
        .stmt.for_stmt.body_node = body_node,
    });
}

void make_while_node(struct node* exp_node, struct node* body_node){
    node_create(&(struct node){
        .type                      = NODE_TYPE_STATEMENT_WHILE,
        .stmt.while_stmt.exp_node  = exp_node,
        .stmt.while_stmt.body_node = body_node,
    });
}

void make_do_while_node(struct node* body_node, struct node* exp_node){
    node_create(&(struct node){
        .type                         = NODE_TYPE_STATEMENT_DO_WHILE,
        .stmt.do_while_stmt.body_node = body_node,
        .stmt.do_while_stmt.exp_node  = exp_node,
    });
}

void make_switch_node(struct node* exp_node, struct node* body_node,
                      struct vector* cases, bool has_default_case){
    node_create(&(struct node){
        .type                              = NODE_TYPE_STATEMENT_SWITCH,
        .stmt.switch_stmt.exp              = exp_node,
        .stmt.switch_stmt.body             = body_node,
        .stmt.switch_stmt.cases            = cases,
        .stmt.switch_stmt.has_default_case = has_default_case,
    });
}

void make_case_node(struct node* exp_node){
    node_create(&(struct node){
        .type           = NODE_TYPE_STATEMENT_CASE,
        .stmt._case.exp = exp_node,
    });
}

void make_continue_node(void){
    node_create(&(struct node){ .type = NODE_TYPE_STATEMENT_CONTINUE });
}

void make_break_node(void){
    node_create(&(struct node){ .type = NODE_TYPE_STATEMENT_BREAK });
}

void make_goto_node(struct node* label_node){
    node_create(&(struct node){
        .type             = NODE_TYPE_STATEMENT_GOTO,
        .stmt._goto.label = label_node,
    });
}

void make_label_node(struct node* name_node){
    node_create(&(struct node){
        .type            = NODE_TYPE_LABEL,
        .stmt.label.name = name_node,
    });
}

void make_default_node(void){
    node_create(&(struct node){ .type = NODE_TYPE_STATEMENT_DEFAULT });
}

void make_tenary_node(struct node* true_node, struct node* false_node){
    node_create(&(struct node){
        .type              = NODE_TYPE_TENARY,
        .tenary.true_node  = true_node,
        .tenary.false_node = false_node,
    });
}

void make_cast_node(struct datatype* dtype, struct node* operand_node){
    node_create(&(struct node){
        .type         = NODE_TYPE_CAST,
        .cast.dtype   = *dtype,
        .cast.operand = operand_node,
    });
}

bool node_is_expression_or_parentheses(struct node* node){
    return node && (node->type == NODE_TYPE_EXPRESSION_PARENTHESES
                 || node->type == NODE_TYPE_EXPRESSION);
}

bool node_is_value_type(struct node* node){
    return node_is_expression_or_parentheses(node)
        || node->type == NODE_TYPE_IDENTIFIER
        || node->type == NODE_TYPE_NUMBER
        || node->type == NODE_TYPE_UNARY
        || node->type == NODE_TYPE_TENARY
        || node->type == NODE_TYPE_STRING;
}

// ch101: small classifiers used by the parser's extra reordering pass
// (array-subscript and assignment subtrees both need their nested
// EXPRESSION pulled to the left).
bool node_is_expression(struct node* node, const char* op){
    return node->type == NODE_TYPE_EXPRESSION && S_EQ(node->exp.op, op);
}

// ch127: is_array_node moved to helper.c alongside the other
// access-/array-/parens predicates.

bool is_node_assignment(struct node* node){
    if(node->type != NODE_TYPE_EXPRESSION){
        return false;
    }
    return S_EQ(node->exp.op, "=")
        || S_EQ(node->exp.op, "+=")
        || S_EQ(node->exp.op, "-=")
        || S_EQ(node->exp.op, "/=")
        || S_EQ(node->exp.op, "*=");
}
