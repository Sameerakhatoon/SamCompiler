#include <stdlib.h>
#include <string.h>
#include "compiler.h"
#include "helpers/vector.h"

// Stage 1: the target-independent part of -O1.
// It runs on the validated tree, before the code generator, and only
// makes changes that keep the program's meaning exactly:
//   - integer expressions whose operands are constants are folded
//     (the parser folds only where C needs a constant), as long as the
//     result is what the generated code would compute: nothing with a
//     floating operand, no division by zero or INT_MIN / -1, no shift by
//     the width or more, no ?: whose branches have different types;
//   - x + 0, x - 0, x | 0, x ^ 0, x * 1, x / 1 and shifts by 0 become x,
//     x * 0 and x & 0 become 0 when x is a variable that is not volatile
//     (reading it has no effect); x is an integer variable, or a pointer
//     for + 0 and - 0, so the identities hold as they are written;
//   - if, while, for and do-while with a constant condition keep only
//     the code that can run, unless that code has a label or a case;
//   - statements after return, break, continue or goto in the same
//     block are dropped up to the next label, case or declaration;
//   - a static function nothing refers to is not generated.
// Stage 3, register allocation, is here too (regalloc_function, below):
// the 64-bit backends ask it which locals live in registers.
// The volatile objects of the file are also found here, from the tokens
// (the parser drops the qualifier): the peephole pass of stage 2 keeps
// the reloads in a function that has one.

static struct compile_process* opt_process;
// The variables in scope (struct node*), innermost last.
static struct vector*          opt_scope;
// Names declared with volatile anywhere, the functions that have a
// volatile local or parameter, and those whose value fits one register
// (const char*).
static struct vector*          opt_volatile_names;
static struct vector*          opt_volatile_functions;
static struct vector*          opt_narrow_functions;
static struct node*            opt_current_function;

static bool opt_name_listed(struct vector* names, const char* name){
    for(int i = 0; names && i < vector_count(names); i++){
        if(S_EQ(*(const char**)vector_at(names, i), name)){
            return true;
        }
    }
    return false;
}

static void opt_list_name(struct vector* names, const char* name){
    if(!opt_name_listed(names, name)){
        vector_push(names, &name);
    }
}

bool opt_function_has_volatile(const char* name){
    return opt_name_listed(opt_volatile_functions, name);
}

bool opt_function_returns_narrow(const char* name){
    return opt_name_listed(opt_narrow_functions, name);
}

// A return value in one integer register at most: no struct or union
// (which may come back in two) and no long double.
static bool opt_returns_narrow(struct node* function){
    struct datatype* type = &function->func.rtype;
    bool pointer = (type->flags & DATATYPE_FLAG_IS_POINTER) && type->pointer_depth > 0;
    return pointer || !(datatype_is_struct_or_union(type) || datatype_is_long_double(type));
}

// ---------------------------------------------------------------
// volatile
// ---------------------------------------------------------------

static bool opt_token_is(struct token* token, const char* text){
    if(token->type == TOKEN_TYPE_SYMBOL){
        return !text[1] && token->cval == text[0];
    }
    return (token->type == TOKEN_TYPE_OPERATOR || token->type == TOKEN_TYPE_KEYWORD) && S_EQ(token->sval, text);
}

static bool opt_token_is_volatile(struct token* token){
    if(token->type == TOKEN_TYPE_KEYWORD){
        return S_EQ(token->sval, "volatile");
    }
    return token->type == TOKEN_TYPE_IDENTIFIER && (S_EQ(token->sval, "__volatile__") || S_EQ(token->sval, "__volatile"));
}

static bool opt_token_is_asm(struct token* token){
    return (token->type == TOKEN_TYPE_KEYWORD || token->type == TOKEN_TYPE_IDENTIFIER)
        && (S_EQ(token->sval, "asm") || S_EQ(token->sval, "__asm__") || S_EQ(token->sval, "__asm"));
}

// Every name from a `volatile` to the end of its declaration (the `;`,
// or the `)` closing a parameter list) counts as volatile. That takes
// in more names than the volatile objects (the type's name, the other
// parameters), which only means fewer reloads are dropped.
static void opt_find_volatile_names(struct compile_process* process){
    struct vector* tokens = process->token_vec;
    int count = vector_count(tokens);
    for(int i = 0; i < count; i++){
        struct token* token = vector_at(tokens, i);
        if(!opt_token_is_volatile(token) || (i > 0 && opt_token_is_asm(vector_at(tokens, i - 1)))){
            continue;
        }
        int depth = 0;
        for(int j = i + 1; j < count; j++){
            struct token* next = vector_at(tokens, j);
            if(opt_token_is(next, "(") || opt_token_is(next, "[") || opt_token_is(next, "{")){
                // A function body ends the declaration.
                if(depth == 0 && opt_token_is(next, "{") && opt_token_is(vector_at(tokens, j - 1), ")")){
                    break;
                }
                depth++;
            } else if(opt_token_is(next, ")") || opt_token_is(next, "]") || opt_token_is(next, "}")){
                if(depth == 0){
                    break;
                }
                depth--;
            } else if(depth == 0 && opt_token_is(next, ";")){
                break;
            } else if(next->type == TOKEN_TYPE_IDENTIFIER){
                opt_list_name(opt_volatile_names, next->sval);
            }
        }
    }
}

// ---------------------------------------------------------------
// Scope, for the types of the variables an identity applies to
// ---------------------------------------------------------------

static void opt_declare(struct node* var){
    if(!var || var->type != NODE_TYPE_VARIABLE || !var->var.name){
        return;
    }
    vector_push(opt_scope, &var);
    if(opt_current_function && opt_name_listed(opt_volatile_names, var->var.name)){
        opt_list_name(opt_volatile_functions, opt_current_function->func.name);
    }
}

static struct node* opt_lookup(const char* name){
    for(int i = vector_count(opt_scope) - 1; i >= 0; i--){
        struct node* var = *(struct node**)vector_at(opt_scope, i);
        if(S_EQ(var->var.name, name)){
            return var;
        }
    }
    return 0;
}

// The scope is a stack: leaving a block drops what it declared.
static int opt_scope_begin(void){
    return vector_count(opt_scope);
}

