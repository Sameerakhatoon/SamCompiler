#include <setjmp.h>
#include <stdlib.h>
#include <string.h>
#include "compiler.h"
#include "helpers/vector.h"

// ch243: scaffolded validator. Sits between parse and codegen so
// later chapters can hang checks (scope, types, statement shape,
// etc.) off this pipeline stage.
// ch244: validator-owned scope + tree iteration. The validator
// borrows the default resolver to track scope state while it walks
// the parse tree.
//
// Diagnostics: the validator walks every statement
// and every expression. It checks that each name is declared, works
// out the type of each expression far enough to reject what has no
// meaning (a member of an int, a call of an int, `*` of an int, the
// wrong number of arguments, a string literal stored in an int,
// `return;` in an int function, an assignment to a const object), and
// warns about unused locals, a missing return and a pointer stored in
// an integer. Where a type is not known for sure the check is skipped:
// a valid program must never be rejected here. An error ends only its
// statement; the walk goes on with the next one, and the count is
// printed at the end.

static struct compile_process* validator_current_compile_process;
static struct node*            current_function;
// The locals of the function being validated (struct node*), for the
// unused-variable warning, and whether it has inline asm (which may use
// them by name).
static struct vector*          validator_function_locals;
static bool                    validator_function_has_asm;
// Where an error in the statement being checked goes on.
static jmp_buf*                validator_recovery;

// ch257: forward decls so validate_statement can call back into
// validate_variable / validate_body without ordering pain.
void validate_variable(struct node* var_node);
void validate_body(struct body* body);
static void validate_statement_recovering(struct node* node);

// ch244: open/close a validator scope (delegates to the default
// resolver's scope manager so symbol lookups in the validator agree
// with what codegen will see).
void validation_new_scope(int flags)
{
    resolver_default_new_scope(validator_current_compile_process->resolver, flags);
}

void validation_end_scope(void)
{
    resolver_default_finish_scope(validator_current_compile_process->resolver);
}

// ch244: walk to the next top-level node in the parse tree.
struct node* validation_next_tree_node(void)
{
    return vector_peek_ptr(validator_current_compile_process->node_tree_vec);
}

void validate_initialize(struct compile_process* process)
{
    validator_current_compile_process = process;
    vector_set_peek_pointer(process->node_tree_vec, 0);
    symresolver_new_table(process);
}

void validate_destruct(struct compile_process* process)
{
    symresolver_end_table(process);
    vector_set_peek_pointer(process->node_tree_vec, 0);
}

static void validator_set_recovery(jmp_buf* point)
{
    validator_recovery = point;
    compiler_set_recovery(point);
}

// ch255: assert that no symbol with `name` is already registered. The
// caller passes a human-readable kind so the diagnostic is useful.
static bool validate_after_forward_declaration(const char* name);

void validate_symbol_unique(const char* name, const char* type_of_symbol, struct node* node)
{
    struct symbol* sym = symresolver_get_symbol(validator_current_compile_process, name);
    if (sym)
    {
        compiler_node_error(node, "Cannot define %s you have already defined a symbol with the name %s", type_of_symbol, name);
    }
}

// ---------------------------------------------------------------
// Expression types
// ---------------------------------------------------------------

// The type of an expression as far as the validator knows it. With
// known false nothing is checked against it.
struct vtype
{
    bool            known;
    struct datatype t;
    // A string literal: storing it in an integer is an error.
    bool            string;
    // A function named directly, which a call checks arguments against.
    struct node*    function;
};

static struct vtype vtype_unknown(void)
{
    struct vtype v = {0};
    return v;
}

static struct vtype vtype_of(struct datatype* t)
{
    struct vtype v = {0};
    v.known = true;
    v.t = *t;
    return v;
}

static struct vtype vtype_int(void)
{
    struct vtype v = {0};
    v.known = true;
    v.t.type = DATA_TYPE_INTEGER;
    v.t.type_str = "int";
    v.t.size = DATA_SIZE_DWORD;
    v.t.flags = DATATYPE_FLAG_IS_SIGNED;
    return v;
}

static bool vtype_is_array(struct datatype* t)
{
    return t->flags & DATATYPE_FLAG_IS_ARRAY;
}

static bool vtype_is_function_pointer(struct datatype* t)
{
    return t->flags & DATATYPE_FLAG_IS_FUNCTION_POINTER;
}

static bool vtype_is_pointer(struct datatype* t)
{
    return !vtype_is_array(t)
        && (vtype_is_function_pointer(t) || ((t->flags & DATATYPE_FLAG_IS_POINTER) && t->pointer_depth > 0));
}

// A pointer, an array (which decays to one) or a function.
static bool vtype_is_address(struct vtype* v)
{
    return v->known && (v->function || vtype_is_array(&v->t) || vtype_is_pointer(&v->t));
}

static bool vtype_is_struct(struct vtype* v)
{
    return v->known && !v->function && !vtype_is_array(&v->t) && !vtype_is_pointer(&v->t)
        && (v->t.type == DATA_TYPE_STRUCT || v->t.type == DATA_TYPE_UNION);
}

