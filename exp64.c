#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "compiler.h"
#include "helpers/vector.h"

// the expression generator of the three 64-bit backends.
//
// Every expression is generated from its C type: an lvalue by its
// address (exp64_address), anything else by its value (exp64_value),
// so `*d++ = *s++`, `&a[i]`, `p->q->r[i]` and the rest all take one
// path. The result is in the backend's value register R0, in canonical
// form: a signed type sign extended to 64 bits, an unsigned one zero
// extended. The usual arithmetic conversions pick the width (32 or 64
// bits) and the signedness of every operation, which the old resolver
// chains did not know: they compared, shifted and divided everything
// as signed int.
//
// A float or double value is its bits in R0 (a float's in the low 32),
// so it is loaded, stored, pushed and passed like an integer; the
// backend's floating-point ops (fbinop, fcompare and the conversions)
// move it to a floating-point register for the arithmetic and back.
//
// The backend supplies the machine operations (struct exp64_ops):
// pushes and pops on its expression stack with the frame ledger, loads
// and stores of 1, 2, 4 and 8 bytes, the arithmetic, and calls. A
// struct value is the address of the struct; a struct a call returns
// lives in space the call reserves on the expression stack.
//
// The type of an expression is found by generating it with the output
// muted (exp64_typeof), so there is one set of typing rules.

static const struct exp64_ops* ops;
static struct compile_process* current_process;
static struct node*            current_function;
static bool                    exp64_muted;
static int                     exp64_label_count;

static struct datatype exp64_value(struct node* node);
static struct datatype exp64_rvalue(struct node* node, bool decay);
static struct datatype exp64_address(struct node* node);
static struct datatype exp64_number_type(struct node* node);
static long long       exp64_number_value(struct node* node);
static bool            exp64_names_function(struct node* node);

void exp64_begin(const struct exp64_ops* backend_ops, struct compile_process* process){
    ops             = backend_ops;
    current_process = process;
}

void exp64_set_function(struct node* function){
    current_function = function;
}

bool exp64_output_muted(void){
    return exp64_muted;
}

// ---------------------------------------------------------------
// Types
// ---------------------------------------------------------------

static struct datatype exp64_int_type(int size, bool is_signed){
    struct datatype t = {0};
    t.type     = size == 1 ? DATA_TYPE_CHAR : size == 2 ? DATA_TYPE_SHORT : size == 4 ? DATA_TYPE_INTEGER : DATA_TYPE_LONG;
    t.type_str = size == 1 ? "char" : size == 2 ? "short" : size == 4 ? "int" : "long";
    t.size     = size;
    t.flags    = is_signed ? DATATYPE_FLAG_IS_SIGNED : 0;
    return t;
}

static bool exp64_is_array(struct datatype* t){
    return t->flags & DATATYPE_FLAG_IS_ARRAY;
}

static bool exp64_is_pointer(struct datatype* t){
    return !exp64_is_array(t) && (t->flags & DATATYPE_FLAG_IS_POINTER) && t->pointer_depth > 0;
}

static bool exp64_is_struct(struct datatype* t){
    return !exp64_is_array(t) && !exp64_is_pointer(t) && datatype_is_struct_or_union(t);
}

static bool exp64_is_void(struct datatype* t){
    return !exp64_is_array(t) && !exp64_is_pointer(t) && t->type == DATA_TYPE_VOID;
}

// float or double: its bits in R0, the arithmetic through the
// backend's floating-point ops.
static bool exp64_is_floating(struct datatype* t){
    return !exp64_is_array(t) && !exp64_is_pointer(t) && datatype_is_floating(t);
}

static bool exp64_is_signed(struct datatype* t){
    return !exp64_is_pointer(t) && !exp64_is_floating(t) && (t->flags & DATATYPE_FLAG_IS_SIGNED);
}

// Bytes of a scalar value: 8 for a pointer.
static int exp64_scalar_size(struct datatype* t){
    return exp64_is_pointer(t) ? DATA_SIZE_DDWORD : (int)t->size;
}

static size_t exp64_size(struct datatype* t){
    return datatype_size(t);
}

static struct datatype exp64_pointer_to(struct datatype* t){
    struct datatype p = *t;
    if(exp64_is_array(&p)){
        p = datatype_array_decay(&p);
        return p;
    }
    p.flags |= DATATYPE_FLAG_IS_POINTER;
    p.pointer_depth++;
    return p;
}

// The element of an array: the same type with the first bracket gone.
static struct datatype exp64_array_element(struct datatype* t){
    struct datatype e = *t;
    struct vector* brackets = t->array.brackets->n_brackets;
    if(vector_count(brackets) <= 1){
        e.flags &= ~DATATYPE_FLAG_IS_ARRAY;
        e.array.brackets = 0;
        e.array.size = 0;
        return e;
    }
    e.array.brackets = array_brackets_new();
    for(int i = 1; i < vector_count(brackets); i++){
        array_brackets_add(e.array.brackets, *(struct node**)vector_at(brackets, i));
    }
    e.array.size = array_brackets_calculate_size(&e, e.array.brackets);
    return e;
}

// What a pointer points to. Through a function pointer the callee is
// the pointer itself: `(*f)(x)` calls f.
static struct datatype exp64_deref(struct datatype* t){
    if(exp64_is_array(t)){
        return exp64_array_element(t);
    }
    if(t->flags & DATATYPE_FLAG_IS_FUNCTION_POINTER){
        return *t;
    }
    struct datatype d = *t;
    d.pointer_depth--;
    if(d.pointer_depth <= 0){
        d.pointer_depth = 0;
        d.flags &= ~DATATYPE_FLAG_IS_POINTER;
    }
    return d;
}

// An array used as a value is a pointer to its first element.
static struct datatype exp64_decay(struct datatype* t){
    if(!exp64_is_array(t)){
        return *t;
    }
    if(array_brackets_count(t) > 1){
        // A pointer to an array has no type of its own here: keep the
        // array, which indexes and sizes the same way.
        return *t;
    }
    return datatype_array_decay(t);
}

static struct datatype exp64_promote(struct datatype* t){
    if(exp64_is_pointer(t) || t->size >= DATA_SIZE_DWORD){
        return *t;
    }
    return exp64_int_type(DATA_SIZE_DWORD, true);
}