static void opt_scope_end(int mark){
    while(vector_count(opt_scope) > mark){
        vector_pop(opt_scope);
    }
}

// ---------------------------------------------------------------
// Expressions
// ---------------------------------------------------------------

static struct node* opt_strip_parentheses(struct node* node){
    while(node && node->type == NODE_TYPE_EXPRESSION_PARENTHESES){
        node = node->parenthesis.exp;
    }
    return node;
}

// An integer constant: a number (or one in parentheses) that is not
// floating.
static bool opt_is_constant(struct node* node){
    node = opt_strip_parentheses(node);
    return node && node->type == NODE_TYPE_NUMBER
        && !(node->num.flags & (NUMBER_FLAG_FLOATING | NUMBER_FLAG_FLOAT | NUMBER_FLAG_LONG_DOUBLE));
}

static bool opt_constant_value(struct node* node, struct constant* out){
    return opt_is_constant(node) && constant_fold(opt_process, opt_strip_parentheses(node), out);
}

// A constant in the place of `old`.
static struct node* opt_constant_node(struct node* old, struct constant* c){
    struct node* node = constant_node(c);
    node->pos    = old->pos;
    node->binded = old->binded;
    return node;
}

static long long opt_signed(struct constant* c){
    return (long long)c->value;
}

static bool opt_is_arithmetic_operator(const char* op){
    static const char* ops[] = { "+", "-", "*", "/", "%", "<<", ">>", "&", "|", "^",
                                 "==", "!=", "<", ">", "<=", ">=" };
    for(size_t i = 0; i < sizeof(ops) / sizeof(ops[0]); i++){
        if(S_EQ(op, ops[i])){
            return true;
        }
    }
    return false;
}

// Whether `left op right`, both constants, folds to what the generated
// code computes. A division by zero, INT_MIN / -1 and a shift by the
// width or more are left to run (and trap, or give what the machine
// gives), as gcc leaves them to run.
static bool opt_binary_is_exact(const char* op, struct constant* left, struct constant* right){
    if(S_EQ(op, "/") || S_EQ(op, "%")){
        if(right->value == 0){
            return false;
        }
        long long l = opt_signed(left);
        if(opt_signed(right) == -1 && (l == -2147483647LL - 1 || l == -9223372036854775807LL - 1)){
            return false;
        }
    }
    if(S_EQ(op, "<<") || S_EQ(op, ">>")){
        struct datatype promoted = datatype_promote(&left->type);
        long long bits = (long long)datatype_element_size(&promoted) * 8;
        if(opt_signed(right) < 0 || opt_signed(right) >= bits){
            return false;
        }
    }
    return true;
}

// A plain int literal (no suffix, int range) with the value v: the
// constant side of an identity, which does not change x's type.
static bool opt_is_int_literal(struct node* node, unsigned long long v){
    node = opt_strip_parentheses(node);
    if(!opt_is_constant(node) || node->llnum != v){
        return false;
    }
    struct datatype type = datatype_for_number_node(node);
    return type.size == DATA_SIZE_DWORD && (type.flags & DATATYPE_FLAG_IS_SIGNED) && type.type != DATA_TYPE_LONG;
}

enum {
    OPT_CLASS_OTHER,
    OPT_CLASS_INTEGER,
    OPT_CLASS_POINTER,
};

// The kind of value a plain variable holds; OTHER for anything else.
static int opt_variable_class(struct node* node, struct node** var_out){
    node = opt_strip_parentheses(node);
    if(!node || node->type != NODE_TYPE_IDENTIFIER){
        return OPT_CLASS_OTHER;
    }
    struct node* var = opt_lookup(node->sval);
    if(!var){
        return OPT_CLASS_OTHER;
    }
    *var_out = var;
    struct datatype* type = &var->var.type;
    if(type->flags & (DATATYPE_FLAG_IS_ARRAY | DATATYPE_FLAG_IS_FUNCTION_POINTER)){
        return OPT_CLASS_OTHER;
    }
    if((type->flags & DATATYPE_FLAG_IS_POINTER) && type->pointer_depth > 0){
        return OPT_CLASS_POINTER;
    }
    if(datatype_is_floating(type) || type->atomic_level){
        return OPT_CLASS_OTHER;
    }
    switch(type->type){
        case DATA_TYPE_CHAR:
        case DATA_TYPE_SHORT:
        case DATA_TYPE_INTEGER:
        case DATA_TYPE_LONG:
            return OPT_CLASS_INTEGER;
    }
    return OPT_CLASS_OTHER;
}

// x op c or c op x, with c a plain int literal: what it simplifies to,
// or NULL. The result has x's own type rather than x promoted, which
// only differs for a char or short x, where every use promotes it again.
static struct node* opt_identity(struct node* node){
    const char* op = node->exp.op;
    struct node* left = node->exp.left;
    struct node* right = node->exp.right;
    struct node* var = 0;
    int left_class = opt_variable_class(left, &var);
    int right_class = opt_variable_class(right, &var);
    bool additive = S_EQ(op, "+") || S_EQ(op, "-");
    // A volatile x is read by `x + 0`, and may not be by `x` alone (a
    // statement of just a name generates nothing).
    if(var && opt_name_listed(opt_volatile_names, var->var.name)){
        return 0;
    }

    // x op 0 and x op 1 that give x.
    if((left_class == OPT_CLASS_INTEGER || (left_class == OPT_CLASS_POINTER && additive))
       && ((opt_is_int_literal(right, 0) && (additive || S_EQ(op, "|") || S_EQ(op, "^") || S_EQ(op, "<<") || S_EQ(op, ">>")))
           || (opt_is_int_literal(right, 1) && (S_EQ(op, "*") || S_EQ(op, "/"))))){
        return left;
    }
    // 0 + x, 0 | x, 0 ^ x and 1 * x.
    if((right_class == OPT_CLASS_INTEGER || (right_class == OPT_CLASS_POINTER && S_EQ(op, "+")))
       && ((opt_is_int_literal(left, 0) && (S_EQ(op, "+") || S_EQ(op, "|") || S_EQ(op, "^")))
           || (opt_is_int_literal(left, 1) && S_EQ(op, "*")))){
        return right;
    }
    // x * 0, 0 * x, x & 0 and 0 & x are 0 of x's promoted type, when
    // reading x does nothing.
    if(S_EQ(op, "*") || S_EQ(op, "&")){
        struct node* x = 0;
        if(left_class == OPT_CLASS_INTEGER && opt_is_int_literal(right, 0)){
            x = left;
        } else if(right_class == OPT_CLASS_INTEGER && opt_is_int_literal(left, 0)){
            x = right;
        }
        if(x){
            struct node* xvar = 0;
            opt_variable_class(x, &xvar);
            if(xvar){
                struct constant zero = { 0, datatype_promote(&xvar->var.type) };
                return opt_constant_node(node, &zero);
            }
        }
    }
    return 0;
}