static bool vtype_is_integer(struct vtype* v)
{
    if (!v->known || v->function || vtype_is_array(&v->t) || vtype_is_pointer(&v->t))
    {
        return false;
    }
    switch (v->t.type)
    {
        case DATA_TYPE_CHAR:
        case DATA_TYPE_SHORT:
        case DATA_TYPE_INTEGER:
        case DATA_TYPE_LONG:
            return true;
    }
    return false;
}

// What `*` or `[]` of v gives; unknown where the element type is not
// simple (a multi-dimensional array).
static struct vtype vtype_deref(struct vtype* v)
{
    struct datatype t = v->t;
    if (vtype_is_array(&t))
    {
        if (t.array.brackets && array_brackets_count(&t) > 1)
        {
            return vtype_unknown();
        }
        t.flags &= ~DATATYPE_FLAG_IS_ARRAY;
        t.array.brackets = 0;
        t.array.size = 0;
        return vtype_of(&t);
    }
    if (vtype_is_function_pointer(&t))
    {
        return vtype_of(&t);
    }
    t.pointer_depth--;
    if (t.pointer_depth <= 0)
    {
        t.pointer_depth = 0;
        t.flags &= ~DATATYPE_FLAG_IS_POINTER;
    }
    return vtype_of(&t);
}

// An array used as a value is a pointer to its first element.
static struct vtype vtype_decay(struct vtype* v)
{
    if (!v->known || !vtype_is_array(&v->t))
    {
        return *v;
    }
    if (v->t.array.brackets && array_brackets_count(&v->t) > 1)
    {
        return vtype_unknown();
    }
    struct datatype t = datatype_array_decay(&v->t);
    return vtype_of(&t);
}

// The name of a type for a message: "int", "char*", "struct point".
static const char* vtype_name(struct vtype* v)
{
    static char text[2][96];
    static int which;
    char* out = text[which ^= 1];
    if (v->function || vtype_is_function_pointer(&v->t))
    {
        return "a function";
    }
    const char* base = v->t.type_str ? v->t.type_str : "?";
    const char* kind = v->t.type == DATA_TYPE_STRUCT ? "struct " : v->t.type == DATA_TYPE_UNION ? "union " : "";
    int depth = vtype_is_pointer(&v->t) ? v->t.pointer_depth : 0;
    snprintf(out, sizeof(text[0]), "%s%.60s%s%s%s%s", kind, base,
        depth > 0 ? "*" : "", depth > 1 ? "*" : "", depth > 2 ? "*" : "",
        vtype_is_array(&v->t) ? "[]" : "");
    return out;
}

static bool vtype_unchecked(struct vtype* v)
{
    return !v->known || (v->t.flags & DATATYPE_FLAG_IGNORE_TYPE_CHECKING);
}

// Storing `from` in an object of type `to` (an assignment, an
// initializer, a return, an argument). An integer from a string literal
// is an error (GCC 14 rejects any pointer there; we warn about the
// rest), and a struct from a scalar or the reverse is an error.
static void validate_conversion(struct node* at, struct datatype* to_type, struct vtype* from, const char* what)
{
    struct vtype to = vtype_of(to_type);
    if (vtype_unchecked(&to) || vtype_unchecked(from) || (to_type->flags & DATATYPE_FLAG_IS_BOOL))
    {
        return;
    }
    if (vtype_is_integer(&to) && vtype_is_address(from))
    {
        if (from->string)
        {
            compiler_node_error(at, "%s: a string literal cannot be stored in %s", what, vtype_name(&to));
        }
        compiler_node_warning(at, "%s makes %s from a pointer (%s) without a cast", what, vtype_name(&to), vtype_name(from));
        return;
    }
    if ((vtype_is_struct(&to) && (vtype_is_integer(from) || vtype_is_address(from)))
        || ((vtype_is_integer(&to) || vtype_is_pointer(&to.t)) && vtype_is_struct(from)))
    {
        compiler_node_error(at, "%s: incompatible types, %s from %s", what, vtype_name(&to), vtype_name(from));
    }
}

// An assignment, ++ or -- of `node`, whose type is v. A const object
// (const at the last level: `const int x`, `*p` for a `const int* p`)
// cannot change. A28: the store was silently dropped.
static void validate_modifiable(struct node* node, struct vtype* v)
{
    if (!v->known || v->function || vtype_is_array(&v->t) || vtype_is_pointer(&v->t))
    {
        return;
    }
    if (v->t.flags & DATATYPE_FLAG_IS_CONST)
    {
        if (node->type == NODE_TYPE_IDENTIFIER)
        {
            compiler_node_error(node, "Cannot assign to %s, it is const", node->sval);
        }
        compiler_node_error(node, "Cannot assign to a const object");
    }
}

static struct vtype validate_expression(struct node* node);

// ch257 (extended): a name in an expression must be a variable in
// scope, a function declared before it, or a native function. Before,
// an undeclared name went on to codegen, which asserted (x86) or said
// "y is not declared" at the end of the file.
static struct vtype validate_identifier(struct node* node)
{
    const char* name = node->sval;
    struct resolver_entity* entity = resolver_get_variable(0, validator_current_compile_process->resolver, name);
    if (entity)
    {
        if (entity->node)
        {
            entity->node->flags |= NODE_FLAG_IS_REFERENCED;
        }
        return vtype_of(&entity->dtype);
    }

