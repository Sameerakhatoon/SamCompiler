#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "compiler.h"
#include "helpers/vector.h"

// ch104: skeleton. ch105: iterate the AST and emit the three usual
// asm sections (.data, .text, .rodata) - the per-node emit hooks are
// still placeholders; the bodies fill in across the rest of Module 2.

static struct compile_process* current_process  = 0;
// ch137: function being emitted. Asm push/pop helpers tag stack
// frame elements against this.
static struct node*            current_function = 0;

// ch147: per-expression bookkeeping for logical && / || codegen.
struct history_exp {
    const char* logical_start_op;
    char        logical_end_label[20];
    char        logical_end_label_positive[20];
};

// ch137: codegen-side history. Separate from the parser history but
// carries equivalent flags (IS_ALONE_STATEMENT etc.).
struct history {
    int flags;
    // ch147: union grows as new expression contexts need state.
    union {
        struct history_exp exp;
    };
};

static struct history* codegen_history_begin(int flags){
    struct history* h = calloc(1, sizeof(struct history));
    h->flags = flags;
    return h;
}

static struct history* codegen_history_down(struct history* h, int flags){
    struct history* n = calloc(1, sizeof(struct history));
    memcpy(n, h, sizeof(struct history));
    n->flags = flags;
    return n;
}

static void          codegen_new_scope(int flags);
static void          codegen_finish_scope(void);
static struct node*  codegen_node_next(void);
static void          asm_push_args(const char* ins, va_list args);
static void          asm_push(const char* ins, ...);
static const char*   asm_keyword_for_size(size_t size, char* tmp_buf);
static void          codegen_generate_global_variable_for_primitive(struct node* node);
static const char*   codegen_register_string(const char* str);
static void          codegen_generate_global_variable(struct node* node);
static void          codegen_generate_data_section_part(struct node* node);
static void          codegen_generate_data_section(void);
static void          codegen_generate_root_node(struct node* node);
static void          codegen_generate_root(void);
static void          codegen_write_strings(void);
static void          codegen_generate_rod(void);

// ch137: now delegate to the default resolver.
static void codegen_new_scope(int flags){
    resolver_default_new_scope(current_process->resolver, flags);
}

static void codegen_finish_scope(void){
    resolver_default_finish_scope(current_process->resolver);
}

static struct node* codegen_node_next(void){
    return vector_peek_ptr(current_process->node_tree_vec);
}

static void asm_push_args(const char* ins, va_list args){
    va_list args2;
    va_copy(args2, args);
    vfprintf(stdout, ins, args);
    fprintf(stdout, "\n");
    if(current_process->ofile){
        vfprintf(current_process->ofile, ins, args2);
        fprintf(current_process->ofile, "\n");
    }
    va_end(args2);
}

static void asm_push(const char* ins, ...){
    va_list args;
    va_start(args, ins);
    asm_push_args(ins, args);
    va_end(args);
}

// ch110: same as asm_push but does not append the trailing newline.
// Lets the string emitter assemble one line via several calls.
static void asm_push_no_nl(const char* ins, ...){
    va_list args;
    va_start(args, ins);
    vfprintf(stdout, ins, args);
    va_end(args);
    if(current_process->ofile){
        va_list args2;
        va_start(args2, ins);
        vfprintf(current_process->ofile, ins, args2);
        va_end(args2);
    }
}

// ch137: emit a `push <fmt>` line and record a matching stack-frame
// element on the current function. The stack-frame model tracks every
// push so we can assert frame balance on return.
static void asm_push_ins_push(const char* fmt, int stack_entity_type, const char* stack_entity_name, ...){
    char tmp_buf[200];
    sprintf(tmp_buf, "push %s", fmt);
    va_list args;
    va_start(args, stack_entity_name);
    asm_push_args(tmp_buf, args);
    va_end(args);
    assert(current_function);
    stackframe_push(current_function, &(struct stack_frame_element){
        .type = stack_entity_type, .name = stack_entity_name,
    });
}

// ch150: push variant that also tags the ledger element with extra
// flags (e.g. IS_PUSHED_ADDRESS for an address rather than a value).
static void asm_push_ins_push_with_flags(const char* fmt, int stack_entity_type, const char* stack_entity_name, int flags, ...){
    char tmp_buf[200];
    sprintf(tmp_buf, "push %s", fmt);
    va_list args;
    va_start(args, flags);
    asm_push_args(tmp_buf, args);
    va_end(args);
    assert(current_function);
    stackframe_push(current_function, &(struct stack_frame_element){
        .type = stack_entity_type, .name = stack_entity_name, .flags = flags,
    });
}

// ch137: matching pop that asserts the top stack-frame element has
// the expected type/name. Returns the popped element's flags.
static int asm_push_ins_pop(const char* fmt, int expecting_stack_entity_type, const char* expecting_stack_entity_name, ...){
    char tmp_buf[200];
    sprintf(tmp_buf, "pop %s", fmt);
    va_list args;
    va_start(args, expecting_stack_entity_name);
    asm_push_args(tmp_buf, args);
    va_end(args);
    assert(current_function);
    struct stack_frame_element* el = stackframe_back(current_function);
    int flags = el->flags;
    stackframe_pop_expecting(current_function, expecting_stack_entity_type, expecting_stack_entity_name);
    return flags;
}

// ch138: push variant that also carries a stack_frame_data payload
// (e.g. the datatype of the value being pushed). Sets HAS_DATATYPE.
static void asm_push_ins_push_with_data(const char* fmt, int stack_entity_type, const char* stack_entity_name, int flags, struct stack_frame_data* data, ...){
    char tmp_buf[200];
    sprintf(tmp_buf, "push %s", fmt);
    va_list args;
    va_start(args, data);
    asm_push_args(tmp_buf, args);
    va_end(args);
    flags |= STACK_FRAME_ELEMENT_FLAG_HAS_DATATYPE;
    assert(current_function);
    stackframe_push(current_function, &(struct stack_frame_element){
        .type = stack_entity_type, .name = stack_entity_name,
        .flags = flags, .data = *data,
    });
}

static void asm_push_ebp(void){
    asm_push_ins_push("ebp", STACK_FRAME_ELEMENT_TYPE_SAVED_BP, "function_entry_saved_ebp");
}

static void asm_pop_ebp(void){
    asm_push_ins_pop("ebp", STACK_FRAME_ELEMENT_TYPE_SAVED_BP, "function_entry_saved_ebp");
}

// ch137: bump esp down (allocate locals). The stack-frame ledger
// gets `amount/STACK_PUSH_SIZE` UNKNOWN entries pushed so it matches
// what codegen will pop on exit.
static void codegen_stack_sub_with_name(size_t stack_size, const char* name){
    if(stack_size != 0){
        stackframe_sub(current_function, STACK_FRAME_ELEMENT_TYPE_UNKNOWN, name, stack_size);
        asm_push("sub esp, %lu", (unsigned long)stack_size);
    }
}

static void codegen_stack_sub(size_t stack_size){
    codegen_stack_sub_with_name(stack_size, "literal_stack_change");
}

static void codegen_stack_add_with_name(size_t stack_size, const char* name){
    if(stack_size != 0){
        stackframe_add(current_function, STACK_FRAME_ELEMENT_TYPE_UNKNOWN, name, stack_size);
        asm_push("add esp, %lu", (unsigned long)stack_size);
    }
}

static void codegen_stack_add(size_t stack_size){
    codegen_stack_add_with_name(stack_size, "literal_stack_change");
}

static struct resolver_entity* codegen_new_scope_entity(struct node* var_node, int offset, int flags){
    return resolver_default_new_scope_entity(current_process->resolver, var_node, offset, flags);
}