static struct node* opt_expression(struct node* node);

// A unary, cast or binary operator of constants as one constant.
static struct node* opt_fold(struct node* node){
    struct constant c;
    switch(node->type){
        case NODE_TYPE_UNARY: {
            const char* op = node->unary.op;
            if(!(S_EQ(op, "-") || S_EQ(op, "~") || S_EQ(op, "!")) || !opt_is_constant(node->unary.operand)){
                return node;
            }
            break;
        }
        case NODE_TYPE_CAST: {
            struct datatype* type = &node->cast.dtype;
            // A cast to _Bool or a pointer, or a floating one, keeps its
            // code: constant_fold does not give _Bool its 0/1.
            if(!opt_is_constant(node->cast.operand) || datatype_is_floating(type) || datatype_is_struct_or_union(type)
               || (type->flags & (DATATYPE_FLAG_IS_POINTER | DATATYPE_FLAG_IS_BOOL | DATATYPE_FLAG_IS_ARRAY
                                  | DATATYPE_FLAG_IS_FUNCTION_POINTER))
               || type->type == DATA_TYPE_VOID){
                return node;
            }
            break;
        }
        case NODE_TYPE_EXPRESSION: {
            const char* op = node->exp.op;
            struct constant left;
            struct constant right;
            if(S_EQ(op, "&&") || S_EQ(op, "||")){
                if(!opt_constant_value(node->exp.left, &left)){
                    return node;
                }
                // `0 && x` and `1 || x` do not evaluate x.
                bool l = left.value != 0;
                if(S_EQ(op, "&&") ? !l : l){
                    struct constant result = { l, datatype_for_int() };
                    return opt_constant_node(node, &result);
                }
                if(!opt_is_constant(node->exp.right)){
                    return node;
                }
                break;
            }
            if(S_EQ(op, "?")){
                struct node* choice = node->exp.right;
                if(!opt_is_constant(node->exp.left) || !choice || choice->type != NODE_TYPE_TENARY
                   || !opt_is_constant(choice->tenary.true_node) || !opt_is_constant(choice->tenary.false_node)){
                    return node;
                }
                break;
            }
            if(!opt_is_arithmetic_operator(op)){
                return node;
            }
            if(!opt_constant_value(node->exp.left, &left) || !opt_constant_value(node->exp.right, &right)){
                struct node* simpler = opt_identity(node);
                return simpler ? simpler : node;
            }
            if(!opt_binary_is_exact(op, &left, &right)){
                return node;
            }
            break;
        }
        default:
            return node;
    }
    if(!constant_fold(opt_process, node, &c) || !constant_is_integer(&c)
       || (c.type.flags & DATATYPE_FLAG_IS_POINTER)){
        return node;
    }
    return opt_constant_node(node, &c);
}

// The expression with its constant parts folded; the parentheses
// themselves stay (a call's arguments are a parentheses node).
static struct node* opt_expression(struct node* node){
    if(!node){
        return node;
    }
    compiler_check_stack(opt_process);
    switch(node->type){
        case NODE_TYPE_EXPRESSION:
            node->exp.left = opt_expression(node->exp.left);
            node->exp.right = opt_expression(node->exp.right);
            return opt_fold(node);
        case NODE_TYPE_EXPRESSION_PARENTHESES:
            node->parenthesis.exp = opt_expression(node->parenthesis.exp);
            return node;
        case NODE_TYPE_UNARY:
            node->unary.operand = opt_expression(node->unary.operand);
            return opt_fold(node);
        case NODE_TYPE_CAST:
            node->cast.operand = opt_expression(node->cast.operand);
            return opt_fold(node);
        case NODE_TYPE_TENARY:
            node->tenary.true_node = opt_expression(node->tenary.true_node);
            node->tenary.false_node = opt_expression(node->tenary.false_node);
            return node;
        case NODE_TYPE_BRACKET:
            node->bracket.inner = opt_expression(node->bracket.inner);
            return node;
    }
    return node;
}

static bool opt_is_expression(struct node* node){
    switch(node->type){
        case NODE_TYPE_EXPRESSION:
        case NODE_TYPE_EXPRESSION_PARENTHESES:
        case NODE_TYPE_UNARY:
        case NODE_TYPE_CAST:
        case NODE_TYPE_TENARY:
        case NODE_TYPE_IDENTIFIER:
        case NODE_TYPE_NUMBER:
            return true;
    }
    return false;
}

// ---------------------------------------------------------------
// Statements
// ---------------------------------------------------------------

// A label, case, default or inline asm in the statement: code that may
// be reached from elsewhere (or define a symbol) is never dropped.
static bool opt_has_target(struct node* node){
    if(!node){
        return false;
    }
    switch(node->type){
        case NODE_TYPE_LABEL:
        case NODE_TYPE_STATEMENT_CASE:
        case NODE_TYPE_STATEMENT_DEFAULT:
        case NODE_TYPE_ASM:
            return true;
        case NODE_TYPE_BODY: {
            struct vector* statements = node->body.statements;
            for(int i = 0; i < vector_count(statements); i++){
                if(opt_has_target(*(struct node**)vector_at(statements, i))){
                    return true;
                }
            }
            return false;
        }
        case NODE_TYPE_STATEMENT_IF:
            return opt_has_target(node->stmt.if_stmt.body_node) || opt_has_target(node->stmt.if_stmt.next);
        case NODE_TYPE_STATEMENT_ELSE:
            return opt_has_target(node->stmt.else_stmt.body_node);
        case NODE_TYPE_STATEMENT_WHILE:
            return opt_has_target(node->stmt.while_stmt.body_node);
        case NODE_TYPE_STATEMENT_DO_WHILE:
            return opt_has_target(node->stmt.do_while_stmt.body_node);
        case NODE_TYPE_STATEMENT_FOR:
            return opt_has_target(node->stmt.for_stmt.body_node);
        case NODE_TYPE_STATEMENT_SWITCH:
            return opt_has_target(node->stmt.switch_stmt.body);
    }
    return false;
}

