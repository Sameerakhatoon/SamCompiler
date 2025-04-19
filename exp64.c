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
// The current function's first variadic argument (exp64_parameters):
// frame offsets of its register home slot (or of its stack slot when
// the registers are used up), of the end of the register slots, and of
// the first stack argument no named parameter took; and of the home
// slot of its floating-point register and the end of those slots.
static int                     exp64_va_gp;
static int                     exp64_va_gp_end;
static int                     exp64_va_overflow;
static int                     exp64_va_fp;
static int                     exp64_va_fp_end;
// the frame offset of the room where va_arg puts together a
// struct whose pieces came in two kinds of registers (exp64_fp_home_size).
static int                     exp64_va_scratch;
// the frame offset of the q register home of a variadic
// aarch64 function that reads long doubles, or 0.
static int                     exp64_va_qhome;
#define EXP64_VA_SCRATCH 32
// on x86_64 a va_list points at the psABI's
// `struct __va_list_tag`: the byte offsets of the next integer and
// floating-point register in the register save area, the next stack
// argument and the save area, which holds the six integer argument
// registers and then the eight xmm registers 16 bytes apart. A variadic
// function's prologue fills a save area (exp64_va_save), and each
// va_start or __builtin_va_copy of the function has 24 bytes of its own
// for the state, from exp64_va_slots on.
#define EXP64_PSABI_GP_OFFSET 0
#define EXP64_PSABI_FP_OFFSET 4
#define EXP64_PSABI_OVERFLOW  8
#define EXP64_PSABI_SAVE_AREA 16
#define EXP64_PSABI_STATE     24
#define EXP64_PSABI_GP_END    48
#define EXP64_PSABI_FP_END    176
#define EXP64_PSABI_FP_STEP   16
static int                     exp64_va_save;
static int                     exp64_va_slots;
static int                     exp64_va_next_slot;
static int                     exp64_va_gp_offset;
static int                     exp64_va_fp_offset;
static bool                    exp64_needs_scratch(struct node* function);
static bool                    exp64_needs_qhome(struct node* function);
static bool                    exp64_needs_save_area(struct node* function);

static struct datatype exp64_value(struct node* node);
static struct datatype exp64_rvalue(struct node* node, bool decay);
static struct datatype exp64_address(struct node* node);
static struct datatype exp64_number_type(struct node* node);
static long long       exp64_number_value(struct node* node);
static bool            exp64_names_function(struct node* node);
static int             exp64_struct_pieces(struct datatype* t, struct exp64_piece* out);

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

// a long double's value is the address of its 16 bytes, as a
// struct's is; the backend's ld_* ops work on them in memory.
static bool exp64_is_ldouble(struct datatype* t){
    return datatype_is_long_double(t);
}

// A value that is the address of the object: a struct or a long double.
static bool exp64_in_memory(struct datatype* t){
    return exp64_is_struct(t) || exp64_is_ldouble(t);
}

#define EXP64_LD_SIZE 16

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
    if(exp64_is_ldouble(a) || exp64_is_ldouble(b)){
        return datatype_long_double();
    }
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

// R0 held a value of type `from` and one of the two is a long
// double. A conversion to long double puts it in new room on the
// expression stack (R0 its address); one from it reads the long double
// at R0.
static void exp64_convert_ldouble(struct datatype* from, struct datatype* to){
    if(exp64_is_ldouble(from) && exp64_is_ldouble(to)){
        return;
    }
    if(exp64_is_ldouble(to)){
        if(exp64_is_struct(from) || exp64_is_void(from) || exp64_is_pointer(from) || exp64_is_array(from)){
            compiler_error(current_process, "Only an arithmetic value can become a long double\n");
        }
        int kind = EXP64_LD_S64;
        if(exp64_is_floating(from)){
            kind = from->size == DATA_SIZE_DWORD ? EXP64_LD_FLOAT : EXP64_LD_DOUBLE;
        } else if(exp64_scalar_size(from) == DATA_SIZE_DDWORD && !exp64_is_signed(from)){
            kind = EXP64_LD_U64;
        }
        ops->reserve(EXP64_LD_SIZE);
        ops->ld_from(kind);
        return;
    }
    if(to->flags & DATATYPE_FLAG_IS_BOOL){
        ops->ld_truth();
        return;
    }
    if(exp64_is_floating(to)){
        ops->ld_to(to->size == DATA_SIZE_DWORD ? EXP64_LD_FLOAT : EXP64_LD_DOUBLE);
        return;
    }
    if(exp64_is_pointer(to)){
        compiler_error(current_process, "A long double cannot become a pointer\n");
    }
    int size = exp64_scalar_size(to);
    ops->ld_to(size == DATA_SIZE_DDWORD && !exp64_is_signed(to) ? EXP64_LD_U64 : EXP64_LD_S64);
    if(size < DATA_SIZE_DDWORD){
        ops->extend(0, size, exp64_is_signed(to));
    }
}