// The usual arithmetic conversions of two promoted integer types; a
// floating operand makes it float, or double when either is double.
static struct datatype exp64_common(struct datatype* a, struct datatype* b){
    if(exp64_is_floating(a) || exp64_is_floating(b)){
        bool is_double = (exp64_is_floating(a) && a->size == DATA_SIZE_DDWORD)
                      || (exp64_is_floating(b) && b->size == DATA_SIZE_DDWORD);
        return datatype_floating(is_double ? DATA_SIZE_DDWORD : DATA_SIZE_DWORD);
    }
    struct datatype pa = exp64_promote(a);
    struct datatype pb = exp64_promote(b);
    int sa = exp64_scalar_size(&pa), sb = exp64_scalar_size(&pb);
    if(sa == sb){
        return exp64_int_type(sa, exp64_is_signed(&pa) && exp64_is_signed(&pb));
    }
    struct datatype* wide = sa > sb ? &pa : &pb;
    return exp64_int_type(exp64_scalar_size(wide), exp64_is_signed(wide));
}

// R0 (or R1) held a value of type `from`; make it a canonical `to`.
// Widening needs nothing (canonical form is already the C
// conversion); narrowing or a change of sign at 32 bits or less
// re-extends.
// R0 held a value of type `from` and one of the two is a float or a
// double (C11 6.3.1.4, 6.3.1.5): a floating value changes precision,
// truncates toward zero into an integer (`_Bool` is its test against
// 0), and an integer or pointer is converted to the floating type.
static void exp64_convert_floating(struct datatype* from, struct datatype* to){
    bool from_floating = exp64_is_floating(from);
    if(from_floating && exp64_is_floating(to)){
        if(from->size != to->size){
            ops->float_resize(from->size, to->size);
        }
        return;
    }
    if(!from_floating){
        if(exp64_is_struct(from) || exp64_is_void(from)){
            compiler_error(current_process, "A struct or void value cannot become a floating value\n");
        }
        bool from_u64 = exp64_scalar_size(from) == DATA_SIZE_DDWORD && !exp64_is_signed(from);
        ops->int_to_float(to->size, from_u64);
        return;
    }
    if(to->flags & DATATYPE_FLAG_IS_BOOL){
        // R1 is kept on the stack meanwhile.
        struct datatype word = exp64_int_type(DATA_SIZE_DDWORD, false);
        ops->swap();
        ops->push(&word);
        ops->swap();
        ops->imm(1, 0);
        ops->fcompare("!=", from->size);
        ops->pop(1);
        return;
    }
    if(exp64_is_pointer(to)){
        compiler_error(current_process, "A floating value cannot become a pointer\n");
    }
    int size = exp64_scalar_size(to);
    ops->float_to_int(from->size, size == DATA_SIZE_DDWORD && !exp64_is_signed(to));
    if(size < DATA_SIZE_DDWORD){
        ops->extend(0, size, exp64_is_signed(to));
    }
}

static void exp64_convert(int reg, struct datatype* from, struct datatype* to){
    if(exp64_is_void(to) || exp64_is_struct(to) || exp64_is_array(to)){
        return;
    }
    if(exp64_is_floating(from) || exp64_is_floating(to)){
        // The floating ops work on R0 and keep R1.
        if(reg){
            ops->swap();
        }
        exp64_convert_floating(from, to);
        if(reg){
            ops->swap();
        }
        return;
    }
    int size = exp64_scalar_size(to);
    if(size >= DATA_SIZE_DDWORD){
        return;
    }
    if(!exp64_is_void(from) && !exp64_is_struct(from) && !exp64_is_array(from)
       && exp64_scalar_size(from) <= size && exp64_is_signed(from) == exp64_is_signed(to)
       && !exp64_is_pointer(from)){
        return;
    }
    ops->extend(reg, size, exp64_is_signed(to));
}

static int exp64_new_label(void){
    return ++exp64_label_count;
}

// The type of an expression, from generating it with no output. The
// pushes it records are dropped from the ledger again.
struct datatype exp64_typeof(struct node* node){
    bool muted = exp64_muted;
    int ledger = vector_count(current_function->func.frame.elements);
    exp64_muted = true;
    struct datatype t = exp64_rvalue(node, false);
    exp64_muted = muted;
    while(vector_count(current_function->func.frame.elements) > ledger){
        stackframe_pop(current_function);
    }
    return t;
}

// ---------------------------------------------------------------
// Names
// ---------------------------------------------------------------

static struct datatype exp64_identifier_address(struct node* node){
    struct resolver_process* resolver = current_process->resolver;
    struct resolver_entity* entity = resolver_get_variable(0, resolver, node->sval);
    if(entity){
        struct resolver_default_entity_data* data = resolver_default_entity_private(entity);
        if(data && (data->flags & RESOLVER_DEFAULT_ENTITY_FLAG_IS_LOCAL_STACK)){
            ops->frame_address(data->offset);
            if(data->flags & RESOLVER_DEFAULT_ENTITY_FLAG_IS_REFERENCE){
                // B1: a big struct parameter: the slot holds the
                // address of the caller's copy.
                ops->load(DATA_SIZE_DDWORD, false);
            }
        } else {
            // The symbol: the name, or a static local's own label.
            ops->symbol_address(data ? data->base_address : entity->name);
        }
        return entity->dtype;
    }
    entity = resolver_get_function(0, resolver, node->sval);
    if(!entity){
        compiler_error(current_process, "%s is not declared\n", node->sval);
    }
    ops->symbol_address(entity->name);
    struct datatype t = entity->dtype;
    t.flags |= DATATYPE_FLAG_IS_POINTER | DATATYPE_FLAG_IS_FUNCTION_POINTER;
    t.pointer_depth++;
    return t;
}

static bool exp64_names_function(struct node* node){
    return node->type == NODE_TYPE_IDENTIFIER && !resolver_get_variable(0, current_process->resolver, node->sval)
        && resolver_get_function(0, current_process->resolver, node->sval);
}

// Offset and type of `member` in the struct or union type t.
static int exp64_member(struct datatype* t, const char* member, struct datatype* member_type){
    if(!datatype_is_struct_or_union(t)){
        compiler_error(current_process, "Member %s of something that is not a struct\n", member);
    }
    struct node* member_node = 0;
    int offset = struct_offset(current_process, t->type_str, member, &member_node, 0, 0);
    if(!member_node || !S_EQ(member_node->var.name, member)){
        compiler_error(current_process, "%s has no member %s\n", t->type_str, member);
    }
    *member_type = member_node->var.type;
    return t->type == DATA_TYPE_UNION ? 0 : offset;
}

// ---------------------------------------------------------------
// Addresses
// ---------------------------------------------------------------

static struct datatype exp64_index_address(struct node* node){
    struct datatype base = exp64_rvalue(node->exp.left, false);
    struct datatype element = exp64_deref(&base);
    ops->push(&base);
    struct node* index = node->exp.right->type == NODE_TYPE_BRACKET ? node->exp.right->bracket.inner : node->exp.right;
    exp64_value(index);
    ops->mul_imm(0, exp64_size(&element));
    ops->pop(1);
    ops->binop("+", DATA_SIZE_DDWORD, false);
    return element;
}