// A break or continue that leaves the loop whose body this is (one
// inside a nested loop belongs to that loop; a continue inside a nested
// switch still belongs to this one).
static bool opt_leaves_loop(struct node* node, bool in_switch){
    if(!node){
        return false;
    }
    switch(node->type){
        case NODE_TYPE_STATEMENT_BREAK:
            return !in_switch;
        case NODE_TYPE_STATEMENT_CONTINUE:
            return true;
        case NODE_TYPE_BODY: {
            struct vector* statements = node->body.statements;
            for(int i = 0; i < vector_count(statements); i++){
                if(opt_leaves_loop(*(struct node**)vector_at(statements, i), in_switch)){
                    return true;
                }
            }
            return false;
        }
        case NODE_TYPE_STATEMENT_IF:
            return opt_leaves_loop(node->stmt.if_stmt.body_node, in_switch) || opt_leaves_loop(node->stmt.if_stmt.next, in_switch);
        case NODE_TYPE_STATEMENT_ELSE:
            return opt_leaves_loop(node->stmt.else_stmt.body_node, in_switch);
        case NODE_TYPE_STATEMENT_SWITCH:
            return opt_leaves_loop(node->stmt.switch_stmt.body, true);
    }
    return false;
}

static bool opt_is_jump(struct node* node){
    return node->type == NODE_TYPE_STATEMENT_RETURN || node->type == NODE_TYPE_STATEMENT_BREAK
        || node->type == NODE_TYPE_STATEMENT_CONTINUE || node->type == NODE_TYPE_STATEMENT_GOTO;
}

// A statement after a jump that may go: not a declaration (a later
// statement past a label may use the name), nor one with a target.
static bool opt_is_removable(struct node* node){
    switch(node->type){
        case NODE_TYPE_VARIABLE:
        case NODE_TYPE_VARIABLE_LIST:
        case NODE_TYPE_STRUCT:
        case NODE_TYPE_UNION:
        case NODE_TYPE_FUNCTION:
            return false;
    }
    return !opt_has_target(node);
}

// The value of a constant condition: 1 true, 0 false, -1 not constant.
static int opt_condition(struct node* node){
    struct constant c;
    if(!node || !opt_constant_value(node, &c)){
        return -1;
    }
    return c.value != 0;
}

static struct node* opt_statement(struct node* node);

// an expression whose value nobody uses (a statement, a for
// loop's step): x++ and x-- there are ++x and --x, which do not work
// out the old value.
static void opt_value_unused(struct node* node){
    node = opt_strip_parentheses(node);
    if(!node){
        return;
    }
    if(node->type == NODE_TYPE_UNARY && is_increment_operator(node->unary.op)){
        node->unary.flags &= ~UNARY_FLAG_IS_POSTFIX;
    } else if(node->type == NODE_TYPE_EXPRESSION && S_EQ(node->exp.op, ",")){
        opt_value_unused(node->exp.left);
        opt_value_unused(node->exp.right);
    }
}

static void opt_body(struct node* body){
    int mark = opt_scope_begin();
    struct vector* statements = body->body.statements;
    struct vector* kept = vector_create(sizeof(struct node*));
    bool unreachable = false;
    for(int i = 0; i < vector_count(statements); i++){
        struct node* statement = *(struct node**)vector_at(statements, i);
        if(unreachable){
            if(opt_is_removable(statement)){
                continue;
            }
            if(opt_has_target(statement)){
                unreachable = false;
            }
        }
        struct node* result = opt_statement(statement);
        if(result){
            vector_push(kept, &result);
            if(opt_is_jump(result)){
                unreachable = true;
            }
        }
    }
    vector_clear(statements);
    for(int i = 0; i < vector_count(kept); i++){
        vector_push(statements, vector_at(kept, i));
    }
    opt_scope_end(mark);
}

static void opt_variable(struct node* var){
    if(var->type != NODE_TYPE_VARIABLE){
        return;
    }
    // A static local's initializer was folded by the parser; an
    // automatic one is code like any other expression.
    if(var->var.val && !(var->var.type.flags & DATATYPE_FLAG_IS_STATIC) && opt_is_expression(var->var.val)){
        var->var.val = opt_expression(var->var.val);
    }
    opt_declare(var);
}

// if / else if / else. Returns what the statement becomes: the taken
// branch for a constant condition, else the statement itself; NULL when
// nothing is left.
static struct node* opt_if(struct node* node){
    struct if_stmt* s = &node->stmt.if_stmt;
    s->cond_node = opt_expression(s->cond_node);
    s->body_node = opt_statement(s->body_node);
    if(s->next){
        if(s->next->type == NODE_TYPE_STATEMENT_IF){
            struct node* next = opt_if(s->next);
            if(next && next->type != NODE_TYPE_STATEMENT_IF && next->type != NODE_TYPE_STATEMENT_ELSE){
                // `else if(1) body` is `else body`.
                s->next->type = NODE_TYPE_STATEMENT_ELSE;
                s->next->stmt.else_stmt.body_node = next;
                next = s->next;
            }
            s->next = next;
        } else if(s->next->type == NODE_TYPE_STATEMENT_ELSE){
            s->next->stmt.else_stmt.body_node = opt_statement(s->next->stmt.else_stmt.body_node);
        }
    }
    int cond = opt_condition(s->cond_node);
    if(cond == 1 && !opt_has_target(s->next)){
        return s->body_node;
    }
    if(cond == 0 && !opt_has_target(s->body_node)){
        if(!s->next){
            return 0;
        }
        if(s->next->type == NODE_TYPE_STATEMENT_ELSE){
            return s->next->stmt.else_stmt.body_node;
        }
        return s->next;
    }
    // An if whose body went away keeps an empty one.
    if(!s->body_node){
        make_body_node(vector_create(sizeof(struct node*)), 0, false, 0);
        s->body_node = node_pop();
    }
    return node;
}