    struct symbol* sym = symresolver_get_symbol(validator_current_compile_process, name);
    struct node* function = sym ? symresolver_node(sym) : 0;
    if (!function || function->type != NODE_TYPE_FUNCTION)
    {
        struct symbol* any = symresolver_get_symbol_any(validator_current_compile_process, name);
        if (any && any->type == SYMBOL_TYPE_NATIVE_FUNCTION)
        {
            return vtype_unknown();
        }
        compiler_node_error(node, "'%s' is not declared", name);
    }
    struct vtype v = vtype_of(&function->func.rtype);
    v.t.flags |= DATATYPE_FLAG_IS_POINTER | DATATYPE_FLAG_IS_FUNCTION_POINTER;
    v.t.pointer_depth++;
    v.function = function;
    return v;
}

static void validate_flatten_arguments(struct node* node, struct vector* out)
{
    if (!node || node->type == NODE_TYPE_BLANK)
    {
        return;
    }
    if (node->type == NODE_TYPE_EXPRESSION && S_EQ(node->exp.op, ","))
    {
        validate_flatten_arguments(node->exp.left, out);
        validate_flatten_arguments(node->exp.right, out);
        return;
    }
    vector_push(out, &node);
}

// A call. The callee must be a function or a function pointer; a
// function named directly takes as many arguments as its prototype
// says (more with `...`; any number with an empty `()`).
static struct vtype validate_call(struct node* node)
{
    struct node* callee = node->exp.left;
    while (callee->type == NODE_TYPE_EXPRESSION_PARENTHESES)
    {
        callee = callee->parenthesis.exp;
    }
    struct node* right = node->exp.right;
    if (right && right->type == NODE_TYPE_EXPRESSION_PARENTHESES)
    {
        right = right->parenthesis.exp;
    }
    struct vector* args = vector_create(sizeof(struct node*));
    validate_flatten_arguments(right, args);

    struct vtype callee_type = validate_expression(callee);
    int count = vector_count(args);
    struct vtype* arg_types = calloc(count + 1, sizeof(struct vtype));
    for (int i = 0; i < count; i++)
    {
        arg_types[i] = validate_expression(*(struct node**)vector_at(args, i));
    }

    struct node* function = callee_type.function;
    if (function)
    {
        struct vector* params = function->func.args.vector;
        int fixed = params ? vector_count(params) : 0;
        bool prototype = fixed > 0;
        if (fixed == 1)
        {
            struct node* only = *(struct node**)vector_at(params, 0);
            if (datatype_is_void_no_ptr(&only->var.type))
            {
                fixed = 0;
            }
        }
        bool variadic = function->func.flags & FUNCTION_NODE_FLAG_IS_VARIADIC;
        if (prototype && (count < fixed || (count > fixed && !variadic)))
        {
            compiler_node_error(node, "Too %s arguments in the call of %s: it takes %s%i, %i given",
                count < fixed ? "few" : "many", function->func.name, variadic ? "at least " : "", fixed, count);
        }
        for (int i = 0; prototype && i < fixed && i < count; i++)
        {
            struct node* param = *(struct node**)vector_at(params, i);
            char what[160];
            snprintf(what, sizeof(what), "Argument %i of %.100s", i + 1, function->func.name);
            validate_conversion(*(struct node**)vector_at(args, i), &param->var.type, &arg_types[i], what);
        }
        free(arg_types);
        return vtype_of(&function->func.rtype);
    }
    free(arg_types);
    if (vtype_unchecked(&callee_type))
    {
        return vtype_unknown();
    }
    if (!vtype_is_function_pointer(&callee_type.t))
    {
        compiler_node_error(node, "Called object is not a function or function pointer (it is %s)", vtype_name(&callee_type));
    }
    if (callee_type.t.function_return)
    {
        return vtype_of(callee_type.t.function_return);
    }
    return vtype_unknown();
}

// The member `name` of a struct or union body, looking inside `int a,
// b;` lists. *unsure is set when the body has members this search does
// not see into (an anonymous struct or union).
static struct node* validate_find_member(struct node* body, const char* name, bool* unsure)
{
    struct vector* statements = body->body.statements;
    for (int i = 0; i < vector_count(statements); i++)
    {
        struct node* member = *(struct node**)vector_at(statements, i);
        if (member->type == NODE_TYPE_VARIABLE)
        {
            if (!member->var.name)
            {
                *unsure = true;
            }
            else if (S_EQ(member->var.name, name))
            {
                return member;
            }
        }
        else if (member->type == NODE_TYPE_VARIABLE_LIST)
        {
            struct vector* list = member->var_list.list;
            for (int j = 0; j < vector_count(list); j++)
            {
                struct node* var = *(struct node**)vector_at(list, j);
                if (var->var.name && S_EQ(var->var.name, name))
                {
                    return var;
                }
            }
        }
        else
        {
            *unsure = true;
        }
    }
    return 0;
}