static struct datatype exp64_indirection_address(struct node* node){
    struct datatype t = exp64_value(node->unary.operand);
    for(int i = 1; i < node->unary.indirection.depth; i++){
        t = exp64_deref(&t);
        if(!(t.flags & DATATYPE_FLAG_IS_FUNCTION_POINTER)){
            ops->load(DATA_SIZE_DDWORD, false);
        }
    }
    if(!exp64_is_pointer(&t) && !exp64_is_array(&t)){
        compiler_error(current_process, "Indirection through something that is not a pointer\n");
    }
    return exp64_deref(&t);
}

static struct datatype exp64_address(struct node* node){
    if(node->pos.line){
        current_process->pos = node->pos;
    }
    switch(node->type){
        case NODE_TYPE_IDENTIFIER:
            return exp64_identifier_address(node);
        case NODE_TYPE_EXPRESSION_PARENTHESES:
            return exp64_address(node->parenthesis.exp);
        case NODE_TYPE_UNARY:
            if(op_is_indirection(node->unary.op)){
                return exp64_indirection_address(node);
            }
            break;
        case NODE_TYPE_EXPRESSION:
            if(is_array_node(node)){
                return exp64_index_address(node);
            }
            if(is_access_node(node)){
                struct datatype t;
                if(S_EQ(node->exp.op, "->")){
                    struct datatype p = exp64_value(node->exp.left);
                    t = exp64_deref(&p);
                } else {
                    t = exp64_address(node->exp.left);
                }
                struct datatype member;
                if(node->exp.right->type != NODE_TYPE_IDENTIFIER){
                    compiler_error(current_process, "Expecting a member name after %s\n", node->exp.op);
                }
                ops->add_imm(exp64_member(&t, node->exp.right->sval, &member));
                return member;
            }
            break;
    }
    // A struct value (a call's result) is already an address.
    struct datatype t = exp64_value(node);
    if(!exp64_is_struct(&t)){
        compiler_error(current_process, "This expression has no address\n");
    }
    return t;
}

// The value of the object at the address in R0.
static struct datatype exp64_load(struct datatype* t, bool decay){
    if(exp64_is_array(t)){
        return decay ? exp64_decay(t) : *t;
    }
    if(exp64_is_struct(t) || exp64_is_void(t)){
        return *t;
    }
    ops->load(exp64_scalar_size(t), exp64_is_signed(t));
    return *t;
}

// ---------------------------------------------------------------
// Operators
// ---------------------------------------------------------------

static bool exp64_is_comparison(const char* op){
    return S_EQ(op, "==") || S_EQ(op, "!=") || S_EQ(op, "<") || S_EQ(op, ">")
        || S_EQ(op, "<=") || S_EQ(op, ">=");
}

// R0 = R0 op R1, the left operand of type lt in R0, the right of type
// rt in R1.
// R0 = R0 op R1 when either operand is floating: both are converted to
// the common floating type, and a comparison gives an int.
static struct datatype exp64_arith_floating(const char* op, struct datatype* lt, struct datatype* rt){
    if(exp64_is_pointer(lt) || exp64_is_pointer(rt) || exp64_is_struct(lt) || exp64_is_struct(rt)){
        compiler_error(current_process, "Invalid operands to %s: a floating value with a pointer or struct\n", op);
    }
    struct datatype c = exp64_common(lt, rt);
    exp64_convert(0, lt, &c);
    exp64_convert(1, rt, &c);
    if(exp64_is_comparison(op)){
        ops->fcompare(op, c.size);
        return exp64_int_type(DATA_SIZE_DWORD, true);
    }
    if(!(S_EQ(op, "+") || S_EQ(op, "-") || S_EQ(op, "*") || S_EQ(op, "/"))){
        compiler_error(current_process, "The operator %s does not take floating operands\n", op);
    }
    ops->fbinop(op, c.size);
    return c;
}

static struct datatype exp64_arith(const char* op, struct datatype* lt, struct datatype* rt){
    if(exp64_is_floating(lt) || exp64_is_floating(rt)){
        return exp64_arith_floating(op, lt, rt);
    }
    bool lp = exp64_is_pointer(lt), rp = exp64_is_pointer(rt);
    if(exp64_is_comparison(op)){
        if(lp || rp){
            ops->compare(op, false);
        } else {
            struct datatype c = exp64_common(lt, rt);
            exp64_convert(0, lt, &c);
            exp64_convert(1, rt, &c);
            ops->compare(op, exp64_is_signed(&c));
        }
        return exp64_int_type(DATA_SIZE_DWORD, true);
    }
    if((lp || rp) && (S_EQ(op, "+") || S_EQ(op, "-"))){
        if(lp && rp){
            struct datatype element = exp64_deref(lt);
            ops->binop("-", DATA_SIZE_DDWORD, true);
            if(exp64_size(&element) > 1){
                ops->imm(1, exp64_size(&element));
                ops->binop("/", DATA_SIZE_DDWORD, true);
            }
            return exp64_int_type(DATA_SIZE_DDWORD, true);
        }
        struct datatype* pointer = lp ? lt : rt;
        struct datatype element = exp64_deref(pointer);
        size_t step = exp64_size(&element);
        if(step > 1){
            ops->mul_imm(lp ? 1 : 0, step);
        }
        ops->binop(op, DATA_SIZE_DDWORD, false);
        return *pointer;
    }
    if(S_EQ(op, "<<") || S_EQ(op, ">>")){
        struct datatype t = exp64_promote(lt);
        if(lp){
            t = exp64_int_type(DATA_SIZE_DDWORD, false);
        }
        ops->binop(op, exp64_scalar_size(&t), exp64_is_signed(&t));
        if(exp64_scalar_size(&t) < DATA_SIZE_DDWORD){
            ops->extend(0, exp64_scalar_size(&t), exp64_is_signed(&t));
        }
        return t;
    }
    struct datatype c = lp || rp ? exp64_int_type(DATA_SIZE_DDWORD, false) : exp64_common(lt, rt);
    exp64_convert(0, lt, &c);
    exp64_convert(1, rt, &c);
    ops->binop(op, exp64_scalar_size(&c), exp64_is_signed(&c));
    if(exp64_scalar_size(&c) < DATA_SIZE_DDWORD){
        ops->extend(0, exp64_scalar_size(&c), exp64_is_signed(&c));
    }
    return c;
}