static struct node* opt_statement(struct node* node){
    if(!node){
        return node;
    }
    compiler_check_stack(opt_process);
    switch(node->type){
        case NODE_TYPE_EXPRESSION:
        case NODE_TYPE_EXPRESSION_PARENTHESES:
        case NODE_TYPE_UNARY:
        case NODE_TYPE_CAST: {
            struct node* result = opt_expression(node);
            opt_value_unused(result);
            // A constant on its own does nothing.
            return result->type == NODE_TYPE_NUMBER ? 0 : result;
        }
        case NODE_TYPE_VARIABLE:
            opt_variable(node);
            return node;
        case NODE_TYPE_VARIABLE_LIST:
            for(int i = 0; i < vector_count(node->var_list.list); i++){
                opt_variable(*(struct node**)vector_at(node->var_list.list, i));
            }
            return node;
        case NODE_TYPE_STRUCT:
            opt_statement(node->_struct.var);
            return node;
        case NODE_TYPE_UNION:
            opt_statement(node->_union.var);
            return node;
        case NODE_TYPE_BODY:
            opt_body(node);
            return node;
        case NODE_TYPE_STATEMENT_RETURN:
            node->stmt.return_stmt.exp = opt_expression(node->stmt.return_stmt.exp);
            return node;
        case NODE_TYPE_STATEMENT_IF:
            return opt_if(node);
        case NODE_TYPE_STATEMENT_WHILE: {
            struct while_stmt* s = &node->stmt.while_stmt;
            s->exp_node = opt_expression(s->exp_node);
            s->body_node = opt_statement(s->body_node);
            if(opt_condition(s->exp_node) == 0 && !opt_has_target(s->body_node)){
                return 0;
            }
            return node;
        }
        case NODE_TYPE_STATEMENT_DO_WHILE: {
            struct do_while_stmt* s = &node->stmt.do_while_stmt;
            s->body_node = opt_statement(s->body_node);
            s->exp_node = opt_expression(s->exp_node);
            // do { ... } while(0) runs its body once.
            if(opt_condition(s->exp_node) == 0 && s->body_node && !opt_has_target(s->body_node)
               && !opt_leaves_loop(s->body_node, false)){
                return s->body_node;
            }
            return node;
        }
        case NODE_TYPE_STATEMENT_FOR: {
            struct for_stmt* s = &node->stmt.for_stmt;
            int mark = opt_scope_begin();
            if(s->init_node && (s->init_node->type == NODE_TYPE_VARIABLE || s->init_node->type == NODE_TYPE_VARIABLE_LIST)){
                opt_statement(s->init_node);
            } else {
                s->init_node = opt_expression(s->init_node);
            }
            s->cond_node = opt_expression(s->cond_node);
            s->loop_node = opt_expression(s->loop_node);
            opt_value_unused(s->loop_node);
            s->body_node = opt_statement(s->body_node);
            opt_scope_end(mark);
            if(opt_condition(s->cond_node) == 0 && !opt_has_target(s->body_node)){
                if(!s->init_node){
                    return 0;
                }
                if(opt_is_expression(s->init_node)){
                    return s->init_node->type == NODE_TYPE_NUMBER ? 0 : s->init_node;
                }
            }
            return node;
        }
        case NODE_TYPE_STATEMENT_SWITCH:
            node->stmt.switch_stmt.exp = opt_expression(node->stmt.switch_stmt.exp);
            node->stmt.switch_stmt.body = opt_statement(node->stmt.switch_stmt.body);
            return node;
    }
    return node;
}

// ---------------------------------------------------------------
// Unused static functions
// ---------------------------------------------------------------

// The names a part of the tree refers to (any identifier, whatever it
// names), and the texts of its inline asm, which may name a function.
struct opt_names {
    struct vector* names;
    struct vector* asms;
};

static void opt_references(struct node* node, struct opt_names* names);

static void opt_references_type(struct datatype* type, struct opt_names* names){
    if(!(type->flags & DATATYPE_FLAG_IS_ARRAY) || !type->array.brackets || !type->array.brackets->n_brackets){
        return;
    }
    struct vector* brackets = type->array.brackets->n_brackets;
    for(int i = 0; i < vector_count(brackets); i++){
        opt_references(*(struct node**)vector_at(brackets, i), names);
    }
}

static void opt_references_vector(struct vector* nodes, struct opt_names* names){
    for(int i = 0; nodes && i < vector_count(nodes); i++){
        opt_references(*(struct node**)vector_at(nodes, i), names);
    }
}