// The body of the struct or union type t, or NULL when it is not
// known here (an incomplete type, a name the validator cannot find).
static struct node* validate_aggregate_body(struct datatype* t)
{
    struct node* aggregate = t->struct_node;
    if ((!aggregate || !node_is_struct_or_union(aggregate) || !aggregate->_struct.body_n) && t->type_str)
    {
        struct symbol* sym = symresolver_get_symbol_any(validator_current_compile_process, t->type_str);
        aggregate = sym ? symresolver_node(sym) : 0;
    }
    if (!aggregate || !node_is_struct_or_union(aggregate))
    {
        return 0;
    }
    return aggregate->type == NODE_TYPE_UNION ? aggregate->_union.body_n : aggregate->_struct.body_n;
}

// `s.m` and `p->m`. Before, a member of an int was a bad address on
// x86, and `.` on a pointer was accepted.
static struct vtype validate_member(struct node* node)
{
    bool arrow = S_EQ(node->exp.op, "->");
    struct vtype base = validate_expression(node->exp.left);
    struct node* member_name = node->exp.right;
    if (!member_name || member_name->type != NODE_TYPE_IDENTIFIER)
    {
        compiler_node_error(node, "Expected a member name after '%s'", node->exp.op);
    }
    if (vtype_unchecked(&base))
    {
        return vtype_unknown();
    }
    struct vtype aggregate = base;
    if (arrow)
    {
        if (!vtype_is_address(&base) || base.function)
        {
            compiler_node_error(node, "'->%s' on %s, which is not a pointer", member_name->sval, vtype_name(&base));
        }
        aggregate = vtype_deref(&base);
        if (!aggregate.known)
        {
            return vtype_unknown();
        }
    }
    else if (vtype_is_address(&base))
    {
        compiler_node_error(node, "'.%s' on %s, which is not a struct; use '->' for a pointer", member_name->sval, vtype_name(&base));
    }
    if (!vtype_is_struct(&aggregate))
    {
        compiler_node_error(node, "Member %s of %s, which is not a struct or union", member_name->sval, vtype_name(&aggregate));
    }
    struct node* body = validate_aggregate_body(&aggregate.t);
    if (!body)
    {
        return vtype_unknown();
    }
    bool unsure = false;
    struct node* member = validate_find_member(body, member_name->sval, &unsure);
    if (!member)
    {
        if (unsure)
        {
            return vtype_unknown();
        }
        compiler_node_error(node, "%s has no member named %s", vtype_name(&aggregate), member_name->sval);
    }
    struct vtype result = vtype_of(&member->var.type);
    // A member of a const struct is const; only for a scalar or struct
    // member, since a single const flag cannot say which level of a
    // pointer it applies to.
    if ((aggregate.t.flags & DATATYPE_FLAG_IS_CONST) && !vtype_is_pointer(&result.t) && !vtype_is_array(&result.t))
    {
        result.t.flags |= DATATYPE_FLAG_IS_CONST;
    }
    return result;
}

static bool validate_is_compound_assignment(const char* op)
{
    return S_EQ(op, "+=") || S_EQ(op, "-=") || S_EQ(op, "*=") || S_EQ(op, "/=") || S_EQ(op, "%=")
        || S_EQ(op, "&=") || S_EQ(op, "|=") || S_EQ(op, "^=") || S_EQ(op, "<<=") || S_EQ(op, ">>=");
}

static bool validate_is_comparison(const char* op)
{
    return S_EQ(op, "==") || S_EQ(op, "!=") || S_EQ(op, "<") || S_EQ(op, ">")
        || S_EQ(op, "<=") || S_EQ(op, ">=");
}

static struct vtype validate_binary(struct node* node)
{
    const char* op = node->exp.op;
    struct vtype left = validate_expression(node->exp.left);
    struct vtype right = validate_expression(node->exp.right);
    // A struct is no operand of arithmetic or a comparison: x86 failed
    // a stack frame assert on `*(p + 1) + 1` with p a struct pointer.
    if (vtype_is_struct(&left) || vtype_is_struct(&right))
    {
        compiler_node_error(node, "Invalid operands to '%s' (%s and %s)", op,
            left.known ? vtype_name(&left) : "?", right.known ? vtype_name(&right) : "?");
    }
    if (validate_is_comparison(op) || is_logical_operator(op))
    {
        return vtype_int();
    }
    if (S_EQ(op, "+") || S_EQ(op, "-"))
    {
        if (vtype_is_address(&left) && !left.function && vtype_is_integer(&right))
        {
            return vtype_decay(&left);
        }
        if (S_EQ(op, "+") && vtype_is_address(&right) && !right.function && vtype_is_integer(&left))
        {
            return vtype_decay(&right);
        }
    }
    if (vtype_is_integer(&left) && vtype_is_integer(&right))
    {
        return vtype_int();
    }
    return vtype_unknown();
}