// Both operands: the left pushed, then R0 = left, R1 = right. A
// number on the right goes straight into R1: no push, no pop
// (`n < 20000000ul`, `i + 1`). The loops SamOS's tests time ran ten
// times slower than gcc -O0's.
static struct datatype exp64_binary(struct node* node){
    struct datatype lt = exp64_value(node->exp.left);
    if(node->exp.right->type == NODE_TYPE_NUMBER){
        struct datatype rt = exp64_number_type(node->exp.right);
        ops->imm(1, exp64_number_value(node->exp.right));
        return exp64_arith(node->exp.op, &lt, &rt);
    }
    ops->push(&lt);
    struct datatype rt = exp64_value(node->exp.right);
    ops->mov_r1();
    ops->pop(0);
    return exp64_arith(node->exp.op, &lt, &rt);
}

static bool exp64_is_compound_assignment(const char* op){
    return S_EQ(op, "+=") || S_EQ(op, "-=") || S_EQ(op, "*=") || S_EQ(op, "/=") || S_EQ(op, "%=")
        || S_EQ(op, "&=") || S_EQ(op, "|=") || S_EQ(op, "^=") || S_EQ(op, "<<=") || S_EQ(op, ">>=");
}

// `a = b` and `a op= b`. The target's address is worked out once.
static struct datatype exp64_assignment(struct node* node){
    const char* op = node->exp.op;
    struct datatype lt;
    // A plain variable's address takes only R0, so the value can wait
    // in R1 instead of on the stack.
    bool simple = node->exp.left->type == NODE_TYPE_IDENTIFIER && !exp64_names_function(node->exp.left);
    if(S_EQ(op, "=") && simple){
        struct datatype rt = exp64_value(node->exp.right);
        ops->mov_r1();
        lt = exp64_address(node->exp.left);
        if(exp64_is_struct(&lt)){
            // copy wants the source in R1 and the target in R0.
            ops->copy(exp64_size(&lt));
            return lt;
        }
        ops->swap();
        exp64_convert(0, &rt, &lt);
    } else if(S_EQ(op, "=")){
        struct datatype rt = exp64_value(node->exp.right);
        ops->push(&rt);
        lt = exp64_address(node->exp.left);
        if(exp64_is_struct(&lt)){
            ops->pop(1);
            ops->copy(exp64_size(&lt));
            return lt;
        }
        ops->mov_r1();
        ops->pop(0);
        exp64_convert(0, &rt, &lt);
    } else {
        char arith_op[4] = {0};
        strncpy(arith_op, op, strlen(op) - 1);
        lt = exp64_address(node->exp.left);
        ops->push(&lt);
        exp64_load(&lt, true);
        struct datatype old = lt;
        struct datatype rt;
        if(node->exp.right->type == NODE_TYPE_NUMBER){
            rt = exp64_number_type(node->exp.right);
            ops->imm(1, exp64_number_value(node->exp.right));
        } else {
            ops->push(&old);
            rt = exp64_value(node->exp.right);
            ops->mov_r1();
            ops->pop(0);
        }
        struct datatype result = exp64_arith(arith_op, &old, &rt);
        exp64_convert(0, &result, &lt);
        ops->pop(1);
    }
    ops->store(exp64_scalar_size(&lt));
    return lt;
}

// A condition's value in R0, zero when it is false. A floating value is
// compared with 0.0, so -0.0 is false and a NaN true.
static struct datatype exp64_truth(struct node* node){
    struct datatype t = exp64_value(node);
    if(exp64_is_floating(&t)){
        ops->imm(1, 0);
        ops->fcompare("!=", t.size);
        return exp64_int_type(DATA_SIZE_DWORD, true);
    }
    return t;
}

void exp64_test(struct node* node){
    exp64_truth(node);
}

static struct datatype exp64_logical(struct node* node){
    bool is_and = S_EQ(node->exp.op, "&&");
    int short_label = exp64_new_label();
    int end_label = exp64_new_label();
    exp64_truth(node->exp.left);
    ops->branch(short_label, is_and ? EXP64_BRANCH_IF_ZERO : EXP64_BRANCH_IF_NOT_ZERO);
    exp64_truth(node->exp.right);
    ops->branch(short_label, is_and ? EXP64_BRANCH_IF_ZERO : EXP64_BRANCH_IF_NOT_ZERO);
    ops->imm(0, is_and ? 1 : 0);
    ops->branch(end_label, EXP64_BRANCH_ALWAYS);
    ops->label(short_label);
    ops->imm(0, is_and ? 0 : 1);
    ops->label(end_label);
    return exp64_int_type(DATA_SIZE_DWORD, true);
}

static struct datatype exp64_ternary(struct node* node){
    struct node* yes = node->exp.right->tenary.true_node;
    struct node* no  = node->exp.right->tenary.false_node;
    struct datatype yt = exp64_typeof(yes);
    struct datatype nt = exp64_typeof(no);
    yt = exp64_decay(&yt);
    nt = exp64_decay(&nt);
    struct datatype t = yt;
    if(exp64_is_pointer(&yt) || exp64_is_struct(&yt) || exp64_is_void(&yt)){
        t = yt;
    } else if(exp64_is_pointer(&nt)){
        t = nt;
    } else if(!exp64_is_struct(&nt) && !exp64_is_void(&nt)){
        t = exp64_common(&yt, &nt);
    }
    int no_label = exp64_new_label();
    int end_label = exp64_new_label();
    exp64_truth(node->exp.left);
    ops->branch(no_label, EXP64_BRANCH_IF_ZERO);
    exp64_value(yes);
    exp64_convert(0, &yt, &t);
    ops->branch(end_label, EXP64_BRANCH_ALWAYS);
    ops->label(no_label);
    exp64_value(no);
    exp64_convert(0, &nt, &t);
    ops->label(end_label);
    return t;
}

// ---------------------------------------------------------------
// Calls
// ---------------------------------------------------------------

static void exp64_flatten_arguments(struct node* node, struct vector* out){
    if(!node || node->type == NODE_TYPE_BLANK){
        return;
    }
    if(node->type == NODE_TYPE_EXPRESSION && S_EQ(node->exp.op, ",")){
        exp64_flatten_arguments(node->exp.left, out);
        exp64_flatten_arguments(node->exp.right, out);
        return;
    }
    vector_push(out, &node);
}

static struct vector* exp64_call_arguments(struct node* node){
    struct vector* args = vector_create(sizeof(struct node*));
    struct node* right = node->exp.right;
    if(right->type == NODE_TYPE_EXPRESSION_PARENTHESES){
        right = right->parenthesis.exp;
    }
    exp64_flatten_arguments(right, args);
    return args;
}