static void opt_references(struct node* node, struct opt_names* names){
    if(!node){
        return;
    }
    compiler_check_stack(opt_process);
    switch(node->type){
        case NODE_TYPE_IDENTIFIER:
            vector_push(names->names, &node->sval);
            break;
        case NODE_TYPE_ASM:
            vector_push(names->asms, &node->sval);
            break;
        case NODE_TYPE_EXPRESSION:
            opt_references(node->exp.left, names);
            opt_references(node->exp.right, names);
            break;
        case NODE_TYPE_EXPRESSION_PARENTHESES:
            opt_references(node->parenthesis.exp, names);
            break;
        case NODE_TYPE_UNARY:
            opt_references(node->unary.operand, names);
            break;
        case NODE_TYPE_CAST:
            opt_references_type(&node->cast.dtype, names);
            opt_references(node->cast.operand, names);
            break;
        case NODE_TYPE_TENARY:
            opt_references(node->tenary.true_node, names);
            opt_references(node->tenary.false_node, names);
            break;
        case NODE_TYPE_BRACKET:
            opt_references(node->bracket.inner, names);
            break;
        case NODE_TYPE_VARIABLE:
            opt_references_type(&node->var.type, names);
            opt_references(node->var.val, names);
            break;
        case NODE_TYPE_VARIABLE_LIST:
            opt_references_vector(node->var_list.list, names);
            break;
        case NODE_TYPE_INITIALIZER:
            for(int i = 0; node->init.leaves && i < vector_count(node->init.leaves); i++){
                struct initializer_leaf* leaf = vector_at(node->init.leaves, i);
                opt_references(leaf->value, names);
            }
            break;
        case NODE_TYPE_STRUCT:
            opt_references(node->_struct.var, names);
            break;
        case NODE_TYPE_UNION:
            opt_references(node->_union.var, names);
            break;
        case NODE_TYPE_FUNCTION:
            opt_references(node->func.body_n, names);
            break;
        case NODE_TYPE_BODY:
            opt_references_vector(node->body.statements, names);
            break;
        case NODE_TYPE_STATEMENT_RETURN:
            opt_references(node->stmt.return_stmt.exp, names);
            break;
        case NODE_TYPE_STATEMENT_IF:
            opt_references(node->stmt.if_stmt.cond_node, names);
            opt_references(node->stmt.if_stmt.body_node, names);
            opt_references(node->stmt.if_stmt.next, names);
            break;
        case NODE_TYPE_STATEMENT_ELSE:
            opt_references(node->stmt.else_stmt.body_node, names);
            break;
        case NODE_TYPE_STATEMENT_WHILE:
            opt_references(node->stmt.while_stmt.exp_node, names);
            opt_references(node->stmt.while_stmt.body_node, names);
            break;
        case NODE_TYPE_STATEMENT_DO_WHILE:
            opt_references(node->stmt.do_while_stmt.exp_node, names);
            opt_references(node->stmt.do_while_stmt.body_node, names);
            break;
        case NODE_TYPE_STATEMENT_FOR:
            opt_references(node->stmt.for_stmt.init_node, names);
            opt_references(node->stmt.for_stmt.cond_node, names);
            opt_references(node->stmt.for_stmt.loop_node, names);
            opt_references(node->stmt.for_stmt.body_node, names);
            break;
        case NODE_TYPE_STATEMENT_SWITCH:
            opt_references(node->stmt.switch_stmt.exp, names);
            opt_references(node->stmt.switch_stmt.body, names);
            break;
    }
}