static struct resolver_entity* codegen_register_function(struct node* func_node, int flags){
    return resolver_default_register_function(current_process->resolver, func_node, flags);
}

static void codegen_generate_function_prototype(struct node* node){
    codegen_register_function(node, 0);
    asm_push("extern %s", node->func.name);
}

static void codegen_generate_function_arguments(struct vector* argument_vector){
    vector_set_peek_pointer(argument_vector, 0);
    struct node* current = vector_peek_ptr(argument_vector);
    while(current){
        codegen_new_scope_entity(current, current->var.aoffset, RESOLVER_DEFAULT_ENTITY_FLAG_IS_LOCAL_STACK);
        current = vector_peek_ptr(argument_vector);
    }
}

// ch138: NUMBER -> push dword <literal>. The stack-frame ledger
// gets a PUSHED_VALUE / "result_value" element with a numeric flag
// and an int dtype attached.
static void codegen_generate_number_node(struct node* node, struct history* history){
    (void)history;
    asm_push_ins_push_with_data("dword %i",
        STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value",
        STACK_FRAME_ELEMENT_FLAG_IS_NUMERICAL,
        &(struct stack_frame_data){.dtype = datatype_for_numeric()},
        (int)node->llnum);
}

static bool codegen_is_exp_root_for_flags(int flags){
    return !(flags & EXPRESSION_IS_NOT_ROOT_NODE);
}

static bool codegen_is_exp_root(struct history* history){
    return codegen_is_exp_root_for_flags(history->flags);
}

// Forward decl - real impl lands further down (ch142).
static void codegen_generate_exp_node(struct node* node, struct history* history);
// ch145: forward decl for the identifier value-load path.
static void codegen_generate_identifier(struct node* node, struct history* history);
// ch147: forward decl - real impl lives in the label-system block below.
static int  codegen_label_count(void);
// ch151: forward decl for the UNARY codegen used by the dispatcher
// (real impl lives further down).
static void codegen_generate_unary(struct node* node, struct history* history);

// ch150: forward decls for the structure helpers (real impls live
// further down).
static void codegen_generate_structure_push_or_return(struct resolver_entity* entity, struct history* history, int start_pos);
static void codegen_generate_structure_push(struct resolver_entity* entity, struct history* history, int start_pos);
static void codegen_generate_move_struct(struct datatype* dtype, const char* base_address, int offset);

// ch138: dispatch by node type. Sets EXPRESSION_IS_NOT_ROOT_NODE for
// nested calls so the recursive walk knows it's deeper than the top.
static void codegen_generate_expressionable(struct node* node, struct history* history){
    if(codegen_is_exp_root(history)){
        history->flags |= EXPRESSION_IS_NOT_ROOT_NODE;
    }
    switch(node->type){
        case NODE_TYPE_NUMBER:
            codegen_generate_number_node(node, history);
            break;
        // ch145: identifiers take the value-load path; (sub)expressions
        // route back through the expression dispatcher.
        case NODE_TYPE_IDENTIFIER:
            codegen_generate_identifier(node, history);
            break;
        case NODE_TYPE_EXPRESSION:
            codegen_generate_exp_node(node, history);
            break;
        // ch151: UNARY in expression position (e.g. `&x`).
        case NODE_TYPE_UNARY:
            codegen_generate_unary(node, history);
            break;
    }
}

// ch138: low-byte / word / full-register aliases for eax/ebx/ecx/edx.
static const char* codegen_sub_register(const char* original_register, size_t size){
    const char* reg = 0;
    if(S_EQ(original_register, "eax")){
        if(size == DATA_SIZE_BYTE)       reg = "al";
        else if(size == DATA_SIZE_WORD)  reg = "ax";
        else if(size == DATA_SIZE_DWORD) reg = "eax";
    } else if(S_EQ(original_register, "ebx")){
        if(size == DATA_SIZE_BYTE)       reg = "bl";
        else if(size == DATA_SIZE_WORD)  reg = "bx";
        else if(size == DATA_SIZE_DWORD) reg = "ebx";
    } else if(S_EQ(original_register, "ecx")){
        if(size == DATA_SIZE_BYTE)       reg = "cl";
        else if(size == DATA_SIZE_WORD)  reg = "cx";
        else if(size == DATA_SIZE_DWORD) reg = "ecx";
    } else if(S_EQ(original_register, "edx")){
        if(size == DATA_SIZE_BYTE)       reg = "dl";
        else if(size == DATA_SIZE_WORD)  reg = "dx";
        else if(size == DATA_SIZE_DWORD) reg = "edx";
    }
    return reg;
}

static const char* codegen_byte_word_or_dword_or_ddword(size_t size, const char** reg_to_use){
    const char* type = 0;
    const char* new_register = *reg_to_use;
    if(size == DATA_SIZE_BYTE){
        type = "byte";   new_register = codegen_sub_register(*reg_to_use, DATA_SIZE_BYTE);
    } else if(size == DATA_SIZE_WORD){
        type = "word";   new_register = codegen_sub_register(*reg_to_use, DATA_SIZE_WORD);
    } else if(size == DATA_SIZE_DWORD){
        type = "dword";  new_register = codegen_sub_register(*reg_to_use, DATA_SIZE_DWORD);
    } else if(size == DATA_SIZE_DDWORD){
        type = "ddword"; new_register = codegen_sub_register(*reg_to_use, DATA_SIZE_DDWORD);
    }
    *reg_to_use = new_register;
    return type;
}

// ch138: emit the assignment store instruction. Currently `=` and
// `+=`; later chapters fill in the rest of the compound ops.
static void codegen_generate_assignment_instruction_for_operator(const char* mov_type_keyword, const char* address, const char* reg_to_use, const char* op, bool is_signed){
    (void)is_signed;
    if(S_EQ(op, "=")){
        asm_push("mov %s [%s], %s", mov_type_keyword, address, reg_to_use);
    } else if(S_EQ(op, "+=")){
        asm_push("add %s [%s], %s", mov_type_keyword, address, reg_to_use);
    }
}

static struct resolver_default_entity_data* codegen_entity_private(struct resolver_entity* entity){
    return resolver_default_entity_private(entity);
}