// va_start(list, last): list = the address of last's argument slot.
// __builtin_va_arg(list, size): step list one slot (or more for a
// bigger type) and read the slot's 8 bytes, a long that the caller
// converts to the argument's type.
static struct datatype exp64_stdarg(const char* name, struct vector* args){
    struct datatype long_type = exp64_int_type(DATA_SIZE_DDWORD, true);
    if(S_EQ(name, "va_end")){
        ops->imm(0, 0);
        return exp64_int_type(DATA_SIZE_DWORD, true);
    }
    // The va_arg macro passes `(type*)0` as a third argument, which
    // gives the type. A float or double variadic argument travels in a
    // floating-point register on x86_64 and aarch64, which the home
    // area does not hold: rejected there. riscv64 passes it in the
    // integer registers and on the stack, so its slot has the bits.
    struct datatype result_type = long_type;
    if(vector_count(args) == 3 && !S_EQ(name, "va_start")){
        struct node* type_argument = *(struct node**)vector_at(args, 2);
        if(type_argument->type == NODE_TYPE_CAST){
            struct datatype t = *datatype_pointer_reduce(&type_argument->cast.dtype, 1);
            if(exp64_is_floating(&t)){
                if(!S_EQ(current_process->target->backend->name, "riscv64")){
                    compiler_error(current_process, "va_arg of a float or double is not supported on %s\n",
                        current_process->target->backend->name);
                }
                result_type = datatype_floating(DATA_SIZE_DDWORD);
            }
        }
        vector_pop(args);
    }
    if(vector_count(args) != 2){
        compiler_error(current_process, "%s expects two arguments\n", name);
    }
    struct node* list = *(struct node**)vector_at(args, 0);
    struct node* second = *(struct node**)vector_at(args, 1);
    if(S_EQ(name, "va_start")){
        exp64_address(second);
        ops->push(&long_type);
        exp64_address(list);
        ops->mov_r1();
        ops->pop(0);
        ops->store(DATA_SIZE_DDWORD);
        return exp64_int_type(DATA_SIZE_DWORD, true);
    }
    if(second->type != NODE_TYPE_NUMBER){
        compiler_error(current_process, "__builtin_va_arg expects a constant size\n");
    }
    exp64_address(list);
    ops->push(&long_type);
    ops->load(DATA_SIZE_DDWORD, false);
    ops->add_imm(align_value(second->llnum, DATA_SIZE_DDWORD));
    ops->pop(1);
    ops->store(DATA_SIZE_DDWORD);
    ops->load(DATA_SIZE_DDWORD, false);
    return result_type;
}

// B1: a struct result of more than 16 bytes goes through memory (the
// hidden pointer in rdi, x8 or a0); a smaller one comes back in the
// two return registers (rax:rdx, x0:x1, a0:a1).
bool exp64_returns_in_memory(struct datatype* dtype){
    return exp64_is_struct(dtype) && exp64_size(dtype) > 2 * DATA_SIZE_DDWORD;
}

// B1: where each argument of a call goes, for the caller (exp64_call)
// and the callee (exp64_parameters) alike. The psABIs' rules for
// integer-class values:
//   x86_64 SysV: up to 16 bytes in 1 or 2 of rdi..r9 when all of them
//     are free, else the whole value on the stack, and later scalars
//     still take the registers left; a bigger struct (class MEMORY) is
//     copied onto the stack.
//   AAPCS64: up to 16 bytes in 1 or 2 of x0-x7 when all of them are
//     free, else on the stack, and no register is used after it (NGRN
//     = 8); a bigger struct is passed as the address of a copy the
//     caller makes.
//   RISC-V LP64: up to 16 bytes in 1 or 2 of a0-a7, split between a7
//     and the stack when only a7 is left; bigger by reference too.
// `hidden` is a struct result through memory: its pointer takes the
// first register on x86_64 (rdi) and riscv64 (a0); aarch64 passes it
// in x8.
//
// A float or double takes the next floating-point register (xmm0-7,
// v0-7, fa0-7), counted apart from the integer ones. With those used
// up it goes on the stack on x86_64 and aarch64, and to an integer
// register (else the stack) on riscv64 (LP64D), which also passes the
// variadic arguments, from `named` on, in integer registers. A struct
// is always INTEGER class here, floating members included: right
// between SamCompiler functions, not what gcc does with them
//. Returns the outgoing stack words.
static int exp64_arg_layout(struct datatype* types, int count, int named, bool hidden, struct exp64_arg* out){
    const char* abi = current_process->target->backend->name;
    bool x86_64  = S_EQ(abi, "x86_64");
    bool aarch64 = S_EQ(abi, "aarch64");
    int max = current_process->target->max_reg_args;
    int next = hidden && !aarch64 ? 1 : 0;
    int next_fp = 0;
    int stack = 0;
    for(int i = 0; i < count; i++){
        struct exp64_arg* a = &out[i];
        bool is_struct = exp64_is_struct(&types[i]);
        a->kind      = EXP64_ARG_WORDS;
        a->size      = is_struct ? (int)exp64_size(&types[i]) : DATA_SIZE_DDWORD;
        a->words     = align_value(a->size, DATA_SIZE_DDWORD) / DATA_SIZE_DDWORD;
        a->fp        = false;
        a->reg       = -1;
        a->reg_words = 0;
        a->stack     = -1;
        a->copy      = -1;
        int words = a->words;
        if(exp64_is_floating(&types[i]) && (i < named || x86_64 || aarch64)){
            a->size = types[i].size;
            if(next_fp < EXP64_FP_ARG_REGISTERS){
                a->fp = true;
                a->reg = next_fp++;
                a->reg_words = 1;
                continue;
            }
            if(x86_64 || aarch64){
                a->stack = stack++;
                continue;
            }
        }
        if(a->size > 2 * DATA_SIZE_DDWORD){
            if(x86_64){
                a->stack = stack;
                stack += words;
                continue;
            }
            a->kind = EXP64_ARG_REF;
            words = 1;
        }
        if(next + words <= max){
            a->reg = next;
            a->reg_words = words;
            next += words;
            continue;
        }
        if(aarch64){
            next = max;
        } else if(!x86_64 && next < max){
            a->reg = next;
            a->reg_words = max - next;
            next = max;
        }
        a->stack = stack;
        stack += words - a->reg_words;
    }
    return stack;
}

// The register of the first argument word: one later when the hidden
// result pointer takes the first (x86_64, riscv64).
static int exp64_first_reg(bool hidden){
    return hidden && !S_EQ(current_process->target->backend->name, "aarch64") ? 1 : 0;
}