static bool opt_is_word_char(char c){
    return c == '_' || c == '$' || (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static bool opt_is_static_definition(struct node* node){
    return node->type == NODE_TYPE_FUNCTION && node->func.body_n
        && (node->func.rtype.flags & DATATYPE_FLAG_IS_STATIC);
}

// Whether `name` is among the names, or a word of an asm text.
static bool opt_referenced(struct opt_names* names, const char* name){
    if(opt_name_listed(names->names, name)){
        return true;
    }
    size_t len = strlen(name);
    for(int i = 0; i < vector_count(names->asms); i++){
        const char* text = *(const char**)vector_at(names->asms, i);
        for(const char* at = strstr(text, name); at; at = strstr(at + 1, name)){
            char before = at == text ? ' ' : at[-1];
            char after = at[len];
            if(!opt_is_word_char(before) && !opt_is_word_char(after)){
                return true;
            }
        }
    }
    return false;
}

// Everything but the static function definitions is used, and so is
// what a used part refers to. The static definitions left over are
// dropped from the tree; their prototypes stay (they generate nothing
// a program can see).
static void opt_drop_unused_static_functions(struct compile_process* process){
    struct vector* tree = process->node_tree_vec;
    struct opt_names names = { vector_create(sizeof(const char*)), vector_create(sizeof(const char*)) };
    int count = vector_count(tree);
    bool* used = calloc(count + 1, sizeof(bool));
    for(int i = 0; i < count; i++){
        struct node* node = *(struct node**)vector_at(tree, i);
        if(!opt_is_static_definition(node)){
            used[i] = true;
            opt_references(node, &names);
        }
    }
    bool more = true;
    while(more){
        more = false;
        for(int i = 0; i < count; i++){
            struct node* node = *(struct node**)vector_at(tree, i);
            if(!used[i] && opt_referenced(&names, node->func.name)){
                used[i] = true;
                opt_references(node, &names);
                more = true;
            }
        }
    }
    for(int i = count - 1; i >= 0; i--){
        if(!used[i]){
            vector_pop_at(tree, i);
        }
    }
    free(used);
}

// ---------------------------------------------------------------
// Stage 3: register allocation
// ---------------------------------------------------------------

// The 64-bit backends ask, as they start a function, which of its
// scalar locals and parameters live in one of their callee-saved
// registers for the whole function instead of in a stack slot. A
// variable can have one when nothing reaches it but its name: an
// integer or a pointer that is not static, extern, thread-local,
// _Atomic or volatile, whose address is never taken (`&x`, or a builtin
// such as va_start, which works on the object), in a function without
// inline asm and without setjmp. The ones used most get the registers
// (a use in a loop counts 8 times, in a loop in a loop 64); one used
// fewer than OPT_RA_MIN_WEIGHT times is not worth the save and the
// restore.

#define OPT_RA_MAX        16
#define OPT_RA_MIN_WEIGHT 3

struct opt_ra_candidate {
    struct node* var;
    long         weight;
    bool         taken;
};

// A declaration in scope while the function is walked, and its
// candidate (-1 for one that cannot have a register).
struct opt_ra_declaration {
    struct node* var;
    int          candidate;
};

static struct compile_process* opt_ra_process;
static struct vector*          opt_ra_candidates;
static struct vector*          opt_ra_scope;
static struct vector*          opt_ra_taken;
static bool                    opt_ra_refused;
// The current function's: the variable in register k and the
// register's DWARF number.
static struct node*            opt_ra_vars[OPT_RA_MAX];
static int                     opt_ra_dwarf[OPT_RA_MAX];
static int                     opt_ra_count;

static bool opt_ra_type_ok(struct datatype* type){
    int excluded = DATATYPE_FLAG_IS_ARRAY | DATATYPE_FLAG_IS_STATIC | DATATYPE_FLAG_IS_EXTERN | DATATYPE_FLAG_IS_THREAD_LOCAL;
    if((type->flags & excluded) || datatype_is_atomic(type)){
        return false;
    }
    if((type->flags & DATATYPE_FLAG_IS_POINTER) && type->pointer_depth > 0){
        return true;
    }
    switch(type->type){
        case DATA_TYPE_CHAR:
        case DATA_TYPE_SHORT:
        case DATA_TYPE_INTEGER:
        case DATA_TYPE_LONG:
            return type->size == 1 || type->size == 2 || type->size == 4 || type->size == 8;
    }
    return false;
}

// A use inside n loops weighs 8^n (up to 8^4).
static long opt_ra_weight(int depth){
    return 1L << (3 * (depth < 4 ? depth : 4));
}

static struct opt_ra_candidate* opt_ra_candidate_at(int index){
    return index >= 0 ? vector_at(opt_ra_candidates, index) : 0;
}

static struct opt_ra_declaration* opt_ra_declare(struct node* var, bool may_have_register){
    struct opt_ra_declaration declaration = { var, -1 };
    if(may_have_register && var->var.name && opt_ra_type_ok(&var->var.type)){
        struct opt_ra_candidate candidate = { var, 0, false };
        vector_push(opt_ra_candidates, &candidate);
        declaration.candidate = vector_count(opt_ra_candidates) - 1;
    }
    vector_push(opt_ra_scope, &declaration);
    return vector_at(opt_ra_scope, vector_count(opt_ra_scope) - 1);
}

static struct opt_ra_declaration* opt_ra_lookup(const char* name){
    for(int i = vector_count(opt_ra_scope) - 1; i >= 0; i--){
        struct opt_ra_declaration* declaration = vector_at(opt_ra_scope, i);
        if(declaration->var->var.name && S_EQ(declaration->var->var.name, name)){
            return declaration;
        }
    }
    return 0;
}

static void opt_ra_scope_end(int mark){
    while(vector_count(opt_ra_scope) > mark){
        vector_pop(opt_ra_scope);
    }
}

// Every name in a builtin's arguments counts as taken: va_start, va_arg
// and the atomics work on the object, not on its value.
static void opt_ra_take_names(struct node* node){
    struct opt_names names = { vector_create(sizeof(const char*)), vector_create(sizeof(const char*)) };
    opt_references(node, &names);
    for(int i = 0; i < vector_count(names.names); i++){
        vector_push(opt_ra_taken, vector_at(names.names, i));
    }
}

static bool opt_ra_returns_twice(const char* name){
    return strstr(name, "setjmp") || S_EQ(name, "vfork") || S_EQ(name, "getcontext") || S_EQ(name, "savectx");
}

static bool opt_ra_is_builtin(const char* name){
    return strncmp(name, "__builtin", 9) == 0 || strncmp(name, "va_", 3) == 0 || atomic_builtin_get(name)
        || symresolver_get_symbol_for_native_function(opt_ra_process, name);
}

static void opt_ra_walk(struct node* node, int depth);

static void opt_ra_walk_vector(struct vector* nodes, int depth){
    for(int i = 0; nodes && i < vector_count(nodes); i++){
        opt_ra_walk(*(struct node**)vector_at(nodes, i), depth);
    }
}

// A declaration: the sizes of a VLA, the name, then the initializer,
// whose store is a use.
static void opt_ra_variable(struct node* var, bool may_have_register, int depth){
    if(!var || var->type != NODE_TYPE_VARIABLE){
        opt_ra_walk(var, depth);
        return;
    }
    struct datatype* type = &var->var.type;
    if((type->flags & DATATYPE_FLAG_IS_ARRAY) && type->array.brackets && type->array.brackets->n_brackets){
        opt_ra_walk_vector(type->array.brackets->n_brackets, depth);
    }
    struct opt_ra_candidate* candidate = opt_ra_candidate_at(opt_ra_declare(var, may_have_register)->candidate);
    if(var->var.val){
        if(candidate){
            candidate->weight += opt_ra_weight(depth);
        }
        opt_ra_walk(var->var.val, depth);
    }
}

static void opt_ra_walk(struct node* node, int depth){
    if(!node){
        return;
    }
    compiler_check_stack(opt_ra_process);
    switch(node->type){
        case NODE_TYPE_IDENTIFIER: {
            struct opt_ra_declaration* declaration = opt_ra_lookup(node->sval);
            struct opt_ra_candidate* candidate = declaration ? opt_ra_candidate_at(declaration->candidate) : 0;
            if(candidate){
                candidate->weight += opt_ra_weight(depth);
            }
            break;
        }
        case NODE_TYPE_ASM:
            opt_ra_refused = true;
            break;
        case NODE_TYPE_EXPRESSION:
            if(is_parentheses_node(node)){
                struct node* callee = opt_strip_parentheses(node->exp.left);
                if(callee && callee->type == NODE_TYPE_IDENTIFIER && !opt_ra_lookup(callee->sval)){
                    if(opt_ra_returns_twice(callee->sval)){
                        opt_ra_refused = true;
                    }
                    if(opt_ra_is_builtin(callee->sval)){
                        opt_ra_take_names(node->exp.right);
                    }
                }
            }
            opt_ra_walk(node->exp.left, depth);
            opt_ra_walk(node->exp.right, depth);
            break;
        case NODE_TYPE_EXPRESSION_PARENTHESES:
            opt_ra_walk(node->parenthesis.exp, depth);
            break;
        case NODE_TYPE_UNARY: {
            struct node* operand = opt_strip_parentheses(node->unary.operand);
            if(op_is_address(node->unary.op) && operand && operand->type == NODE_TYPE_IDENTIFIER){
                vector_push(opt_ra_taken, &operand->sval);
            }
            opt_ra_walk(node->unary.operand, depth);
            break;
        }
        case NODE_TYPE_CAST:
            opt_ra_walk(node->cast.operand, depth);
            break;
        case NODE_TYPE_TENARY:
            opt_ra_walk(node->tenary.true_node, depth);
            opt_ra_walk(node->tenary.false_node, depth);
            break;
        case NODE_TYPE_BRACKET:
            opt_ra_walk(node->bracket.inner, depth);
            break;
        case NODE_TYPE_VARIABLE:
            opt_ra_variable(node, true, depth);
            break;
        case NODE_TYPE_VARIABLE_LIST:
            for(int i = 0; i < vector_count(node->var_list.list); i++){
                opt_ra_variable(*(struct node**)vector_at(node->var_list.list, i), true, depth);
            }
            break;
        case NODE_TYPE_STRUCT:
            opt_ra_variable(node->_struct.var, false, depth);
            break;
        case NODE_TYPE_UNION:
            opt_ra_variable(node->_union.var, false, depth);
            break;
        case NODE_TYPE_BODY: {
            int mark = vector_count(opt_ra_scope);
            opt_ra_walk_vector(node->body.statements, depth);
            opt_ra_scope_end(mark);
            break;
        }
        case NODE_TYPE_STATEMENT_RETURN:
            opt_ra_walk(node->stmt.return_stmt.exp, depth);
            break;
        case NODE_TYPE_STATEMENT_IF:
            opt_ra_walk(node->stmt.if_stmt.cond_node, depth);
            opt_ra_walk(node->stmt.if_stmt.body_node, depth);
            opt_ra_walk(node->stmt.if_stmt.next, depth);
            break;
        case NODE_TYPE_STATEMENT_ELSE:
            opt_ra_walk(node->stmt.else_stmt.body_node, depth);
            break;
        case NODE_TYPE_STATEMENT_WHILE:
            opt_ra_walk(node->stmt.while_stmt.exp_node, depth + 1);
            opt_ra_walk(node->stmt.while_stmt.body_node, depth + 1);
            break;
        case NODE_TYPE_STATEMENT_DO_WHILE:
            opt_ra_walk(node->stmt.do_while_stmt.exp_node, depth + 1);
            opt_ra_walk(node->stmt.do_while_stmt.body_node, depth + 1);
            break;
        case NODE_TYPE_STATEMENT_FOR: {
            int mark = vector_count(opt_ra_scope);
            opt_ra_walk(node->stmt.for_stmt.init_node, depth);
            opt_ra_walk(node->stmt.for_stmt.cond_node, depth + 1);
            opt_ra_walk(node->stmt.for_stmt.loop_node, depth + 1);
            opt_ra_walk(node->stmt.for_stmt.body_node, depth + 1);
            opt_ra_scope_end(mark);
            break;
        }
        case NODE_TYPE_STATEMENT_SWITCH:
            opt_ra_walk(node->stmt.switch_stmt.exp, depth);
            opt_ra_walk(node->stmt.switch_stmt.body, depth);
            break;
    }
}

int regalloc_function(struct compile_process* process, struct node* function, int registers, const int* dwarf){
    opt_ra_count = 0;
    if(!(process->flags & COMPILE_PROCESS_OPTIMIZE) || !function->func.body_n || opt_function_has_volatile(function->func.name)){
        return 0;
    }
    opt_ra_process    = process;
    opt_ra_candidates = vector_create(sizeof(struct opt_ra_candidate));
    opt_ra_scope      = vector_create(sizeof(struct opt_ra_declaration));
    opt_ra_taken      = vector_create(sizeof(const char*));
    opt_ra_refused    = false;
    struct vector* args = function->func.args.vector;
    for(int i = 0; args && i < vector_count(args); i++){
        struct node* param = *(struct node**)vector_at(args, i);
        if(param && param->type == NODE_TYPE_VARIABLE){
            opt_ra_declare(param, true);
        }
    }
    opt_ra_walk(function->func.body_n, 0);
    if(opt_ra_refused){
        return 0;
    }
    if(registers > OPT_RA_MAX){
        registers = OPT_RA_MAX;
    }
    int count = vector_count(opt_ra_candidates);
    for(int i = 0; i < count; i++){
        struct opt_ra_candidate* candidate = vector_at(opt_ra_candidates, i);
        candidate->taken = opt_name_listed(opt_ra_taken, candidate->var->var.name);
    }
    // The heaviest first; of equal ones the one declared first.
    while(opt_ra_count < registers){
        struct opt_ra_candidate* best = 0;
        for(int i = 0; i < count; i++){
            struct opt_ra_candidate* candidate = vector_at(opt_ra_candidates, i);
            if(!candidate->taken && candidate->weight >= OPT_RA_MIN_WEIGHT && (!best || candidate->weight > best->weight)){
                best = candidate;
            }
        }
        if(!best){
            break;
        }
        best->taken = true;
        opt_ra_dwarf[opt_ra_count] = dwarf ? dwarf[opt_ra_count] : -1;
        opt_ra_vars[opt_ra_count++] = best->var;
    }
    return opt_ra_count;
}

int regalloc_register(struct node* var){
    for(int k = 0; k < opt_ra_count; k++){
        if(opt_ra_vars[k] == var){
            return k;
        }
    }
    return -1;
}

int regalloc_dwarf_register(struct node* var){
    int k = regalloc_register(var);
    return k >= 0 ? opt_ra_dwarf[k] : -1;
}

// ---------------------------------------------------------------
// Entry
// ---------------------------------------------------------------

void optimize(struct compile_process* process){
    opt_process            = process;
    opt_scope              = vector_create(sizeof(struct node*));
    opt_volatile_names     = vector_create(sizeof(const char*));
    opt_volatile_functions = vector_create(sizeof(const char*));
    opt_narrow_functions   = vector_create(sizeof(const char*));
    opt_current_function   = 0;
    opt_find_volatile_names(process);

    struct vector* tree = process->node_tree_vec;
    for(int i = 0; i < vector_count(tree); i++){
        struct node* node = *(struct node**)vector_at(tree, i);
        if(node->type == NODE_TYPE_VARIABLE){
            opt_declare(node);
        } else if(node->type == NODE_TYPE_VARIABLE_LIST){
            for(int k = 0; k < vector_count(node->var_list.list); k++){
                opt_declare(*(struct node**)vector_at(node->var_list.list, k));
            }
        } else if(node->type == NODE_TYPE_FUNCTION && node->func.body_n){
            opt_current_function = node;
            if(opt_returns_narrow(node)){
                opt_list_name(opt_narrow_functions, node->func.name);
            }
            int mark = opt_scope_begin();
            struct vector* args = node->func.args.vector;
            for(int k = 0; args && k < vector_count(args); k++){
                opt_declare(*(struct node**)vector_at(args, k));
            }
            opt_statement(node->func.body_n);
            opt_scope_end(mark);
            opt_current_function = 0;
        }
    }
    opt_drop_unused_static_functions(process);
}