// ch138: a local variable declaration. Register it as a scope entity
// (stack-resident) and, if it has an initializer, evaluate the RHS
// onto the stack and store it at the variable's address.
static void codegen_generate_scope_variable(struct node* node){
    struct resolver_entity* entity = codegen_new_scope_entity(node, node->var.aoffset, RESOLVER_DEFAULT_ENTITY_FLAG_IS_LOCAL_STACK);
    if(node->var.val){
        codegen_generate_expressionable(node->var.val,
            codegen_history_begin(EXPRESSION_IS_ASSIGNMENT | IS_RIGHT_OPERAND_OF_ASSIGNMENT));
        asm_push_ins_pop("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
        const char* reg_to_use = "eax";
        const char* mov_type   = codegen_byte_word_or_dword_or_ddword(datatype_element_size(&entity->dtype), &reg_to_use);
        codegen_generate_assignment_instruction_for_operator(mov_type, codegen_entity_private(entity)->address, reg_to_use, "=",
            entity->dtype.flags & DATATYPE_FLAG_IS_SIGNED);
    }
}

// ch140: emit the address / value of the root entity of an assignment
// LHS expression. Three cases:
//   - UNSUPPORTED root: descend into the node directly.
//   - FIRST_ENTITY_PUSH_VALUE: push the dword at base.address.
//   - FIRST_ENTITY_LOAD_TO_EBX: `lea ebx, [base]` (or `mov ebx, [base]`
//                               for a pointer-array next entity),
//                               then push ebx.
static void codegen_generate_entity_access_start(struct resolver_result* result, struct resolver_entity* root_assignment_entity, struct history* history){
    if(root_assignment_entity->type == RESOLVER_ENTITY_TYPE_UNSUPPORTED){
        codegen_generate_expressionable(root_assignment_entity->node, history);
    } else if(result->flags & RESOLVER_RESULT_FLAG_FIRST_ENTITY_PUSH_VALUE){
        asm_push_ins_push_with_data("dword [%s]",
            STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value", 0,
            &(struct stack_frame_data){.dtype = root_assignment_entity->dtype},
            result->base.address);
    } else if(result->flags & RESOLVER_RESULT_FLAG_FIRST_ENTITY_LOAD_TO_EBX){
        if(root_assignment_entity->next && (root_assignment_entity->next->flags & RESOLVER_ENTITY_FLAG_IS_POINTER_ARRAY_ENTITY)){
            asm_push("mov ebx, [%s]", result->base.address);
        } else {
            asm_push("lea ebx, [%s]", result->base.address);
        }
        asm_push_ins_push_with_data("ebx",
            STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value", 0,
            &(struct stack_frame_data){.dtype = root_assignment_entity->dtype});
    }
}

// ch140: pop ebx, apply DO_INDIRECTION if requested, add the
// entity's offset, push ebx back.
static void codegen_generate_entity_access_for_variable_or_general(struct resolver_result* result, struct resolver_entity* entity){
    (void)result;
    asm_push_ins_pop("ebx", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
    if(entity->flags & RESOLVER_ENTITY_FLAG_DO_INDIRECTION){
        asm_push("mov ebx, [ebx]");
    }
    asm_push("add ebx, %i", entity->offset);
    asm_push_ins_push_with_data("ebx",
        STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value", 0,
        &(struct stack_frame_data){.dtype = entity->dtype});
}

static void codegen_generate_entity_access_for_function_call(struct resolver_result* result, struct resolver_entity* entity);

static void codegen_generate_entity_access_for_entity_for_assignment_left_operand(struct resolver_result* result, struct resolver_entity* entity, struct history* history){
    (void)history;
    switch(entity->type){
        case RESOLVER_ENTITY_TYPE_ARRAY_BRACKET:
            // todo: array bracket
            break;
        case RESOLVER_ENTITY_TYPE_VARIABLE:
        case RESOLVER_ENTITY_TYPE_GENERAL:
            codegen_generate_entity_access_for_variable_or_general(result, entity);
            break;
        case RESOLVER_ENTITY_TYPE_FUNCTION_CALL:
            // ch148
            codegen_generate_entity_access_for_function_call(result, entity);
            break;
        case RESOLVER_ENTITY_TYPE_UNARY_INDIRECTION:
            // todo: unary indirection
            break;
        case RESOLVER_ENTITY_TYPE_UNARY_GET_ADDRESS:
            // todo: unary get address
            break;
        case RESOLVER_ENTITY_TYPE_UNSUPPORTED:
            // todo: unsupported
            break;
        case RESOLVER_ENTITY_TYPE_CAST:
            // todo: cast
            break;
        default:
            compiler_error(current_process, "COMPILER BUG: unexpected entity type in assignment LHS\n");
    }
}

static void codegen_generate_entity_access_for_assignment_left_operand(struct resolver_result* result, struct resolver_entity* root_assignment_entity, struct node* top_most_node, struct history* history){
    (void)top_most_node;
    codegen_generate_entity_access_start(result, root_assignment_entity, history);
    struct resolver_entity* current = resolver_result_entity_next(root_assignment_entity);
    while(current){
        codegen_generate_entity_access_for_entity_for_assignment_left_operand(result, current, history);
        current = resolver_result_entity_next(current);
    }
}

// ch140: assignment LHS handler. Single-entity path: store directly
// into the resolved base address. Multi-entity path: compute the
// address into ebx via the access walker, pop the rhs (eax) and the
// addr (edx), then mov.
static void codegen_generate_assignment_part(struct node* node, const char* op, struct history* history){
    struct resolver_result* result = resolver_follow(current_process->resolver, node);
    assert(resolver_result_ok(result));
    struct resolver_entity* root_assignment_entity = resolver_result_entity_root(result);
    const char* reg_to_use = "eax";
    const char* mov_type   = codegen_byte_word_or_dword_or_ddword(datatype_element_size(&result->last_entity->dtype), &reg_to_use);
    struct resolver_entity* next_entity = resolver_result_entity_next(root_assignment_entity);
    if(!next_entity){
        if(datatype_is_struct_or_union_non_pointer(&result->last_entity->dtype)){
            // ch150: struct-by-value assignment - pop chunks and store.
            codegen_generate_move_struct(&result->last_entity->dtype, result->base.address, 0);
        } else {
            asm_push_ins_pop("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
            codegen_generate_assignment_instruction_for_operator(mov_type, result->base.address, reg_to_use, op,
                result->last_entity->dtype.flags & DATATYPE_FLAG_IS_SIGNED);
        }
    } else {
        codegen_generate_entity_access_for_assignment_left_operand(result, root_assignment_entity, node, history);
        asm_push_ins_pop("edx", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
        asm_push_ins_pop("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
        codegen_generate_assignment_instruction_for_operator(mov_type, "edx", reg_to_use, op,
            result->last_entity->flags & DATATYPE_FLAG_IS_SIGNED);
    }
}

static void codegen_generate_assignment_expression(struct node* node, struct history* history){
    codegen_generate_expressionable(node->exp.right,
        codegen_history_down(history, EXPRESSION_IS_ASSIGNMENT | IS_RIGHT_OPERAND_OF_ASSIGNMENT));
    codegen_generate_assignment_part(node->exp.left, node->exp.op, history);
}

// ch145 helpers (codegen_reduce_register, codegen_gen_mem_access,
// codegen_generate_variable_access, codegen_generate_identifier)
// live further down, after the codegen response system that they
// depend on.

// ch142: real `codegen_generate_exp_node` body lives below; this
// chapter's ch140 stub is gone.

// ch142: codegen response system. Recursive expression emit pushes a
// response slot before recursing; deeper levels acknowledge it with
// the resulting entity / pushed-struct info etc.
struct response_data {
    union {
        struct resolver_entity* resolved_entity;
    };
};

struct response {
    int                  flags;
    struct response_data data;
};

static void codegen_response_expect(void){
    struct response* res = calloc(1, sizeof(struct response));
    vector_push(current_process->generator->responses, &res);
}

static struct response* codegen_response_pull(void){
    struct response* res = vector_back_ptr_or_null(current_process->generator->responses);
    if(res){
        vector_pop(current_process->generator->responses);
    }
    return res;
}

static void codegen_response_acknowledge(struct response* response_in){
    struct response* res = vector_back_ptr_or_null(current_process->generator->responses);
    if(res){
        res->flags |= response_in->flags;
        if(response_in->data.resolved_entity){
            res->data.resolved_entity = response_in->data.resolved_entity;
        }
        res->flags |= RESPONSE_FLAG_ACKNOWLEDGED;
    }
}

static bool codegen_response_acknowledged(struct response* res){
    // Book ships `res->flags && FLAG_ACKNOWLEDGED` which checks both
    // truthy without the `&`. Replicated verbatim.
    return res && res->flags && RESPONSE_FLAG_ACKNOWLEDGED;
}

static bool codegen_response_has_entity(struct response* res){
    return codegen_response_acknowledged(res)
        && (res->flags & RESPONSE_FLAG_RESOLVED_ENTITY)
        && res->data.resolved_entity;
}

// ch145: load a sub-DWORD value through eax via movsx/movzx so the
// stack push always carries a full dword.
static void codegen_reduce_register(const char* reg, size_t size, bool is_signed){
    (void)reg;
    if(size != DATA_SIZE_DWORD){
        const char* ins = is_signed ? "movsx" : "movzx";
        asm_push("%s eax, %s", ins, codegen_sub_register("eax", size));
    }
}

// ch150: push the address of an entity (lea ebx + push ebx) tagged
// as IS_PUSHED_ADDRESS on the ledger.
static void codegen_gen_mem_access_get_address(struct node* node, int flags, struct resolver_entity* entity){
    (void)node; (void)flags;
    asm_push("lea ebx, [%s]", codegen_entity_private(entity)->address);
    asm_push_ins_push_with_flags("ebx",
        STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value",
        STACK_FRAME_ELEMENT_FLAG_IS_PUSHED_ADDRESS);
}

// ch145/150/151: emit the value-load for a variable / general entity.
// - EXPRESSION_GET_ADDRESS flag set: push the address, not the value.
// - struct/union value: push the address, pop ebx, push chunks.
// - DWORD primitive: push straight from memory.
// - smaller primitive: load through eax + movsx/movzx, then push.
static void codegen_gen_mem_access(struct node* node, int flags, struct resolver_entity* entity){
    (void)node;
    if(flags & EXPRESSION_GET_ADDRESS){
        codegen_gen_mem_access_get_address(node, flags, entity);
        return;
    }
    if(datatype_is_struct_or_union_non_pointer(&entity->dtype)){
        codegen_gen_mem_access_get_address(node, 0, entity);
        asm_push_ins_pop("ebx", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
        codegen_generate_structure_push_or_return(entity, codegen_history_begin(0), 0);
    } else if(datatype_element_size(&entity->dtype) != DATA_SIZE_DWORD){
        asm_push("mov eax, [%s]", codegen_entity_private(entity)->address);
        codegen_reduce_register("eax", datatype_element_size(&entity->dtype),
            entity->dtype.flags & DATATYPE_FLAG_IS_SIGNED);
        asm_push_ins_push_with_data("eax",
            STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value", 0,
            &(struct stack_frame_data){.dtype = entity->dtype});
    } else {
        asm_push_ins_push_with_data("dword [%s]",
            STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value", 0,
            &(struct stack_frame_data){.dtype = entity->dtype},
            codegen_entity_private(entity)->address);
    }
}

static void codegen_generate_variable_access_for_entity(struct node* node, struct resolver_entity* entity, struct history* history){
    codegen_gen_mem_access(node, history->flags, entity);
}

static void codegen_generate_variable_access(struct node* node, struct resolver_entity* entity, struct history* history){
    codegen_generate_variable_access_for_entity(node, entity, codegen_history_down(history, history->flags));
}

// ch151: forward decl for the resolver-driven value path used by
// the unary dispatch below.
static bool codegen_resolve_node_for_value(struct node* node, struct history* history);

// ch145: NODE_TYPE_IDENTIFIER read path. Resolve the name, emit a
// value-load, then acknowledge the response with the entity.
static void codegen_generate_identifier(struct node* node, struct history* history){
    struct resolver_result* result = resolver_follow(current_process->resolver, node);
    assert(resolver_result_ok(result));
    struct resolver_entity* entity = resolver_result_entity(result);
    codegen_generate_variable_access(node, entity, history);
    codegen_response_acknowledge(&(struct response){
        .flags = RESPONSE_FLAG_RESOLVED_ENTITY,
        .data.resolved_entity = entity,
    });
}

// ch151: `&x` - walk the operand with EXPRESSION_GET_ADDRESS so the
// mem-access path emits an address rather than a value; acknowledge
// UNARY_GET_ADDRESS upstream.
static void codegen_generate_unary_address(struct node* node, struct history* history){
    int flags = history->flags;
    codegen_generate_expressionable(node->unary.operand,
        codegen_history_down(history, flags | EXPRESSION_GET_ADDRESS));
    codegen_response_acknowledge(&(struct response){.flags = RESPONSE_FLAG_UNARY_GET_ADDRESS});
}

// ch151: NODE_TYPE_UNARY codegen dispatch. Indirection / normal
// unaries are stubs; only address-of fires today.
static void codegen_generate_unary(struct node* node, struct history* history){
    if(codegen_resolve_node_for_value(node, history)){
        return;
    }
    if(op_is_indirection(node->unary.op)){
        // todo: implement pointer indirection later.
        return;
    } else if(op_is_address(node->unary.op)){
        codegen_generate_unary_address(node, history);
        return;
    }
    // todo: generate normal unary (-, !, ~).
}

// ch142: asm stackframe peek helpers (current_function back / peek).
static struct stack_frame_element* asm_stack_back(void){
    return stackframe_back(current_function);
}

static bool asm_datatype_back(struct datatype* dtype_out){
    struct stack_frame_element* last = asm_stack_back();
    if(!last){ return false; }
    if(!(last->flags & STACK_FRAME_ELEMENT_FLAG_HAS_DATATYPE)){ return false; }
    *dtype_out = last->data.dtype;
    return true;
}

// ch142: read-side entity-access dispatch. Mirrors the assignment
// LHS version. Used by codegen_resolve_node_return_result so a bare
// `b` expression resolves through entity-access too.
static void codegen_generate_entity_access_for_entity(struct resolver_result* result, struct resolver_entity* entity, struct history* history){
    (void)history;
    switch(entity->type){
        case RESOLVER_ENTITY_TYPE_VARIABLE:
        case RESOLVER_ENTITY_TYPE_GENERAL:
            codegen_generate_entity_access_for_variable_or_general(result, entity);
            break;
        // ch148: function-call entity.
        case RESOLVER_ENTITY_TYPE_FUNCTION_CALL:
            codegen_generate_entity_access_for_function_call(result, entity);
            break;
        default:
            // todo: other entity kinds land in later chapters.
            break;
    }
}

static void codegen_generate_entity_access(struct resolver_result* result, struct resolver_entity* root_assignment_entity, struct node* top_most_node, struct history* history){
    (void)top_most_node;
    codegen_generate_entity_access_start(result, root_assignment_entity, history);
    struct resolver_entity* current = resolver_result_entity_next(root_assignment_entity);
    while(current){
        codegen_generate_entity_access_for_entity(result, current, history);
        current = resolver_result_entity_next(current);
    }
    // ch148: acknowledge the resolved last entity for the parent.
    codegen_response_acknowledge(&(struct response){
        .flags = RESPONSE_FLAG_RESOLVED_ENTITY,
        .data.resolved_entity = result->last_entity,
    });
}

// ch148: function call code. Iterate args last-to-first (cdecl push
// order), then `call ecx` (callee address sits in ecx), pop args
// off the stack, push the eax return value as our result.
static void codegen_generate_entity_access_for_function_call(struct resolver_result* result, struct resolver_entity* entity){
    (void)result;
    vector_set_flag(entity->func_call_data.arguments, VECTOR_FLAG_PEEK_DECREMENT);
    vector_set_peek_pointer_end(entity->func_call_data.arguments);
    struct node* node = vector_peek_ptr(entity->func_call_data.arguments);
    asm_push_ins_pop("ebx", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
    asm_push("mov ecx, ebx");
    if(datatype_is_struct_or_union_non_pointer(&entity->dtype)){
        // ch150: caller allocates the return slot; push a pointer to
        // it as the hidden first argument the callee will write to.
        asm_push("; SUBTRACT ROOM FOR RETURNED STRUCTURE/UNION DATATYPE");
        codegen_stack_sub_with_name(align_value(datatype_size(&entity->dtype), DATA_SIZE_DWORD), "result_value");
        asm_push_ins_push("esp", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
    }
    while(node){
        codegen_generate_expressionable(node, codegen_history_begin(EXPRESSION_IN_FUNCTION_CALL_ARGUMENTS));
        node = vector_peek_ptr(entity->func_call_data.arguments);
    }
    asm_push("call ecx");
    size_t stack_size = entity->func_call_data.stack_size;
    if(datatype_is_struct_or_union_non_pointer(&entity->dtype)){
        stack_size += DATA_SIZE_DWORD;
    }
    codegen_stack_add(stack_size);
    if(datatype_is_struct_or_union_non_pointer(&entity->dtype)){
        // ch150: copy the struct return value into a real push chain.
        asm_push("mov ebx, eax");
        codegen_generate_structure_push(entity, codegen_history_begin(0), 0);
    } else {
        asm_push_ins_push_with_data("eax",
            STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value", 0,
            &(struct stack_frame_data){.dtype = entity->dtype});
    }
}

// ch142: resolve `node` through the resolver, emit entity-access, and
// acknowledge the top response with the resolved last_entity. Returns
// false (without emitting) if the resolver fails.
static bool codegen_resolve_node_return_result(struct node* node, struct history* history, struct resolver_result** result_out){
    struct resolver_result* result = resolver_follow(current_process->resolver, node);
    if(resolver_result_ok(result)){
        struct resolver_entity* root = resolver_result_entity_root(result);
        codegen_generate_entity_access(result, root, node, history);
        if(result_out){ *result_out = result; }
        codegen_response_acknowledge(&(struct response){
            .flags = RESPONSE_FLAG_RESOLVED_ENTITY,
            .data.resolved_entity = result->last_entity,
        });
        return true;
    }
    return false;
}

static bool codegen_resolve_node_for_value(struct node* node, struct history* history){
    struct resolver_result* result = 0;
    if(!codegen_resolve_node_return_result(node, history, &result)){
        return false;
    }
    // ch153: peek the dtype off the top of the ledger and post-process
    // the pushed value:
    //   - struct/union value: push the structure proper.
    //   - non-pointer primitive: pop -> optional final indirection ->
    //     reduce-register (sign/zero extend) -> push as the typed
    //     result_value.
    struct datatype dtype;
    assert(asm_datatype_back(&dtype));
    if(datatype_is_struct_or_union_non_pointer(&dtype)){
        codegen_generate_structure_push(result->last_entity, history, 0);
    } else if(!(dtype.flags & DATATYPE_FLAG_IS_POINTER)){
        asm_push_ins_pop("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
        if(result->flags & RESOLVER_RESULT_FLAG_FINAL_INDIRECTION_REQUIRED_FOR_VALUE){
            asm_push("mov eax, [eax]");
        }
        codegen_reduce_register("eax", datatype_element_size(&dtype), dtype.flags & DATATYPE_FLAG_IS_SIGNED);
        asm_push_ins_push_with_data("eax",
            STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value", 0,
            &(struct stack_frame_data){.dtype = dtype});
    }
    return true;
}

// ch142: map an expression op string to its EXPRESSION_IS_* flag.
static int codegen_set_flag_for_operator(const char* op){
    int flag = 0;
    if(S_EQ(op, "+"))         flag |= EXPRESSION_IS_ADDITION;
    else if(S_EQ(op, "-"))    flag |= EXPRESSION_IS_SUBTRACTION;
    else if(S_EQ(op, "*"))    flag |= EXPRESSION_IS_MULTIPLICATION;
    else if(S_EQ(op, "/"))    flag |= EXPRESSION_IS_DIVISION;
    else if(S_EQ(op, "%"))    flag |= EXPRESSION_IS_MODULAS;
    // ch147: comparison + logical + bitwise.
    else if(S_EQ(op, ">"))    flag |= EXPRESSION_IS_ABOVE;
    else if(S_EQ(op, "<"))    flag |= EXPRESSION_IS_BELOW;
    else if(S_EQ(op, ">="))   flag |= EXPRESSION_IS_ABOVE_OR_EQUAL;
    else if(S_EQ(op, "<="))   flag |= EXPRESSION_IS_BELOW_OR_EQUAL;
    else if(S_EQ(op, "!="))   flag |= EXPRESSION_IS_NOT_EQUAL;
    else if(S_EQ(op, "=="))   flag |= EXPRESSION_IS_EQUAL;
    else if(S_EQ(op, "&&"))   flag |= EXPRESSION_LOGICAL_AND;
    else if(S_EQ(op, "<<"))   flag |= EXPRESSION_IS_BITSHIFT_LEFT;
    else if(S_EQ(op, ">>"))   flag |= EXPRESSION_IS_BITSHIFT_RIGHT;
    else if(S_EQ(op, "&"))    flag |= EXPRESSION_IS_BITWISE_AND;
    else if(S_EQ(op, "|"))    flag |= EXPRESSION_IS_BITWISE_OR;
    else if(S_EQ(op, "^"))    flag |= EXPRESSION_IS_BITWISE_XOR;
    return flag;
}

static bool codegen_can_gen_math(int flags){
    return flags & EXPRESSION_GEN_MATHABLE;
}

static int codegen_remove_uninheritable_flags(int flags){
    return flags & ~EXPRESSION_UNINHERITABLE_FLAGS;
}

static int get_additional_flags(int current_flags, struct node* node){
    if(node->type != NODE_TYPE_EXPRESSION){ return 0; }
    int extra = 0;
    bool keep_call_args = (current_flags & EXPRESSION_IN_FUNCTION_CALL_ARGUMENTS) && S_EQ(node->exp.op, ",");
    if(keep_call_args){
        extra |= EXPRESSION_IN_FUNCTION_CALL_ARGUMENTS;
    }
    return extra;
}

// ch147: emit a comparison: cmp eax, <value>; set<cc> al; movzx eax, al.
static void codegen_gen_cmp(const char* value, const char* set_ins){
    asm_push("cmp eax, %s", value);
    asm_push("%s al", set_ins);
    asm_push("movzx eax, al");
}

// ch142/147: emit the actual instruction for `reg op= value`.
// imul/idiv used for signed; mul/div for unsigned. ch147 adds
// comparison, bitshift, and bitwise paths.
static void codegen_gen_math_for_value(const char* reg, const char* value, int flags, bool is_signed){
    if(flags & EXPRESSION_IS_ADDITION){
        asm_push("add %s, %s", reg, value);
    } else if(flags & EXPRESSION_IS_SUBTRACTION){
        asm_push("sub %s, %s", reg, value);
    } else if(flags & EXPRESSION_IS_MULTIPLICATION){
        asm_push("mov ecx, %s", value);
        asm_push(is_signed ? "imul ecx" : "mul ecx");
    } else if(flags & EXPRESSION_IS_DIVISION){
        asm_push("mov ecx, %s", value);
        if(is_signed){ asm_push("cdq"); asm_push("idiv ecx"); }
        else         { asm_push("xor edx, edx"); asm_push("div ecx"); }
    } else if(flags & EXPRESSION_IS_MODULAS){
        asm_push("mov ecx, %s", value);
        asm_push("cdq");
        asm_push(is_signed ? "idiv ecx" : "div ecx");
        asm_push("mov eax, edx");
    } else if(flags & EXPRESSION_IS_ABOVE){
        codegen_gen_cmp(value, "setg");
    } else if(flags & EXPRESSION_IS_BELOW){
        codegen_gen_cmp(value, "setl");
    } else if(flags & EXPRESSION_IS_EQUAL){
        codegen_gen_cmp(value, "sete");
    } else if(flags & EXPRESSION_IS_ABOVE_OR_EQUAL){
        codegen_gen_cmp(value, "setge");
    } else if(flags & EXPRESSION_IS_BELOW_OR_EQUAL){
        codegen_gen_cmp(value, "setle");
    } else if(flags & EXPRESSION_IS_NOT_EQUAL){
        codegen_gen_cmp(value, "setne");
    } else if(flags & EXPRESSION_IS_BITSHIFT_LEFT){
        value = codegen_sub_register(value, DATA_SIZE_BYTE);
        asm_push("sal %s, %s", reg, value);
    } else if(flags & EXPRESSION_IS_BITSHIFT_RIGHT){
        value = codegen_sub_register(value, DATA_SIZE_BYTE);
        asm_push("sar %s, %s", reg, value);
    } else if(flags & EXPRESSION_IS_BITWISE_AND){
        asm_push("and %s, %s", reg, value);
    } else if(flags & EXPRESSION_IS_BITWISE_OR){
        asm_push("or %s, %s", reg, value);
    } else if(flags & EXPRESSION_IS_BITWISE_XOR){
        asm_push("xor %s, %s", reg, value);
    }
}

// ch147: short-circuit && / || codegen. Each new logical expression
// allocates an end label pair; nested logical ops share the same
// labels via the EXPRESSION_IN_LOGICAL_EXPRESSION flag.
static void codegen_setup_new_logical_expression(struct history* history, struct node* node){
    int label_index = codegen_label_count();
    sprintf(history->exp.logical_end_label,          ".endc_%i",          label_index);
    sprintf(history->exp.logical_end_label_positive, ".endc_%i_positive", label_index);
    history->exp.logical_start_op = node->exp.op;
    history->flags |= EXPRESSION_IN_LOGICAL_EXPRESSION;
}

static void codegen_generate_logical_cmp_and(const char* reg, const char* fail_label){
    asm_push("cmp %s, 0", reg);
    asm_push("je %s", fail_label);
}

static void codegen_generate_logical_cmp_or(const char* reg, const char* equal_label){
    asm_push("cmp %s, 0", reg);
    asm_push("jg %s", equal_label);
}

static void codegen_generate_logical_cmp(const char* op, const char* fail_label, const char* equal_label){
    if(S_EQ(op, "&&"))      codegen_generate_logical_cmp_and("eax", fail_label);
    else if(S_EQ(op, "||")) codegen_generate_logical_cmp_or("eax", equal_label);
}

static void codegen_generate_end_labels_for_logical_expression(const char* op, const char* end_label, const char* end_label_positive){
    if(S_EQ(op, "&&")){
        asm_push("; && END CLAUSE");
        asm_push("mov eax, 1");
        asm_push("jmp %s", end_label_positive);
        asm_push("%s:", end_label);
        asm_push("xor eax, eax");
        asm_push("%s:", end_label_positive);
    } else if(S_EQ(op, "||")){
        asm_push("; || END CLAUSE");
        asm_push("jmp %s", end_label);
        asm_push("%s:", end_label_positive);
        asm_push("mov eax, 1");
        asm_push("%s:", end_label);
    }
}

static void codegen_generate_exp_node_for_logical_arithmetic(struct node* node, struct history* history){
    bool start_of_logical = !(history->flags & EXPRESSION_IN_LOGICAL_EXPRESSION);
    if(start_of_logical){
        codegen_setup_new_logical_expression(history, node);
    }
    codegen_generate_expressionable(node->exp.left,
        codegen_history_down(history, history->flags | EXPRESSION_IN_LOGICAL_EXPRESSION));
    asm_push_ins_pop("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
    codegen_generate_logical_cmp(node->exp.op, history->exp.logical_end_label, history->exp.logical_end_label_positive);
    codegen_generate_expressionable(node->exp.right,
        codegen_history_down(history, history->flags | EXPRESSION_IN_LOGICAL_EXPRESSION));
    if(!is_logical_node(node->exp.right)){
        asm_push_ins_pop("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
        codegen_generate_logical_cmp(node->exp.op, history->exp.logical_end_label, history->exp.logical_end_label_positive);
        codegen_generate_end_labels_for_logical_expression(node->exp.op, history->exp.logical_end_label, history->exp.logical_end_label_positive);
        asm_push_ins_push("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
    }
}

// ch142: emit code for a binary arithmetic expression. Walk left,
// walk right (both pushed), pop right into ecx, pop left into eax,
// run the chosen op, push the result.
static void codegen_generate_exp_node_for_arithmetic(struct node* node, struct history* history){
    assert(node->type == NODE_TYPE_EXPRESSION);
    // ch147: short-circuit && / || routes to the logical path.
    if(is_logical_operator(node->exp.op)){
        codegen_generate_exp_node_for_logical_arithmetic(node, history);
        return;
    }
    int flags    = history->flags;
    int op_flags = codegen_set_flag_for_operator(node->exp.op);
    codegen_generate_expressionable(node->exp.left,  codegen_history_down(history, flags));
    codegen_generate_expressionable(node->exp.right, codegen_history_down(history, flags));
    struct datatype last_dtype = datatype_for_numeric();
    asm_datatype_back(&last_dtype);
    if(codegen_can_gen_math(op_flags)){
        // ch146: pull both side dtypes off the ledger (right is on top).
        struct datatype right_dtype = datatype_for_numeric();
        asm_datatype_back(&right_dtype);
        asm_push_ins_pop("ecx", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
        struct datatype left_dtype = datatype_for_numeric();
        asm_datatype_back(&left_dtype);
        asm_push_ins_pop("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
        // ch146: pointer arithmetic - scale the non-pointer side by
        // sizeof(*pointer) so `p + 1` advances one element. Byte
        // pointers need no scaling.
        struct datatype* pointer_dtype = datatype_thats_a_pointer(&left_dtype, &right_dtype);
        if(pointer_dtype && datatype_size(datatype_pointer_reduce(pointer_dtype, 1)) > DATA_SIZE_BYTE){
            const char* reg = (pointer_dtype == &right_dtype) ? "eax" : "ecx";
            asm_push("imul %s, %i", reg,
                (int)datatype_size(datatype_pointer_reduce(pointer_dtype, 1)));
        }
        codegen_gen_math_for_value("eax", "ecx", op_flags, last_dtype.flags & DATATYPE_FLAG_IS_SIGNED);
    }
    asm_push_ins_push_with_data("eax",
        STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value", 0,
        &(struct stack_frame_data){.dtype = last_dtype});
}

static void codegen_generate_exp_node(struct node* node, struct history* history){
    if(is_node_assignment(node)){
        codegen_generate_assignment_expression(node, history);
        return;
    }
    // Try resolving the node to a single entity (e.g. a bare
    // identifier) - if we do, the value's pushed and we're done.
    if(codegen_resolve_node_for_value(node, history)){
        return;
    }
    int extra = get_additional_flags(history->flags, node);
    codegen_generate_exp_node_for_arithmetic(node,
        codegen_history_down(history, codegen_remove_uninheritable_flags(history->flags) | extra));
}

// ch138 expressionable dispatch was NUMBER-only; ch142 adds the
// EXPRESSION case routed through codegen_generate_exp_node.

// ch150: format an offset string like "+12" or "-4" (no `+` for
// negatives because the minus already prints).
static void codegen_plus_or_minus_string_for_value(char* out, int val, size_t len){
    memset(out, 0, len);
    if(val < 0){
        sprintf(out, "%i", val);
    } else {
        sprintf(out, "+%i", val);
    }
}

// ch150: chunk a struct on the stack into DWORD-aligned pushes. We
// read the chunks from highest offset to lowest so they land on the
// stack in the canonical order callers expect.
static void codegen_generate_structure_push(struct resolver_entity* entity, struct history* history, int start_pos){
    (void)history;
    asm_push("; STRUCTURE PUSH");
    size_t structure_size = align_value(entity->dtype.size, DATA_SIZE_DWORD);
    int pushes = structure_size / DATA_SIZE_DWORD;
    for(int i = pushes - 1; i >= start_pos; i--){
        char fmt[10];
        int chunk_offset = (i * DATA_SIZE_DWORD);
        codegen_plus_or_minus_string_for_value(fmt, chunk_offset, sizeof(fmt));
        asm_push_ins_push_with_data("dword [%s%s]",
            STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value", 0,
            &(struct stack_frame_data){.dtype = entity->dtype}, "ebx", fmt);
    }
    asm_push("; END STRUCTURE PUSH");
    codegen_response_acknowledge(&(struct response){.flags = RESPONSE_FLAG_PUSHED_STRUCTURE});
}

static void codegen_generate_structure_push_or_return(struct resolver_entity* entity, struct history* history, int start_pos){
    codegen_generate_structure_push(entity, history, start_pos);
}

// ch150: pop struct chunks off the stack and write them into the
// destination via `mov [base+offset], eax`. Used for struct-value
// assignment (`s = t`).
static void codegen_generate_move_struct(struct datatype* dtype, const char* base_address, int offset){
    size_t structure_size = align_value(datatype_size(dtype), DATA_SIZE_DWORD);
    int pops = structure_size / DATA_SIZE_DWORD;
    for(int i = 0; i < pops; i++){
        asm_push_ins_pop("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
        char fmt[10];
        int chunk_offset = offset + (i * DATA_SIZE_DWORD);
        codegen_plus_or_minus_string_for_value(fmt, chunk_offset, sizeof(fmt));
        asm_push("mov [%s%s], eax", base_address, fmt);
    }
}

// ch148: after a statement runs, any "result_value" entries left on
// the ledger are values nothing's going to consume - bump esp past
// them so the frame stays balanced.
static struct stack_frame_element* asm_stack_peek(void){
    return stackframe_peek(current_function);
}
static void asm_stack_peek_start(void){
    stackframe_peek_start(current_function);
}

static void codegen_discard_unused_stack(void){
    asm_stack_peek_start();
    struct stack_frame_element* el = asm_stack_peek();
    size_t stack_adjustment = 0;
    while(el){
        if(!S_EQ(el->name, "result_value")){
            break;
        }
        stack_adjustment += DATA_SIZE_DWORD;
        el = asm_stack_peek();
    }
    codegen_stack_add(stack_adjustment);
}

static void codegen_generate_statement(struct node* node, struct history* history){
    (void)history;
    switch(node->type){
        case NODE_TYPE_EXPRESSION:
            codegen_generate_exp_node(node, codegen_history_begin(history->flags));
            break;
        case NODE_TYPE_VARIABLE:
            codegen_generate_scope_variable(node);
            break;
    }
    // ch148: drain leftover result_value pushes.
    codegen_discard_unused_stack();
}

static void codegen_generate_scope_no_new_scope(struct vector* statements, struct history* history){
    vector_set_peek_pointer(statements, 0);
    struct node* stmt = vector_peek_ptr(statements);
    while(stmt){
        codegen_generate_statement(stmt, history);
        stmt = vector_peek_ptr(statements);
    }
}

static void codegen_generate_stack_scope(struct vector* statements, size_t scope_size, struct history* history){
    (void)scope_size;
    codegen_new_scope(RESOLVER_SCOPE_FLAG_IS_STACK);
    codegen_generate_scope_no_new_scope(statements, history);
    codegen_finish_scope();
}

// ch137 stub -> ch138 real impl.
static void codegen_generate_body(struct node* node, struct history* history){
    codegen_generate_stack_scope(node->body.statements, node->body.size, history);
}

static void codegen_generate_function_with_body(struct node* node){
    codegen_register_function(node, 0);
    asm_push("global %s", node->func.name);
    asm_push("; %s function", node->func.name);
    asm_push("%s:", node->func.name);

    asm_push_ebp();
    asm_push("mov ebp, esp");
    codegen_stack_sub(C_ALIGN(function_node_stack_size(node)));
    codegen_new_scope(RESOLVER_DEFAULT_ENTITY_FLAG_IS_LOCAL_STACK);
    codegen_generate_function_arguments(function_node_argument_vec(node));

    codegen_generate_body(node->func.body_n, codegen_history_begin(IS_ALONE_STATEMENT));
    codegen_finish_scope();
    codegen_stack_add(C_ALIGN(function_node_stack_size(node)));
    asm_pop_ebp();
    stackframe_assert_empty(current_function);
    asm_push("ret");
}

static void codegen_generate_function(struct node* node){
    current_function = node;
    // ch137: each function gets a fresh stack-frame element vector.
    node->func.frame.elements = vector_create(sizeof(struct stack_frame_element));
    if(function_node_is_prototype(node)){
        codegen_generate_function_prototype(node);
        return;
    }
    codegen_generate_function_with_body(node);
}

// ch108: codegen "label" system. Each break / continue spans the most
// recent entry / exit point. We model entries (loop start, for goto
// continue) and exits (loop end, for break) as stacks of small
// records with an integer id; the asm label is `.entry_point_<id>`
// or `.exit_point_<id>`.

struct code_generator* codegenerator_new(struct compile_process* process){
    (void)process;
    struct code_generator* gen = calloc(1, sizeof(struct code_generator));
    // ch110: string table lives alongside entry/exit stacks.
    gen->string_table = vector_create(sizeof(struct string_table_element*));
    gen->entry_points = vector_create(sizeof(struct codegen_entry_point*));
    gen->exit_points  = vector_create(sizeof(struct codegen_exit_point*));
    // ch142: codegen response stack.
    gen->responses    = vector_create(sizeof(struct response*));
    return gen;
}

static int codegen_label_count(void){
    static int count = 0;
    count++;
    return count;
}

static void codegen_register_exit_point(int exit_point_id){
    struct code_generator* gen = current_process->generator;
    struct codegen_exit_point* ep = calloc(1, sizeof(struct codegen_exit_point));
    ep->id = exit_point_id;
    vector_push(gen->exit_points, &ep);
}

static struct codegen_exit_point* codegen_current_exit_point(void){
    struct code_generator* gen = current_process->generator;
    return vector_back_ptr_or_null(gen->exit_points);
}

static void codegen_begin_exit_point(void){
    codegen_register_exit_point(codegen_label_count());
}

static void codegen_end_exit_point(void){
    struct code_generator* gen = current_process->generator;
    struct codegen_exit_point* ep = codegen_current_exit_point();
    assert(ep);
    asm_push(".exit_point_%i:", ep->id);
    free(ep);
    vector_pop(gen->exit_points);
}

static void codegen_goto_exit_point(struct node* node){
    (void)node;
    struct codegen_exit_point* ep = codegen_current_exit_point();
    asm_push("jmp .exit_point_%i", ep->id);
}

static void codegen_register_entry_point(int entry_point_id){
    struct code_generator* gen = current_process->generator;
    struct codegen_entry_point* ep = calloc(1, sizeof(struct codegen_entry_point));
    ep->id = entry_point_id;
    vector_push(gen->entry_points, &ep);
}

static struct codegen_entry_point* codegen_current_entry_point(void){
    struct code_generator* gen = current_process->generator;
    return vector_back_ptr_or_null(gen->entry_points);
}

static void codegen_begin_entry_point(void){
    int id = codegen_label_count();
    codegen_register_entry_point(id);
    asm_push(".entry_point_%i:", id);
}

static void codegen_end_entry_point(void){
    struct code_generator* gen = current_process->generator;
    struct codegen_entry_point* ep = codegen_current_entry_point();
    assert(ep);
    free(ep);
    vector_pop(gen->entry_points);
}

static void codegen_goto_entry_point(struct node* current_node){
    (void)current_node;
    struct codegen_entry_point* ep = codegen_current_entry_point();
    asm_push("jmp .entry_point_%i", ep->id);
}

static void codegen_begin_entry_exit_point(void){
    codegen_begin_entry_point();
    codegen_begin_exit_point();
}

static void codegen_end_entry_exit_point(void){
    codegen_end_entry_point();
    codegen_end_exit_point();
}

// ch106: map a primitive's byte size to its NASM "db / dw / dd / dq"
// keyword. For non-primitive sizes we fall back to "times N db".
// tmp_buf is supplied by the caller so we don't return a static.
static const char* asm_keyword_for_size(size_t size, char* tmp_buf){
    const char* keyword = 0;
    switch(size){
        case DATA_SIZE_BYTE:   keyword = "db"; break;
        case DATA_SIZE_WORD:   keyword = "dw"; break;
        case DATA_SIZE_DWORD:  keyword = "dd"; break;
        case DATA_SIZE_DDWORD: keyword = "dq"; break;
        default:
            sprintf(tmp_buf, "times %lu db ", (unsigned long)size);
            return tmp_buf;
    }
    strcpy(tmp_buf, keyword);
    return tmp_buf;
}

static void codegen_generate_global_variable_for_primitive(struct node* node){
    char tmp_buf[256];
    if(node->var.val){
        if(node->var.val->type == NODE_TYPE_STRING){
            // ch112: register the literal in the string table and
            // emit the label as the variable's value (so the global
            // holds the address of the string in .rodata).
            const char* label = codegen_register_string(node->var.val->sval);
            asm_push("%s: %s %s",
                node->var.name,
                asm_keyword_for_size(variable_size(node), tmp_buf),
                label);
        } else {
            // ch111: emit the integer initializer literal.
            asm_push("%s: %s %lld",
                node->var.name,
                asm_keyword_for_size(variable_size(node), tmp_buf),
                node->var.val->llnum);
        }
    } else {
        asm_push("%s: %s 0",
            node->var.name,
            asm_keyword_for_size(variable_size(node), tmp_buf));
    }
}

// ch149: struct-typed global var. We don't support struct
// initializers yet, just emit zero-initialized bytes of the right
// total size.
static void codegen_generate_global_variable_for_struct(struct node* node){
    if(node->var.val){
        compiler_error(current_process, "We dont yet support values for structures");
        return;
    }
    char tmp_buf[256];
    asm_push("%s: %s 0", node->var.name,
        asm_keyword_for_size(variable_size(node), tmp_buf));
}

static void codegen_generate_global_variable(struct node* node){
    asm_push("; %s %s", node->var.type.type_str, node->var.name);
    switch(node->var.type.type){
        case DATA_TYPE_VOID:
        case DATA_TYPE_CHAR:
        case DATA_TYPE_SHORT:
        case DATA_TYPE_INTEGER:
        case DATA_TYPE_LONG:
            codegen_generate_global_variable_for_primitive(node);
            break;
        case DATA_TYPE_STRUCT:
            codegen_generate_global_variable_for_struct(node);
            break;
        case DATA_TYPE_DOUBLE:
        case DATA_TYPE_FLOAT:
            compiler_error(current_process, "Doubles and floats are not supported in our subset of C\n");
            break;
    }
}

// ch149: top-level struct definition with an attached variable
// (`struct foo { ... } v;`) emits the variable; the body itself was
// already captured by the parser.
static void codegen_generate_struct(struct node* node){
    if(node->flags & NODE_FLAG_HAS_VARIABLE_COMBINED){
        codegen_generate_global_variable(node->_struct.var);
    }
}

static void codegen_generate_data_section_part(struct node* node){
    switch(node->type){
        case NODE_TYPE_VARIABLE:
            codegen_generate_global_variable(node);
            break;
        // ch149: top-level struct (`struct foo {...} v;`) emits its
        // attached variable into .data.
        case NODE_TYPE_STRUCT:
            codegen_generate_struct(node);
            break;
        default:
            break;
    }
}

static void codegen_generate_data_section(void){
    asm_push("section .data");
    struct node* node = codegen_node_next();
    while(node){
        codegen_generate_data_section_part(node);
        node = codegen_node_next();
    }
}

static void codegen_generate_root_node(struct node* node){
    switch(node->type){
        case NODE_TYPE_VARIABLE:
            // Already emitted during the .data pass.
            break;
        case NODE_TYPE_FUNCTION:
            codegen_generate_function(node);
            break;
        default:
            break;
    }
}

static void codegen_generate_root(void){
    asm_push("section .text");
    struct node* node = 0;
    while((node = codegen_node_next()) != 0){
        codegen_generate_root_node(node);
    }
}

// ch110: write escape-sequence chars as their decimal ASCII value
// (`'\n'` -> `10`). Returns true if c was handled.
static bool codegen_write_string_char_escaped(char c){
    const char* c_out = 0;
    switch(c){
        case '\n': c_out = "10"; break;
        case '\t': c_out = "9";  break;
    }
    if(c_out){
        asm_push_no_nl("%s, ", c_out);
    }
    return c_out != 0;
}

static void codegen_write_string(struct string_table_element* element){
    asm_push_no_nl("%s: db ", element->label);
    size_t len = strlen(element->str);
    for(size_t i = 0; i < len; i++){
        char c = element->str[i];
        if(codegen_write_string_char_escaped(c)){
            continue;
        }
        asm_push_no_nl("'%c', ", c);
    }
    asm_push_no_nl("0");
    asm_push("");
}

static void codegen_write_strings(void){
    struct code_generator* gen = current_process->generator;
    vector_set_peek_pointer(gen->string_table, 0);
    struct string_table_element* element = vector_peek_ptr(gen->string_table);
    while(element){
        codegen_write_string(element);
        element = vector_peek_ptr(gen->string_table);
    }
}

// ch110: look up a registered string by content; returns its label or
// NULL if not yet seen.
static const char* codegen_get_label_for_string(const char* str){
    struct code_generator* gen = current_process->generator;
    vector_set_peek_pointer(gen->string_table, 0);
    struct string_table_element* cur = vector_peek_ptr(gen->string_table);
    while(cur){
        if(S_EQ(cur->str, str)){
            return cur->label;
        }
        cur = vector_peek_ptr(gen->string_table);
    }
    return 0;
}

static const char* codegen_register_string(const char* str){
    const char* label = codegen_get_label_for_string(str);
    if(label){
        return label;
    }
    struct string_table_element* el = calloc(1, sizeof(struct string_table_element));
    sprintf((char*)el->label, "str_%i", codegen_label_count());
    el->str = str;
    vector_push(current_process->generator->string_table, &el);
    return el->label;
}

static void codegen_generate_rod(void){
    asm_push("section .rodata");
    codegen_write_strings();
}

int codegen(struct compile_process* process){
    current_process = process;
    scope_create_root(process);

    vector_set_peek_pointer(process->node_tree_vec, 0);
    codegen_new_scope(0);
    codegen_generate_data_section();

    vector_set_peek_pointer(process->node_tree_vec, 0);
    codegen_generate_root();
    codegen_finish_scope();

    codegen_generate_rod();
    return CODEGEN_ALL_OK;
}