// Whether the backend may pop the words straight into the registers
// and leave the rest in place as the stack arguments: no copies, no
// temporaries between the words, the register words a prefix in
// register order and the stack words the rest in order.
static bool exp64_call_is_simple(struct exp64_call* call){
    int word = 0;
    int regs = 0;
    for(int i = 0; i < call->count; i++){
        struct exp64_arg* a = &call->args[i];
        if(a->kind == EXP64_ARG_REF || a->fp || a->slot != word){
            return false;
        }
        for(int w = 0; w < a->words; w++, word++){
            if(w < a->reg_words){
                if(regs != word || a->reg + w != call->first_reg + word){
                    return false;
                }
                regs++;
            } else if(a->stack + w - a->reg_words != word - regs){
                return false;
            }
        }
    }
    call->reg_words = regs;
    return word == call->slots;
}

// The callee's side of exp64_arg_layout: every parameter of `function`
// gets its frame offset and its entity. The prologue spilled argument
// register k to register_home + 8*k; the caller's outgoing stack word j
// is at stack_home + 8*j. A struct passed by reference keeps the
// address of the caller's copy in its slot. The prologue spilled
// floating-point argument register k to fp_home + 8*k.
static struct exp64_arg* exp64_parameter_layout(struct node* function, int* count_out){
    struct vector* params = function_node_argument_vec(function);
    int count = vector_count(params);
    struct datatype* types = calloc(count + 1, sizeof(struct datatype));
    struct exp64_arg* layout = calloc(count + 1, sizeof(struct exp64_arg));
    for(int i = 0; i < count; i++){
        types[i] = (*(struct node**)vector_at(params, i))->var.type;
    }
    exp64_arg_layout(types, count, count, exp64_returns_in_memory(&function->func.rtype), layout);
    free(types);
    *count_out = count;
    return layout;
}

void exp64_parameters(struct node* function, int register_home, int stack_home, int fp_home){
    struct vector* params = function_node_argument_vec(function);
    int count = 0;
    struct exp64_arg* layout = exp64_parameter_layout(function, &count);
    for(int i = 0; i < count; i++){
        struct node* param = *(struct node**)vector_at(params, i);
        struct exp64_arg* a = &layout[i];
        int flags = RESOLVER_DEFAULT_ENTITY_FLAG_IS_LOCAL_STACK;
        if(a->kind == EXP64_ARG_REF){
            flags |= RESOLVER_DEFAULT_ENTITY_FLAG_IS_REFERENCE;
        }
        if(a->fp){
            param->var.aoffset = fp_home + DATA_SIZE_DDWORD * a->reg;
        } else {
            param->var.aoffset = a->reg >= 0 ? register_home + DATA_SIZE_DDWORD * a->reg
                                             : stack_home + DATA_SIZE_DDWORD * a->stack;
        }
        resolver_default_new_scope_entity(current_process->resolver, param, param->var.aoffset, flags);
    }
    free(layout);
}

// The floating-point argument registers the parameters of `function`
// arrive in: the prologue spills that many (none for most functions,
// whose code does not change).
int exp64_fp_parameter_registers(struct node* function){
    int count = 0;
    int used = 0;
    struct exp64_arg* layout = exp64_parameter_layout(function, &count);
    for(int i = 0; i < count; i++){
        if(layout[i].fp && layout[i].reg + 1 > used){
            used = layout[i].reg + 1;
        }
    }
    free(layout);
    return used;
}

static struct datatype exp64_call(struct node* node){
    struct node* callee = node->exp.left;
    while(callee->type == NODE_TYPE_EXPRESSION_PARENTHESES){
        callee = callee->parenthesis.exp;
    }
    struct vector* args = exp64_call_arguments(node);
    if(callee->type == NODE_TYPE_IDENTIFIER && !resolver_get_variable(0, current_process->resolver, callee->sval)
       && !resolver_get_function(0, current_process->resolver, callee->sval)
       && symresolver_get_symbol_for_native_function(current_process, callee->sval)){
        return exp64_stdarg(callee->sval, args);
    }

    struct datatype return_type;
    struct vector* params = 0;
    const char* symbol = 0;
    bool variadic = false;
    if(exp64_names_function(callee)){
        struct resolver_entity* function = resolver_get_function(0, current_process->resolver, callee->sval);
        return_type = function->dtype;
        params = function->node->func.args.vector;
        variadic = function->node->func.flags & FUNCTION_NODE_FLAG_IS_VARIADIC;
        symbol = function->name;
    } else {
        struct datatype t = exp64_typeof(callee);
        return_type = t;
        if(!(t.flags & DATATYPE_FLAG_IS_FUNCTION_POINTER)){
            compiler_error(current_process, "Called object is not a function or function pointer\n");
        }
        return_type = *datatype_pointer_reduce(&t, 1);
        return_type.flags &= ~DATATYPE_FLAG_IS_FUNCTION_POINTER;
    }

    struct exp64_call call = { .symbol = symbol };
    if(exp64_is_struct(&return_type)){
        call.struct_size = align_value(exp64_size(&return_type), DATA_SIZE_DDWORD);
        call.struct_in_regs = !exp64_returns_in_memory(&return_type);
        ops->reserve(call.struct_size);
    }
    if(!symbol){
        struct datatype t = exp64_value(callee);
        ops->push(&t);
    }
    // B1: every argument is pushed as its words, a struct by value too,
    // last argument first. The ledger says where each one landed.
    struct vector* ledger = current_function->func.frame.elements;
    int base = vector_count(ledger);
    int count = vector_count(args);
    struct datatype* types = calloc(count + 1, sizeof(struct datatype));
    int* pushed = calloc(count + 1, sizeof(int));
    for(int i = count - 1; i >= 0; i--){
        struct node* arg = *(struct node**)vector_at(args, i);
        struct datatype t = exp64_value(arg);
        if(exp64_is_struct(&t)){
            ops->push_struct(exp64_size(&t));
        } else {
            if(params && i < vector_count(params)){
                struct node* param = *(struct node**)vector_at(params, i);
                exp64_convert(0, &t, &param->var.type);
                if(exp64_is_floating(&t) || exp64_is_floating(&param->var.type)){
                    t = param->var.type;
                }
            } else if(exp64_is_floating(&t) && t.size == DATA_SIZE_DWORD){
                // The default argument promotions: a float passed to
                // `...` or without a prototype is a double.
                struct datatype d = datatype_floating(DATA_SIZE_DDWORD);
                exp64_convert(0, &t, &d);
                t = d;
            }
            ops->push(&t);
        }
        types[i] = t;
        pushed[i] = vector_count(ledger);
    }
    // The arguments past the prototype's parameters of a variadic
    // function are its variadic ones; without a prototype all are named.
    int named = params && variadic ? vector_count(params) : count;
    call.count = count;
    call.args = calloc(count + 1, sizeof(struct exp64_arg));
    call.first_reg = exp64_first_reg(call.struct_size && !call.struct_in_regs);
    call.stack_words = exp64_arg_layout(types, count, named, call.struct_size && !call.struct_in_regs, call.args);
    for(int i = 0; i < count; i++){
        if(call.args[i].fp && call.args[i].reg + 1 > call.fp_regs){
            call.fp_regs = call.args[i].reg + 1;
        }
    }
    if(exp64_is_floating(&return_type)){
        call.float_return = return_type.size;
    }
    call.slots = vector_count(ledger) - base;
    call.area = DATA_SIZE_DDWORD * call.stack_words;
    for(int i = 0; i < count; i++){
        struct exp64_arg* a = &call.args[i];
        a->slot = vector_count(ledger) - pushed[i];
        if(a->kind == EXP64_ARG_REF){
            a->copy = call.area;
            call.area += DATA_SIZE_DDWORD * a->words;
        }
    }
    call.simple = exp64_call_is_simple(&call);
    ops->call(&call);
    free(types);
    free(pushed);
    free(call.args);
    if(!call.struct_size && !exp64_is_void(&return_type) && !exp64_is_pointer(&return_type)
       && !exp64_is_floating(&return_type) && exp64_scalar_size(&return_type) < DATA_SIZE_DDWORD){
        // The ABIs leave the bits above a narrow return value undefined.
        ops->extend(0, exp64_scalar_size(&return_type), exp64_is_signed(&return_type));
    }
    return return_type;
}