static struct vtype validate_exp_node(struct node* node)
{
    const char* op = node->exp.op;
    if (S_EQ(op, "=") || validate_is_compound_assignment(op))
    {
        struct vtype left = validate_expression(node->exp.left);
        struct vtype right = validate_expression(node->exp.right);
        validate_modifiable(node->exp.left, &left);
        if (S_EQ(op, "="))
        {
            validate_conversion(node, &left.t, &right, "Assignment");
        }
        return left;
    }
    if (S_EQ(op, ","))
    {
        validate_expression(node->exp.left);
        return validate_expression(node->exp.right);
    }
    if (S_EQ(op, "?"))
    {
        validate_expression(node->exp.left);
        struct node* choice = node->exp.right;
        if (!choice || choice->type != NODE_TYPE_TENARY)
        {
            validate_expression(choice);
            return vtype_unknown();
        }
        struct vtype yes = validate_expression(choice->tenary.true_node);
        struct vtype no = validate_expression(choice->tenary.false_node);
        if (vtype_is_address(&yes) && !yes.function)
        {
            return vtype_decay(&yes);
        }
        if (vtype_is_address(&no) && !no.function)
        {
            return vtype_decay(&no);
        }
        if (vtype_is_integer(&yes) && vtype_is_integer(&no))
        {
            return vtype_int();
        }
        return vtype_is_struct(&yes) ? yes : vtype_unknown();
    }
    if (is_parentheses_node(node))
    {
        return validate_call(node);
    }
    if (is_access_node(node))
    {
        return validate_member(node);
    }
    if (is_array_node(node))
    {
        struct vtype base = validate_expression(node->exp.left);
        struct vtype index = validate_expression(node->exp.right);
        if (vtype_unchecked(&base) || vtype_unchecked(&index))
        {
            return vtype_unknown();
        }
        if (vtype_is_address(&base) && !base.function)
        {
            return vtype_deref(&base);
        }
        if (vtype_is_address(&index) && !index.function)
        {
            return vtype_deref(&index);
        }
        compiler_node_error(node, "Subscripted value of type %s is not an array or pointer", vtype_name(&base));
    }
    return validate_binary(node);
}

static struct vtype validate_unary(struct node* node)
{
    const char* op = node->unary.op;
    struct vtype operand = validate_expression(node->unary.operand);
    if (op_is_indirection(op))
    {
        int depth = node->unary.indirection.depth > 0 ? node->unary.indirection.depth : 1;
        for (int i = 0; i < depth; i++)
        {
            if (vtype_unchecked(&operand))
            {
                return vtype_unknown();
            }
            if (operand.function)
            {
                // `*f` of a function is the function.
                continue;
            }
            if (!vtype_is_address(&operand))
            {
                compiler_node_error(node, "Indirection through %s, which is not a pointer", vtype_name(&operand));
            }
            operand = vtype_deref(&operand);
        }
        return operand;
    }
    if (op_is_address(op))
    {
        if (!operand.known || operand.function || vtype_is_array(&operand.t))
        {
            return operand.function ? operand : vtype_unknown();
        }
        struct vtype v = operand;
        v.string = false;
        v.t.flags |= DATATYPE_FLAG_IS_POINTER;
        v.t.pointer_depth++;
        return v;
    }
    if (is_increment_operator(op))
    {
        validate_modifiable(node->unary.operand, &operand);
        return operand;
    }
    if (vtype_is_struct(&operand))
    {
        compiler_node_error(node, "Invalid operand to '%s' (%s)", op, vtype_name(&operand));
    }
    if (S_EQ(op, "!"))
    {
        return vtype_int();
    }
    return vtype_is_integer(&operand) ? vtype_int() : vtype_unknown();
}

// Validate an expression; its type.
static struct vtype validate_expression(struct node* node)
{
    if (!node)
    {
        return vtype_unknown();
    }
    switch (node->type)
    {
        case NODE_TYPE_NUMBER:
            if (node->num.sizeof_operand)
            {
                validate_expression(node->num.sizeof_operand);
            }
            return vtype_int();

        case NODE_TYPE_STRING:
        {
            struct vtype v = vtype_int();
            v.t.type = DATA_TYPE_CHAR;
            v.t.type_str = "char";
            v.t.size = DATA_SIZE_BYTE;
            v.t.flags |= DATATYPE_FLAG_IS_POINTER;
            v.t.pointer_depth = 1;
            v.string = true;
            return v;
        }

        case NODE_TYPE_IDENTIFIER:
            return validate_identifier(node);

        case NODE_TYPE_EXPRESSION_PARENTHESES:
            return validate_expression(node->parenthesis.exp);

        case NODE_TYPE_EXPRESSION:
            return validate_exp_node(node);

        case NODE_TYPE_UNARY:
            return validate_unary(node);

        case NODE_TYPE_CAST:
            validate_expression(node->cast.operand);
            return vtype_of(&node->cast.dtype);

        case NODE_TYPE_BRACKET:
            return validate_expression(node->bracket.inner);

        case NODE_TYPE_TENARY:
            validate_expression(node->tenary.true_node);
            validate_expression(node->tenary.false_node);
            return vtype_unknown();

        case NODE_TYPE_INITIALIZER:
        {
            struct vector* leaves = node->init.leaves;
            for (int i = 0; leaves && i < vector_count(leaves); i++)
            {
                struct initializer_leaf* leaf = vector_at(leaves, i);
                validate_expression(leaf->value);
            }
            return vtype_unknown();
        }
    }
    return vtype_unknown();
}

// ---------------------------------------------------------------
// Statements
// ---------------------------------------------------------------