static void exp64_convert(int reg, struct datatype* from, struct datatype* to){
    if(exp64_is_void(to) || exp64_is_struct(to) || exp64_is_array(to)){
        return;
    }
    if(exp64_is_ldouble(from) || exp64_is_ldouble(to)){
        if(reg){
            compiler_error(current_process, "BUG: a long double conversion of R1\n");
        }
        exp64_convert_ldouble(from, to);
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
    if((to->flags & DATATYPE_FLAG_IS_BOOL) && !exp64_is_pointer(to) && !(from->flags & DATATYPE_FLAG_IS_BOOL)
       && !exp64_is_void(from) && !exp64_is_struct(from)){
        // To _Bool: 0 or 1 (C11 6.3.1.2), not the low byte, so an
        // argument `count(0x200)` for a _Bool parameter is 1. R1 is kept
        // on the stack meanwhile.
        struct datatype word = exp64_int_type(DATA_SIZE_DDWORD, false);
        if(reg){
            ops->swap();
        }
        ops->swap();
        ops->push(&word);
        ops->swap();
        ops->imm(1, 0);
        ops->compare("!=", false);
        ops->pop(1);
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

// the register a local or parameter named by `node` lives in
// at -O1 (regalloc_function), or -1; its type goes to *type. Such a
// variable is read, assigned and stepped right in its register and
// never has an address.
static int exp64_register_of(struct node* node, struct datatype* type){
    while(node && node->type == NODE_TYPE_EXPRESSION_PARENTHESES){
        node = node->parenthesis.exp;
    }
    if(!node || node->type != NODE_TYPE_IDENTIFIER || !ops->reg_get){
        return -1;
    }
    struct resolver_entity* entity = resolver_get_variable(0, current_process->resolver, node->sval);
    if(!entity){
        return -1;
    }
    struct resolver_default_entity_data* data = resolver_default_entity_private(entity);
    if(!data || !(data->flags & RESOLVER_DEFAULT_ENTITY_FLAG_IS_LOCAL_STACK)){
        return -1;
    }
    int reg = regalloc_register(entity->node);
    if(reg >= 0){
        *type = entity->dtype;
    }
    return reg;
}

static struct datatype exp64_identifier_address(struct node* node){
    struct resolver_process* resolver = current_process->resolver;
    struct resolver_entity* entity = resolver_get_variable(0, resolver, node->sval);
    if(entity){
        struct resolver_default_entity_data* data = resolver_default_entity_private(entity);
        if(data && (data->flags & RESOLVER_DEFAULT_ENTITY_FLAG_IS_LOCAL_STACK)){
            if(regalloc_register(entity->node) >= 0){
                // regalloc_function missed a use that needs the object.
                compiler_error(current_process, "Internal error: %s lives in a register and has no address\n", node->sval);
            }
            ops->frame_address(data->offset);
            if(data->flags & RESOLVER_DEFAULT_ENTITY_FLAG_IS_REFERENCE){
                // B1: a big struct parameter: the slot holds the
                // address of the caller's copy.
                ops->load(DATA_SIZE_DDWORD, false);
            }
        } else if(entity->dtype.flags & DATATYPE_FLAG_IS_THREAD_LOCAL){
            // this thread's copy. A static local is always
            // defined here.
            const char* name = data ? data->base_address : entity->name;
            bool here = !S_EQ(name, entity->name) || variable_defined_in_tree(current_process, name);
            ops->tls_address(name, here);
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
    // A union member is at 0, unless it is inside an anonymous struct
    // member of the union (struct_member_named).
    return offset;
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
    compiler_check_stack(current_process);
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
    // A struct value (a call's result) is already an address, a long
    // double too.
    struct datatype t = exp64_value(node);
    if(!exp64_in_memory(&t)){
        compiler_error(current_process, "This expression has no address\n");
    }
    return t;
}

static struct datatype exp64_unqualified(struct datatype* t);
static void exp64_atomic_memory_load(struct datatype* t, int order);

// The value of the object at the address in R0. That of an
// _Atomic struct, union or long double is a copy read atomically.
static struct datatype exp64_load(struct datatype* t, bool decay){
    if(exp64_is_array(t)){
        return decay ? exp64_decay(t) : *t;
    }
    if(exp64_in_memory(t) || exp64_is_void(t)){
        if(datatype_is_atomic(t)){
            if(exp64_is_void(t)){
                compiler_error(current_process, "An _Atomic void object is not supported\n");
            }
            exp64_atomic_memory_load(t, ATOMIC_ORDER_SEQ_CST);
            return exp64_unqualified(t);
        }
        return *t;
    }
    if(datatype_is_atomic(t)){
        ops->atomic_load(exp64_scalar_size(t), exp64_is_signed(t), ATOMIC_ORDER_SEQ_CST);
        return *t;
    }
    ops->load(exp64_scalar_size(t), exp64_is_signed(t));
    return *t;
}

// *R1 = R0 for an object of type t: an atomic store for an _Atomic one
// (C11 6.5.16.1 with 5.1.2.4: sequentially consistent).
static void exp64_store(struct datatype* t){
    if(datatype_is_atomic(t)){
        ops->atomic_store(exp64_scalar_size(t), ATOMIC_ORDER_SEQ_CST);
        return;
    }
    ops->store(exp64_scalar_size(t));
}

// ---------------------------------------------------------------
// long double operations
// ---------------------------------------------------------------

// R0 = the word in the expression-stack slot at ledger index `index`.
static void exp64_slot_load(int index){
    ops->slot_address(vector_count(current_function->func.frame.elements) - 1 - index);
    ops->load(DATA_SIZE_DDWORD, false);
}

// Push R0 and return its ledger index.
static int exp64_push_index(void){
    struct datatype word = exp64_int_type(DATA_SIZE_DDWORD, false);
    ops->push(&word);
    return vector_count(current_function->func.frame.elements) - 1;
}

// R0 = the long double at the address in the slot at left_index op the
// one at the address in the slot at right_index: a new long double, or
// the int of a comparison.
static bool exp64_is_comparison(const char* op);

static struct datatype exp64_ld_operate(const char* op, int left_index, int right_index){
    exp64_slot_load(right_index);
    ops->mov_r1();
    exp64_slot_load(left_index);
    if(exp64_is_comparison(op)){
        ops->ld_compare(op);
        return exp64_int_type(DATA_SIZE_DWORD, true);
    }
    if(!(S_EQ(op, "+") || S_EQ(op, "-") || S_EQ(op, "*") || S_EQ(op, "/"))){
        compiler_error(current_process, "The operator %s does not take long double operands\n", op);
    }
    ops->reserve(EXP64_LD_SIZE);
    ops->ld_arith(op);
    return datatype_long_double();
}

// A long double constant: its bytes stored into new room.
static struct datatype exp64_ld_number(struct node* node){
    unsigned char bytes[16];
    long_double_from_text(node->num.text, bytes);
    unsigned long long lo = 0, hi = 0;
    for(int i = 7; i >= 0; i--){
        lo = (lo << 8) | bytes[i];
        hi = (hi << 8) | bytes[8 + i];
    }
    ops->reserve(EXP64_LD_SIZE);
    ops->slot_address(0);
    ops->mov_r1();
    ops->imm(0, (long long)lo);
    ops->store(DATA_SIZE_DDWORD);
    ops->slot_address(0);
    ops->add_imm(DATA_SIZE_DDWORD);
    ops->mov_r1();
    ops->imm(0, (long long)hi);
    ops->store(DATA_SIZE_DDWORD);
    ops->slot_address(0);
    return datatype_long_double();
}

// R0 = the address of a copy of the long double at R0, in new room.
static void exp64_ld_copy(void){
    ops->mov_r1();
    ops->reserve(EXP64_LD_SIZE);
    ops->slot_address(0);
    ops->copy(EXP64_LD_SIZE);
    ops->slot_address(0);
}

// -x: a copy with the sign bit flipped (byte 9 of the x87 format, 15 of
// binary128), -0.0 and NaNs included.
static struct datatype exp64_ld_negate(void){
    exp64_ld_copy();
    ops->add_imm(long_double_is_x87() ? 9 : 15);
    int at = exp64_push_index();
    ops->load(DATA_SIZE_BYTE, false);
    ops->imm(1, 0x80);
    ops->binop("^", DATA_SIZE_DWORD, false);
    ops->mov_r1();
    exp64_slot_load(at);
    ops->swap();
    ops->store(DATA_SIZE_BYTE);
    ops->drop(1);
    ops->slot_address(0);
    return datatype_long_double();
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
    // exp64_rvalue rather than exp64_value: the left operands of a long
    // chain nest, and each frame counts.
    struct datatype lt = exp64_rvalue(node->exp.left, true);
    struct datatype ld = datatype_long_double();
    if(exp64_is_ldouble(&lt)){
        // both operands as long doubles in memory.
        int left = exp64_push_index();
        struct datatype rt = exp64_value(node->exp.right);
        exp64_convert(0, &rt, &ld);
        int right = exp64_push_index();
        return exp64_ld_operate(node->exp.op, left, right);
    }
    if(node->exp.right->type == NODE_TYPE_NUMBER && !(node->exp.right->num.flags & NUMBER_FLAG_LONG_DOUBLE)){
        struct datatype rt = exp64_number_type(node->exp.right);
        ops->imm(1, exp64_number_value(node->exp.right));
        return exp64_arith(node->exp.op, &lt, &rt);
    }
    ops->push(&lt);
    int left_value = vector_count(current_function->func.frame.elements) - 1;
    struct datatype rt = exp64_value(node->exp.right);
    if(exp64_is_ldouble(&rt)){
        // the left operand converted once the right one turns
        // out to be a long double.
        int right = exp64_push_index();
        exp64_slot_load(left_value);
        exp64_convert(0, &lt, &ld);
        int left = exp64_push_index();
        return exp64_ld_operate(node->exp.op, left, right);
    }
    ops->mov_r1();
    ops->pop(0);
    return exp64_arith(node->exp.op, &lt, &rt);
}

static bool exp64_is_compound_assignment(const char* op){
    return S_EQ(op, "+=") || S_EQ(op, "-=") || S_EQ(op, "*=") || S_EQ(op, "/=") || S_EQ(op, "%=")
        || S_EQ(op, "&=") || S_EQ(op, "|=") || S_EQ(op, "^=") || S_EQ(op, "<<=") || S_EQ(op, ">>=");
}

// the assignment of a value of type rt (R1, an address for a
// long double) to the object of type lt at R0, either of them a long
// double and the other not.
static struct datatype exp64_ld_store(struct datatype* rt, struct datatype* lt){
    int target = exp64_push_index();
    ops->swap();
    exp64_convert(0, rt, lt);
    ops->mov_r1();
    exp64_slot_load(target);
    if(exp64_is_ldouble(lt)){
        ops->copy(EXP64_LD_SIZE);
        exp64_slot_load(target);
        return *lt;
    }
    ops->swap();
    ops->store(exp64_scalar_size(lt));
    return *lt;
}

// ---------------------------------------------------------------
// Atomics
// ---------------------------------------------------------------

static struct datatype exp64_void_type(void){
    struct datatype t = {0};
    t.type     = DATA_TYPE_VOID;
    t.type_str = "void";
    return t;
}

// The bytes of an atomic access to an object of type t: a scalar of 1,
// 2, 4 or 8 bytes, which every target has atomic instructions for.
static int exp64_atomic_size(struct datatype* t){
    if(exp64_is_struct(t) || exp64_is_void(t) || exp64_is_array(t)){
        compiler_error(current_process, "An atomic operation needs an integer, pointer or floating object, not a struct, union, array or void\n");
    }
    return exp64_scalar_size(t);
}

// The value type of an atomic object: the same type, not atomic.
static struct datatype exp64_unqualified(struct datatype* t){
    struct datatype u = *t;
    u.atomic_level = 0;
    return u;
}

// R0 (of type `from`) converted for storing in an object of type `to`:
// as exp64_convert, and to a _Bool its test against 0 (C11 6.3.1.2).
// Keeps nothing in R1.
static void exp64_convert_for_store(struct datatype* from, struct datatype* to){
    if((to->flags & DATATYPE_FLAG_IS_BOOL) && !exp64_is_pointer(to) && !exp64_is_floating(from)){
        ops->imm(1, 0);
        ops->compare("!=", false);
        return;
    }
    exp64_convert(0, from, to);
}

// R0 = R0 op R1 for the fetch-and-op operators on an object of type t,
// in canonical form: what the backend's atomic_rmw stored.
static void exp64_atomic_op_value(const char* op, struct datatype* t){
    int size = exp64_scalar_size(t);
    int width = size == DATA_SIZE_DDWORD ? DATA_SIZE_DDWORD : DATA_SIZE_DWORD;
    if(S_EQ(op, "nand")){
        ops->binop("&", width, false);
        ops->unop("~", width);
    } else {
        ops->binop(op, width, exp64_is_signed(t));
    }
    if(size < DATA_SIZE_DDWORD){
        ops->extend(0, size, exp64_is_signed(t));
    }
}

// `a op= v`, `++a`, `a++` on an _Atomic a whose address is in R0 (C11
// 6.5.2.4, 6.5.16.2): one atomic read-modify-write. + - & | ^ of an
// integer on an integer object, and + - on a pointer (the step scaled),
// are the backend's fetch-and-op: the low bits of the result only
// depend on the low bits of the operands. Anything else (* / % << >>,
// a floating object or operand, a _Bool) is a compare-and-swap loop that
// works out `old op v` in the usual arithmetic conversions and stores
// it only if no other thread changed the object meanwhile, else tries
// again with the value it found. right is NULL for ++ and --. The value
// is the new one, the old one for the postfix forms.
static struct datatype exp64_atomic_ld_update(const char* op, struct datatype* lt, struct node* right, bool postfix);
static struct datatype exp64_atomic_memory_assign(struct datatype* lt, struct datatype* rt);

static struct datatype exp64_atomic_update(const char* op, struct datatype* lt, struct node* right, bool postfix){
    if(exp64_is_ldouble(lt)){
        return exp64_atomic_ld_update(op, lt, right, postfix);
    }
    int size = exp64_atomic_size(lt);
    struct datatype t = exp64_unqualified(lt);
    struct datatype address = exp64_pointer_to(lt);
    ops->push(&address);                                    // [A]
    struct datatype rt = exp64_int_type(DATA_SIZE_DWORD, true);
    if(right){
        rt = exp64_value(right);
    } else {
        ops->imm(0, 1);
    }
    bool integer_operand = !exp64_is_pointer(&rt) && !exp64_is_floating(&rt) && !exp64_is_struct(&rt) && !exp64_is_void(&rt);
    bool bitwise = S_EQ(op, "&") || S_EQ(op, "|") || S_EQ(op, "^");
    bool additive = S_EQ(op, "+") || S_EQ(op, "-");
    bool fetch = integer_operand && !exp64_is_floating(&t) && !(t.flags & DATATYPE_FLAG_IS_BOOL)
              && (additive || (bitwise && !exp64_is_pointer(&t)));
    if(fetch){
        if(exp64_is_pointer(&t)){
            struct datatype element = exp64_deref(&t);
            struct datatype wide = exp64_int_type(DATA_SIZE_DDWORD, true);
            exp64_convert(0, &rt, &wide);
            if(exp64_size(&element) > 1){
                ops->mul_imm(0, exp64_size(&element));
            }
            rt = wide;
        }
        ops->push(&rt);                                     // [A, v]
        ops->peek(1, 1);
        ops->atomic_rmw(op, size, ATOMIC_ORDER_SEQ_CST);    // R0 = old
        if(size < DATA_SIZE_DDWORD){
            ops->extend(0, size, exp64_is_signed(&t));
        }
        if(!postfix){
            ops->peek(1, 0);
            exp64_atomic_op_value(op, &t);
        }
        ops->drop(2);
        return t;
    }
    ops->push(&rt);                                         // [A, v]
    ops->peek(0, 1);
    ops->atomic_load(size, exp64_is_signed(&t), ATOMIC_ORDER_SEQ_CST);
    ops->mov_r1();                                          // R1 = old
    int retry = exp64_new_label();
    ops->label(retry);
    ops->swap();
    ops->push(&t);                                          // [A, v, old]
    ops->peek(1, 1);
    struct datatype result = exp64_arith(op, &t, &rt);
    exp64_convert_for_store(&result, &t);                   // R0 = new
    ops->pop(1);                                            // R1 = old, [A, v]
    ops->atomic_cas(size, 1, ATOMIC_ORDER_SEQ_CST);     // R0 = what was there
    if(size < DATA_SIZE_DDWORD){
        ops->extend(0, size, exp64_is_signed(&t));
    }
    // Stored when that was old; otherwise it is the old value of the
    // next try.
    ops->push(&t);                                          // [A, v, seen]
    ops->compare("==", false);
    ops->mov_r1();
    ops->pop(0);                                            // R0 = seen, [A, v]
    ops->swap();                                            // R0 = stored?, R1 = seen
    ops->branch(retry, EXP64_BRANCH_IF_ZERO);
    ops->swap();                                            // R0 = old
    if(!postfix){
        ops->peek(1, 0);
        result = exp64_arith(op, &t, &rt);
        exp64_convert_for_store(&result, &t);
    }
    ops->drop(2);
    return t;
}

// register k (of type t, an integer other than _Bool or a
// pointer) += n of its steps: an element for a pointer, else 1. R0 is
// the new value; false for a type this does not take.
static bool exp64_register_step(int k, struct datatype* t, long long n){
    long long step = 1;
    if(exp64_is_pointer(t)){
        struct datatype element = exp64_deref(t);
        step = exp64_size(&element);
        if(step <= 0){
            return false;
        }
    } else if(exp64_is_floating(t) || (t->flags & DATATYPE_FLAG_IS_BOOL) || t->type == DATA_TYPE_VOID){
        return false;
    }
    ops->reg_get(k);
    ops->add_imm(n * step);
    if(!exp64_is_pointer(t)){
        // The sum in 64 bits, cut to x's type: what the int arithmetic
        // and the conversion back give.
        struct datatype wide = exp64_int_type(DATA_SIZE_DDWORD, true);
        exp64_convert(0, &wide, t);
    }
    ops->reg_set(k);
    return true;
}

// R0, the new value of a register variable exp64_register_step moved by
// n steps, back to the old one (the value of x++ and x--).
static void exp64_register_step_back(struct datatype* t, long long n){
    if(exp64_is_pointer(t)){
        struct datatype element = exp64_deref(t);
        ops->add_imm(-n * (long long)exp64_size(&element));
        return;
    }
    ops->add_imm(-n);
    struct datatype wide = exp64_int_type(DATA_SIZE_DDWORD, true);
    exp64_convert(0, &wide, t);
}

// `x = b` and `x op= b` for x in register k, of type lt. b
// cannot change x (x has no address), so x is read after it.
static struct datatype exp64_register_assignment(struct node* node, int k, struct datatype* lt){
    const char* op = node->exp.op;
    struct node* right = node->exp.right;
    if((S_EQ(op, "+=") || S_EQ(op, "-=")) && right->type == NODE_TYPE_NUMBER
       && !(right->num.flags & (NUMBER_FLAG_FLOATING | NUMBER_FLAG_FLOAT | NUMBER_FLAG_LONG_DOUBLE))
       && exp64_number_type(right).size <= DATA_SIZE_DWORD){
        long long n = exp64_number_value(right);
        if(exp64_register_step(k, lt, S_EQ(op, "+=") ? n : -n)){
            return *lt;
        }
    }
    if(S_EQ(op, "=")){
        struct datatype rt = exp64_value(node->exp.right);
        exp64_convert(0, &rt, lt);
        ops->reg_set(k);
        return *lt;
    }
    char arith_op[4] = {0};
    strncpy(arith_op, op, strlen(op) - 1);
    struct datatype old = *lt;
    struct datatype rt;
    if(node->exp.right->type == NODE_TYPE_NUMBER && !(node->exp.right->num.flags & NUMBER_FLAG_LONG_DOUBLE)){
        rt = exp64_number_type(node->exp.right);
        ops->reg_get(k);
        ops->imm(1, exp64_number_value(node->exp.right));
    } else {
        rt = exp64_value(node->exp.right);
        if(exp64_is_ldouble(&rt)){
            // worked out in long double, then converted.
            struct datatype ld = rt;
            int right = exp64_push_index();
            ops->reg_get(k);
            exp64_convert(0, &old, &ld);
            int left = exp64_push_index();
            exp64_ld_operate(arith_op, left, right);
            exp64_convert(0, &ld, lt);
            ops->reg_set(k);
            return *lt;
        }
        ops->mov_r1();
        ops->reg_get(k);
    }
    struct datatype result = exp64_arith(arith_op, &old, &rt);
    exp64_convert(0, &result, lt);
    ops->reg_set(k);
    return *lt;
}

// `a = b` and `a op= b`. The target's address is worked out once.
static struct datatype exp64_assignment(struct node* node){
    const char* op = node->exp.op;
    struct datatype lt;
    int k = exp64_register_of(node->exp.left, &lt);
    if(k >= 0){
        return exp64_register_assignment(node, k, &lt);
    }
    // A plain variable's address takes only R0, so the value can wait
    // in R1 instead of on the stack.
    bool simple = node->exp.left->type == NODE_TYPE_IDENTIFIER && !exp64_names_function(node->exp.left);
    if(S_EQ(op, "=") && simple){
        struct datatype rt = exp64_value(node->exp.right);
        ops->mov_r1();
        lt = exp64_address(node->exp.left);
        if(datatype_is_atomic(&lt) && exp64_in_memory(&lt)){
            return exp64_atomic_memory_assign(&lt, &rt);
        }
        if(exp64_is_struct(&lt) || (exp64_is_ldouble(&lt) && exp64_is_ldouble(&rt))){
            // copy wants the source in R1 and the target in R0.
            ops->copy(exp64_size(&lt));
            return lt;
        }
        if(exp64_is_ldouble(&lt) || exp64_is_ldouble(&rt)){
            return exp64_ld_store(&rt, &lt);
        }
        ops->swap();
        exp64_convert(0, &rt, &lt);
    } else if(S_EQ(op, "=")){
        struct datatype rt = exp64_value(node->exp.right);
        ops->push(&rt);
        lt = exp64_address(node->exp.left);
        if(datatype_is_atomic(&lt) && exp64_in_memory(&lt)){
            ops->pop(1);
            return exp64_atomic_memory_assign(&lt, &rt);
        }
        if(exp64_is_struct(&lt) || (exp64_is_ldouble(&lt) && exp64_is_ldouble(&rt))){
            ops->pop(1);
            ops->copy(exp64_size(&lt));
            return lt;
        }
        if(exp64_is_ldouble(&lt) || exp64_is_ldouble(&rt)){
            ops->pop(1);
            return exp64_ld_store(&rt, &lt);
        }
        ops->mov_r1();
        ops->pop(0);
        exp64_convert(0, &rt, &lt);
    } else {
        char arith_op[4] = {0};
        strncpy(arith_op, op, strlen(op) - 1);
        lt = exp64_address(node->exp.left);
        if(datatype_is_atomic(&lt)){
            return exp64_atomic_update(arith_op, &lt, node->exp.right, false);
        }
        ops->push(&lt);
        int target = vector_count(current_function->func.frame.elements) - 1;
        if(exp64_is_ldouble(&lt)){
            // x op= y on a long double x.
            struct datatype ld = lt;
            int left = exp64_push_index();
            struct datatype rt = exp64_value(node->exp.right);
            exp64_convert(0, &rt, &ld);
            int right = exp64_push_index();
            exp64_ld_operate(arith_op, left, right);
            ops->mov_r1();
            exp64_slot_load(target);
            ops->copy(EXP64_LD_SIZE);
            exp64_slot_load(target);
            return lt;
        }
        exp64_load(&lt, true);
        struct datatype old = lt;
        struct datatype rt;
        if(node->exp.right->type == NODE_TYPE_NUMBER && !(node->exp.right->num.flags & NUMBER_FLAG_LONG_DOUBLE)){
            rt = exp64_number_type(node->exp.right);
            ops->imm(1, exp64_number_value(node->exp.right));
        } else {
            ops->push(&old);
            int old_value = vector_count(current_function->func.frame.elements) - 1;
            rt = exp64_value(node->exp.right);
            if(exp64_is_ldouble(&rt)){
                // x op= y with y a long double: worked out in
                // long double, then converted to x's type.
                struct datatype ld = rt;
                int right = exp64_push_index();
                exp64_slot_load(old_value);
                exp64_convert(0, &old, &ld);
                int left = exp64_push_index();
                exp64_ld_operate(arith_op, left, right);
                exp64_convert(0, &ld, &lt);
                ops->mov_r1();
                exp64_slot_load(target);
                ops->swap();
                ops->store(exp64_scalar_size(&lt));
                return lt;
            }
            ops->mov_r1();
            ops->pop(0);
        }
        struct datatype result = exp64_arith(arith_op, &old, &rt);
        exp64_convert(0, &result, &lt);
        ops->pop(1);
    }
    exp64_store(&lt);
    return lt;
}

// A condition's value in R0, zero when it is false. A floating value is
// compared with 0.0, so -0.0 is false and a NaN true.
static struct datatype exp64_truth(struct node* node){
    struct datatype t = exp64_value(node);
    if(exp64_is_ldouble(&t)){
        ops->ld_truth();
        return exp64_int_type(DATA_SIZE_DWORD, true);
    }
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
    if(exp64_in_memory(&yt) || exp64_in_memory(&nt)){
        // the result goes to room made before the branches,
        // so both leave the expression stack the same. A struct too:
        // each branch's call result had its own room, and the stack
        // after the join counted both.
        bool is_struct = exp64_is_struct(&t);
        if(!is_struct){
            t = datatype_long_double();
        }
        int size = is_struct ? (int)exp64_size(&t) : EXP64_LD_SIZE;
        ops->reserve(align_value(size, DATA_SIZE_DDWORD));
        int room = vector_count(current_function->func.frame.elements) - 1;
        exp64_truth(node->exp.left);
        ops->branch(no_label, EXP64_BRANCH_IF_ZERO);
        for(int side = 0; side < 2; side++){
            struct datatype vt = exp64_value(side ? no : yes);
            if(!is_struct){
                exp64_convert(0, &vt, &t);
            }
            ops->mov_r1();
            ops->slot_address(vector_count(current_function->func.frame.elements) - 1 - room);
            ops->copy(size);
            int above = vector_count(current_function->func.frame.elements) - 1 - room;
            if(above){
                ops->drop(above);
            }
            if(!side){
                ops->branch(end_label, EXP64_BRANCH_ALWAYS);
                ops->label(no_label);
            }
        }
        ops->label(end_label);
        ops->slot_address(0);
        return t;
    }
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

// The va_list (pc_includes/stdarg.h) is five addresses, at these
// offsets.
#define EXP64_VA_GP       0
#define EXP64_VA_GP_END   8
#define EXP64_VA_OVERFLOW 16
#define EXP64_VA_FP       24
#define EXP64_VA_FP_END   32
#define EXP64_VA_FIELDS   5

// whether the va_list is the psABI's (x86_64).
static bool exp64_va_psabi(void){
    return S_EQ(current_process->target->backend->name, "x86_64");
}

// R0 = the address of the list's state plus `offset`: on x86_64 the
// va_list's value, elsewhere the va_list itself.
static void exp64_va_state(struct node* list, int offset){
    if(exp64_va_psabi()){
        exp64_value(list);
    } else {
        exp64_address(list);
    }
    ops->add_imm(offset);
}

// R0 = the va_list field at `field`. On x86_64 the fields are worked
// out of the psABI state: the register fields and their ends are
// addresses in the save area.
static void exp64_va_field(struct node* list, int field){
    if(!exp64_va_psabi()){
        exp64_va_state(list, field);
        ops->load(DATA_SIZE_DDWORD, false);
        return;
    }
    if(field == EXP64_VA_OVERFLOW){
        exp64_va_state(list, EXP64_PSABI_OVERFLOW);
        ops->load(DATA_SIZE_DDWORD, false);
        return;
    }
    struct datatype long_type = exp64_int_type(DATA_SIZE_DDWORD, true);
    exp64_va_state(list, EXP64_PSABI_SAVE_AREA);
    ops->load(DATA_SIZE_DDWORD, false);
    ops->push(&long_type);
    if(field == EXP64_VA_GP_END || field == EXP64_VA_FP_END){
        ops->imm(0, field == EXP64_VA_GP_END ? EXP64_PSABI_GP_END : EXP64_PSABI_FP_END);
    } else {
        exp64_va_state(list, field == EXP64_VA_GP ? EXP64_PSABI_GP_OFFSET : EXP64_PSABI_FP_OFFSET);
        ops->load(DATA_SIZE_DWORD, false);
    }
    ops->mov_r1();
    ops->pop(0);
    ops->binop("+", DATA_SIZE_DDWORD, false);
}

// The va_list field at `field` = R0 (on x86_64 a register field takes
// the offset of the address in the save area).
static void exp64_va_set(struct node* list, int field){
    struct datatype long_type = exp64_int_type(DATA_SIZE_DDWORD, true);
    ops->push(&long_type);
    if(!exp64_va_psabi() || field == EXP64_VA_OVERFLOW){
        exp64_va_state(list, exp64_va_psabi() ? EXP64_PSABI_OVERFLOW : field);
        ops->mov_r1();
        ops->pop(0);
        ops->store(DATA_SIZE_DDWORD);
        return;
    }
    exp64_va_state(list, EXP64_PSABI_SAVE_AREA);
    ops->load(DATA_SIZE_DDWORD, false);
    ops->mov_r1();
    ops->pop(0);
    ops->binop("-", DATA_SIZE_DDWORD, false);
    ops->push(&long_type);
    exp64_va_state(list, field == EXP64_VA_GP ? EXP64_PSABI_GP_OFFSET : EXP64_PSABI_FP_OFFSET);
    ops->mov_r1();
    ops->pop(0);
    ops->store(DATA_SIZE_DWORD);
}

// The frame bytes at `offset` = R0.
static void exp64_frame_store(int offset, int size){
    struct datatype long_type = exp64_int_type(DATA_SIZE_DDWORD, true);
    ops->push(&long_type);
    ops->frame_address(offset);
    ops->mov_r1();
    ops->pop(0);
    ops->store(size);
}

// the va_list `list` = the address of the function's next
// state slot (x86_64), whose offset is returned.
static int exp64_va_new_state(struct node* list){
    if(exp64_va_next_slot >= current_function->func.va_lists){
        compiler_error(current_process, "BUG: more va_start and va_copy than the parser counted\n");
    }
    int slot = exp64_va_slots + EXP64_PSABI_STATE * exp64_va_next_slot++;
    struct datatype long_type = exp64_int_type(DATA_SIZE_DDWORD, true);
    ops->frame_address(slot);
    ops->push(&long_type);
    exp64_address(list);
    ops->mov_r1();
    ops->pop(0);
    ops->store(DATA_SIZE_DDWORD);
    return slot;
}

// R0 = the va_list field at `field`, which then moves on `step` bytes.
static void exp64_va_take(struct node* list, int field, int step){
    exp64_va_field(list, field);
    ops->add_imm(step);
    exp64_va_set(list, field);
    exp64_va_field(list, field);
    ops->add_imm(-step);
}

// Jump to `label` when `field_a op field_b + step` holds, unsigned.
static void exp64_va_compare(struct node* list, int field_a, int step, const char* op, int field_b, int label){
    struct datatype long_type = exp64_int_type(DATA_SIZE_DDWORD, true);
    exp64_va_field(list, field_a);
    ops->add_imm(step);
    ops->push(&long_type);
    exp64_va_field(list, field_b);
    ops->mov_r1();
    ops->pop(0);
    ops->compare(op, false);
    ops->branch(label, EXP64_BRANCH_IF_NOT_ZERO);
}

// R0 = the address of the next `bytes` from `field` when `field +
// reg_bytes <= end` (stepping it `reg_bytes`: an xmm register's save
// slot is 16 bytes on x86_64), else from the stack (overflow), where
// `field` is first set to `end` with `close` (aarch64: once a value
// does not fit no register is used again).
static void exp64_va_take_or_overflow(struct node* list, int field, int bytes, int reg_bytes, int end, bool close){
    int stack_label = exp64_new_label();
    int end_label = exp64_new_label();
    exp64_va_compare(list, field, reg_bytes, ">", end, stack_label);
    exp64_va_take(list, field, reg_bytes);
    ops->branch(end_label, EXP64_BRANCH_ALWAYS);
    ops->label(stack_label);
    if(close){
        exp64_va_field(list, end);
        exp64_va_set(list, field);
    }
    exp64_va_take(list, EXP64_VA_OVERFLOW, bytes);
    ops->label(end_label);
}

// The va_list field at `field` rounded up to a multiple of `align`.
static void exp64_va_align(struct node* list, int field, int align){
    exp64_va_field(list, field);
    ops->add_imm(align - 1);
    ops->imm(1, -align);
    ops->binop("&", DATA_SIZE_DDWORD, false);
    exp64_va_set(list, field);
}

// R0 = the address of a long double passed to `...`: on
// x86_64 in a 16-byte aligned stack slot, on riscv64 in an even pair of
// integer registers or a 16-byte aligned stack slot (gp runs on into
// the stack arguments), on aarch64 in the next q register, whose home
// (exp64_va_qhome) is two of fp's 8-byte steps per register, else in a
// 16-byte aligned stack slot (and no v register is used after it).
// on aarch64 `count` long doubles in a row, a homogeneous
// aggregate of one to four of them: in that many q registers when they
// are left, whose homes hold it as it is in memory, else on the stack.
static void exp64_va_ldouble(struct node* list, bool x86_64, bool aarch64, int count){
    if(!aarch64){
        int field = x86_64 ? EXP64_VA_OVERFLOW : EXP64_VA_GP;
        exp64_va_align(list, field, EXP64_LD_SIZE);
        exp64_va_take(list, field, EXP64_LD_SIZE);
        return;
    }
    if(!exp64_va_qhome){
        compiler_error(current_process, "BUG: va_arg of a long double outside a variadic function\n");
    }
    int stack_label = exp64_new_label();
    int end_label = exp64_new_label();
    int mark = vector_count(current_function->func.frame.elements);
    exp64_va_compare(list, EXP64_VA_FP, DATA_SIZE_DDWORD * count, ">", EXP64_VA_FP_END, stack_label);
    exp64_va_take(list, EXP64_VA_FP, DATA_SIZE_DDWORD * count);
    int fp = exp64_push_index();
    ops->frame_address(exp64_va_fp_end - DATA_SIZE_DDWORD * EXP64_FP_ARG_REGISTERS);
    ops->mov_r1();
    exp64_slot_load(fp);
    ops->binop("-", DATA_SIZE_DDWORD, false);
    ops->mul_imm(0, 2);
    int offset = exp64_push_index();
    ops->frame_address(exp64_va_qhome);
    ops->mov_r1();
    exp64_slot_load(offset);
    ops->binop("+", DATA_SIZE_DDWORD, false);
    ops->drop(vector_count(current_function->func.frame.elements) - mark);
    ops->branch(end_label, EXP64_BRANCH_ALWAYS);
    ops->label(stack_label);
    exp64_va_field(list, EXP64_VA_FP_END);
    exp64_va_set(list, EXP64_VA_FP);
    exp64_va_align(list, EXP64_VA_OVERFLOW, EXP64_LD_SIZE);
    exp64_va_take(list, EXP64_VA_OVERFLOW, EXP64_LD_SIZE * count);
    ops->label(end_label);
}

// R0 = the address of a struct that a caller on x86_64 or
// aarch64 passed to `...` in pieces (exp64_struct_pieces): variadic
// arguments go where named ones would. When the registers of all its
// pieces were left, the pieces are taken from the fp and gp homes in
// turn: in place when they are all eightbytes in floating-point
// registers (the home slots then hold the struct as it is in memory),
// else put together in the frame's scratch room. Otherwise the struct
// is on the stack, and on aarch64 no floating-point register is used
// after it (NSRN = 8).
static void exp64_va_pieces(struct node* list, struct exp64_piece* pieces, int count, int size, bool aarch64){
    struct datatype long_type = exp64_int_type(DATA_SIZE_DDWORD, true);
    int ints = 0;
    // on x86_64 the xmm registers' save slots are 16 bytes
    // apart, so two pieces are never in place.
    int fp_step = exp64_va_psabi() ? EXP64_PSABI_FP_STEP : DATA_SIZE_DDWORD;
    bool in_place = !exp64_va_psabi() || count == 1;
    for(int i = 0; i < count; i++){
        ints += !pieces[i].fp;
        if(!pieces[i].fp || (aarch64 && pieces[i].size != DATA_SIZE_DDWORD)){
            in_place = false;
        }
    }
    int fps = count - ints;
    int stack_label = exp64_new_label();
    int end_label = exp64_new_label();
    exp64_va_compare(list, EXP64_VA_FP, fp_step * fps, ">", EXP64_VA_FP_END, stack_label);
    if(ints){
        exp64_va_compare(list, EXP64_VA_GP, DATA_SIZE_DDWORD * ints, ">", EXP64_VA_GP_END, stack_label);
    }
    if(in_place){
        exp64_va_take(list, EXP64_VA_FP, fp_step * fps);
    } else {
        for(int i = 0; i < count; i++){
            int field = pieces[i].fp ? EXP64_VA_FP : EXP64_VA_GP;
            exp64_va_take(list, field, pieces[i].fp ? fp_step : DATA_SIZE_DDWORD);
            ops->load(pieces[i].size, false);
            ops->push(&long_type);
            ops->frame_address(exp64_va_scratch + pieces[i].offset);
            ops->mov_r1();
            ops->pop(0);
            ops->store(pieces[i].size);
        }
        ops->frame_address(exp64_va_scratch);
    }
    ops->branch(end_label, EXP64_BRANCH_ALWAYS);
    ops->label(stack_label);
    if(aarch64){
        exp64_va_field(list, EXP64_VA_FP_END);
        exp64_va_set(list, EXP64_VA_FP);
    }
    exp64_va_take(list, EXP64_VA_OVERFLOW, align_value(size, DATA_SIZE_DDWORD));
    ops->label(end_label);
}

static bool exp64_struct_x87(struct datatype* t);

// va_start(list, last): the five addresses of the first variadic
// argument, from the layout of the named parameters (exp64_parameters),
// so a named struct that went to the stack while later named scalars
// took registers is counted where it went.
// __builtin_va_arg(list, size, (type*)0): the address of the next
// argument, and the list stepped past it; va_arg reads the type from
// there. Where it is, by each psABI's rule for variadic arguments (the
// caller placed them with exp64_arg_layout):
//   x86_64: up to 16 bytes from the registers when all its words fit
//     in the ones left, else from the stack; later arguments still
//     take registers. A bigger struct is on the stack. A double from
//     the next xmm register's home slot, else from the stack.
//   aarch64: the same, but once a value does not fit no register is
//     used again (NGRN = 8); a bigger struct is passed by reference.
//     A double from the next v register's home slot (NSRN), else from
//     the stack.
//   riscv64: every word in order, a double too, the register slots
//     running on into the stack arguments; a bigger struct by
//     reference.
// The prologue of a variadic function spills all the floating-point
// argument registers on x86_64 and aarch64 (exp64_fp_parameter_registers).
static struct datatype exp64_stdarg(const char* name, struct vector* args){
    struct datatype long_type = exp64_int_type(DATA_SIZE_DDWORD, true);
    if(S_EQ(name, "va_end")){
        ops->imm(0, 0);
        return exp64_int_type(DATA_SIZE_DWORD, true);
    }
    if(S_EQ(name, "__builtin_va_copy")){
        // On x86_64 dest gets a state of its own, a copy of src's.
        if(vector_count(args) != 2 || !exp64_va_psabi()){
            compiler_error(current_process, "__builtin_va_copy expects two va_lists (x86_64)\n");
        }
        struct node* source = *(struct node**)vector_at(args, 1);
        int slot = exp64_va_new_state(*(struct node**)vector_at(args, 0));
        for(int k = 0; k < EXP64_PSABI_STATE; k += DATA_SIZE_DDWORD){
            exp64_va_state(source, k);
            ops->load(DATA_SIZE_DDWORD, false);
            exp64_frame_store(slot + k, DATA_SIZE_DDWORD);
        }
        ops->imm(0, 0);
        return exp64_int_type(DATA_SIZE_DWORD, true);
    }
    // The va_arg macro passes `(type*)0` as a third argument, which
    // gives the type: a double is read where floating-point arguments
    // went. A float passed to `...` was promoted to double, so there is
    // no float to read (gcc makes it a trap).
    bool floating = false;
    bool ldouble = false;
    bool aligned16 = false;
    bool x87_struct = false;
    struct exp64_piece pieces[EXP64_MAX_PIECES];
    int piece_count = 0;
    int struct_size = 0;
    if(vector_count(args) == 3 && !S_EQ(name, "va_start")){
        struct node* type_argument = *(struct node**)vector_at(args, 2);
        if(type_argument->type == NODE_TYPE_CAST){
            struct datatype t = *datatype_pointer_reduce(&type_argument->cast.dtype, 1);
            piece_count = exp64_struct_pieces(&t, pieces);
            struct_size = piece_count ? (int)exp64_size(&t) : 0;
            ldouble = exp64_is_ldouble(&t);
            aligned16 = exp64_is_struct(&t) && datatype_alignment(&t) > DATA_SIZE_DDWORD;
            x87_struct = exp64_struct_x87(&t);
            if(exp64_is_floating(&t)){
                if(t.size != DATA_SIZE_DDWORD){
                    compiler_error(current_process, "va_arg of a float: a float passed to `...` is promoted to double, read a double\n");
                }
                floating = true;
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
        if(second->type != NODE_TYPE_IDENTIFIER || !resolver_get_variable(0, current_process->resolver, second->sval)){
            compiler_error(current_process, "va_start expects the last named parameter\n");
        }
        if(exp64_va_psabi()){
            if(!(current_function->func.flags & FUNCTION_NODE_FLAG_IS_VARIADIC)){
                compiler_error(current_process, "va_start in a function without `...`\n");
            }
            int slot = exp64_va_new_state(list);
            ops->imm(0, exp64_va_gp_offset);
            exp64_frame_store(slot + EXP64_PSABI_GP_OFFSET, DATA_SIZE_DWORD);
            ops->imm(0, exp64_va_fp_offset);
            exp64_frame_store(slot + EXP64_PSABI_FP_OFFSET, DATA_SIZE_DWORD);
            ops->frame_address(exp64_va_overflow);
            exp64_frame_store(slot + EXP64_PSABI_OVERFLOW, DATA_SIZE_DDWORD);
            ops->frame_address(exp64_va_save);
            exp64_frame_store(slot + EXP64_PSABI_SAVE_AREA, DATA_SIZE_DDWORD);
            return exp64_int_type(DATA_SIZE_DWORD, true);
        }
        int fields[]  = { EXP64_VA_GP, EXP64_VA_GP_END, EXP64_VA_OVERFLOW, EXP64_VA_FP, EXP64_VA_FP_END };
        int offsets[] = { exp64_va_gp, exp64_va_gp_end, exp64_va_overflow, exp64_va_fp, exp64_va_fp_end };
        for(int i = 0; i < EXP64_VA_FIELDS; i++){
            ops->frame_address(offsets[i]);
            exp64_va_set(list, fields[i]);
        }
        return exp64_int_type(DATA_SIZE_DWORD, true);
    }
    if(second->type != NODE_TYPE_NUMBER){
        compiler_error(current_process, "__builtin_va_arg expects a constant size\n");
    }
    const char* abi = current_process->target->backend->name;
    bool x86_64  = S_EQ(abi, "x86_64");
    bool aarch64 = S_EQ(abi, "aarch64");
    bool big = second->llnum > 2 * DATA_SIZE_DDWORD;
    int bytes = align_value(second->llnum, DATA_SIZE_DDWORD);
    struct datatype result = exp64_pointer_to(&long_type);
    if(floating && (x86_64 || aarch64)){
        exp64_va_take_or_overflow(list, EXP64_VA_FP, DATA_SIZE_DDWORD, x86_64 ? EXP64_PSABI_FP_STEP : DATA_SIZE_DDWORD,
                                  EXP64_VA_FP_END, false);
        return result;
    }
    if(ldouble){
        exp64_va_ldouble(list, x86_64, aarch64, 1);
        return result;
    }
    if(piece_count && aarch64 && pieces[0].size == EXP64_LD_SIZE){
        exp64_va_ldouble(list, false, true, piece_count);
        return result;
    }
    if(piece_count && (x86_64 || aarch64)){
        exp64_va_pieces(list, pieces, piece_count, struct_size, aarch64);
        return result;
    }
    // a struct with a long double member is of class MEMORY on
    // x86_64 whatever its size. A struct aligned to 16 (a long double
    // member) is 16-byte aligned on the stack there, and on riscv64 one
    // of up to 16 bytes takes an even register pair or an aligned slot.
    if(x86_64 && (big || x87_struct)){
        if(aligned16){
            exp64_va_align(list, EXP64_VA_OVERFLOW, EXP64_LD_SIZE);
        }
        exp64_va_take(list, EXP64_VA_OVERFLOW, bytes);
        return result;
    }
    if(big){
        bytes = DATA_SIZE_DDWORD;
    }
    if(x86_64 || aarch64){
        exp64_va_take_or_overflow(list, EXP64_VA_GP, bytes, bytes, EXP64_VA_GP_END, aarch64);
    } else {
        if(aligned16 && !big){
            exp64_va_align(list, EXP64_VA_GP, EXP64_LD_SIZE);
        }
        exp64_va_take(list, EXP64_VA_GP, bytes);
    }
    if(big){
        ops->load(DATA_SIZE_DDWORD, false);
    }
    return result;
}

// the stack-pointer builtins behind variable-length arrays,
// each in a statement of its own (the parser's lowering), so nothing
// of the expression stack is under the stack pointer when it moves.
//   __builtin_vla_save():        the stack pointer
//   __builtin_vla_alloc(bytes):  the stack pointer moved down by bytes
//                                rounded up to 16, which is the result
//   __builtin_vla_restore(sp):   the stack pointer set back to sp
static struct datatype exp64_vla(const char* name, struct vector* args){
    struct datatype ulong = exp64_int_type(DATA_SIZE_DDWORD, false);
    if(S_EQ(name, "__builtin_vla_save")){
        ops->sp_get();
        return ulong;
    }
    if(vector_count(args) != 1){
        compiler_error(current_process, "%s expects one argument\n", name);
    }
    struct datatype t = exp64_value(*(struct node**)vector_at(args, 0));
    exp64_convert(0, &t, &ulong);
    if(S_EQ(name, "__builtin_vla_restore")){
        ops->sp_set();
        return exp64_int_type(DATA_SIZE_DWORD, true);
    }
    ops->add_imm(15);
    ops->imm(1, -16);
    ops->binop("&", DATA_SIZE_DDWORD, false);
    ops->mov_r1();
    ops->sp_get();
    ops->binop("-", DATA_SIZE_DDWORD, false);
    ops->sp_set();
    struct datatype byte = exp64_int_type(DATA_SIZE_BYTE, false);
    return exp64_pointer_to(&byte);
}

// ---------------------------------------------------------------
// structs with float or double members
// ---------------------------------------------------------------

// A struct's scalar members flattened, nested structs and arrays
// opened up, which is what each psABI classifies.
struct exp64_flat {
    int  count;                                 // scalars seen
    struct exp64_piece leaf[EXP64_MAX_PIECES];  // the first of them
    bool int_in[2];                             // an integer scalar in eightbyte 0, 1
    bool fp_in[2];                              // a float or double in eightbyte 0, 1
    bool other;                                 // something no register class takes
    bool has_union;                             // a union inside
    bool has_ldouble;                           // a long double inside
};

static struct node* exp64_struct_body(struct datatype* t){
    struct node* s = t->struct_node;
    if(!s || (s->type == NODE_TYPE_STRUCT ? !s->_struct.body_n : !s->_union.body_n)){
        s = t->type == DATA_TYPE_UNION ? union_node_for_name(current_process, t->type_str)
                                       : struct_node_for_name(current_process, t->type_str);
    }
    if(!s){
        return 0;
    }
    return s->type == NODE_TYPE_STRUCT ? s->_struct.body_n : s->_union.body_n;
}

static void exp64_flatten(struct datatype* t, int offset, struct exp64_flat* flat);

static void exp64_flatten_member(struct node* member, bool is_union, int offset, struct exp64_flat* flat){
    if(member->type != NODE_TYPE_VARIABLE || (member->var.bitfield.is_bitfield && member->var.bitfield.unnamed)){
        return;
    }
    exp64_flatten(&member->var.type, offset + (is_union ? 0 : member->var.aoffset), flat);
}

static void exp64_flatten(struct datatype* t, int offset, struct exp64_flat* flat){
    if(exp64_is_array(t)){
        struct datatype e = exp64_array_element(t);
        size_t step = exp64_size(&e);
        size_t total = exp64_size(t);
        // Every element of the first 16 bytes counts for the x86_64
        // eightbytes; past them only whether there are more scalars.
        for(size_t o = 0; step && o < total && (offset + o < 16 || flat->count <= EXP64_MAX_PIECES); o += step){
            exp64_flatten(&e, offset + (int)o, flat);
        }
        return;
    }
    if(exp64_is_struct(t)){
        struct node* body = exp64_struct_body(t);
        if(!body){
            flat->other = true;
            return;
        }
        bool is_union = t->type == DATA_TYPE_UNION;
        flat->has_union |= is_union;
        struct vector* statements = body->body.statements;
        for(int i = 0; i < vector_count(statements); i++){
            struct node* member = *(struct node**)vector_at(statements, i);
            if(member->type == NODE_TYPE_VARIABLE_LIST){
                struct vector* list = member->var_list.list;
                for(int j = 0; j < vector_count(list); j++){
                    exp64_flatten_member(*(struct node**)vector_at(list, j), is_union, offset, flat);
                }
            } else {
                exp64_flatten_member(member, is_union, offset, flat);
            }
        }
        return;
    }
    bool fp = exp64_is_floating(t) || exp64_is_ldouble(t);
    int size = exp64_is_ldouble(t) ? EXP64_LD_SIZE : exp64_scalar_size(t);
    flat->has_ldouble |= exp64_is_ldouble(t);
    if(offset / DATA_SIZE_DDWORD < 2){
        if(fp){
            flat->fp_in[offset / DATA_SIZE_DDWORD] = true;
        } else {
            flat->int_in[offset / DATA_SIZE_DDWORD] = true;
        }
    }
    if(flat->count < EXP64_MAX_PIECES){
        struct exp64_piece* leaf = &flat->leaf[flat->count];
        leaf->fp        = fp;
        leaf->is_signed = exp64_is_signed(t);
        leaf->offset    = offset;
        leaf->size      = size;
        leaf->reg       = -1;
    }
    flat->count++;
}

// The register pieces of a struct with float or double members, by the
// psABI's classification; 0 for any other struct, which keeps the
// integer rules (registers by the word, the stack, memory):
//   x86_64: a struct of up to 16 bytes is one or two eightbytes; one
//     that holds only floats and doubles is of class SSE and goes in an
//     xmm register, any other INTEGER, in a general one.
//   AAPCS64: a homogeneous floating-point aggregate, one to four
//     members all float or all double, goes in as many v registers
//     (s or d), whatever its size.
//   RISC-V LP64D: a struct of one float or double, of two, or of one
//     and one integer member of up to 8 bytes goes in fa registers (the
//     integer in an a register). A union is not flattened.
// The pieces' registers are left at -1 for the caller to number.
static int exp64_struct_pieces(struct datatype* t, struct exp64_piece* out){
    if(!exp64_is_struct(t)){
        return 0;
    }
    struct exp64_flat flat = {0};
    exp64_flatten(t, 0, &flat);
    if(flat.other || flat.count == 0){
        return 0;
    }
    const char* abi = current_process->target->backend->name;
    int size = (int)exp64_size(t);
    // A long double member: class X87, so MEMORY, on x86_64
    // (exp64_struct_x87); more than FLEN bits on riscv64, so the integer
    // rules; an aggregate of q registers on aarch64.
    if(flat.has_ldouble && !S_EQ(abi, "aarch64")){
        return 0;
    }
    if(S_EQ(abi, "x86_64")){
        bool sse = (flat.fp_in[0] && !flat.int_in[0]) || (flat.fp_in[1] && !flat.int_in[1]);
        if(size > 2 * DATA_SIZE_DDWORD || !sse){
            return 0;
        }
        int count = 0;
        for(int e = 0; e * DATA_SIZE_DDWORD < size; e++){
            struct exp64_piece* p = &out[count++];
            p->fp        = !flat.int_in[e];
            p->is_signed = false;
            p->offset    = e * DATA_SIZE_DDWORD;
            p->size      = size - p->offset < DATA_SIZE_DDWORD ? size - p->offset : DATA_SIZE_DDWORD;
            p->reg       = -1;
        }
        return count;
    }
    bool aarch64 = S_EQ(abi, "aarch64");
    // RISC-V flattens structs only; a union in an AAPCS64 aggregate
    // counts as its biggest member, which the flat list does not tell,
    // so such structs keep the integer rules here.
    if(flat.has_union || flat.count > (aarch64 ? EXP64_MAX_PIECES : 2)){
        return 0;
    }
    int fps = 0;
    for(int i = 0; i < flat.count; i++){
        fps += flat.leaf[i].fp;
        if(aarch64 && flat.leaf[i].size != flat.leaf[0].size){
            return 0;
        }
    }
    if(aarch64 ? fps != flat.count : fps == 0){
        return 0;
    }
    memcpy(out, flat.leaf, sizeof(struct exp64_piece) * flat.count);
    return flat.count;
}

// Numbers the pieces' registers from *next (integer) and *next_fp,
// when enough of both are left (max of each); returns whether they
// were.
static bool exp64_assign_pieces(struct exp64_piece* pieces, int count, int* next, int max, int* next_fp){
    int ints = 0, fps = 0;
    for(int i = 0; i < count; i++){
        if(pieces[i].fp){
            fps++;
        } else {
            ints++;
        }
    }
    if(*next + ints > max || *next_fp + fps > EXP64_FP_ARG_REGISTERS){
        return false;
    }
    for(int i = 0; i < count; i++){
        pieces[i].reg = pieces[i].fp ? (*next_fp)++ : (*next)++;
    }
    return true;
}

// a struct with a long double member is of class MEMORY on
// x86_64, whatever its size: on the stack, and returned through memory.
static bool exp64_struct_x87(struct datatype* t){
    if(!exp64_is_struct(t) || !S_EQ(current_process->target->backend->name, "x86_64")){
        return false;
    }
    struct exp64_flat flat = {0};
    exp64_flatten(t, 0, &flat);
    return flat.has_ldouble;
}

// a result of class X87 on x86_64, which comes back in st0: a
// long double, and a struct whose one member is a long double (other
// structs with one are of class MEMORY).
bool exp64_returns_x87(struct datatype* dtype){
    if(exp64_is_ldouble(dtype)){
        return true;
    }
    if(!exp64_struct_x87(dtype) || exp64_size(dtype) != EXP64_LD_SIZE){
        return false;
    }
    struct exp64_flat flat = {0};
    exp64_flatten(dtype, 0, &flat);
    return flat.count == 1;
}

// a struct result in registers by pieces: x86_64 xmm0, xmm1
// and rax, rdx in turn; AAPCS64 s0-s3 or d0-d3; riscv64 fa0, fa1 and
// a0. The pieces' reg is the return register's number in its class.
int exp64_return_pieces(struct datatype* dtype, struct exp64_piece* out){
    int count = exp64_struct_pieces(dtype, out);
    int next = 0, next_fp = 0;
    exp64_assign_pieces(out, count, &next, 2, &next_fp);
    return count;
}

// argument i of an atomic builtin.
static struct node* exp64_atomic_argument(struct vector* args, int i){
    return *(struct node**)vector_at(args, i);
}

// the order memory order argument i asks for, made valid for
// the access (atomic_order). One that is not a constant is evaluated
// for its side effects and gives seq_cst.
static int exp64_atomic_order(struct vector* args, int i, int access){
    struct node* order = exp64_atomic_argument(args, i);
    bool constant;
    int o = atomic_order(current_process, order, access, &constant);
    if(!constant){
        exp64_value(order);
    }
    return o;
}

// R0 = the value of argument i as an object of type t.
static void exp64_atomic_operand(struct vector* args, int i, struct datatype* t){
    struct datatype vt = exp64_value(exp64_atomic_argument(args, i));
    exp64_convert_for_store(&vt, t);
}

// R0 = the value of argument i, which must be a pointer.
static void exp64_atomic_pointer(struct vector* args, int i){
    struct datatype pt = exp64_value(exp64_atomic_argument(args, i));
    if(!exp64_is_pointer(&pt)){
        compiler_error(current_process, "Argument %i of the atomic builtin must be a pointer\n", i + 1);
    }
}

// R0 = the value of the object of type t argument i points to (the
// generic forms pass their values by address).
static void exp64_atomic_pointed(struct vector* args, int i, struct datatype* t){
    exp64_atomic_pointer(args, i);
    ops->load(exp64_scalar_size(t), exp64_is_signed(t));
}

static int exp64_arg_layout(struct datatype* types, int count, int named, bool hidden, struct exp64_arg* out,
                            int* next_reg, int* next_fp_out);
static int exp64_first_reg(bool hidden);
static bool exp64_call_is_simple(struct exp64_call* call);

// The ledger index of the top of the expression stack.
static int exp64_top(void){
    return vector_count(current_function->func.frame.elements) - 1;
}

// a call of the libatomic routine `symbol` with `count` word
// arguments on the expression stack, the first on top, which it
// consumes. R0 = its result.
static void exp64_libatomic_call(const char* symbol, int count){
    struct datatype types[6];
    struct exp64_call call = { .symbol = symbol, .count = count };
    for(int i = 0; i < count; i++){
        types[i] = exp64_int_type(DATA_SIZE_DDWORD, false);
    }
    call.args = calloc(count + 1, sizeof(struct exp64_arg));
    call.first_reg = exp64_first_reg(false);
    call.stack_words = exp64_arg_layout(types, count, count, false, call.args, 0, 0);
    call.slots = count;
    call.area = DATA_SIZE_DDWORD * call.stack_words;
    for(int i = 0; i < count; i++){
        call.args[i].slot = i;
    }
    call.simple = exp64_call_is_simple(&call);
    if(S_EQ(current_process->target->backend->name, "x86_64")){
        // NASM wants every outside symbol declared; it takes the
        // declaration again at each call.
        char text[64];
        snprintf(text, sizeof(text), "extern $%s", symbol);
        ops->asm_text(text);
    }
    ops->call(&call);
    free(call.args);
}

// Push the word in the slot at ledger index `index`, or a constant.
static void exp64_push_slot(int index){
    exp64_slot_load(index);
    exp64_push_index();
}

static void exp64_push_constant(long long value){
    ops->imm(0, value);
    exp64_push_index();
}

// the generic operations (ATOMIC_BUILTIN_GENERIC_*) on the
// object of type t whose address is in the slot at ledger index a; v,
// e and r are the slots of the addresses of the value to store, the
// expected value and the result's room. An object of 1, 2, 4 or 8
// bytes is worked on as an integer of its size with the backend's
// instructions; any other size (a long double, a struct or union of
// 3 or 16 bytes) goes to libatomic's __atomic_load, _store, _exchange
// and _compare_exchange, which take the size first and lock where the
// target has no instruction, as gcc's code does. A compare-exchange
// has the orders success and failure and leaves R0 = 0 or 1.
static void exp64_atomic_generic(int kind, struct datatype* t, int order, int failure, int a, int v, int e, int r){
    int size = (int)exp64_size(t);
    if(atomic_size_is_lock_free(size)){
        switch(kind){
            case ATOMIC_BUILTIN_GENERIC_LOAD:
                exp64_slot_load(a);
                ops->atomic_load(size, false, order);
                ops->mov_r1();
                exp64_slot_load(r);
                ops->swap();
                ops->store(size);
                return;
            case ATOMIC_BUILTIN_GENERIC_STORE:
            case ATOMIC_BUILTIN_GENERIC_EXCHANGE:
                exp64_slot_load(v);
                ops->load(size, false);
                ops->mov_r1();
                exp64_slot_load(a);
                ops->swap();                                // R0 = the bits, R1 = A
                if(kind == ATOMIC_BUILTIN_GENERIC_STORE){
                    ops->atomic_store(size, order);
                    return;
                }
                ops->atomic_rmw("=", size, order);
                ops->mov_r1();
                exp64_slot_load(r);
                ops->swap();
                ops->store(size);
                return;
            case ATOMIC_BUILTIN_GENERIC_CAS: {
                exp64_push_slot(a);                         // [A]
                exp64_slot_load(e);
                ops->load(size, false);
                exp64_push_index();                         // [A, expected]
                exp64_slot_load(v);
                ops->load(size, false);
                ops->pop(1);                                // R0 = desired, R1 = expected, [A]
                ops->atomic_cas(size, 0, atomic_order_cas(order, failure));
                if(size < DATA_SIZE_DDWORD){
                    ops->extend(0, size, false);
                }
                int seen = exp64_push_index();              // [A, seen]
                exp64_slot_load(e);
                ops->load(size, false);
                ops->mov_r1();
                exp64_slot_load(seen);
                ops->compare("==", false);
                int done = exp64_new_label();
                ops->branch(done, EXP64_BRANCH_IF_NOT_ZERO);
                // Failed: *E = what was there, the result 0.
                exp64_slot_load(e);
                ops->mov_r1();
                exp64_slot_load(seen);
                ops->store(size);
                ops->imm(0, 0);
                ops->label(done);
                ops->drop(2);
                return;
            }
        }
    }
    const char* symbol = "__atomic_load";
    int count = 4;
    switch(kind){
        case ATOMIC_BUILTIN_GENERIC_LOAD:
            exp64_push_constant(order);
            exp64_push_slot(r);
            break;
        case ATOMIC_BUILTIN_GENERIC_STORE:
            symbol = "__atomic_store";
            exp64_push_constant(order);
            exp64_push_slot(v);
            break;
        case ATOMIC_BUILTIN_GENERIC_EXCHANGE:
            symbol = "__atomic_exchange";
            count = 5;
            exp64_push_constant(order);
            exp64_push_slot(r);
            exp64_push_slot(v);
            break;
        case ATOMIC_BUILTIN_GENERIC_CAS:
            symbol = "__atomic_compare_exchange";
            count = 6;
            exp64_push_constant(failure);
            exp64_push_constant(order);
            exp64_push_slot(v);
            exp64_push_slot(e);
            break;
    }
    exp64_push_slot(a);
    exp64_push_constant(size);
    exp64_libatomic_call(symbol, count);
    if(kind == ATOMIC_BUILTIN_GENERIC_CAS){
        ops->extend(0, DATA_SIZE_BYTE, false);
    }
}

// R0 = the address of a copy, in new room, of the _Atomic
// struct, union or long double of type t at R0, read with `order`.
static void exp64_atomic_memory_load(struct datatype* t, int order){
    int a = exp64_push_index();
    ops->reserve(align_value((int)exp64_size(t), DATA_SIZE_DDWORD));
    ops->slot_address(0);
    int r = exp64_push_index();
    exp64_atomic_generic(ATOMIC_BUILTIN_GENERIC_LOAD, t, order, order, a, 0, 0, r);
    ops->drop(1);
    ops->slot_address(0);
}

// `a = v` on the _Atomic struct, union or long double a at
// R0, with v (of type rt) in R1: one sequentially consistent atomic
// store. The value of the assignment is v's: R0 = its address.
static struct datatype exp64_atomic_memory_assign(struct datatype* lt, struct datatype* rt){
    struct datatype t = exp64_unqualified(lt);
    int a = exp64_push_index();
    ops->swap();
    if(exp64_is_ldouble(&t) && !exp64_is_ldouble(rt)){
        exp64_convert(0, rt, &t);
    }
    int v = exp64_push_index();
    exp64_atomic_generic(ATOMIC_BUILTIN_GENERIC_STORE, &t, ATOMIC_ORDER_SEQ_CST, ATOMIC_ORDER_SEQ_CST, a, v, 0, 0);
    exp64_slot_load(v);
    return t;
}

// `a op= v`, `++a`, `a++` on the _Atomic long double a at R0:
// libatomic's load, then a compare-exchange loop that works out
// `old op v` and stores it if the object still holds old; a failed
// compare-exchange leaves what it found in old for the next try.
static struct datatype exp64_atomic_ld_update(const char* op, struct datatype* lt, struct node* right, bool postfix){
    struct datatype t = exp64_unqualified(lt);
    int a = exp64_push_index();                             // [A]
    struct datatype rt = exp64_int_type(DATA_SIZE_DWORD, true);
    if(right){
        rt = exp64_value(right);
    } else {
        ops->imm(0, 1);
    }
    exp64_convert(0, &rt, &t);
    int v = exp64_push_index();                             // [A, (room), V]
    ops->reserve(EXP64_LD_SIZE);
    ops->slot_address(0);
    int old = exp64_push_index();                           // [A, V, room, O]
    exp64_atomic_generic(ATOMIC_BUILTIN_GENERIC_LOAD, &t, ATOMIC_ORDER_SEQ_CST, ATOMIC_ORDER_SEQ_CST, a, 0, 0, old);
    int mark = vector_count(current_function->func.frame.elements);
    int retry = exp64_new_label();
    ops->label(retry);
    exp64_ld_operate(op, old, v);
    int next = exp64_push_index();
    exp64_atomic_generic(ATOMIC_BUILTIN_GENERIC_CAS, &t, ATOMIC_ORDER_SEQ_CST, ATOMIC_ORDER_SEQ_CST, a, next, old, 0);
    ops->drop(vector_count(current_function->func.frame.elements) - mark);
    ops->branch(retry, EXP64_BRANCH_IF_ZERO);
    if(postfix){
        exp64_slot_load(old);
        return t;
    }
    return exp64_ld_operate(op, old, v);
}

// R0 = the address of the value argument i gives an object of
// type t, a struct or union or a long double (converted to one).
static void exp64_atomic_memory_value(struct vector* args, int i, struct datatype* t){
    struct datatype vt = exp64_value(exp64_atomic_argument(args, i));
    if(exp64_is_ldouble(t)){
        exp64_convert(0, &vt, t);
    } else if(!exp64_is_struct(&vt) || exp64_size(&vt) != exp64_size(t)){
        compiler_error(current_process, "Argument %i of the atomic builtin must be a value of the object's type\n", i + 1);
    }
}

// the builtins on an _Atomic struct, union or long double
// object of type t, whose address is in R0: the generic forms, and
// the _n forms <stdatomic.h> uses (which gcc only has for scalars),
// with the value and the result by value. The order is that of the
// builtin, failure the failure order of a compare-exchange.
static struct datatype exp64_atomic_memory_builtin(const struct atomic_builtin* b, struct vector* args, struct datatype* t,
                                                   int order, int failure){
    int kind = b->kind;
    int a = exp64_push_index();                             // [A]
    int v = 0, e = 0, r = 0;
    switch(kind){
        case ATOMIC_BUILTIN_LOAD:
            exp64_slot_load(a);
            exp64_atomic_memory_load(t, order);
            return *t;
        case ATOMIC_BUILTIN_GENERIC_LOAD:
            exp64_atomic_pointer(args, 1);
            r = exp64_push_index();
            exp64_atomic_generic(kind, t, order, order, a, 0, 0, r);
            ops->drop(exp64_top() - a + 1);
            return exp64_void_type();
        case ATOMIC_BUILTIN_STORE:
        case ATOMIC_BUILTIN_GENERIC_STORE:
            if(kind == ATOMIC_BUILTIN_STORE){
                exp64_atomic_memory_value(args, 1, t);
            } else {
                exp64_atomic_pointer(args, 1);
            }
            v = exp64_push_index();
            exp64_atomic_generic(ATOMIC_BUILTIN_GENERIC_STORE, t, order, order, a, v, 0, 0);
            ops->drop(exp64_top() - a + 1);
            return exp64_void_type();
        case ATOMIC_BUILTIN_EXCHANGE:
            if(!strncmp(b->name, "__sync_", 7)){
                break;
            }
            exp64_atomic_memory_value(args, 1, t);
            v = exp64_push_index();
            ops->reserve(align_value((int)exp64_size(t), DATA_SIZE_DDWORD));
            ops->slot_address(0);
            r = exp64_push_index();
            exp64_atomic_generic(ATOMIC_BUILTIN_GENERIC_EXCHANGE, t, order, order, a, v, 0, r);
            ops->drop(1);
            ops->slot_address(0);
            return *t;
        case ATOMIC_BUILTIN_GENERIC_EXCHANGE:
            exp64_atomic_pointer(args, 1);
            v = exp64_push_index();
            exp64_atomic_pointer(args, 2);
            r = exp64_push_index();
            exp64_atomic_generic(kind, t, order, order, a, v, 0, r);
            ops->drop(exp64_top() - a + 1);
            return exp64_void_type();
        case ATOMIC_BUILTIN_CAS:
        case ATOMIC_BUILTIN_GENERIC_CAS:
            exp64_atomic_pointer(args, 1);
            e = exp64_push_index();
            if(kind == ATOMIC_BUILTIN_CAS){
                exp64_atomic_memory_value(args, 2, t);
            } else {
                exp64_atomic_pointer(args, 2);
            }
            v = exp64_push_index();
            exp64_atomic_generic(ATOMIC_BUILTIN_GENERIC_CAS, t, order, failure, a, v, e, 0);
            ops->drop(exp64_top() - a + 1);
            return exp64_int_type(DATA_SIZE_DWORD, true);
    }
    compiler_error(current_process, "%s needs an integer or pointer object\n", b->name);
    return *t;
}

// __atomic_always_lock_free(size, p) and
// __atomic_is_lock_free(size, p) are 1 for a constant size of 1, 2, 4
// or 8 bytes (and a p of 0 or one aligned to it), as gcc says on each
// of these targets. Otherwise the first is 0 (its size must be a
// constant, as in gcc) and the second asks libatomic at run time, as
// gcc's code does: its answer can depend on the address.
static struct datatype exp64_atomic_lock_free(const struct atomic_builtin* b, struct vector* args){
    struct datatype int_type = exp64_int_type(DATA_SIZE_DWORD, true);
    struct datatype ulong = exp64_int_type(DATA_SIZE_DDWORD, false);
    struct node* size = exp64_atomic_argument(args, 0);
    struct node* p = exp64_atomic_argument(args, 1);
    bool always = S_EQ(b->name, "__atomic_always_lock_free");
    struct constant c;
    if(constant_fold(current_process, size, &c) && constant_is_integer(&c)){
        struct constant pc;
        bool aligned = true;
        if(c.value && constant_fold(current_process, p, &pc) && constant_is_integer(&pc)){
            aligned = pc.value % c.value == 0;
        }
        if(atomic_size_is_lock_free((long long)c.value) && aligned){
            ops->imm(0, 1);
            return int_type;
        }
        if(always){
            ops->imm(0, 0);
            return int_type;
        }
    } else if(always){
        compiler_error(current_process, "The size given to __atomic_always_lock_free must be a constant\n");
    }
    struct datatype pt = exp64_value(p);
    ops->push(&pt);
    struct datatype st = exp64_value(size);
    exp64_convert(0, &st, &ulong);
    ops->push(&ulong);
    exp64_libatomic_call("__atomic_is_lock_free", 2);
    ops->extend(0, DATA_SIZE_BYTE, false);
    return int_type;
}

// gcc's __atomic and __sync builtins (atomic.c): the object's address is
// pushed first, [A], and every access to the object is one atomic
// instruction (or LL/SC loop) of the backend. The value type is the
// type the first argument points to; the builtins do not scale an
// operand for a pointer object, as in gcc. The instructions
// are those of the memory order argument; the __sync forms are seq_cst
// but for __sync_lock_test_and_set (acquire) and __sync_lock_release
// (release), as gcc documents them.
static struct datatype exp64_atomic_builtin(const struct atomic_builtin* b, struct vector* args){
    int kind = b->kind;
    if(vector_count(args) != b->args){
        compiler_error(current_process, "%s expects %i arguments\n", b->name, b->args);
    }
    struct datatype int_type = exp64_int_type(DATA_SIZE_DWORD, true);
    bool sync = !strncmp(b->name, "__sync_", 7);
    if(kind == ATOMIC_BUILTIN_FENCE || kind == ATOMIC_BUILTIN_SIGNAL_FENCE){
        int order = b->args ? exp64_atomic_order(args, 0, ATOMIC_ACCESS_FENCE) : ATOMIC_ORDER_SEQ_CST;
        if(kind == ATOMIC_BUILTIN_FENCE){
            ops->fence(order);
        }
        return exp64_void_type();
    }
    if(kind == ATOMIC_BUILTIN_LOCK_FREE){
        return exp64_atomic_lock_free(b, args);
    }
    // The memory orders first: the arguments after them are on the
    // stack meanwhile.
    int order = ATOMIC_ORDER_SEQ_CST;
    int failure = ATOMIC_ORDER_SEQ_CST;
    if(kind == ATOMIC_BUILTIN_CAS || kind == ATOMIC_BUILTIN_GENERIC_CAS){
        struct node* weak = exp64_atomic_argument(args, 3);
        struct constant c;
        if(!constant_fold(current_process, weak, &c)){
            exp64_value(weak);
        }
        order = exp64_atomic_order(args, 4, ATOMIC_ACCESS_RMW);
        failure = exp64_atomic_order(args, 5, ATOMIC_ACCESS_LOAD);
    } else if(!sync && b->args > 1){
        int access = kind == ATOMIC_BUILTIN_LOAD || kind == ATOMIC_BUILTIN_GENERIC_LOAD ? ATOMIC_ACCESS_LOAD
                   : kind == ATOMIC_BUILTIN_STORE || kind == ATOMIC_BUILTIN_GENERIC_STORE || kind == ATOMIC_BUILTIN_CLEAR
                     ? ATOMIC_ACCESS_STORE : ATOMIC_ACCESS_RMW;
        order = exp64_atomic_order(args, b->args - 1, access);
    } else if(kind == ATOMIC_BUILTIN_RELEASE){
        order = ATOMIC_ORDER_RELEASE;
    } else if(kind == ATOMIC_BUILTIN_EXCHANGE){
        order = ATOMIC_ORDER_ACQUIRE;
    }
    int cas_order = atomic_order_cas(order, failure);
    struct datatype pt = exp64_value(exp64_atomic_argument(args, 0));
    if(!exp64_is_pointer(&pt)){
        compiler_error(current_process, "The first argument of %s must be a pointer\n", b->name);
    }
    struct datatype pointee = exp64_deref(&pt);
    struct datatype t = exp64_unqualified(&pointee);
    if(kind == ATOMIC_BUILTIN_TEST_AND_SET || kind == ATOMIC_BUILTIN_CLEAR){
        // These work on the byte at p, whatever it points to.
        t = exp64_int_type(DATA_SIZE_BYTE, false);
    } else if(exp64_in_memory(&t)){
        return exp64_atomic_memory_builtin(b, args, &t, order, failure);
    }
    int size = exp64_atomic_size(&t);
    if((kind == ATOMIC_BUILTIN_FETCH_OP || kind == ATOMIC_BUILTIN_OP_FETCH) && exp64_is_floating(&t)){
        compiler_error(current_process, "%s needs an integer or pointer object\n", b->name);
    }
    bool is_signed = exp64_is_signed(&t);
    ops->push(&pt);                                         // [A]
    switch(kind){
        case ATOMIC_BUILTIN_LOAD:
            ops->peek(0, 0);
            ops->atomic_load(size, is_signed, order);
            ops->drop(1);
            return t;
        case ATOMIC_BUILTIN_STORE:
        case ATOMIC_BUILTIN_GENERIC_STORE:
        case ATOMIC_BUILTIN_CLEAR:
        case ATOMIC_BUILTIN_RELEASE:
            if(kind == ATOMIC_BUILTIN_STORE){
                exp64_atomic_operand(args, 1, &t);
            } else if(kind == ATOMIC_BUILTIN_GENERIC_STORE){
                exp64_atomic_pointed(args, 1, &t);
            } else {
                ops->imm(0, 0);
            }
            ops->peek(1, 0);
            ops->atomic_store(size, order);
            ops->drop(1);
            return exp64_void_type();
        case ATOMIC_BUILTIN_EXCHANGE:
        case ATOMIC_BUILTIN_TEST_AND_SET:
            if(kind == ATOMIC_BUILTIN_EXCHANGE){
                exp64_atomic_operand(args, 1, &t);
            } else {
                ops->imm(0, 1);
            }
            ops->peek(1, 0);
            ops->atomic_rmw("=", size, order);
            if(size < DATA_SIZE_DDWORD){
                ops->extend(0, size, is_signed);
            }
            ops->drop(1);
            if(kind == ATOMIC_BUILTIN_TEST_AND_SET){
                ops->imm(1, 0);
                ops->compare("!=", false);
                return int_type;
            }
            return t;
        case ATOMIC_BUILTIN_GENERIC_LOAD: {
            struct datatype rt = exp64_value(exp64_atomic_argument(args, 1));
            ops->push(&rt);                                 // [A, ret]
            ops->peek(0, 1);
            ops->atomic_load(size, is_signed, order);
            ops->peek(1, 0);
            ops->store(size);
            ops->drop(2);
            return exp64_void_type();
        }
        case ATOMIC_BUILTIN_GENERIC_EXCHANGE: {
            struct datatype rt = exp64_value(exp64_atomic_argument(args, 2));
            ops->push(&rt);                                 // [A, ret]
            exp64_atomic_pointed(args, 1, &t);
            ops->peek(1, 1);
            ops->atomic_rmw("=", size, order);
            ops->peek(1, 0);
            ops->store(size);
            ops->drop(2);
            return exp64_void_type();
        }
        case ATOMIC_BUILTIN_FETCH_OP:
        case ATOMIC_BUILTIN_OP_FETCH:
            exp64_atomic_operand(args, 1, &t);
            ops->push(&t);                                  // [A, v]
            ops->peek(1, 1);
            ops->atomic_rmw(b->op, size, order);
            if(size < DATA_SIZE_DDWORD){
                ops->extend(0, size, is_signed);
            }
            if(kind == ATOMIC_BUILTIN_OP_FETCH){
                ops->peek(1, 0);
                exp64_atomic_op_value(b->op, &t);
            }
            ops->drop(2);
            return t;
        case ATOMIC_BUILTIN_CAS_VALUE:
        case ATOMIC_BUILTIN_CAS_BOOL:
            exp64_atomic_operand(args, 1, &t);
            ops->push(&t);                                  // [A, old]
            exp64_atomic_operand(args, 2, &t);
            ops->peek(1, 0);                                // R0 = new, R1 = old
            ops->atomic_cas(size, 1, order);
            if(size < DATA_SIZE_DDWORD){
                ops->extend(0, size, is_signed);
            }
            ops->drop(2);
            if(kind == ATOMIC_BUILTIN_CAS_BOOL){
                ops->compare("==", false);
                return int_type;
            }
            return t;
        case ATOMIC_BUILTIN_CAS:
        case ATOMIC_BUILTIN_GENERIC_CAS: {
            // The weak form never fails spuriously here: it is the
            // strong one. Its argument was evaluated with the orders.
            struct datatype et = exp64_value(exp64_atomic_argument(args, 1));
            if(!exp64_is_pointer(&et)){
                compiler_error(current_process, "The second argument of %s must be a pointer\n", b->name);
            }
            ops->push(&et);                                 // [A, E]
            if(kind == ATOMIC_BUILTIN_CAS){
                exp64_atomic_operand(args, 2, &t);
            } else {
                exp64_atomic_pointed(args, 2, &t);
            }
            ops->push(&t);                                  // [A, E, desired]
            ops->peek(0, 1);
            ops->load(size, is_signed);
            ops->mov_r1();                                  // R1 = expected
            ops->peek(0, 0);
            ops->atomic_cas(size, 2, cas_order);            // R0 = what was there
            if(size < DATA_SIZE_DDWORD){
                ops->extend(0, size, is_signed);
            }
            ops->push(&t);                                  // [A, E, desired, seen]
            ops->compare("==", false);
            int done = exp64_new_label();
            ops->branch(done, EXP64_BRANCH_IF_NOT_ZERO);
            // Failed: *E = what was there, the result 0.
            ops->peek(0, 0);
            ops->peek(1, 2);
            ops->store(size);
            ops->imm(0, 0);
            ops->label(done);
            ops->drop(4);
            return int_type;
        }
    }
    compiler_error(current_process, "BUG: atomic builtin %s\n", b->name);
    return int_type;
}

// B1: a struct result of more than 16 bytes goes through memory (the
// hidden pointer in rdi, x8 or a0); a smaller one comes back in the
// two return registers (rax:rdx, x0:x1, a0:a1). An AAPCS64
// homogeneous floating-point aggregate of up to 32 bytes comes back in
// v registers.
bool exp64_returns_in_memory(struct datatype* dtype){
    struct exp64_piece pieces[EXP64_MAX_PIECES];
    if(exp64_struct_pieces(dtype, pieces) || exp64_returns_x87(dtype)){
        return false;
    }
    return exp64_is_struct(dtype) && (exp64_size(dtype) > 2 * DATA_SIZE_DDWORD || exp64_struct_x87(dtype));
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
// variadic arguments, from `named` on, in integer registers.
//
// a struct with float or double members goes in registers by
// its pieces (exp64_struct_pieces) when enough of both kinds are left;
// else as an integer struct would on riscv64, and whole on the stack on
// x86_64 and aarch64 (where no floating-point register is used after
// it: NSRN = 8), an AAPCS64 aggregate of more than 16 bytes too, by
// value. riscv64 passes such a struct to `...` by the integer rules.
// Returns the outgoing stack words;
// *next_reg and *next_fp_out (when given) are the first integer and
// floating-point registers no argument took.
static int exp64_arg_layout(struct datatype* types, int count, int named, bool hidden, struct exp64_arg* out,
                            int* next_reg, int* next_fp_out){
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
        a->pieces    = 0;
        if(exp64_is_ldouble(&types[i])){
            // a long double. x86_64: in memory (class X87) in
            // a 16-byte aligned stack slot. AAPCS64: the next q register,
            // else a 16-byte aligned stack slot. RISC-V: a pair of
            // integer registers, a variadic one from an even register;
            // a named one split between a7 and the stack when only a7 is
            // left; else a 16-byte aligned stack slot, and after a
            // variadic one there no register is used.
            a->size  = EXP64_LD_SIZE;
            a->words = 2;
            if(aarch64 && next_fp < EXP64_FP_ARG_REGISTERS){
                a->fp = true;
                a->reg = next_fp++;
                a->reg_words = 1;
                continue;
            }
            if(!x86_64 && !aarch64){
                if(i >= named){
                    next = align_value(next, 2);
                }
                if(next + 2 <= max){
                    a->reg = next;
                    a->reg_words = 2;
                    next += 2;
                    continue;
                }
                if(next < max && i < named){
                    a->reg = next;
                    a->reg_words = 1;
                    next = max;
                    a->stack = stack++;
                    continue;
                }
                next = max;
            }
            stack = align_value(stack, 2);
            a->stack = stack;
            stack += 2;
            continue;
        }
        int words = a->words;
        int pieces = i < named || x86_64 || aarch64 ? exp64_struct_pieces(&types[i], a->piece) : 0;
        if(pieces){
            if(exp64_assign_pieces(a->piece, pieces, &next, max, &next_fp)){
                a->pieces = pieces;
                continue;
            }
            if(x86_64 || aarch64){
                if(aarch64){
                    next_fp = EXP64_FP_ARG_REGISTERS;
                }
                if(datatype_alignment(&types[i]) > DATA_SIZE_DDWORD){
                    stack = align_value(stack, 2);      // an aggregate of long doubles
                }
                a->stack = stack;
                stack += words;
                continue;
            }
        }
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
        if(a->size > 2 * DATA_SIZE_DDWORD || exp64_struct_x87(&types[i])){
            if(x86_64){
                if(datatype_alignment(&types[i]) > DATA_SIZE_DDWORD){
                    stack = align_value(stack, 2);      // a long double member
                }
                a->stack = stack;
                stack += words;
                continue;
            }
            a->kind = EXP64_ARG_REF;
            words = 1;
        }
        // on riscv64 a struct aligned to 16 (a long double
        // member) passed to `...` takes an even register pair, as a long
        // double does, and a 16-byte aligned stack slot.
        bool aligned16 = !x86_64 && !aarch64 && a->kind == EXP64_ARG_WORDS && is_struct
                      && datatype_alignment(&types[i]) > DATA_SIZE_DDWORD;
        if(aligned16 && i >= named){
            next = align_value(next, 2);
        }
        if(next + words <= max){
            a->reg = next;
            a->reg_words = words;
            next += words;
            continue;
        }
        if(aarch64){
            next = max;
        } else if(!x86_64 && next < max && !(aligned16 && i >= named)){
            a->reg = next;
            a->reg_words = max - next;
            next = max;
        }
        if(aligned16 && !a->reg_words){
            next = max;
            stack = align_value(stack, 2);
        }
        a->stack = stack;
        stack += words - a->reg_words;
    }
    if(next_reg){
        *next_reg = next;
    }
    if(next_fp_out){
        *next_fp_out = next_fp;
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
        if(a->kind == EXP64_ARG_REF || a->fp || a->pieces || a->slot != word){
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
// *next_reg, *next_fp and *stack_words (when given) are where the
// variadic arguments of a variadic function begin.
static struct exp64_arg* exp64_parameter_layout(struct node* function, int* count_out,
                                                int* next_reg, int* next_fp, int* stack_words){
    struct vector* params = function_node_argument_vec(function);
    int count = vector_count(params);
    struct datatype* types = calloc(count + 1, sizeof(struct datatype));
    struct exp64_arg* layout = calloc(count + 1, sizeof(struct exp64_arg));
    for(int i = 0; i < count; i++){
        types[i] = (*(struct node**)vector_at(params, i))->var.type;
    }
    int words = exp64_arg_layout(types, count, count, exp64_returns_in_memory(&function->func.rtype), layout,
                                 next_reg, next_fp);
    if(stack_words){
        *stack_words = words;
    }
    free(types);
    *count_out = count;
    return layout;
}

void exp64_parameters(struct node* function, int register_home, int stack_home, int fp_home){
    struct vector* params = function_node_argument_vec(function);
    int count = 0;
    int next_reg = 0;
    int next_fp = 0;
    int stack_words = 0;
    struct exp64_arg* layout = exp64_parameter_layout(function, &count, &next_reg, &next_fp, &stack_words);
    // Where va_start finds the first variadic argument.
    int max = current_process->target->max_reg_args;
    exp64_va_gp_end   = register_home + DATA_SIZE_DDWORD * max;
    exp64_va_overflow = stack_home + DATA_SIZE_DDWORD * stack_words;
    exp64_va_gp       = next_reg < max ? register_home + DATA_SIZE_DDWORD * next_reg : exp64_va_overflow;
    exp64_va_fp_end   = fp_home + DATA_SIZE_DDWORD * EXP64_FP_ARG_REGISTERS;
    exp64_va_fp       = next_fp < EXP64_FP_ARG_REGISTERS ? fp_home + DATA_SIZE_DDWORD * next_fp : exp64_va_fp_end;
    exp64_va_gp_offset = DATA_SIZE_DDWORD * (next_reg < max ? next_reg : max);
    exp64_va_fp_offset = EXP64_PSABI_GP_END
                       + EXP64_PSABI_FP_STEP * (next_fp < EXP64_FP_ARG_REGISTERS ? next_fp : EXP64_FP_ARG_REGISTERS);
    int assembly = fp_home + DATA_SIZE_DDWORD * EXP64_FP_ARG_REGISTERS;
    for(int i = 0; i < count; i++){
        struct node* param = *(struct node**)vector_at(params, i);
        struct exp64_arg* a = &layout[i];
        int flags = RESOLVER_DEFAULT_ENTITY_FLAG_IS_LOCAL_STACK;
        if(a->kind == EXP64_ARG_REF){
            flags |= RESOLVER_DEFAULT_ENTITY_FLAG_IS_REFERENCE;
        }
        if(a->fp && a->size == EXP64_LD_SIZE){
            // a long double in a q register (aarch64).
            ops->ld_spill_q(a->reg, assembly);
            param->var.aoffset = assembly;
            assembly += EXP64_LD_SIZE;
        } else if(a->pieces){
            // the struct is put together from the homes of
            // its registers, in the area after the floating-point home.
            for(int k = 0; k < a->pieces; k++){
                struct exp64_piece* p = &a->piece[k];
                if(p->size == EXP64_LD_SIZE){
                    ops->ld_spill_q(p->reg, assembly + p->offset);
                    continue;
                }
                ops->frame_address(assembly + p->offset);
                ops->mov_r1();
                ops->frame_address((p->fp ? fp_home : register_home) + DATA_SIZE_DDWORD * p->reg);
                ops->load(p->size, false);
                ops->store(p->size);
            }
            param->var.aoffset = assembly;
            assembly += align_value(a->size, DATA_SIZE_DDWORD);
        } else if(a->fp){
            param->var.aoffset = fp_home + DATA_SIZE_DDWORD * a->reg;
        } else {
            param->var.aoffset = a->reg >= 0 ? register_home + DATA_SIZE_DDWORD * a->reg
                                             : stack_home + DATA_SIZE_DDWORD * a->stack;
        }
        resolver_default_new_scope_entity(current_process->resolver, param, param->var.aoffset, flags);
        debug_parameter(current_process, param, a->kind == EXP64_ARG_REF);
        int k = ops->reg_set ? regalloc_register(param) : -1;
        if(k >= 0){
            // a parameter that lives in a register goes there
            // from its home slot (an integer or a pointer: never a
            // struct or a floating one).
            ops->frame_address(param->var.aoffset);
            exp64_load(&param->var.type, true);
            ops->reg_set(k);
        }
    }
    exp64_va_scratch = assembly;
    // a variadic function on aarch64 that reads a long double
    // with va_arg keeps all eight q registers after the scratch room.
    exp64_va_qhome = 0;
    if(exp64_needs_qhome(function)){
        exp64_va_qhome = assembly + (exp64_needs_scratch(function) ? EXP64_VA_SCRATCH : 0);
        for(int k = 0; k < EXP64_FP_ARG_REGISTERS; k++){
            ops->ld_spill_q(k, exp64_va_qhome + EXP64_LD_SIZE * k);
        }
    }
    // on x86_64 the psABI register save area of a variadic
    // function, then the va_list states, after the rest of the home.
    int end = assembly + (exp64_needs_scratch(function) ? EXP64_VA_SCRATCH : 0)
            + (exp64_needs_qhome(function) ? EXP64_LD_SIZE * EXP64_FP_ARG_REGISTERS : 0);
    exp64_va_save = end;
    exp64_va_slots = end + (exp64_needs_save_area(function) ? EXP64_PSABI_FP_END : 0);
    exp64_va_next_slot = 0;
    if(exp64_needs_save_area(function)){
        for(int k = 0; k < max; k++){
            ops->frame_address(exp64_va_save + DATA_SIZE_DDWORD * k);
            ops->mov_r1();
            ops->frame_address(register_home + DATA_SIZE_DDWORD * k);
            ops->load(DATA_SIZE_DDWORD, false);
            ops->store(DATA_SIZE_DDWORD);
        }
        for(int k = 0; k < EXP64_FP_ARG_REGISTERS; k++){
            ops->frame_address(exp64_va_save + EXP64_PSABI_GP_END + EXP64_PSABI_FP_STEP * k);
            ops->mov_r1();
            ops->frame_address(fp_home + DATA_SIZE_DDWORD * k);
            ops->load(DATA_SIZE_DDWORD, false);
            ops->store(DATA_SIZE_DDWORD);
        }
    }
    free(layout);
}

// The floating-point argument registers the parameters of `function`
// arrive in: the prologue spills that many (none for most functions,
// whose code does not change). A variadic function on x86_64 and
// aarch64 spills all of them: va_arg reads its doubles from there.
int exp64_fp_parameter_registers(struct node* function){
    const char* abi = current_process->target->backend->name;
    if((function->func.flags & FUNCTION_NODE_FLAG_IS_VARIADIC) && (S_EQ(abi, "x86_64") || S_EQ(abi, "aarch64"))){
        return EXP64_FP_ARG_REGISTERS;
    }
    int count = 0;
    int used = 0;
    struct exp64_arg* layout = exp64_parameter_layout(function, &count, 0, 0, 0);
    for(int i = 0; i < count; i++){
        if(layout[i].fp && layout[i].reg + 1 > used){
            used = layout[i].reg + 1;
        }
        for(int k = 0; k < layout[i].pieces; k++){
            if(layout[i].piece[k].fp && layout[i].piece[k].reg + 1 > used){
                used = layout[i].piece[k].reg + 1;
            }
        }
    }
    free(layout);
    return used;
}

static bool exp64_needs_scratch(struct node* function){
    const char* abi = current_process->target->backend->name;
    return (function->func.flags & FUNCTION_NODE_FLAG_STRUCT_POINTER_CAST)
        && (S_EQ(abi, "x86_64") || S_EQ(abi, "aarch64"));
}

// a variadic aarch64 function with `(long double*)0`, which
// va_arg of a long double has, keeps its q registers whole.
static bool exp64_needs_qhome(struct node* function){
    return (function->func.flags & FUNCTION_NODE_FLAG_LONG_DOUBLE_POINTER_CAST)
        && (function->func.flags & FUNCTION_NODE_FLAG_IS_VARIADIC)
        && S_EQ(current_process->target->backend->name, "aarch64");
}

// a variadic function on x86_64 has a psABI register save area.
static bool exp64_needs_save_area(struct node* function){
    return (function->func.flags & FUNCTION_NODE_FLAG_IS_VARIADIC) && exp64_va_psabi();
}

// the va_list states of a function on x86_64.
static int exp64_va_states_size(struct node* function){
    return exp64_va_psabi() ? EXP64_PSABI_STATE * function->func.va_lists : 0;
}

// bytes of the floating-point home under the locals, which
// the prologue makes when exp64_fp_parameter_registers is not 0: the
// eight registers' slots, then a copy of each struct parameter that
// arrived in pieces (exp64_parameters puts it together), then for a
// function on x86_64 and aarch64 that has `(struct s*)0` (va_arg of a
// struct does) 32 bytes where va_arg puts such a struct together. A
// multiple of 16.
int exp64_fp_home_size(struct node* function){
    bool scratch = exp64_needs_scratch(function);
    if(!exp64_fp_parameter_registers(function) && !scratch && !exp64_va_states_size(function)){
        return 0;
    }
    int size = DATA_SIZE_DDWORD * EXP64_FP_ARG_REGISTERS;
    int count = 0;
    struct exp64_arg* layout = exp64_parameter_layout(function, &count, 0, 0, 0);
    for(int i = 0; i < count; i++){
        if(layout[i].pieces){
            size += align_value(layout[i].size, DATA_SIZE_DDWORD);
        } else if(layout[i].fp && layout[i].size == EXP64_LD_SIZE){
            size += EXP64_LD_SIZE;
        }
    }
    free(layout);
    if(scratch){
        size += EXP64_VA_SCRATCH;
    }
    if(exp64_needs_qhome(function)){
        size += EXP64_LD_SIZE * EXP64_FP_ARG_REGISTERS;
    }
    if(exp64_needs_save_area(function)){
        size += EXP64_PSABI_FP_END;
    }
    size += exp64_va_states_size(function);
    return align_value(size, 16);
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
        const struct atomic_builtin* atomic = atomic_builtin_get(callee->sval);
        if(atomic){
            return exp64_atomic_builtin(atomic, args);
        }
        if(!strncmp(callee->sval, "__builtin_vla_", 14)){
            return exp64_vla(callee->sval, args);
        }
        return exp64_stdarg(callee->sval, args);
    }

    struct datatype return_type;
    // The parameter types the arguments are converted to: a named
    // function's parameters, or those of the function pointer's type.
    struct datatype* param_types = 0;
    int param_count = 0;
    // A variadic prototype: the arguments past its parameters are the
    // variadic ones.
    bool variadic = false;
    bool own_param_types = false;
    const char* symbol = 0;
    if(exp64_names_function(callee)){
        struct resolver_entity* function = resolver_get_function(0, current_process->resolver, callee->sval);
        return_type = function->dtype;
        struct vector* params = function->node->func.args.vector;
        variadic = function->node->func.flags & FUNCTION_NODE_FLAG_IS_VARIADIC;
        param_count = params ? vector_count(params) : 0;
        param_types = calloc(param_count + 1, sizeof(struct datatype));
        own_param_types = true;
        for(int i = 0; i < param_count; i++){
            param_types[i] = (*(struct node**)vector_at(params, i))->var.type;
        }
        symbol = function->name;
    } else {
        struct datatype t = exp64_typeof(callee);
        return_type = t;
        if(!(t.flags & DATATYPE_FLAG_IS_FUNCTION_POINTER)){
            compiler_error(current_process, "Called object is not a function or function pointer\n");
        }
        return_type = *datatype_pointer_reduce(&t, 1);
        return_type.flags &= ~DATATYPE_FLAG_IS_FUNCTION_POINTER;
        struct function_prototype* prototype = t.function_params;
        if(prototype){
            param_count = vector_count(prototype->types);
            int given = vector_count(args);
            if(given < param_count || (given > param_count && !prototype->variadic)){
                compiler_error(current_process, "The call through a function pointer passes %i arguments, its type has %i parameters\n",
                    given, param_count);
            }
            param_types = vector_data_ptr(prototype->types);
            variadic = prototype->variadic;
        }
    }

    struct exp64_call call = { .symbol = symbol };
    if(exp64_in_memory(&return_type)){
        call.struct_size = align_value(exp64_size(&return_type), DATA_SIZE_DDWORD);
        call.struct_in_regs = !exp64_returns_in_memory(&return_type);
        call.ld_return = exp64_returns_x87(&return_type) || exp64_is_ldouble(&return_type);
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
        if(i < param_count && exp64_is_struct(&param_types[i]) != exp64_is_struct(&t)){
            compiler_error(current_process, "Argument %i of the call is %s struct, the parameter %s\n",
                i + 1, exp64_is_struct(&t) ? "a" : "not a", exp64_is_struct(&t) ? "is not" : "is one");
        }
        if(exp64_is_struct(&t)){
            ops->push_struct(exp64_size(&t));
        } else {
            if(i < param_count){
                exp64_convert(0, &t, &param_types[i]);
                if(exp64_is_floating(&t) || exp64_is_floating(&param_types[i])
                   || exp64_is_ldouble(&t) || exp64_is_ldouble(&param_types[i])){
                    t = param_types[i];
                }
            } else if(exp64_is_floating(&t) && t.size == DATA_SIZE_DWORD){
                // The default argument promotions: a float passed to
                // `...` or without a prototype is a double.
                struct datatype d = datatype_floating(DATA_SIZE_DDWORD);
                exp64_convert(0, &t, &d);
                t = d;
            }
            if(exp64_is_ldouble(&t)){
                ops->push_struct(EXP64_LD_SIZE);
            } else {
                ops->push(&t);
            }
        }
        types[i] = t;
        pushed[i] = vector_count(ledger);
    }
    // The arguments past the prototype's parameters of a variadic
    // function are its variadic ones; without a prototype all are named.
    int named = variadic ? param_count : count;
    call.count = count;
    call.args = calloc(count + 1, sizeof(struct exp64_arg));
    call.first_reg = exp64_first_reg(call.struct_size && !call.struct_in_regs);
    call.stack_words = exp64_arg_layout(types, count, named, call.struct_size && !call.struct_in_regs, call.args, 0, 0);
    for(int i = 0; i < count; i++){
        if(call.args[i].fp && call.args[i].reg + 1 > call.fp_regs){
            call.fp_regs = call.args[i].reg + 1;
        }
        for(int k = 0; k < call.args[i].pieces; k++){
            if(call.args[i].piece[k].fp && call.args[i].piece[k].reg + 1 > call.fp_regs){
                call.fp_regs = call.args[i].piece[k].reg + 1;
            }
        }
    }
    if(call.struct_in_regs && !call.ld_return){
        call.return_pieces = exp64_return_pieces(&return_type, call.return_piece);
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
    if(own_param_types){
        free(param_types);
    }
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
    if(exp64_is_ldouble(&t)){
        return datatype_long_double();
    }
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
    if(node->num.flags & NUMBER_FLAG_LONG_DOUBLE){
        return exp64_ld_number(node);
    }
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
    struct datatype rlt;
    int k = exp64_register_of(node->unary.operand, &rlt);
    if(k >= 0){
        // stepped in its register.
        bool postfix = node->unary.flags & UNARY_FLAG_IS_POSTFIX;
        long long n = S_EQ(node->unary.op, "++") ? 1 : -1;
        if(exp64_register_step(k, &rlt, n)){
            if(postfix){
                exp64_register_step_back(&rlt, n);
            }
            return rlt;
        }
        ops->reg_get(k);
        ops->imm(1, 1);
        struct datatype result = exp64_arith(step_op, &rlt, &one);
        exp64_convert(0, &result, &rlt);
        ops->reg_set(k);
        if(node->unary.flags & UNARY_FLAG_IS_POSTFIX){
            ops->imm(1, 1);
            result = exp64_arith(back_op, &rlt, &one);
            exp64_convert(0, &result, &rlt);
        }
        return rlt;
    }
    struct datatype lt = exp64_address(node->unary.operand);
    if(exp64_is_struct(&lt) || exp64_is_array(&lt)){
        compiler_error(current_process, "%s needs a scalar operand\n", node->unary.op);
    }
    if(datatype_is_atomic(&lt)){
        return exp64_atomic_update(step_op, &lt, 0, node->unary.flags & UNARY_FLAG_IS_POSTFIX);
    }
    if(exp64_is_ldouble(&lt)){
        // x + 1 into x; the postfix forms keep a copy of the
        // old value.
        int target = exp64_push_index();
        int old = -1;
        if(node->unary.flags & UNARY_FLAG_IS_POSTFIX){
            exp64_ld_copy();
            old = vector_count(current_function->func.frame.elements) - 1;
        }
        ops->imm(0, 1);
        exp64_convert(0, &one, &lt);
        int step = exp64_push_index();
        exp64_ld_operate(step_op, target, step);
        ops->mov_r1();
        exp64_slot_load(target);
        ops->copy(EXP64_LD_SIZE);
        if(old >= 0){
            ops->slot_address(vector_count(current_function->func.frame.elements) - 1 - old);
        } else {
            exp64_slot_load(target);
        }
        return lt;
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
    if(exp64_is_ldouble(&t)){
        if(S_EQ(op, "+")){
            return t;
        }
        if(!S_EQ(op, "-")){
            compiler_error(current_process, "The operator %s does not take a long double operand\n", op);
        }
        return exp64_ld_negate();
    }
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
    compiler_check_stack(current_process);
    struct vector* ledger = current_function->func.frame.elements;
    int mark = vector_count(ledger);
    // An operator goes straight to exp64_expression: one frame less for
    // each level of a long `a + b + ...`, whose left operands nest (the
    // stack limit).
    struct datatype t;
    if(node->type == NODE_TYPE_EXPRESSION){
        if(node->pos.line){
            current_process->pos = node->pos;
        }
        t = exp64_expression(node, decay);
    } else {
        t = exp64_rvalue_of(node, decay);
    }
    // An array member of such a struct (`(c ? s : t).arr`) is
    // an address into the room, which has to stay until the element is
    // read.
    if(vector_count(ledger) > mark && !exp64_in_memory(&t) && !exp64_is_array(&t)){
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
            struct datatype rt;
            int k = exp64_register_of(node, &rt);
            if(k >= 0){
                ops->reg_get(k);
                return rt;
            }
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

// The value of `node` in R0; its type. The callers (expression
// statements, the parts of `for`, `switch`) use no struct or long
// double value, so the room such a value took is dropped again
//: a `for` step such as `x /= 10` on a long double would
// otherwise take more stack on every turn of the loop.
struct datatype exp64_generate(struct node* node){
    int mark = vector_count(current_function->func.frame.elements);
    struct datatype t = exp64_value(node);
    if(exp64_in_memory(&t) && vector_count(current_function->func.frame.elements) > mark){
        ops->drop(vector_count(current_function->func.frame.elements) - mark);
    }
    return t;
}

// A `return` value: converted to the function's return type when either
// is floating (`return 1;` in a double function), since the result
// travels in a floating-point register. Integer results are left as
// they were; the caller extends a narrow one.
struct datatype exp64_return_value(struct node* node, struct datatype* rtype){
    struct datatype t = exp64_value(node);
    if(exp64_is_floating(&t) || exp64_is_floating(rtype) || exp64_is_ldouble(&t) || exp64_is_ldouble(rtype)){
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
    // with long doubles about, a comparison is worked out as a
    // value and tested.
    if(node->type != NODE_TYPE_EXPRESSION || !exp64_is_comparison(node->exp.op) || current_process->uses_long_double){
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
    int mark = vector_count(current_function->func.frame.elements);
    int k = ops->reg_set ? regalloc_register(var) : -1;
    if(k >= 0){
        // a register variable.
        struct datatype rt = exp64_value(value);
        exp64_convert(0, &rt, t);
        ops->reg_set(k);
        return;
    }
    struct datatype vt = exp64_value(value);
    if(exp64_is_ldouble(t)){
        // the room of the value is dropped after the copy, so
        // a `for` that declares a long double leaves nothing behind.
        exp64_convert(0, &vt, t);
        ops->mov_r1();
        ops->frame_address(offset);
        ops->copy(exp64_size(t));
        if(vector_count(current_function->func.frame.elements) > mark){
            ops->drop(vector_count(current_function->func.frame.elements) - mark);
        }
        return;
    }
    if(exp64_in_memory(t)){
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