// ---------------------------------------------------------------
// Values
// ---------------------------------------------------------------

// The C type of a constant (C11 6.4.4.1), from its spelling's flags:
// datatype_for_number_node, which the 32-bit backend uses too.
static struct datatype exp64_number_type(struct node* node){
    struct datatype t = datatype_for_number_node(node);
    if(exp64_is_floating(&t)){
        return datatype_floating(t.size);
    }
    return exp64_int_type(t.size, t.flags & DATATYPE_FLAG_IS_SIGNED);
}

// A constant in canonical form for its type: an int -1 folded to
// 0xFFFFFFFF is sign extended again. A floating constant is the bits
// of its float or double.
static long long exp64_number_value(struct node* node){
    struct datatype t = datatype_for_number_node(node);
    if(exp64_is_floating(&t)){
        return (long long)floating_bits(number_node_double(node), t.size);
    }
    return datatype_canonical_value(node->llnum, &t);
}

static struct datatype exp64_number(struct node* node){
    ops->imm(0, exp64_number_value(node));
    return exp64_number_type(node);
}

// `++x`, `x++`, `--x`, `x--`: the target's address is worked out once,
// and the step is the one `x + 1` takes (an element for a pointer). The
// value is the new one, or for the postfix forms the new one stepped
// back, which is the old one in x's type.
static struct datatype exp64_increment(struct node* node){
    const char* step_op = S_EQ(node->unary.op, "++") ? "+" : "-";
    const char* back_op = S_EQ(node->unary.op, "++") ? "-" : "+";
    struct datatype one = exp64_int_type(DATA_SIZE_DWORD, true);
    struct datatype lt = exp64_address(node->unary.operand);
    if(exp64_is_struct(&lt) || exp64_is_array(&lt)){
        compiler_error(current_process, "%s needs a scalar operand\n", node->unary.op);
    }
    if(exp64_is_floating(&lt)){
        // x + 1 - 1 need not be x in floating point, so the old value
        // is kept: on the stack under the address.
        struct datatype address = exp64_pointer_to(&lt);
        ops->push(&address);
        exp64_load(&lt, true);
        ops->pop(1);
        ops->push(&lt);
        ops->swap();
        ops->push(&address);
        ops->swap();
        ops->imm(1, 1);
        struct datatype result = exp64_arith(step_op, &lt, &one);
        exp64_convert(0, &result, &lt);
        ops->pop(1);
        ops->store(exp64_scalar_size(&lt));
        if(node->unary.flags & UNARY_FLAG_IS_POSTFIX){
            ops->pop(0);
        } else {
            ops->pop(1);
        }
        return lt;
    }
    ops->push(&lt);
    exp64_load(&lt, true);
    ops->imm(1, 1);
    struct datatype result = exp64_arith(step_op, &lt, &one);
    exp64_convert(0, &result, &lt);
    ops->pop(1);
    ops->store(exp64_scalar_size(&lt));
    if(node->unary.flags & UNARY_FLAG_IS_POSTFIX){
        ops->imm(1, 1);
        result = exp64_arith(back_op, &lt, &one);
        exp64_convert(0, &result, &lt);
    }
    return lt;
}

static struct datatype exp64_unary(struct node* node){
    const char* op = node->unary.op;
    if(is_increment_operator(op)){
        return exp64_increment(node);
    }
    if(op_is_address(op)){
        struct datatype t = exp64_address(node->unary.operand);
        return exp64_pointer_to(&t);
    }
    if(S_EQ(op, "!")){
        exp64_truth(node->unary.operand);
        ops->unop("!", DATA_SIZE_DDWORD);
        return exp64_int_type(DATA_SIZE_DWORD, true);
    }
    struct datatype t = exp64_value(node->unary.operand);
    if(exp64_is_floating(&t)){
        if(S_EQ(op, "+")){
            return t;
        }
        if(!S_EQ(op, "-")){
            compiler_error(current_process, "The operator %s does not take a floating operand\n", op);
        }
        // Negation flips the sign bit, -0.0 and NaNs included.
        ops->imm(1, t.size == DATA_SIZE_DWORD ? 0x80000000LL : (long long)0x8000000000000000ULL);
        ops->binop("^", DATA_SIZE_DDWORD, false);
        return t;
    }
    struct datatype p = exp64_promote(&t);
    ops->unop(op, exp64_scalar_size(&p));
    if(exp64_scalar_size(&p) < DATA_SIZE_DDWORD){
        ops->extend(0, exp64_scalar_size(&p), exp64_is_signed(&p));
    }
    return p;
}

static struct datatype exp64_expression(struct node* node, bool decay){
    const char* op = node->exp.op;
    if(S_EQ(op, "=") || exp64_is_compound_assignment(op)){
        return exp64_assignment(node);
    }
    if(S_EQ(op, ",")){
        exp64_value(node->exp.left);
        return exp64_rvalue(node->exp.right, decay);
    }
    if(is_logical_operator(op)){
        return exp64_logical(node);
    }
    if(S_EQ(op, "?")){
        return exp64_ternary(node);
    }
    if(is_parentheses_node(node)){
        return exp64_call(node);
    }
    if(is_array_node(node) || is_access_node(node)){
        struct datatype t = exp64_address(node);
        return exp64_load(&t, decay);
    }
    return exp64_binary(node);
}