// ch257: return statement. If the function's return type is void
// (with no pointer indirection) and the return carries a value,
// it's a compile error, as is a bare `return;` in a function that
// returns a value (it was accepted and returned garbage).
void validate_return_node(struct node* node)
{
    bool returns_void = datatype_is_void_no_ptr(&current_function->func.rtype);
    if (node->stmt.return_stmt.exp)
    {
        if (returns_void)
        {
            compiler_node_error(node, "You are returning a value in a function %s which has a return type of void\n", current_function->func.name);
        }
        struct vtype value = validate_expression(node->stmt.return_stmt.exp);
        validate_conversion(node, &current_function->func.rtype, &value, "Return");
    }
    else if (!returns_void)
    {
        compiler_node_error(node, "'return;' with no value in function %s, which returns a value", current_function->func.name);
    }
}

// ch257: if-statement and its else / else-if chain.
void validate_if_stmt(struct node* node)
{
    validate_expression(node->stmt.if_stmt.cond_node);
    validate_statement_recovering(node->stmt.if_stmt.body_node);
    validate_statement_recovering(node->stmt.if_stmt.next);
}

static void validate_for_stmt(struct node* node)
{
    validation_new_scope(0);
    validate_statement_recovering(node->stmt.for_stmt.init_node);
    validate_expression(node->stmt.for_stmt.cond_node);
    validate_expression(node->stmt.for_stmt.loop_node);
    validate_statement_recovering(node->stmt.for_stmt.body_node);
    validation_end_scope();
}

void validate_statement(struct node* node)
{
    switch (node->type)
    {
        case NODE_TYPE_VARIABLE:
            validate_variable(node);
            break;

        case NODE_TYPE_VARIABLE_LIST:
        {
            struct vector* list = node->var_list.list;
            for (int i = 0; i < vector_count(list); i++)
            {
                validate_variable(*(struct node**)vector_at(list, i));
            }
            break;
        }

        case NODE_TYPE_BODY:
            validation_new_scope(0);
            validate_body(&node->body);
            validation_end_scope();
            break;

        case NODE_TYPE_STATEMENT_RETURN:
            validate_return_node(node);
            break;

        case NODE_TYPE_STATEMENT_IF:
            validate_if_stmt(node);
            break;

        case NODE_TYPE_STATEMENT_ELSE:
            validate_statement_recovering(node->stmt.else_stmt.body_node);
            break;

        case NODE_TYPE_STATEMENT_WHILE:
            validate_expression(node->stmt.while_stmt.exp_node);
            validate_statement_recovering(node->stmt.while_stmt.body_node);
            break;

        case NODE_TYPE_STATEMENT_DO_WHILE:
            validate_statement_recovering(node->stmt.do_while_stmt.body_node);
            validate_expression(node->stmt.do_while_stmt.exp_node);
            break;

        case NODE_TYPE_STATEMENT_FOR:
            validate_for_stmt(node);
            break;

        case NODE_TYPE_STATEMENT_SWITCH:
            validate_expression(node->stmt.switch_stmt.exp);
            validate_statement_recovering(node->stmt.switch_stmt.body);
            break;

        case NODE_TYPE_ASM:
            validator_function_has_asm = true;
            break;

        case NODE_TYPE_EXPRESSION:
        case NODE_TYPE_EXPRESSION_PARENTHESES:
        case NODE_TYPE_UNARY:
        case NODE_TYPE_CAST:
        case NODE_TYPE_IDENTIFIER:
        case NODE_TYPE_TENARY:
            validate_expression(node);
            break;
    }
}

// One statement; an error in it is counted and the walk goes on after
// it, with the scopes it opened dropped.
static void validate_statement_recovering(struct node* node)
{
    if (!node)
    {
        return;
    }
    struct resolver_process* resolver = validator_current_compile_process->resolver;
    struct resolver_scope* scope = resolver->scope.current;
    jmp_buf* outer = validator_recovery;
    jmp_buf point;
    validator_set_recovery(&point);
    if (!setjmp(point))
    {
        validate_statement(node);
    }
    else
    {
        resolver->scope.current = scope;
    }
    validator_set_recovery(outer);
}

// ch255 (extended in ch257): walk a body's statement vector and
// dispatch each through validate_statement.
void validate_body(struct body* body)
{
    struct vector* statements = body->statements;
    for (int i = 0; i < vector_count(statements); i++)
    {
        validate_statement_recovering(*(struct node**)vector_at(statements, i));
    }
}

// Whether control can run off the end of `node`. Unsure cases (a call
// that may not return, inline asm) count as not running off, so the
// warning is not given about a function that is fine.
static bool validate_has_break(struct node* node);

static bool validate_is_constant_true(struct node* cond)
{
    return !cond || (cond->type == NODE_TYPE_NUMBER && cond->llnum != 0);
}