static struct datatype exp64_rvalue_of(struct node* node, bool decay);

// B1: a scalar worked out of a struct a call returned (`f().x`,
// `g(h())`) leaves the call's room on the expression stack; with the
// value in R0 the room is dropped, so whatever was pushed before is
// on top again.
static struct datatype exp64_rvalue(struct node* node, bool decay){
    struct vector* ledger = current_function->func.frame.elements;
    int mark = vector_count(ledger);
    struct datatype t = exp64_rvalue_of(node, decay);
    if(vector_count(ledger) > mark && !exp64_is_struct(&t)){
        ops->drop(vector_count(ledger) - mark);
    }
    return t;
}

static struct datatype exp64_rvalue_of(struct node* node, bool decay){
    // Errors name the line of the expression being generated.
    if(node->pos.line){
        current_process->pos = node->pos;
    }
    switch(node->type){
        case NODE_TYPE_NUMBER:
            return exp64_number(node);
        case NODE_TYPE_STRING: {
            ops->string_address(node->sval);
            struct datatype t = exp64_int_type(DATA_SIZE_BYTE, true);
            return exp64_pointer_to(&t);
        }
        case NODE_TYPE_IDENTIFIER: {
            // A function used as a value is its address.
            struct datatype t = exp64_address(node);
            return exp64_names_function(node) ? t : exp64_load(&t, decay);
        }
        case NODE_TYPE_EXPRESSION_PARENTHESES:
            return exp64_rvalue(node->parenthesis.exp, decay);
        case NODE_TYPE_EXPRESSION:
            return exp64_expression(node, decay);
        case NODE_TYPE_UNARY:
            if(op_is_indirection(node->unary.op)){
                // `*f` of a function pointer is the function: f again.
                struct datatype ot = exp64_typeof(node->unary.operand);
                if(ot.flags & DATATYPE_FLAG_IS_FUNCTION_POINTER){
                    return exp64_value(node->unary.operand);
                }
                struct datatype t = exp64_address(node);
                return exp64_load(&t, decay);
            }
            return exp64_unary(node);
        case NODE_TYPE_CAST: {
            struct datatype from = exp64_value(node->cast.operand);
            exp64_convert(0, &from, &node->cast.dtype);
            return node->cast.dtype;
        }
        case NODE_TYPE_BLANK: {
            ops->imm(0, 0);
            return exp64_int_type(DATA_SIZE_DWORD, true);
        }
    }
    compiler_error(current_process, "Cannot generate this expression (node type %i)\n", node->type);
    return exp64_int_type(DATA_SIZE_DWORD, true);
}

static struct datatype exp64_value(struct node* node){
    return exp64_rvalue(node, true);
}

// ---------------------------------------------------------------
// Entry points for the backends' statements
// ---------------------------------------------------------------

// a static local lives in the data section under a label of its
// own, `__static_<n>_<name>`, which the variable's entity now names.
// The backend emits the returned copy as a static global.
struct node* exp64_static_local(struct resolver_entity* entity, struct node* var){
    static int count;
    struct resolver_default_entity_data* data = resolver_default_entity_private(entity);
    struct node* copy = calloc(1, sizeof(struct node));
    *copy = *var;
    char* label = malloc(strlen(var->var.name) + 32);
    sprintf(label, "__static_%i_%s", ++count, var->var.name);
    copy->var.name = label;
    snprintf(data->base_address, sizeof(data->base_address), "%s", label);
    return copy;
}

// The value of `node` in R0; its type.
struct datatype exp64_generate(struct node* node){
    return exp64_value(node);
}

// A `return` value: converted to the function's return type when either
// is floating (`return 1;` in a double function), since the result
// travels in a floating-point register. Integer results are left as
// they were; the caller extends a narrow one.
struct datatype exp64_return_value(struct node* node, struct datatype* rtype){
    struct datatype t = exp64_value(node);
    if(exp64_is_floating(&t) || exp64_is_floating(rtype)){
        exp64_convert(0, &t, rtype);
        return *rtype;
    }
    return t;
}

// A condition: jump to false_label when it is false. A comparison
// compares and branches; anything else is tested against 0. Saves
// the 0/1 value and its test in every loop condition.
void exp64_condition(struct node* node, const char* false_label){
    while(node->type == NODE_TYPE_EXPRESSION_PARENTHESES){
        node = node->parenthesis.exp;
    }
    if(node->type != NODE_TYPE_EXPRESSION || !exp64_is_comparison(node->exp.op)){
        exp64_truth(node);
        ops->branch_false(0, false, false_label);
        return;
    }
    struct datatype lt = exp64_value(node->exp.left);
    struct datatype rt;
    if(node->exp.right->type == NODE_TYPE_NUMBER){
        rt = exp64_number_type(node->exp.right);
        ops->imm(1, exp64_number_value(node->exp.right));
    } else {
        ops->push(&lt);
        rt = exp64_value(node->exp.right);
        ops->mov_r1();
        ops->pop(0);
    }
    if(exp64_is_floating(&lt) || exp64_is_floating(&rt)){
        // No flags to branch on: the 0/1 of the comparison is tested.
        exp64_arith_floating(node->exp.op, &lt, &rt);
        ops->branch_false(0, false, false_label);
        return;
    }
    bool is_signed = false;
    if(!exp64_is_pointer(&lt) && !exp64_is_pointer(&rt)){
        struct datatype c = exp64_common(&lt, &rt);
        exp64_convert(0, &lt, &c);
        exp64_convert(1, &rt, &c);
        is_signed = exp64_is_signed(&c);
    }
    ops->branch_false(node->exp.op, is_signed, false_label);
}

// The value of `node`, pushed (a struct as its address).
struct datatype exp64_push(struct node* node){
    struct datatype t = exp64_value(node);
    ops->push(&t);
    return t;
}

// A local's `= value`, an expression. A brace initializer or a string
// for a char array reaches the backends as stores after the declaration
// (parser_lower_local_initializer), a struct value as `v = w;`.
void exp64_initialize_local(struct node* var){
    struct datatype* t = &var->var.type;
    struct node* value = var->var.val;
    int offset = var->var.aoffset;
    struct datatype vt = exp64_value(value);
    if(exp64_is_struct(t)){
        ops->mov_r1();
        ops->frame_address(offset);
        ops->copy(exp64_size(t));
        return;
    }
    exp64_convert(0, &vt, t);
    ops->push(&vt);
    ops->frame_address(offset);
    ops->mov_r1();
    ops->pop(0);
    ops->store(exp64_scalar_size(t));
}