static bool validate_falls_through(struct node* node)
{
    if (!node)
    {
        return true;
    }
    switch (node->type)
    {
        case NODE_TYPE_STATEMENT_RETURN:
        case NODE_TYPE_STATEMENT_GOTO:
        case NODE_TYPE_ASM:
            return false;

        case NODE_TYPE_BODY:
        {
            struct vector* statements = node->body.statements;
            int count = vector_count(statements);
            return !count || validate_falls_through(*(struct node**)vector_at(statements, count - 1));
        }

        case NODE_TYPE_STATEMENT_IF:
            return !node->stmt.if_stmt.next
                || validate_falls_through(node->stmt.if_stmt.body_node)
                || validate_falls_through(node->stmt.if_stmt.next);

        case NODE_TYPE_STATEMENT_ELSE:
            return validate_falls_through(node->stmt.else_stmt.body_node);

        case NODE_TYPE_STATEMENT_WHILE:
            return !validate_is_constant_true(node->stmt.while_stmt.exp_node)
                || validate_has_break(node->stmt.while_stmt.body_node);

        case NODE_TYPE_STATEMENT_DO_WHILE:
            return !validate_is_constant_true(node->stmt.do_while_stmt.exp_node)
                || validate_has_break(node->stmt.do_while_stmt.body_node);

        case NODE_TYPE_STATEMENT_FOR:
            return !validate_is_constant_true(node->stmt.for_stmt.cond_node)
                || validate_has_break(node->stmt.for_stmt.body_node);

        case NODE_TYPE_STATEMENT_SWITCH:
            return !node->stmt.switch_stmt.has_default_case
                || validate_has_break(node->stmt.switch_stmt.body)
                || validate_falls_through(node->stmt.switch_stmt.body);

        case NODE_TYPE_EXPRESSION:
            // A call may be to a function that does not return (exit).
            return !is_parentheses_node(node);
    }
    return true;
}

// A `break` that leaves `node` (not one inside a nested loop or switch).
static bool validate_has_break(struct node* node)
{
    if (!node)
    {
        return false;
    }
    switch (node->type)
    {
        case NODE_TYPE_STATEMENT_BREAK:
            return true;

        case NODE_TYPE_BODY:
        {
            struct vector* statements = node->body.statements;
            for (int i = 0; i < vector_count(statements); i++)
            {
                if (validate_has_break(*(struct node**)vector_at(statements, i)))
                {
                    return true;
                }
            }
            return false;
        }

        case NODE_TYPE_STATEMENT_IF:
            return validate_has_break(node->stmt.if_stmt.body_node) || validate_has_break(node->stmt.if_stmt.next);

        case NODE_TYPE_STATEMENT_ELSE:
            return validate_has_break(node->stmt.else_stmt.body_node);
    }
    return false;
}

void validate_function_body(struct node* node)
{
    validate_body(&node->body);
}

// ch256: validate a variable definition. Local-scope lookup first
// to catch shadowing-in-the-same-scope; on miss, register the var
// as a new resolver entity so later references in the same scope
// can find it. The variable is in scope in its own initializer
// (C11 6.2.1p7), which is then checked.
void validate_variable(struct node* var_node)
{
    struct resolver_entity* entity =
        resolver_get_variable_from_local_scope(validator_current_compile_process->resolver, var_node->var.name);
    if (entity)
    {
        compiler_node_error(var_node, "You have already defined the variable %s in the given scope", var_node->var.name);
    }
    resolver_default_new_scope_entity(validator_current_compile_process->resolver, var_node, 0, 0);
    if (validator_function_locals && var_node->var.name
        && strncmp(var_node->var.name, "__", 2) != 0
        && !(var_node->var.type.flags & (DATATYPE_FLAG_IS_STATIC | DATATYPE_FLAG_IS_EXTERN)))
    {
        vector_push(validator_function_locals, &var_node);
    }
    if (var_node->var.val)
    {
        struct vtype value = validate_expression(var_node->var.val);
        validate_conversion(var_node, &var_node->var.type, &value, "Initialization");
    }
}

void validate_function_argument(struct node* func_argument_var_node)
{
    validate_variable(func_argument_var_node);
}

void validate_function_arguments(struct function_arguments* func_arguments)
{
    struct vector* func_arg_vec = func_arguments->vector;
    for (int i = 0; func_arg_vec && i < vector_count(func_arg_vec); i++)
    {
        validate_function_argument(*(struct node**)vector_at(func_arg_vec, i));
    }
}

// Warnings once a function is checked: locals never referred to, and
// a function returning a value whose end can be reached (main returns
// 0 there).
static void validate_function_warnings(struct node* node)
{
    for (int i = 0; !validator_function_has_asm && i < vector_count(validator_function_locals); i++)
    {
        struct node* var = *(struct node**)vector_at(validator_function_locals, i);
        if (!(var->flags & NODE_FLAG_IS_REFERENCED))
        {
            compiler_node_warning(var, "Unused variable %s", var->var.name);
        }
    }
    if (!datatype_is_void_no_ptr(&node->func.rtype) && !S_EQ(node->func.name, "main")
        && validate_falls_through(node->func.body_n))
    {
        compiler_node_warning(node, "Control reaches the end of function %s, which returns a value", node->func.name);
    }
}

// ch255: validate a function node. Forward decls skip the uniqueness
// check (we expect the real def to come later); everything else
// asserts the name hasn't been registered. Then we register the
// function symbol, open a scope, walk arguments + body, close.
void validate_function_node(struct node* node)
{
    current_function = node;
    // A prototype may be repeated and followed by its definition; only
    // a second body, or a function named like another symbol, is an
    // error. Before, `int f(int); int f(int a){...}` was "already
    // defined".
    struct symbol* sym = symresolver_get_symbol(validator_current_compile_process, node->func.name);
    struct node* previous = sym ? symresolver_node(sym) : 0;
    bool previous_is_function = previous && previous->type == NODE_TYPE_FUNCTION;
    if (sym && (!previous_is_function || (previous->func.body_n && node->func.body_n)))
    {
        if (!(node->flags & NODE_FLAG_IS_FORWARD_DECLARATION))
        {
            validate_symbol_unique(node->func.name, "function", node);
        }
    }

    if (!sym)
    {
        symresolver_register_symbol(validator_current_compile_process, node->func.name, SYMBOL_TYPE_NODE, node);
    }
    else if (previous_is_function && node->func.body_n)
    {
        sym->data = node;
    }
    validation_new_scope(0);
    validate_function_arguments(&node->func.args);

    if (node->func.body_n)
    {
        validator_function_locals = vector_create(sizeof(struct node*));
        validator_function_has_asm = false;
        validate_function_body(node->func.body_n);
        validate_function_warnings(node);
        validator_function_locals = 0;
    }
    validation_end_scope();

    current_function = NULL;
}

// A global, or the variable of `struct s {...} v;` at file scope: in
// scope for what follows. Its initializer is checked like a local's.
static void validate_global_variable(struct node* var)
{
    resolver_default_new_scope_entity(validator_current_compile_process->resolver, var, 0, 0);
    if (var->var.val)
    {
        struct vtype value = validate_expression(var->var.val);
        validate_conversion(var, &var->var.type, &value, "Initialization");
    }
}

// ch258: structure validation. Forward decls skip the uniqueness
// check; everything else asserts the name hasn't been registered
// before stamping the struct as SYMBOL_TYPE_NODE.
void validate_structure_node(struct node* node)
{
    if (!(node->flags & NODE_FLAG_IS_FORWARD_DECLARATION) && !validate_after_forward_declaration(node->_struct.name))
    {
        validate_symbol_unique(node->_struct.name, "struct", node);
    }
    symresolver_register_symbol(validator_current_compile_process, node->_struct.name, SYMBOL_TYPE_NODE, node);
    if (node->_struct.var && node->_struct.var->type == NODE_TYPE_VARIABLE)
    {
        validate_global_variable(node->_struct.var);
    }
}

// `struct x;` (shell.h) then `struct x { ... };` (samos.h)
// defines x once.
static bool validate_after_forward_declaration(const char* name)
{
    struct symbol* sym = symresolver_get_symbol(validator_current_compile_process, name);
    return sym && sym->type == SYMBOL_TYPE_NODE
        && (((struct node*)sym->data)->flags & NODE_FLAG_IS_FORWARD_DECLARATION);
}

void validate_union_node(struct node* node)
{
    if (!(node->flags & NODE_FLAG_IS_FORWARD_DECLARATION))
    {
        validate_symbol_unique(node->_union.name, "union", node);
    }
    symresolver_register_symbol(validator_current_compile_process, node->_union.name, SYMBOL_TYPE_NODE, node);
    if (node->_union.var && node->_union.var->type == NODE_TYPE_VARIABLE)
    {
        validate_global_variable(node->_union.var);
    }
}

void validate_node(struct node* node)
{
    switch (node->type)
    {
        case NODE_TYPE_FUNCTION:
            validate_function_node(node);
            break;

        case NODE_TYPE_STRUCT:
            validate_structure_node(node);
            break;

        case NODE_TYPE_UNION:
            validate_union_node(node);
            break;

        // Globals (and static locals, which become globals) are in scope
        // for the functions after them. They were never registered, so a
        // function that used one in some statements failed with "The
        // variable does not exist".
        case NODE_TYPE_VARIABLE:
            validate_global_variable(node);
            break;

        case NODE_TYPE_VARIABLE_LIST:
        {
            struct vector* list = node->var_list.list;
            for (int i = 0; i < vector_count(list); i++)
            {
                validate_global_variable(*(struct node**)vector_at(list, i));
            }
            break;
        }
    }
}

int validate_tree(struct compile_process* process)
{
    struct resolver_process* resolver = process->resolver;
    validation_new_scope(0);
    struct node* node = validation_next_tree_node();
    while (node)
    {
        // An error in a declaration ends it; the next one is checked.
        struct resolver_scope* scope = resolver->scope.current;
        jmp_buf point;
        validator_set_recovery(&point);
        if (!setjmp(point))
        {
            validate_node(node);
        }
        else
        {
            resolver->scope.current = scope;
            current_function = NULL;
            validator_function_locals = 0;
        }
        validator_set_recovery(0);
        node = validation_next_tree_node();
    }
    validation_end_scope();
    return compiler_error_count() ? VALIDATION_GENERAL_ERROR : VALIDATION_ALL_OK;
}

int validate(struct compile_process* process)
{
    int res = 0;
    validate_initialize(process);
    res = validate_tree(process);
    validate_destruct(process);
    // With errors: print the count and stop, as for any other error.
    compiler_finish_errors();
    return res;
}
