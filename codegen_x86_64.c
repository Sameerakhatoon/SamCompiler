#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "compiler.h"
#include "helpers/vector.h"

// x86_64 backend. Built from codegen_x86.c and kept as close
// to it as possible: same NASM syntax, same stack machine, same
// ledger of every push and pop. What changes:
//   - registers are 64-bit: rax holds the value, rcx the right
//     operand, r11 the address (ebx on i386) and rdx the store
//     address. r11 is caller-saved, so unlike ebx it never needs
//     saving.
//   - every push is 8 bytes. int values sit in their slot sign
//     extended to 64 bits, so a slot can always be used as a long or
//     added to a pointer.
//   - int arithmetic runs on 32-bit registers (eax, ecx) so overflow
//     wraps like C expects, then movsxd puts the result back into
//     canonical 64-bit form. long and pointer arithmetic runs on the
//     full registers.
//   - arguments travel in rdi, rsi, rdx, rcx, r8, r9 (SysV). The
//     callee spills all six into a home area right above its frame
//     record, so parameter i is at [rbp+16+8*i], which is what the
//     parser's upward offsets already compute.
//   - globals use `default rel`, so [name] is rip-relative.

static struct compile_process* current_process  = 0;
static struct node*            current_function = 0;

static void codegen_x86_64_asm_push(const char* ins, ...);
static void codegen_x86_64_gen_exp(struct generator* generator, struct node* node, int flags);
static void codegen_x86_64_end_exp(struct generator* generator);
static void codegen_x86_64_entity_address(struct generator* generator, struct resolver_entity* entity, struct generator_entity_address* address_out);
static void codegen_x86_64_ret(struct datatype* dtype, const char* fmt, ...);
static void codegen_x86_64_va_start_store(struct generator* generator, const char* list_address);
static void codegen_x86_64_va_arg_fetch(struct generator* generator, int size);

// Argument registers in SysV order.
static const char* codegen_arg_registers[] = { "rdi", "rsi", "rdx", "rcx", "r8", "r9" };

struct history;
static struct _x86_64_generator_private {
    struct x86_64_generator_remembered {
        struct history* history;
    } remembered;
} _x86_64_generator_private;

static struct generator x86_64_codegen = {
    .asm_push       = codegen_x86_64_asm_push,
    .gen_exp        = codegen_x86_64_gen_exp,
    .end_exp        = codegen_x86_64_end_exp,
    .entity_address = codegen_x86_64_entity_address,
    .ret            = codegen_x86_64_ret,
    .va_start_store = codegen_x86_64_va_start_store,
    .va_arg_fetch   = codegen_x86_64_va_arg_fetch,
    .private        = &_x86_64_generator_private,
};

enum {
    CODEGEN_ENTITY_RULE_IS_STRUCT_OR_UNION_NON_POINTER = 0b00000001,
    CODEGEN_ENTITY_RULE_IS_FUNCTION_CALL               = 0b00000010,
    CODEGEN_ENTITY_RULE_IS_GET_ADDRESS                 = 0b00000100,
    CODEGEN_ENTITY_RULE_WILL_PEEK_AT_EBX               = 0b00001000,
};

struct history_exp {
    const char* logical_start_op;
    char        logical_end_label[20];
    char        logical_end_label_positive[20];
};

struct history {
    int flags;
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

static void asm_push_rbp(void){
    asm_push_ins_push("rbp", STACK_FRAME_ELEMENT_TYPE_SAVED_BP, "function_entry_saved_ebp");
}

static void asm_pop_rbp(void){
    asm_push_ins_pop("rbp", STACK_FRAME_ELEMENT_TYPE_SAVED_BP, "function_entry_saved_ebp");
}

static void codegen_data_section_add(const char* data, ...){
    va_list args;
    va_start(args, data);
    char* new_data = malloc(256);
    vsprintf(new_data, data, args);
    va_end(args);
    vector_push(current_process->generator->custom_data_section, &new_data);
}

static int asm_push_ins_pop_or_ignore(const char* fmt, int expecting_stack_entity_type, const char* expecting_stack_entity_name, ...){
    if(!stackframe_back_expect(current_function, expecting_stack_entity_type, expecting_stack_entity_name)){
        return STACK_FRAME_ELEMENT_FLAG_ELEMENT_NOT_FOUND;
    }
    char tmp_buf[200];
    sprintf(tmp_buf, "pop %s", fmt);
    va_list args;
    va_start(args, expecting_stack_entity_name);
    asm_push_args(tmp_buf, args);
    va_end(args);
    struct stack_frame_element* el = stackframe_back(current_function);
    int flags = el->flags;
    stackframe_pop_expecting(current_function, expecting_stack_entity_type, expecting_stack_entity_name);
    return flags;
}

static void codegen_stack_add_no_compile_time_stack_frame_restore(size_t stack_size){
    if(stack_size != 0){
        asm_push("add rsp, %lu", (unsigned long)stack_size);
    }
}

static void codegen_stack_sub_with_name(size_t stack_size, const char* name){
    if(stack_size != 0){
        stackframe_sub(current_function, STACK_FRAME_ELEMENT_TYPE_UNKNOWN, name, stack_size);
        asm_push("sub rsp, %lu", (unsigned long)stack_size);
    }
}

static void codegen_stack_sub(size_t stack_size){
    codegen_stack_sub_with_name(stack_size, "literal_stack_change");
}

static void codegen_stack_add_with_name(size_t stack_size, const char* name){
    if(stack_size != 0){
        stackframe_add(current_function, STACK_FRAME_ELEMENT_TYPE_UNKNOWN, name, stack_size);
        asm_push("add rsp, %lu", (unsigned long)stack_size);
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
    // A prototype of a function defined further down is not external;
    // NASM rejects a symbol that is both extern and defined.
    if(!function_defined_in_tree(current_process, node->func.name)){
        asm_push("extern %s", node->func.name);
    }
}

static void codegen_generate_function_arguments(struct vector* argument_vector){
    vector_set_peek_pointer(argument_vector, 0);
    struct node* current = vector_peek_ptr(argument_vector);
    while(current){
        codegen_new_scope_entity(current, current->var.aoffset, RESOLVER_DEFAULT_ENTITY_FLAG_IS_LOCAL_STACK);
        current = vector_peek_ptr(argument_vector);
    }
}

// `push qword imm` sign-extends a 32-bit immediate, which is exactly
// the canonical form of an int. A constant whose canonical value does
// not fit one (a long such as 5000000000, an unsigned int above
// 0x7fffffff) goes through rax; the old `(int)` cast cut it to 32 bits.
static void codegen_generate_number_node(struct node* node, struct history* history){
    (void)history;
    struct datatype dtype = datatype_for_number_node(node);
    long long value = datatype_canonical_value(node->llnum, &dtype);
    if(value >= INT32_MIN && value <= INT32_MAX){
        asm_push_ins_push_with_data("qword %lld",
            STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value",
            STACK_FRAME_ELEMENT_FLAG_IS_NUMERICAL,
            &(struct stack_frame_data){.dtype = dtype}, value);
        return;
    }
    asm_push("mov rax, %lld", value);
    asm_push_ins_push_with_data("rax",
        STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value",
        STACK_FRAME_ELEMENT_FLAG_IS_NUMERICAL,
        &(struct stack_frame_data){.dtype = dtype});
}

static bool codegen_is_exp_root_for_flags(int flags){
    return !(flags & EXPRESSION_IS_NOT_ROOT_NODE);
}

static bool codegen_is_exp_root(struct history* history){
    return codegen_is_exp_root_for_flags(history->flags);
}

static void codegen_generate_exp_node(struct node* node, struct history* history);
static void codegen_generate_identifier(struct node* node, struct history* history);
static void codegen_generate_string(struct node* node, struct history* history);
static void codegen_generate_exp_parenthesis_node(struct node* node, struct history* history);
static int  codegen_remove_uninheritable_flags(int flags);
static void codegen_generate_tenary(struct node* node, struct history* history);
static void codegen_generate_cast(struct node* node, struct history* history);
static int  codegen_label_count(void);
static void codegen_generate_unary(struct node* node, struct history* history);
static bool asm_datatype_back(struct datatype* dtype_out);
static void codegen_generate_body(struct node* node, struct history* history);
static void codegen_begin_entry_exit_point(void);
static void codegen_end_entry_exit_point(void);
static void codegen_goto_exit_point(struct node* node);
static void codegen_goto_exit_point_maintain_stack(struct node* node);
static void codegen_begin_switch_statement(void);
static void codegen_end_switch_statement(void);
static int  codegen_switch_id(void);
static void codegen_begin_case_statement(int index);
static void codegen_end_case_statement(void);
static void codegen_generate_switch_default_stmt(struct node* node);
static void codegen_goto_entry_point(struct node* node);
static void codegen_generate_structure_push_or_return(struct resolver_entity* entity, struct history* history, int start_pos);
static void codegen_generate_structure_push(struct resolver_entity* entity, struct history* history, int start_pos);
static void codegen_generate_move_struct(struct datatype* dtype, const char* base_address, int offset);

static void codegen_generate_expressionable(struct node* node, struct history* history){
    if(codegen_is_exp_root(history)){
        history->flags |= EXPRESSION_IS_NOT_ROOT_NODE;
    }
    switch(node->type){
        case NODE_TYPE_NUMBER:
            codegen_generate_number_node(node, history);
            break;
        case NODE_TYPE_STRING:
            codegen_generate_string(node, history);
            break;
        case NODE_TYPE_IDENTIFIER:
            codegen_generate_identifier(node, history);
            break;
        case NODE_TYPE_EXPRESSION:
            codegen_generate_exp_node(node, history);
            break;
        case NODE_TYPE_EXPRESSION_PARENTHESES:
            codegen_generate_exp_parenthesis_node(node, history);
            break;
        case NODE_TYPE_UNARY:
            codegen_generate_unary(node, history);
            break;
        case NODE_TYPE_TENARY:
            codegen_generate_tenary(node, history);
            break;
        case NODE_TYPE_CAST:
            codegen_generate_cast(node, history);
            break;
    }
}

// Byte / word / dword / qword names of the four registers the stack
// machine uses.
static const char* codegen_sub_register(const char* original_register, size_t size){
    static const char* names[][5] = {
        { "rax", "al",   "ax",   "eax",  "rax" },
        { "rcx", "cl",   "cx",   "ecx",  "rcx" },
        { "rdx", "dl",   "dx",   "edx",  "rdx" },
        { "r11", "r11b", "r11w", "r11d", "r11" },
    };
    int column = 0;
    switch(size){
        case DATA_SIZE_BYTE:   column = 1; break;
        case DATA_SIZE_WORD:   column = 2; break;
        case DATA_SIZE_DWORD:  column = 3; break;
        case DATA_SIZE_DDWORD: column = 4; break;
        default: return 0;
    }
    for(size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++){
        if(S_EQ(original_register, names[i][0])){
            return names[i][column];
        }
    }
    return 0;
}

static const char* codegen_byte_word_or_dword_or_ddword(size_t size, const char** reg_to_use){
    const char* type = 0;
    if(size == DATA_SIZE_BYTE)        type = "byte";
    else if(size == DATA_SIZE_WORD)   type = "word";
    else if(size == DATA_SIZE_DWORD)  type = "dword";
    else if(size == DATA_SIZE_DDWORD) type = "qword";
    *reg_to_use = codegen_sub_register(*reg_to_use, size);
    return type;
}

// Load `size` bytes at [address] into the 64-bit register `reg`,
// sign or zero extended. Every value on the expression stack is a
// full 64-bit quantity, so narrower loads always widen here.
static void codegen_load_register(const char* reg, const char* address, size_t size, bool is_signed){
    switch(size){
        case DATA_SIZE_BYTE:
            if(is_signed) asm_push("movsx %s, byte [%s]", reg, address);
            else          asm_push("movzx %s, byte [%s]", codegen_sub_register(reg, DATA_SIZE_DWORD), address);
            break;
        case DATA_SIZE_WORD:
            if(is_signed) asm_push("movsx %s, word [%s]", reg, address);
            else          asm_push("movzx %s, word [%s]", codegen_sub_register(reg, DATA_SIZE_DWORD), address);
            break;
        case DATA_SIZE_DWORD:
            // A 32-bit mov zero-fills the top half by itself.
            if(is_signed) asm_push("movsxd %s, dword [%s]", reg, address);
            else          asm_push("mov %s, dword [%s]", codegen_sub_register(reg, DATA_SIZE_DWORD), address);
            break;
        default:
            asm_push("mov %s, qword [%s]", reg, address);
            break;
    }
}

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

static void codegen_x86_64_entity_address(struct generator* generator, struct resolver_entity* entity, struct generator_entity_address* address_out){
    (void)generator;
    struct resolver_default_entity_data* data = codegen_entity_private(entity);
    address_out->address      = data->address;
    address_out->base_address = data->base_address;
    address_out->is_stack     = data->flags & RESOLVER_DEFAULT_ENTITY_FLAG_IS_LOCAL_STACK;
    address_out->offset       = data->offset;
}

static void codegen_generate_scope_variable(struct node* node){
    struct resolver_entity* entity = codegen_new_scope_entity(node, node->var.aoffset, RESOLVER_DEFAULT_ENTITY_FLAG_IS_LOCAL_STACK);
    if(node->var.val){
        codegen_generate_expressionable(node->var.val,
            codegen_history_begin(EXPRESSION_IS_ASSIGNMENT | IS_RIGHT_OPERAND_OF_ASSIGNMENT));
        asm_push_ins_pop("rax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
        const char* reg_to_use = "rax";
        const char* mov_type   = codegen_byte_word_or_dword_or_ddword(datatype_element_size(&entity->dtype), &reg_to_use);
        codegen_generate_assignment_instruction_for_operator(mov_type, codegen_entity_private(entity)->address, reg_to_use, "=",
            entity->dtype.flags & DATATYPE_FLAG_IS_SIGNED);
    }
}

static void codegen_generate_entity_access_start(struct resolver_result* result, struct resolver_entity* root_assignment_entity, struct history* history){
    if(root_assignment_entity->type == RESOLVER_ENTITY_TYPE_UNSUPPORTED){
        codegen_generate_expressionable(root_assignment_entity->node, history);
    } else if(result->flags & RESOLVER_RESULT_FLAG_FIRST_ENTITY_PUSH_VALUE){
        // i386 pushes the dword straight from memory; here the value
        // is loaded at its real width so it lands sign extended.
        codegen_load_register("rax", result->base.address, datatype_element_size(&root_assignment_entity->dtype),
            root_assignment_entity->dtype.flags & DATATYPE_FLAG_IS_SIGNED);
        asm_push_ins_push_with_data("rax",
            STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value", 0,
            &(struct stack_frame_data){.dtype = root_assignment_entity->dtype});
    } else if(result->flags & RESOLVER_RESULT_FLAG_FIRST_ENTITY_LOAD_TO_EBX){
        if(root_assignment_entity->next && (root_assignment_entity->next->flags & RESOLVER_ENTITY_FLAG_IS_POINTER_ARRAY_ENTITY)){
            asm_push("mov r11, [%s]", result->base.address);
        } else {
            asm_push("lea r11, [%s]", result->base.address);
        }
        asm_push_ins_push_with_data("r11",
            STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value", 0,
            &(struct stack_frame_data){.dtype = root_assignment_entity->dtype});
    }
}

static void codegen_generate_entity_access_for_variable_or_general(struct resolver_result* result, struct resolver_entity* entity){
    (void)result;
    asm_push_ins_pop("r11", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
    if(entity->flags & RESOLVER_ENTITY_FLAG_DO_INDIRECTION){
        asm_push("mov r11, [r11]");
    }
    asm_push("add r11, %i", entity->offset);
    asm_push_ins_push_with_data("r11",
        STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value", 0,
        &(struct stack_frame_data){.dtype = entity->dtype});
}

static void codegen_generate_entity_access_for_function_call(struct resolver_result* result, struct resolver_entity* entity);
static void codegen_generate_entity_access_for_unary_indirection_for_assignment_left_operand(struct resolver_result* result, struct resolver_entity* entity, struct history* history);
static void codegen_generate_entity_access_for_unary_get_address(struct resolver_result* result, struct resolver_entity* entity);
static void codegen_generate_entity_access_for_unsupported(struct resolver_result* result, struct resolver_entity* entity);
static void codegen_generate_entity_access_for_cast(struct resolver_result* result, struct resolver_entity* entity);
static void codegen_generate_entity_access_array_bracket(struct resolver_result* result, struct resolver_entity* entity);

static int codegen_entity_rules(struct resolver_entity* last_entity, struct history* history){
    int rule_flags = 0;
    if(!last_entity){ return 0; }
    if(datatype_is_struct_or_union_non_pointer(&last_entity->dtype)){
        rule_flags |= CODEGEN_ENTITY_RULE_IS_STRUCT_OR_UNION_NON_POINTER;
    }
    if(last_entity->type == RESOLVER_ENTITY_TYPE_FUNCTION_CALL){
        rule_flags |= CODEGEN_ENTITY_RULE_IS_FUNCTION_CALL;
    } else if(history->flags & EXPRESSION_GET_ADDRESS){
        rule_flags |= CODEGEN_ENTITY_RULE_IS_GET_ADDRESS;
    } else if(last_entity->type == RESOLVER_ENTITY_TYPE_UNARY_GET_ADDRESS){
        rule_flags |= CODEGEN_ENTITY_RULE_IS_GET_ADDRESS;
    } else {
        rule_flags |= CODEGEN_ENTITY_RULE_WILL_PEEK_AT_EBX;
    }
    return rule_flags;
}

static void codegen_apply_unary_access(int depth){
    for(int i = 0; i < depth; i++){
        asm_push("mov r11, [r11]");
    }
}

static void codegen_generate_entity_access_for_entity_for_assignment_left_operand(struct resolver_result* result, struct resolver_entity* entity, struct history* history){
    (void)history;
    switch(entity->type){
        case RESOLVER_ENTITY_TYPE_ARRAY_BRACKET:
            codegen_generate_entity_access_array_bracket(result, entity);
            break;
        case RESOLVER_ENTITY_TYPE_VARIABLE:
        case RESOLVER_ENTITY_TYPE_GENERAL:
            codegen_generate_entity_access_for_variable_or_general(result, entity);
            break;
        case RESOLVER_ENTITY_TYPE_FUNCTION_CALL:
            codegen_generate_entity_access_for_function_call(result, entity);
            break;
        case RESOLVER_ENTITY_TYPE_UNARY_INDIRECTION:
            codegen_generate_entity_access_for_unary_indirection_for_assignment_left_operand(result, entity, history);
            break;
        case RESOLVER_ENTITY_TYPE_UNARY_GET_ADDRESS:
            codegen_generate_entity_access_for_unary_get_address(result, entity);
            break;
        case RESOLVER_ENTITY_TYPE_UNSUPPORTED:
            codegen_generate_entity_access_for_unsupported(result, entity);
            break;
        case RESOLVER_ENTITY_TYPE_CAST:
            codegen_generate_entity_access_for_cast(result, entity);
            break;
        default:
            compiler_error(current_process, "COMPILER BUG: unexpected entity type in assignment LHS\n");
    }
}

static void codegen_generate_entity_access_for_unsupported(struct resolver_result* result, struct resolver_entity* entity){
    (void)result;
    codegen_generate_expressionable(entity->node, codegen_history_begin(0));
}

static void codegen_generate_entity_access_for_cast(struct resolver_result* result, struct resolver_entity* entity){
    (void)result; (void)entity;
    asm_push("; CAST");
}

static void codegen_generate_entity_access_array_bracket_pointer(struct resolver_result* result, struct resolver_entity* entity){
    (void)result;
    asm_push_ins_pop("r11", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
    codegen_generate_expressionable(entity->array.array_index_node, codegen_history_begin(0));
    asm_push_ins_pop("rax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
    if(datatype_element_size(&entity->dtype) > DATA_SIZE_BYTE){
        asm_push("imul rax, %i", (int)datatype_size_for_array_access(&entity->dtype));
    }
    asm_push("add r11, rax");
    asm_push_ins_push_with_data("r11",
        STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value", 0,
        &(struct stack_frame_data){.dtype = entity->dtype});
}

static void codegen_generate_entity_access_array_bracket(struct resolver_result* result, struct resolver_entity* entity){
    if(entity->flags & RESOLVER_ENTITY_FLAG_IS_POINTER_ARRAY_ENTITY){
        codegen_generate_entity_access_array_bracket_pointer(result, entity);
        return;
    }
    asm_push_ins_pop("r11", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
    codegen_generate_expressionable(entity->array.array_index_node, codegen_history_begin(0));
    asm_push_ins_pop("rax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
    if(entity->flags & RESOLVER_ENTITY_FLAG_JUST_USE_OFFSET){
        asm_push("add r11, %i", entity->offset);
    } else {
        asm_push("imul rax, %i", entity->offset);
        asm_push("add r11, rax");
    }
    asm_push_ins_push_with_data("r11",
        STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value", 0,
        &(struct stack_frame_data){.dtype = entity->dtype});
}

static void codegen_generate_entity_access_for_unary_indirection_for_assignment_left_operand(struct resolver_result* result, struct resolver_entity* entity, struct history* history){
    asm_push("; INDIRECTION");
    int flags = asm_push_ins_pop("r11", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
    (void)flags;
    int gen_entity_rules = codegen_entity_rules(result->last_entity, history);
    (void)gen_entity_rules;
    // A member read through `->` leaves the member's address, not its
    // value, so `*w->p = 9` must follow one more level.
    int depth = entity->indirection.depth - 1;
    if(result->flags & RESOLVER_RESULT_FLAG_FINAL_INDIRECTION_REQUIRED_FOR_VALUE){
        depth++;
    }
    codegen_apply_unary_access(depth);
    asm_push_ins_push_with_flags("r11",
        STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value",
        STACK_FRAME_ELEMENT_FLAG_IS_PUSHED_ADDRESS);
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

static bool codegen_is_wide(struct datatype* dtype);
static int  codegen_set_flag_for_operator(const char* op);
static void codegen_gen_math_for_value(int flags, bool wide, bool is_signed);
static void codegen_reduce_register(size_t size, bool is_signed);

enum {
    // Push the assignment's value: it is an operand, as in `a = b = 3`.
    CODEGEN_ASSIGN_VALUE_NEEDED = 0b001,
    // `x++` / `x--`: the value is the operand before the step.
    CODEGEN_ASSIGN_POSTFIX      = 0b010,
    // ++ / --: there is no right operand on the stack, the step is 1.
    CODEGEN_ASSIGN_STEP_ONE     = 0b100,
};

// Push rax as the value of an assignment, narrowed to the stored type
// (the value of `c = 300` with a char c is 44).
static void codegen_push_assignment_value(struct datatype* dtype){
    codegen_reduce_register(datatype_element_size(dtype), dtype->flags & DATATYPE_FLAG_IS_SIGNED);
    asm_push_ins_push_with_data("rax",
        STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value", 0,
        &(struct stack_frame_data){.dtype = *dtype});
}

// `lhs = value` where the value is already on the expression stack.
// The instructions for a plain store are the ones this backend has
// always emitted; the value push is new and only happens when the
// assignment is itself an operand.
static void codegen_generate_plain_assignment(struct node* node, struct resolver_result* result, struct history* history, int assign_flags){
    struct resolver_entity* root_assignment_entity = resolver_result_entity_root(result);
    struct datatype* dtype = &result->last_entity->dtype;
    const char* reg_to_use = "rax";
    const char* mov_type   = codegen_byte_word_or_dword_or_ddword(datatype_element_size(dtype), &reg_to_use);
    bool is_struct = datatype_is_struct_or_union_non_pointer(dtype);
    if(is_struct && (assign_flags & CODEGEN_ASSIGN_VALUE_NEEDED)){
        compiler_error(current_process, "x86_64: the value of a struct assignment cannot be used\n");
    }
    if(!resolver_result_entity_next(root_assignment_entity)){
        if(is_struct){
            codegen_generate_move_struct(dtype, result->base.address, 0);
            return;
        }
        asm_push_ins_pop("rax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
        asm_push("mov %s [%s], %s", mov_type, result->base.address, reg_to_use);
    } else {
        codegen_generate_entity_access_for_assignment_left_operand(result, root_assignment_entity, node, history);
        asm_push_ins_pop("rdx", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
        asm_push_ins_pop("rax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
        if(is_struct){
            compiler_error(current_process, "x86_64: struct assignment through a pointer or member is not supported\n");
        }
        asm_push("mov %s [rdx], %s", mov_type, reg_to_use);
    }
    if(assign_flags & CODEGEN_ASSIGN_VALUE_NEEDED){
        codegen_push_assignment_value(dtype);
    }
}

// `lhs op= value`, `++lhs`, `lhs++`: load, compute, store. The address
// of a non-simple lhs is kept in r10 because the arithmetic can use
// rdx (division) and rcx (the right operand, the shift count).
static void codegen_generate_read_modify_write(struct node* node, struct resolver_result* result, const char* op, struct history* history, int assign_flags){
    struct resolver_entity* root_assignment_entity = resolver_result_entity_root(result);
    struct datatype dtype = result->last_entity->dtype;
    if(datatype_is_struct_or_union_non_pointer(&dtype)){
        compiler_error(current_process, "x86_64: %s on a struct is not valid\n", op);
    }
    struct datatype value_dtype = datatype_for_numeric();
    if(!(assign_flags & CODEGEN_ASSIGN_STEP_ONE)){
        asm_datatype_back(&value_dtype);
    }

    const char* address = result->base.address;
    if(resolver_result_entity_next(root_assignment_entity)){
        codegen_generate_entity_access_for_assignment_left_operand(result, root_assignment_entity, node, history);
        asm_push_ins_pop("r10", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
        address = "r10";
    }
    if(assign_flags & CODEGEN_ASSIGN_STEP_ONE){
        asm_push("mov ecx, 1");
    } else {
        asm_push_ins_pop("rcx", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
    }

    size_t size = datatype_element_size(&dtype);
    bool lhs_signed = dtype.flags & DATATYPE_FLAG_IS_SIGNED;
    codegen_load_register("rax", address, size, lhs_signed);
    if(assign_flags & CODEGEN_ASSIGN_POSTFIX){
        asm_push_ins_push_with_data("rax",
            STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value", 0,
            &(struct stack_frame_data){.dtype = dtype});
    }

    // `a op= b` computes `a op b` like the binary operator (in the
    // converted type, a pointer step scaled) and stores it at a's width.
    const char* arith = assignment_arithmetic_operator(op);
    int op_flags = codegen_set_flag_for_operator(arith);
    struct datatype_binary types;
    datatype_binary_types(&dtype, &value_dtype, arith, &types);
    if(types.scale_right > 1){
        asm_push("imul rcx, %zu", types.scale_right);
    }
    codegen_gen_math_for_value(op_flags, codegen_is_wide(&types.operation),
        types.operation.flags & DATATYPE_FLAG_IS_SIGNED);

    const char* reg_to_use = "rax";
    const char* mov_type   = codegen_byte_word_or_dword_or_ddword(size, &reg_to_use);
    asm_push("mov %s [%s], %s", mov_type, address, reg_to_use);
    if((assign_flags & CODEGEN_ASSIGN_VALUE_NEEDED) && !(assign_flags & CODEGEN_ASSIGN_POSTFIX)){
        codegen_push_assignment_value(&dtype);
    }
}

static void codegen_generate_assignment_part(struct node* node, const char* op, struct history* history, int assign_flags){
    struct resolver_result* result = resolver_follow(current_process->resolver, node);
    if(!resolver_result_ok(result)){
        compiler_error(current_process, "The left operand of %s cannot be assigned to\n", op);
    }
    if(S_EQ(op, "=")){
        codegen_generate_plain_assignment(node, result, history, assign_flags);
        return;
    }
    // A statement `x += n` on a non-pointer keeps its single instruction.
    struct resolver_entity* root_assignment_entity = resolver_result_entity_root(result);
    struct datatype* dtype = &result->last_entity->dtype;
    if(S_EQ(op, "+=") && !(assign_flags & CODEGEN_ASSIGN_VALUE_NEEDED)
       && !(dtype->flags & DATATYPE_FLAG_IS_POINTER)
       && !datatype_is_struct_or_union_non_pointer(dtype)){
        const char* reg_to_use = "rax";
        const char* mov_type   = codegen_byte_word_or_dword_or_ddword(datatype_element_size(dtype), &reg_to_use);
        if(!resolver_result_entity_next(root_assignment_entity)){
            asm_push_ins_pop("rax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
            asm_push("add %s [%s], %s", mov_type, result->base.address, reg_to_use);
        } else {
            codegen_generate_entity_access_for_assignment_left_operand(result, root_assignment_entity, node, history);
            asm_push_ins_pop("rdx", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
            asm_push_ins_pop("rax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
            asm_push("add %s [rdx], %s", mov_type, reg_to_use);
        }
        return;
    }
    codegen_generate_read_modify_write(node, result, op, history, assign_flags);
}

// An assignment that is a whole statement (or a for-loop init / step)
// pushes nothing; one that is an operand pushes its value.
static void codegen_generate_assignment_expression(struct node* node, struct history* history){
    codegen_generate_expressionable(node->exp.right,
        codegen_history_down(history, EXPRESSION_IS_ASSIGNMENT | IS_RIGHT_OPERAND_OF_ASSIGNMENT));
    codegen_generate_assignment_part(node->exp.left, node->exp.op, history,
        codegen_is_exp_root(history) ? 0 : CODEGEN_ASSIGN_VALUE_NEEDED);
}

static void codegen_generate_increment(struct node* node, struct history* history){
    int assign_flags = CODEGEN_ASSIGN_STEP_ONE;
    if(!codegen_is_exp_root(history)){
        assign_flags |= CODEGEN_ASSIGN_VALUE_NEEDED;
        if(node->unary.flags & UNARY_FLAG_IS_POSTFIX){
            assign_flags |= CODEGEN_ASSIGN_POSTFIX;
        }
    }
    codegen_generate_assignment_part(node->unary.operand, node->unary.op, history, assign_flags);
}

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
    // Same truthiness check as codegen_x86.c (`&&` where `&` was
    // meant); kept so both backends make the same decisions.
    return res && res->flags && RESPONSE_FLAG_ACKNOWLEDGED;
}

static bool codegen_response_has_entity(struct response* res){
    return codegen_response_acknowledged(res)
        && (res->flags & RESPONSE_FLAG_RESOLVED_ENTITY)
        && res->data.resolved_entity;
}

// Widen the low `size` bytes of rax back to a canonical 64-bit value.
// Sizes that are not 1, 2 or 4 need nothing (8-byte values, structs,
// void).
static void codegen_reduce_register(size_t size, bool is_signed){
    switch(size){
        case DATA_SIZE_BYTE:
            asm_push(is_signed ? "movsx rax, al" : "movzx eax, al");
            break;
        case DATA_SIZE_WORD:
            asm_push(is_signed ? "movsx rax, ax" : "movzx eax, ax");
            break;
        case DATA_SIZE_DWORD:
            asm_push(is_signed ? "movsxd rax, eax" : "mov eax, eax");
            break;
        default:
            break;
    }
}

static void codegen_gen_mem_access_get_address(struct node* node, int flags, struct resolver_entity* entity){
    (void)node; (void)flags;
    asm_push("lea r11, [%s]", codegen_entity_private(entity)->address);
    asm_push_ins_push_with_flags("r11",
        STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value",
        STACK_FRAME_ELEMENT_FLAG_IS_PUSHED_ADDRESS);
}

static void codegen_gen_mem_access(struct node* node, int flags, struct resolver_entity* entity){
    (void)node;
    if(flags & EXPRESSION_GET_ADDRESS){
        codegen_gen_mem_access_get_address(node, flags, entity);
        return;
    }
    if(entity->dtype.flags & DATATYPE_FLAG_IS_ARRAY){
        // An array used as a value is the address of its first element
        // (loading here used to read a[0]).
        asm_push("lea r11, [%s]", codegen_entity_private(entity)->address);
        struct datatype decayed = datatype_decay(&entity->dtype);
        asm_push_ins_push_with_data("r11",
            STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value", 0,
            &(struct stack_frame_data){.dtype = decayed});
    } else if(datatype_is_struct_or_union_non_pointer(&entity->dtype)){
        codegen_gen_mem_access_get_address(node, 0, entity);
        asm_push_ins_pop("r11", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
        codegen_generate_structure_push_or_return(entity, codegen_history_begin(0), 0);
    } else if(datatype_element_size(&entity->dtype) == DATA_SIZE_DDWORD){
        asm_push_ins_push_with_data("qword [%s]",
            STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value", 0,
            &(struct stack_frame_data){.dtype = entity->dtype},
            codegen_entity_private(entity)->address);
    } else {
        codegen_load_register("rax", codegen_entity_private(entity)->address, datatype_element_size(&entity->dtype),
            entity->dtype.flags & DATATYPE_FLAG_IS_SIGNED);
        asm_push_ins_push_with_data("rax",
            STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value", 0,
            &(struct stack_frame_data){.dtype = entity->dtype});
    }
}

static void codegen_generate_variable_access_for_entity(struct node* node, struct resolver_entity* entity, struct history* history){
    codegen_gen_mem_access(node, history->flags, entity);
}

static void codegen_generate_variable_access(struct node* node, struct resolver_entity* entity, struct history* history){
    codegen_generate_variable_access_for_entity(node, entity, codegen_history_down(history, history->flags));
}

static bool codegen_resolve_node_for_value(struct node* node, struct history* history);

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

static void codegen_generate_unary_address(struct node* node, struct history* history){
    int flags = history->flags;
    codegen_generate_expressionable(node->unary.operand,
        codegen_history_down(history, flags | EXPRESSION_GET_ADDRESS));
    codegen_response_acknowledge(&(struct response){.flags = RESPONSE_FLAG_UNARY_GET_ADDRESS});
}

// `*p` / `**p`. Each step but the value load reads a full pointer.
// i386 reads a dword at every step and fixes the width afterwards;
// here the final read of a non-pointer value uses its real width,
// so `*p` on a char* never reads past the char.
static void codegen_generate_unary_indirection(struct node* node, struct history* history){
    const char* reg_to_use = "r11";
    int flags = history->flags;
    codegen_response_expect();
    codegen_generate_expressionable(node->unary.operand,
        codegen_history_down(history, flags | EXPRESSION_GET_ADDRESS | EXPRESSION_INDIRECTION));
    struct response* res = codegen_response_pull();
    assert(codegen_response_has_entity(res));
    struct datatype operand_datatype;
    assert(asm_datatype_back(&operand_datatype));
    asm_push_ins_pop(reg_to_use, STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");

    int depth = node->unary.indirection.depth;
    int real_depth = depth;
    bool loads_value = !(history->flags & EXPRESSION_GET_ADDRESS);
    if(loads_value){
        depth++;
    }
    bool sized_last = loads_value && real_depth == res->data.resolved_entity->dtype.pointer_depth;
    for(int i = 0; i < depth; i++){
        if(sized_last && i == depth - 1){
            codegen_load_register(reg_to_use, reg_to_use, datatype_size_no_ptr(&operand_datatype),
                operand_datatype.flags & DATATYPE_FLAG_IS_SIGNED);
        } else {
            asm_push("mov %s, [%s]", reg_to_use, reg_to_use);
        }
    }
    // A loaded value has the pointed-to type. Tagging it with the
    // operand's pointer type made `*p + 1` scale the 1.
    struct datatype result_dtype = operand_datatype;
    if(loads_value){
        result_dtype = *datatype_pointer_reduce(&operand_datatype, real_depth);
    }
    asm_push_ins_push_with_data(reg_to_use,
        STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value", 0,
        &(struct stack_frame_data){.dtype = result_dtype});
    codegen_response_acknowledge(&(struct response){
        .flags = RESPONSE_FLAG_RESOLVED_ENTITY,
        .data.resolved_entity = res->data.resolved_entity,
    });
}

// Pick 32-bit or 64-bit registers for an operation on values of
// this type: 64-bit for long and pointers, 32-bit otherwise.
static bool codegen_is_wide(struct datatype* dtype){
    return datatype_element_size(dtype) == DATA_SIZE_DDWORD;
}

static void codegen_generate_normal_unary(struct node* node, struct history* history){
    codegen_generate_expressionable(node->unary.operand, history);
    struct datatype last_dtype;
    assert(asm_datatype_back(&last_dtype));
    asm_push_ins_pop("rax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
    // `-` and `~` work on the promoted operand: a char becomes int, an
    // unsigned int stays unsigned and is zero extended afterwards.
    struct datatype promoted = datatype_promote(&last_dtype);
    bool wide = codegen_is_wide(&promoted);
    const char* narrow = (promoted.flags & DATATYPE_FLAG_IS_SIGNED) ? "movsxd rax, eax" : "mov eax, eax";
    if(S_EQ(node->unary.op, "-")){
        asm_push(wide ? "neg rax" : "neg eax");
        if(!wide){ asm_push(narrow); }
        asm_push_ins_push_with_data("rax",
            STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value", 0,
            &(struct stack_frame_data){.dtype = promoted});
    } else if(S_EQ(node->unary.op, "~")){
        asm_push(wide ? "not rax" : "not eax");
        if(!wide){ asm_push(narrow); }
        asm_push_ins_push_with_data("rax",
            STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value", 0,
            &(struct stack_frame_data){.dtype = promoted});
    } else if(S_EQ(node->unary.op, "*")){
        codegen_generate_unary_indirection(node, history);
    } else if(S_EQ(node->unary.op, "!")){
        asm_push("cmp rax, 0");
        asm_push("sete al");
        asm_push("movzx eax, al");
        // `!x` is an int whatever x is (a pointer stayed a pointer).
        struct datatype result = datatype_for_numeric();
        result.flags &= ~DATATYPE_FLAG_IS_LITERAL;
        asm_push_ins_push_with_data("rax",
            STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value", 0,
            &(struct stack_frame_data){.dtype = result});
    }
}

// String literals live in .rodata; `lea` with `default rel` gives a
// rip-relative address.
static void codegen_generate_string(struct node* node, struct history* history){
    (void)history;
    const char* label = codegen_register_string(node->sval);
    asm_push("lea rax, [%s]", label);
    asm_push_ins_push_with_data("rax",
        STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value", 0,
        &(struct stack_frame_data){.dtype = datatype_for_string()});
}

static void codegen_generate_exp_parenthesis_node(struct node* node, struct history* history){
    codegen_generate_expressionable(node->parenthesis.exp,
        codegen_history_down(history, codegen_remove_uninheritable_flags(history->flags)));
}

static void codegen_generate_cast(struct node* node, struct history* history){
    if(!codegen_resolve_node_for_value(node, history)){
        codegen_generate_expressionable(node->cast.operand, history);
    }
    asm_push_ins_pop("rax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
    codegen_reduce_register(datatype_size(&node->cast.dtype),
        node->cast.dtype.flags & DATATYPE_FLAG_IS_SIGNED);
    asm_push_ins_push_with_data("rax",
        STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value", 0,
        &(struct stack_frame_data){.dtype = node->cast.dtype});
}

static void codegen_generate_tenary(struct node* node, struct history* history){
    int true_label_id       = codegen_label_count();
    int false_label_id      = codegen_label_count();
    int tenary_end_label_id = codegen_label_count();

    struct datatype last_dtype;
    assert(asm_datatype_back(&last_dtype));
    asm_push_ins_pop("rax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
    asm_push("cmp rax, 0");
    asm_push("je .tenary_false_%i", false_label_id);
    asm_push(".tenary_true_%i:", true_label_id);
    codegen_generate_expressionable(node->tenary.true_node, codegen_history_down(history, 0));
    asm_push_ins_pop_or_ignore("rax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
    asm_push("jmp .tenary_end_%i", tenary_end_label_id);
    asm_push(".tenary_false_%i:", false_label_id);
    codegen_generate_expressionable(node->tenary.false_node, codegen_history_down(history, 0));
    asm_push_ins_pop_or_ignore("rax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
    asm_push(".tenary_end_%i:", tenary_end_label_id);
}

static void codegen_generate_unary(struct node* node, struct history* history){
    if(is_increment_operator(node->unary.op)){
        codegen_generate_increment(node, history);
        return;
    }
    if(codegen_resolve_node_for_value(node, history)){
        return;
    }
    if(op_is_indirection(node->unary.op)){
        codegen_generate_unary_indirection(node, history);
        return;
    } else if(op_is_address(node->unary.op)){
        codegen_generate_unary_address(node, history);
        return;
    }
    codegen_generate_normal_unary(node, history);
}

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

static void codegen_generate_entity_access_for_unary_indirection(struct resolver_result* result, struct resolver_entity* entity, struct history* history){
    (void)history;
    asm_push("; INDIRECTION");
    struct datatype operand_datatype;
    assert(asm_datatype_back(&operand_datatype));
    int flags = asm_push_ins_pop("r11", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
    (void)flags;
    int gen_entity_rules = codegen_entity_rules(result->last_entity, history);
    (void)gen_entity_rules;
    int depth = entity->indirection.depth;
    // `*v[i]` with `int* v[2]`: the chain left the element's address,
    // and the element is the pointer to follow.
    if(resolver_entity_is_pointer_array_element(entity->prev)){
        asm_push("mov r11, [r11]");
    }
    // The pushed value keeps the operand's pointer type (as on i386),
    // so nothing narrows it later. When the last step reads a
    // non-pointer value, read it at its real width: a full-word read
    // of a char picks up the bytes after it. i386 gets away with that
    // only when those bytes happen to be zero.
    // With a final load due, the chain left an address (a member read
    // through `->`), so follow each level in full and let that load
    // read the value, as i386 does; reading the value here as well
    // loaded twice and crashed on `*w->p`.
    bool final_load = result->flags & RESOLVER_RESULT_FLAG_FINAL_INDIRECTION_REQUIRED_FOR_VALUE;
    if(depth > 0 && !final_load && !(entity->dtype.flags & DATATYPE_FLAG_IS_POINTER)){
        codegen_apply_unary_access(depth - 1);
        codegen_load_register("r11", "r11", datatype_element_size(&entity->dtype),
            entity->dtype.flags & DATATYPE_FLAG_IS_SIGNED);
    } else {
        codegen_apply_unary_access(depth);
    }
    // The pushed value has the pointed-to type, so a following `+ 1`
    // is not scaled as pointer arithmetic.
    struct datatype result_dtype = *datatype_pointer_reduce(&operand_datatype, depth);
    result_dtype.flags &= ~DATATYPE_FLAG_IS_ARRAY;
    asm_push_ins_push_with_data("r11",
        STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value",
        STACK_FRAME_ELEMENT_FLAG_IS_PUSHED_ADDRESS,
        &(struct stack_frame_data){.dtype = result_dtype});
}

static void codegen_generate_entity_access_for_unary_get_address(struct resolver_result* result, struct resolver_entity* entity){
    (void)result;
    asm_push_ins_pop("r11", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
    asm_push("; PUSH ADDRESS &");
    asm_push_ins_push_with_data("r11",
        STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value", 0,
        &(struct stack_frame_data){.dtype = entity->dtype});
}

static void codegen_generate_entity_access_for_entity(struct resolver_result* result, struct resolver_entity* entity, struct history* history){
    switch(entity->type){
        case RESOLVER_ENTITY_TYPE_ARRAY_BRACKET:
            codegen_generate_entity_access_array_bracket(result, entity);
            break;
        case RESOLVER_ENTITY_TYPE_VARIABLE:
        case RESOLVER_ENTITY_TYPE_GENERAL:
            codegen_generate_entity_access_for_variable_or_general(result, entity);
            break;
        case RESOLVER_ENTITY_TYPE_FUNCTION_CALL:
            codegen_generate_entity_access_for_function_call(result, entity);
            break;
        case RESOLVER_ENTITY_TYPE_UNARY_INDIRECTION:
            codegen_generate_entity_access_for_unary_indirection(result, entity, history);
            break;
        case RESOLVER_ENTITY_TYPE_UNARY_GET_ADDRESS:
            codegen_generate_entity_access_for_unary_get_address(result, entity);
            break;
        case RESOLVER_ENTITY_TYPE_UNSUPPORTED:
            codegen_generate_entity_access_for_unsupported(result, entity);
            break;
        case RESOLVER_ENTITY_TYPE_CAST:
            codegen_generate_entity_access_for_cast(result, entity);
            break;
        default:
            break;
    }
}

static void codegen_generate_entity_access(struct resolver_result* result, struct resolver_entity* root_assignment_entity, struct node* top_most_node, struct history* history){
    (void)top_most_node;
    if(root_assignment_entity->type == RESOLVER_ENTITY_TYPE_NATIVE_FUNCTION){
        struct native_function* native_func = native_function_get(current_process, root_assignment_entity->name);
        if(native_func){
            asm_push("; NATIVE FUNCTION %s", root_assignment_entity->name);
            struct resolver_entity* func_call_entity = resolver_result_entity_next(root_assignment_entity);
            assert(func_call_entity && func_call_entity->type == RESOLVER_ENTITY_TYPE_FUNCTION_CALL);
            native_func->callbacks.call(&x86_64_codegen, native_func, func_call_entity->func_call_data.arguments);
            return;
        }
    }
    codegen_generate_entity_access_start(result, root_assignment_entity, history);
    struct resolver_entity* current = resolver_result_entity_next(root_assignment_entity);
    while(current){
        codegen_generate_entity_access_for_entity(result, current, history);
        current = resolver_result_entity_next(current);
    }
    codegen_response_acknowledge(&(struct response){
        .flags = RESPONSE_FLAG_RESOLVED_ENTITY,
        .data.resolved_entity = result->last_entity,
    });
}

// Bytes pushed below rbp right now, from the ledger. The saved rbp
// itself sits at [rbp] and does not count.
static size_t codegen_bytes_below_rbp(void){
    return (vector_count(current_function->func.frame.elements) - 1) * DATA_SIZE_DDWORD;
}

// A function call. The access chain has already pushed the callee's
// address. The arguments are evaluated onto the expression stack
// last to first, exactly like the i386 backend, and then popped into
// rdi, rsi, rdx, rcx, r8, r9: the first argument is on top. The
// callee address is kept on the stack under the arguments (instead
// of the i386 per-call .data slot), so nested calls and recursion
// cannot overwrite it.
//
// A struct return reserves the result space on the caller's stack
// and passes its address as a hidden first argument in rdi, which
// moves the visible arguments one register along.
static void codegen_generate_entity_access_for_function_call(struct resolver_result* result, struct resolver_entity* entity){
    (void)result;
    bool returns_struct = datatype_is_struct_or_union_non_pointer(&entity->dtype);
    int  first_reg      = returns_struct ? 1 : 0;
    int  total_args     = vector_count(entity->func_call_data.arguments);
    if(first_reg + total_args > current_process->target->max_reg_args){
        compiler_error(current_process, "x86_64: calls with more than %i register arguments are not supported\n",
            current_process->target->max_reg_args);
    }

    vector_set_flag(entity->func_call_data.arguments, VECTOR_FLAG_PEEK_DECREMENT);
    vector_set_peek_pointer_end(entity->func_call_data.arguments);
    struct node* node = vector_peek_ptr(entity->func_call_data.arguments);

    asm_push_ins_pop("r11", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
    if(returns_struct){
        asm_push("; SUBTRACT ROOM FOR RETURNED STRUCTURE/UNION DATATYPE");
        codegen_stack_sub_with_name(align_value(datatype_size(&entity->dtype), DATA_SIZE_DDWORD), "result_value");
        asm_push_ins_push("rsp", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "struct_return_pointer");
    }
    asm_push_ins_push("r11", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "function_call_target");

    while(node){
        size_t before = vector_count(current_function->func.frame.elements);
        codegen_generate_expressionable(node, codegen_history_begin(EXPRESSION_IN_FUNCTION_CALL_ARGUMENTS));
        if(vector_count(current_function->func.frame.elements) != before + 1){
            compiler_error(current_process, "x86_64: an argument must fit one register (no struct arguments by value)\n");
        }
        node = vector_peek_ptr(entity->func_call_data.arguments);
    }

    for(int i = 0; i < total_args; i++){
        asm_push_ins_pop(codegen_arg_registers[first_reg + i], STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
    }
    asm_push_ins_pop("r11", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "function_call_target");
    if(returns_struct){
        asm_push_ins_pop("rdi", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "struct_return_pointer");
    }

    // SysV wants rsp 16-byte aligned at the call. rbp is aligned, so
    // an odd number of 8-byte slots below it needs one slot of pad.
    bool pad = (codegen_bytes_below_rbp() % C_STACK_ALIGNMENT) != 0;
    if(pad){
        asm_push("sub rsp, 8");
    }
    // al = number of vector registers used, for variadic callees
    // such as printf.
    asm_push("xor eax, eax");
    asm_push("call r11");
    if(pad){
        asm_push("add rsp, 8");
    }

    if(returns_struct){
        asm_push("mov r11, rax");
        codegen_generate_structure_push(entity, codegen_history_begin(0), 0);
    } else {
        // SysV leaves the bits above an int return undefined, and gcc
        // compiled callees do leave junk there.
        if(!(entity->dtype.flags & DATATYPE_FLAG_IS_POINTER)){
            codegen_reduce_register(datatype_size(&entity->dtype), entity->dtype.flags & DATATYPE_FLAG_IS_SIGNED);
        }
        asm_push_ins_push_with_data("rax",
            STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value", 0,
            &(struct stack_frame_data){.dtype = entity->dtype});
    }
}

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

// The access chain left the address of an array (not a value read from
// one): no final load is due and the first entity was not pushed as a
// value. A constant subscript merged into the variable keeps the array
// type but is pushed as the element's value.
static bool codegen_result_is_array_address(struct resolver_result* result, struct datatype* dtype){
    return (dtype->flags & DATATYPE_FLAG_IS_ARRAY)
        && !(result->flags & RESOLVER_RESULT_FLAG_FINAL_INDIRECTION_REQUIRED_FOR_VALUE)
        && !(result->flags & RESOLVER_RESULT_FLAG_FIRST_ENTITY_PUSH_VALUE);
}

// The chain ends at the address of a pointer (an element of `int* v[2]`,
// a member read as `w->p`): the value is the pointer stored there, a
// full-width load. This path pushed the address itself before.
static bool codegen_result_is_pointer_load(struct resolver_result* result, struct datatype* dtype){
    return (dtype->flags & DATATYPE_FLAG_IS_POINTER)
        && (result->flags & RESOLVER_RESULT_FLAG_FINAL_INDIRECTION_REQUIRED_FOR_VALUE);
}

static bool codegen_resolve_node_for_value(struct node* node, struct history* history){
    struct resolver_result* result = 0;
    if(!codegen_resolve_node_return_result(node, history, &result)){
        return false;
    }
    struct datatype dtype;
    assert(asm_datatype_back(&dtype));
    if(codegen_result_is_array_address(result, &dtype)){
        // The chain left an array's address (a whole array, or a row of
        // a multi-dimensional one): the value is that address, as a
        // pointer to the element type.
        asm_push_ins_pop("rax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
        struct datatype decayed = datatype_decay(&dtype);
        asm_push_ins_push_with_data("rax",
            STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value", 0,
            &(struct stack_frame_data){.dtype = decayed});
    } else if(codegen_result_is_pointer_load(result, &dtype)){
        asm_push_ins_pop("rax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
        asm_push("mov rax, [rax]");
        struct datatype element = dtype;
        element.flags &= ~DATATYPE_FLAG_IS_ARRAY;
        asm_push_ins_push_with_data("rax",
            STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value", 0,
            &(struct stack_frame_data){.dtype = element});
    } else if(datatype_is_struct_or_union_non_pointer(&dtype)){
        codegen_generate_structure_push(result->last_entity, history, 0);
    } else if(!(dtype.flags & DATATYPE_FLAG_IS_POINTER)){
        asm_push_ins_pop("rax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
        if(result->flags & RESOLVER_RESULT_FLAG_FINAL_INDIRECTION_REQUIRED_FOR_VALUE){
            codegen_load_register("rax", "rax", datatype_element_size(&dtype), dtype.flags & DATATYPE_FLAG_IS_SIGNED);
        } else {
            codegen_reduce_register(datatype_element_size(&dtype), dtype.flags & DATATYPE_FLAG_IS_SIGNED);
        }
        asm_push_ins_push_with_data("rax",
            STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value", 0,
            &(struct stack_frame_data){.dtype = dtype});
    }
    return true;
}

static int codegen_set_flag_for_operator(const char* op){
    int flag = 0;
    if(S_EQ(op, "+"))         flag |= EXPRESSION_IS_ADDITION;
    else if(S_EQ(op, "-"))    flag |= EXPRESSION_IS_SUBTRACTION;
    else if(S_EQ(op, "*"))    flag |= EXPRESSION_IS_MULTIPLICATION;
    else if(S_EQ(op, "/"))    flag |= EXPRESSION_IS_DIVISION;
    else if(S_EQ(op, "%"))    flag |= EXPRESSION_IS_MODULAS;
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

// Compare rax/eax with rcx/ecx and leave 0 or 1 in rax. An unsigned
// comparison uses the below/above condition codes.
static void codegen_gen_cmp(bool wide, const char* set_ins){
    asm_push(wide ? "cmp rax, rcx" : "cmp eax, ecx");
    asm_push("%s al", set_ins);
    asm_push("movzx eax, al");
}

// rax = rax <op> rcx in the operation's type. With wide == false the
// operation runs on eax and ecx so int overflow wraps at 32 bits, then
// the result is put back in canonical 64-bit form: sign extended for
// int, zero extended for unsigned int.
static void codegen_gen_math_for_value(int flags, bool wide, bool is_signed){
    const char* a = wide ? "rax" : "eax";
    const char* c = wide ? "rcx" : "ecx";
    bool narrow_result = !wide;
    if(flags & EXPRESSION_IS_ADDITION){
        asm_push("add %s, %s", a, c);
    } else if(flags & EXPRESSION_IS_SUBTRACTION){
        asm_push("sub %s, %s", a, c);
    } else if(flags & EXPRESSION_IS_MULTIPLICATION){
        asm_push("imul %s, %s", a, c);
    } else if(flags & EXPRESSION_IS_DIVISION || flags & EXPRESSION_IS_MODULAS){
        if(is_signed){
            asm_push(wide ? "cqo" : "cdq");
            asm_push("idiv %s", c);
        } else {
            asm_push("xor edx, edx");
            asm_push("div %s", c);
        }
        if(flags & EXPRESSION_IS_MODULAS){
            asm_push(wide ? "mov rax, rdx" : "mov eax, edx");
        }
    } else if(flags & EXPRESSION_IS_ABOVE){
        codegen_gen_cmp(wide, is_signed ? "setg" : "seta");
        narrow_result = false;
    } else if(flags & EXPRESSION_IS_BELOW){
        codegen_gen_cmp(wide, is_signed ? "setl" : "setb");
        narrow_result = false;
    } else if(flags & EXPRESSION_IS_EQUAL){
        codegen_gen_cmp(wide, "sete");
        narrow_result = false;
    } else if(flags & EXPRESSION_IS_ABOVE_OR_EQUAL){
        codegen_gen_cmp(wide, is_signed ? "setge" : "setae");
        narrow_result = false;
    } else if(flags & EXPRESSION_IS_BELOW_OR_EQUAL){
        codegen_gen_cmp(wide, is_signed ? "setle" : "setbe");
        narrow_result = false;
    } else if(flags & EXPRESSION_IS_NOT_EQUAL){
        codegen_gen_cmp(wide, "setne");
        narrow_result = false;
    } else if(flags & EXPRESSION_IS_BITSHIFT_LEFT){
        asm_push("sal %s, cl", a);
    } else if(flags & EXPRESSION_IS_BITSHIFT_RIGHT){
        asm_push("%s %s, cl", is_signed ? "sar" : "shr", a);
    } else if(flags & EXPRESSION_IS_BITWISE_AND){
        asm_push("and %s, %s", a, c);
    } else if(flags & EXPRESSION_IS_BITWISE_OR){
        asm_push("or %s, %s", a, c);
    } else if(flags & EXPRESSION_IS_BITWISE_XOR){
        asm_push("xor %s, %s", a, c);
    }
    if(narrow_result){
        // A 32-bit mov zero-fills the top half.
        asm_push(is_signed ? "movsxd rax, eax" : "mov eax, eax");
    }
}

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

// i386 uses `jg` here, so a negative left operand of || counts as
// false there. Any non-zero value is true in C; this backend uses jne.
static void codegen_generate_logical_cmp_or(const char* reg, const char* equal_label){
    asm_push("cmp %s, 0", reg);
    asm_push("jne %s", equal_label);
}

static void codegen_generate_logical_cmp(const char* op, const char* fail_label, const char* equal_label){
    if(S_EQ(op, "&&"))      codegen_generate_logical_cmp_and("rax", fail_label);
    else if(S_EQ(op, "||")) codegen_generate_logical_cmp_or("rax", equal_label);
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
    asm_push_ins_pop("rax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
    codegen_generate_logical_cmp(node->exp.op, history->exp.logical_end_label, history->exp.logical_end_label_positive);
    codegen_generate_expressionable(node->exp.right,
        codegen_history_down(history, history->flags | EXPRESSION_IN_LOGICAL_EXPRESSION));
    if(!is_logical_node(node->exp.right)){
        asm_push_ins_pop("rax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
        codegen_generate_logical_cmp(node->exp.op, history->exp.logical_end_label, history->exp.logical_end_label_positive);
        codegen_generate_end_labels_for_logical_expression(node->exp.op, history->exp.logical_end_label, history->exp.logical_end_label_positive);
        // The 0/1 result is an int. Pushed without a type, a
        // `return a || b;` asserted on the missing datatype.
        asm_push_ins_push_with_data("rax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value", 0,
            &(struct stack_frame_data){.dtype = datatype_for_int()});
    }
}

// rax = rax / n for a byte difference that is an exact multiple of n
// (a pointer difference).
static void codegen_divide_exact(size_t n){
    if((n & (n - 1)) == 0){
        int shift = 0;
        while(((size_t)1 << shift) < n){
            shift++;
        }
        asm_push("sar rax, %i", shift);
        return;
    }
    asm_push("mov rcx, %zu", n);
    asm_push("cqo");
    asm_push("idiv rcx");
}

// Binary arithmetic. Same shape as i386 (left and right pushed, right
// popped into rcx, left into rax). datatype_binary_types gives the type
// the operation runs in (its width picks 32 or 64-bit registers, its
// signedness picks idiv or div, setl or setb, sar or shr), the pointer
// scaling, and the type of the pushed result.
static void codegen_generate_exp_node_for_arithmetic(struct node* node, struct history* history){
    assert(node->type == NODE_TYPE_EXPRESSION);
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
        struct datatype right_dtype = datatype_for_numeric();
        asm_datatype_back(&right_dtype);
        asm_push_ins_pop("rcx", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
        struct datatype left_dtype = datatype_for_numeric();
        asm_datatype_back(&left_dtype);
        asm_push_ins_pop("rax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
        struct datatype_binary types;
        datatype_binary_types(&left_dtype, &right_dtype, node->exp.op, &types);
        if(types.scale_left > 1){
            asm_push("imul rax, %zu", types.scale_left);
        }
        if(types.scale_right > 1){
            asm_push("imul rcx, %zu", types.scale_right);
        }
        codegen_gen_math_for_value(op_flags, codegen_is_wide(&types.operation),
            types.operation.flags & DATATYPE_FLAG_IS_SIGNED);
        if(types.pointer_difference > 1){
            codegen_divide_exact(types.pointer_difference);
        }
        last_dtype = types.result;
    }
    asm_push_ins_push_with_data("rax",
        STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value", 0,
        &(struct stack_frame_data){.dtype = last_dtype});
}

static void codegen_generate_exp_node(struct node* node, struct history* history){
    if(is_node_assignment(node)){
        codegen_generate_assignment_expression(node, history);
        return;
    }
    if(codegen_resolve_node_for_value(node, history)){
        return;
    }
    int extra = get_additional_flags(history->flags, node);
    codegen_generate_exp_node_for_arithmetic(node,
        codegen_history_down(history, codegen_remove_uninheritable_flags(history->flags) | extra));
}

static void codegen_plus_or_minus_string_for_value(char* out, int val, size_t len){
    memset(out, 0, len);
    if(val < 0){
        sprintf(out, "%i", val);
    } else {
        sprintf(out, "+%i", val);
    }
}

// Split a struct of `size` bytes into the pieces one stack slot each
// carries: 8 bytes while they last, then 4, 2 and 1. i386 copies in
// whole dwords, which writes past the end of a struct whose size is
// not a multiple of 4; here no piece reaches beyond the struct.
static int codegen_struct_pieces(size_t size, int* offsets, int* sizes, int max){
    int count = 0;
    size_t offset = 0;
    while(offset < size && count < max){
        size_t left  = size - offset;
        size_t piece = left >= 8 ? 8 : left >= 4 ? 4 : left >= 2 ? 2 : 1;
        offsets[count] = offset;
        sizes[count]   = piece;
        count++;
        offset += piece;
    }
    if(offset < size){
        compiler_error(current_process, "x86_64: structure too large to copy by value\n");
    }
    return count;
}

#define CODEGEN_MAX_STRUCT_PIECES 256

// Push a struct whose address is in r11, last piece first, so the
// first piece ends up on top.
static void codegen_generate_structure_push(struct resolver_entity* entity, struct history* history, int start_pos){
    (void)history;
    asm_push("; STRUCTURE PUSH");
    int offsets[CODEGEN_MAX_STRUCT_PIECES];
    int sizes[CODEGEN_MAX_STRUCT_PIECES];
    int pieces = codegen_struct_pieces(entity->dtype.size, offsets, sizes, CODEGEN_MAX_STRUCT_PIECES);
    for(int i = pieces - 1; i >= start_pos; i--){
        char fmt[20];
        char address[40];
        codegen_plus_or_minus_string_for_value(fmt, offsets[i], sizeof(fmt));
        snprintf(address, sizeof(address), "r11%s", fmt);
        if(sizes[i] == DATA_SIZE_DDWORD){
            asm_push_ins_push_with_data("qword [%s]",
                STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value", 0,
                &(struct stack_frame_data){.dtype = entity->dtype}, address);
        } else {
            codegen_load_register("rax", address, sizes[i], false);
            asm_push_ins_push_with_data("rax",
                STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value", 0,
                &(struct stack_frame_data){.dtype = entity->dtype});
        }
    }
    asm_push("; END STRUCTURE PUSH");
    codegen_response_acknowledge(&(struct response){.flags = RESPONSE_FLAG_PUSHED_STRUCTURE});
}

static void codegen_generate_structure_push_or_return(struct resolver_entity* entity, struct history* history, int start_pos){
    codegen_generate_structure_push(entity, history, start_pos);
}

// Pop struct pieces and store each at its offset from base_address.
static void codegen_generate_move_struct(struct datatype* dtype, const char* base_address, int offset){
    int offsets[CODEGEN_MAX_STRUCT_PIECES];
    int sizes[CODEGEN_MAX_STRUCT_PIECES];
    int pieces = codegen_struct_pieces(datatype_size(dtype), offsets, sizes, CODEGEN_MAX_STRUCT_PIECES);
    for(int i = 0; i < pieces; i++){
        asm_push_ins_pop("rax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
        char fmt[20];
        codegen_plus_or_minus_string_for_value(fmt, offset + offsets[i], sizeof(fmt));
        const char* reg = "rax";
        const char* keyword = codegen_byte_word_or_dword_or_ddword(sizes[i], &reg);
        asm_push("mov %s [%s%s], %s", keyword, base_address, fmt, reg);
    }
}

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
        stack_adjustment += DATA_SIZE_DDWORD;
        el = asm_stack_peek();
    }
    codegen_stack_add(stack_adjustment);
}

// The hidden struct-return pointer arrived in rdi, so it is in the
// first home-area slot.
static void codegen_generate_statement_return_exp(struct node* node){
    codegen_response_expect();
    codegen_generate_expressionable(node->stmt.return_stmt.exp,
        codegen_history_begin(IS_STATEMENT_RETURN));
    struct datatype dtype;
    assert(asm_datatype_back(&dtype));
    if(datatype_is_struct_or_union_non_pointer(&dtype)){
        asm_push("mov rdx, [rbp+16]");
        codegen_generate_move_struct(&dtype, "rdx", 0);
        asm_push("mov rax, [rbp+16]");
        return;
    }
    asm_push_ins_pop("rax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
}

static void _codegen_generate_if_stmt(struct node* node, int end_label_id);

static void codegen_generate_else_stmt(struct node* node){
    codegen_generate_body(node->stmt.else_stmt.body_node, codegen_history_begin(0));
}

static void codegen_generate_else_or_else_if(struct node* node, int end_label_id){
    if(node->type == NODE_TYPE_STATEMENT_IF){
        _codegen_generate_if_stmt(node, end_label_id);
    } else if(node->type == NODE_TYPE_STATEMENT_ELSE){
        codegen_generate_else_stmt(node);
    } else {
        compiler_error(current_process, "Unexpected keyword compiler bug");
    }
}

static void _codegen_generate_if_stmt(struct node* node, int end_label_id){
    int if_label_id = codegen_label_count();
    codegen_generate_expressionable(node->stmt.if_stmt.cond_node, codegen_history_begin(0));
    asm_push_ins_pop("rax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
    asm_push("cmp rax, 0");
    asm_push("je .if_%i", if_label_id);
    codegen_generate_body(node->stmt.if_stmt.body_node, codegen_history_begin(IS_ALONE_STATEMENT));
    asm_push("jmp .if_end_%i", end_label_id);
    asm_push(".if_%i:", if_label_id);
    if(node->stmt.if_stmt.next){
        codegen_generate_else_or_else_if(node->stmt.if_stmt.next, end_label_id);
    }
}

static void codegen_generate_if_stmt(struct node* node){
    int end_label_id = codegen_label_count();
    _codegen_generate_if_stmt(node, end_label_id);
    asm_push(".if_end_%i:", end_label_id);
}

static void codegen_generate_while_stmt(struct node* node){
    codegen_begin_entry_exit_point();
    int while_start_id = codegen_label_count();
    int while_end_id   = codegen_label_count();
    asm_push(".while_start_%i:", while_start_id);
    codegen_generate_expressionable(node->stmt.while_stmt.exp_node, codegen_history_begin(0));
    asm_push_ins_pop("rax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
    asm_push("cmp rax, 0");
    asm_push("je .while_end_%i", while_end_id);
    codegen_generate_body(node->stmt.while_stmt.body_node, codegen_history_begin(IS_ALONE_STATEMENT));
    asm_push("jmp .while_start_%i", while_start_id);
    asm_push(".while_end_%i:", while_end_id);
    codegen_end_entry_exit_point();
}

// A for-loop's init and step are evaluated for their effect, like a
// statement: an assignment or ++/-- there pushes no value.
static void codegen_generate_statement(struct node* node, struct history* history);

static void codegen_generate_for_effect(struct node* node){
    if(node->type == NODE_TYPE_VARIABLE || node->type == NODE_TYPE_VARIABLE_LIST){
        codegen_generate_statement(node, codegen_history_begin(0));
        return;
    }
    if(is_node_assignment(node)){
        codegen_generate_exp_node(node, codegen_history_begin(0));
        return;
    }
    if(node->type == NODE_TYPE_UNARY && is_increment_operator(node->unary.op)){
        codegen_generate_increment(node, codegen_history_begin(0));
        return;
    }
    codegen_generate_expressionable(node, codegen_history_begin(0));
    asm_push_ins_pop_or_ignore("rax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
}

static void codegen_generate_for_stmt(struct node* node){
    struct for_stmt* fs = &node->stmt.for_stmt;
    int for_loop_start_id = codegen_label_count();
    int for_loop_end_id   = codegen_label_count();
    // `for (int i = 0; ...)`: i is in scope for the whole loop only.
    bool declares = fs->init_node
        && (fs->init_node->type == NODE_TYPE_VARIABLE || fs->init_node->type == NODE_TYPE_VARIABLE_LIST);
    if(declares){
        codegen_new_scope(RESOLVER_SCOPE_FLAG_IS_STACK);
    }
    if(fs->init_node){
        codegen_generate_for_effect(fs->init_node);
    }
    asm_push("jmp .for_loop%i", for_loop_start_id);
    codegen_begin_entry_exit_point();
    if(fs->loop_node){
        codegen_generate_for_effect(fs->loop_node);
    }
    asm_push(".for_loop%i:", for_loop_start_id);
    if(fs->cond_node){
        codegen_generate_expressionable(fs->cond_node, codegen_history_begin(0));
        asm_push_ins_pop_or_ignore("rax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
        asm_push("cmp rax, 0");
        asm_push("je .for_loop_end%i", for_loop_end_id);
    }
    if(fs->body_node){
        codegen_generate_body(fs->body_node, codegen_history_begin(IS_ALONE_STATEMENT));
    }
    if(fs->loop_node){
        codegen_generate_for_effect(fs->loop_node);
    }
    asm_push("jmp .for_loop%i", for_loop_start_id);
    asm_push(".for_loop_end%i:", for_loop_end_id);
    codegen_end_entry_exit_point();
    if(declares){
        codegen_finish_scope();
    }
}

static void codegen_generate_switch_default_stmt(struct node* node){
    (void)node;
    asm_push("; DEFAULT CASE");
    struct code_generator* gen = current_process->generator;
    asm_push(".switch_stmt_%i_case_default:", gen->_switch.current.id);
}

static void codegen_generate_switch_stmt_case_jumps(struct node* node){
    vector_set_peek_pointer(node->stmt.switch_stmt.cases, 0);
    struct parsed_switch_case* sc = vector_peek(node->stmt.switch_stmt.cases);
    while(sc){
        asm_push("cmp eax, %i", sc->index);
        asm_push("je .switch_stmt_%i_case_%i", codegen_switch_id(), sc->index);
        sc = vector_peek(node->stmt.switch_stmt.cases);
    }
    if(node->stmt.switch_stmt.has_default_case){
        asm_push("jmp .switch_stmt_%i_case_default", codegen_switch_id());
        return;
    }
    codegen_goto_exit_point_maintain_stack(node);
}

static void codegen_generate_switch_stmt(struct node* node){
    codegen_begin_entry_exit_point();
    codegen_begin_switch_statement();
    codegen_generate_expressionable(node->stmt.switch_stmt.exp, codegen_history_begin(0));
    asm_push_ins_pop_or_ignore("rax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
    codegen_generate_switch_stmt_case_jumps(node);
    codegen_generate_body(node->stmt.switch_stmt.body, codegen_history_begin(IS_ALONE_STATEMENT));
    codegen_end_switch_statement();
    codegen_end_entry_exit_point();
}

static void codegen_generate_goto_stmt(struct node* node){
    asm_push("jmp label_%s", node->stmt._goto.label->sval);
}

static void codegen_generate_label(struct node* node){
    asm_push("label_%s:", node->stmt.label.name->sval);
}

static void codegen_generate_switch_case_stmt(struct node* node){
    struct node* case_exp = node->stmt._case.exp;
    assert(case_exp->type == NODE_TYPE_NUMBER);
    codegen_begin_case_statement(case_exp->llnum);
    asm_push("; CASE %i", (int)case_exp->llnum);
    codegen_end_case_statement();
}

static void codegen_generate_break_stmt(struct node* node){
    codegen_goto_exit_point(node);
}

static void codegen_generate_continue_stmt(struct node* node){
    codegen_goto_entry_point(node);
}

static void codegen_generate_do_while_stmt(struct node* node){
    codegen_begin_entry_exit_point();
    int do_while_start_id = codegen_label_count();
    asm_push(".do_while_start_%i:", do_while_start_id);
    codegen_generate_body(node->stmt.do_while_stmt.body_node, codegen_history_begin(IS_ALONE_STATEMENT));
    codegen_generate_expressionable(node->stmt.do_while_stmt.exp_node, codegen_history_begin(0));
    asm_push_ins_pop("rax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
    asm_push("cmp rax, 0");
    asm_push("jne .do_while_start_%i", do_while_start_id);
    codegen_end_entry_exit_point();
}

// Bytes of argument registers the prologue spills into the home area.
static int codegen_home_area_size(void){
    return current_process->target->max_reg_args * DATA_SIZE_DDWORD;
}

// The epilogue: drop locals, restore rbp, return and drop the home
// area the prologue built (`ret imm16` pops the return address, then
// adds imm16 to rsp).
static void codegen_generate_epilogue_ret(void){
    asm_push("ret %i", codegen_home_area_size());
}

static void codegen_generate_statement_return(struct node* node){
    if(node->stmt.return_stmt.exp){
        codegen_generate_statement_return_exp(node);
    }
    codegen_stack_add_no_compile_time_stack_frame_restore(
        C_ALIGN(function_node_stack_size(node->binded.function)));
    asm_push("pop rbp");
    codegen_generate_epilogue_ret();
}

static void codegen_generate_statement(struct node* node, struct history* history){
    (void)history;
    switch(node->type){
        case NODE_TYPE_EXPRESSION:
            codegen_generate_exp_node(node, codegen_history_begin(history->flags));
            break;
        case NODE_TYPE_UNARY:
            codegen_generate_unary(node, codegen_history_begin(history->flags));
            break;
        case NODE_TYPE_VARIABLE:
            codegen_generate_scope_variable(node);
            break;
        case NODE_TYPE_VARIABLE_LIST: {
            vector_set_peek_pointer(node->var_list.list, 0);
            struct node* v = vector_peek_ptr(node->var_list.list);
            while(v){
                codegen_generate_scope_variable(v);
                v = vector_peek_ptr(node->var_list.list);
            }
            break;
        }
        case NODE_TYPE_STATEMENT_RETURN:
            codegen_generate_statement_return(node);
            break;
        case NODE_TYPE_STATEMENT_IF:
            codegen_generate_if_stmt(node);
            break;
        case NODE_TYPE_STATEMENT_WHILE:
            codegen_generate_while_stmt(node);
            break;
        case NODE_TYPE_STATEMENT_DO_WHILE:
            codegen_generate_do_while_stmt(node);
            break;
        case NODE_TYPE_STATEMENT_FOR:
            codegen_generate_for_stmt(node);
            break;
        case NODE_TYPE_STATEMENT_BREAK:
            codegen_generate_break_stmt(node);
            break;
        case NODE_TYPE_STATEMENT_CONTINUE:
            codegen_generate_continue_stmt(node);
            break;
        case NODE_TYPE_STATEMENT_SWITCH:
            codegen_generate_switch_stmt(node);
            break;
        case NODE_TYPE_STATEMENT_CASE:
            codegen_generate_switch_case_stmt(node);
            break;
        case NODE_TYPE_STATEMENT_DEFAULT:
            codegen_generate_switch_default_stmt(node);
            break;
        case NODE_TYPE_STATEMENT_GOTO:
            codegen_generate_goto_stmt(node);
            break;
        case NODE_TYPE_LABEL:
            codegen_generate_label(node);
            break;
    }
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

static void codegen_generate_body(struct node* node, struct history* history){
    codegen_generate_stack_scope(node->body.statements, node->body.size, history);
}

// Prologue. The six argument registers are pushed into a home area
// between the caller's frame and our frame record:
//
//   [rbp+56] r9  ...  [rbp+16] rdi   home area (callee-owned)
//   [rbp+8]  return address
//   [rbp]    saved rbp
//
// so parameter i is at [rbp+16+8*i]. The return address is popped
// first and pushed back below the home area, which keeps the layout
// inside this function: callers (including gcc compiled ones such as
// libc calling main) reserve nothing. `ret 48` removes it again.
static void codegen_generate_function_with_body(struct node* node){
    codegen_register_function(node, 0);
    asm_push("global %s", node->func.name);
    asm_push("; %s function", node->func.name);
    asm_push("%s:", node->func.name);

    asm_push("pop r11");
    for(int i = current_process->target->max_reg_args - 1; i >= 0; i--){
        asm_push("push %s", codegen_arg_registers[i]);
    }
    asm_push("push r11");
    asm_push_rbp();
    asm_push("mov rbp, rsp");
    codegen_stack_sub(C_ALIGN(function_node_stack_size(node)));
    codegen_new_scope(RESOLVER_DEFAULT_ENTITY_FLAG_IS_LOCAL_STACK);
    codegen_generate_function_arguments(function_node_argument_vec(node));

    codegen_generate_body(node->func.body_n, codegen_history_begin(IS_ALONE_STATEMENT));
    codegen_finish_scope();
    codegen_stack_add(C_ALIGN(function_node_stack_size(node)));
    asm_pop_rbp();
    stackframe_assert_empty(current_function);
    codegen_generate_epilogue_ret();
}

static void codegen_generate_function(struct node* node){
    current_function = node;
    node->func.frame.elements = vector_create(sizeof(struct stack_frame_element));
    if(function_node_is_prototype(node)){
        codegen_generate_function_prototype(node);
        return;
    }
    codegen_generate_function_with_body(node);
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

static void codegen_goto_exit_point_maintain_stack(struct node* node){
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

static void codegen_begin_switch_statement(void){
    struct code_generator* gen = current_process->generator;
    struct generator_switch_stmt* sd = &gen->_switch;
    vector_push(sd->swtiches, &sd->current);
    memset(&sd->current, 0, sizeof(struct generator_switch_stmt_entity));
    int switch_stmt_id = codegen_label_count();
    asm_push(".switch_stmt_%i:", switch_stmt_id);
    sd->current.id = switch_stmt_id;
}

static void codegen_end_switch_statement(void){
    struct code_generator* gen = current_process->generator;
    struct generator_switch_stmt* sd = &gen->_switch;
    asm_push(".switch_stmt_%i_end:", sd->current.id);
    memcpy(&sd->current, vector_back(sd->swtiches), sizeof(struct generator_switch_stmt_entity));
    vector_pop(sd->swtiches);
}

static int codegen_switch_id(void){
    struct code_generator* gen = current_process->generator;
    return gen->_switch.current.id;
}

static void codegen_begin_case_statement(int index){
    struct code_generator* gen = current_process->generator;
    asm_push(".switch_stmt_%i_case_%i:", gen->_switch.current.id, index);
}

static void codegen_end_case_statement(void){
}

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
            const char* label = codegen_register_string(node->var.val->sval);
            asm_push("%s: %s %s",
                node->var.name,
                asm_keyword_for_size(variable_size(node), tmp_buf),
                label);
        } else {
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

static void codegen_generate_global_variable_for_struct(struct node* node){
    if(node->var.val){
        compiler_error(current_process, "We dont yet support values for structures");
        return;
    }
    char tmp_buf[256];
    asm_push("%s: %s 0", node->var.name,
        asm_keyword_for_size(variable_size(node), tmp_buf));
}

static void codegen_generate_global_variable_for_union(struct node* node){
    if(node->var.val){
        compiler_error(current_process, "We dont yet support values for unions");
        return;
    }
    char tmp_buf[256];
    asm_push("%s: %s 0", node->var.name,
        asm_keyword_for_size(variable_size(node), tmp_buf));
}

static void codegen_generate_variable_for_array(struct node* node){
    if(node->var.val){
        compiler_error(current_process, "We don't support values for arrays yet");
        return;
    }
    char tmp_buf[256];
    asm_push("%s: %s 0", node->var.name,
        asm_keyword_for_size(variable_size(node), tmp_buf));
}

static void codegen_generate_global_variable(struct node* node){
    asm_push("; %s %s", node->var.type.type_str, node->var.name);
    if(node->var.type.flags & DATATYPE_FLAG_IS_ARRAY){
        codegen_generate_variable_for_array(node);
        codegen_new_scope_entity(node, 0, 0);
        return;
    }
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
        case DATA_TYPE_UNION:
            codegen_generate_global_variable_for_union(node);
            break;
        case DATA_TYPE_DOUBLE:
        case DATA_TYPE_FLOAT:
            compiler_error(current_process, "Doubles and floats are not supported in our subset of C\n");
            break;
    }
    assert(node->type == NODE_TYPE_VARIABLE);
    codegen_new_scope_entity(node, 0, 0);
}

static void codegen_generate_struct(struct node* node){
    if(node->flags & NODE_FLAG_HAS_VARIABLE_COMBINED){
        codegen_generate_global_variable(node->_struct.var);
    }
}

static void codegen_generate_union(struct node* node){
    if(node->flags & NODE_FLAG_HAS_VARIABLE_COMBINED){
        codegen_generate_global_variable(node->_union.var);
    }
}

static void codegen_generate_global_variable_list(struct node* var_list_node){
    assert(var_list_node->type == NODE_TYPE_VARIABLE_LIST);
    vector_set_peek_pointer(var_list_node->var_list.list, 0);
    struct node* var = vector_peek_ptr(var_list_node->var_list.list);
    while(var){
        codegen_generate_global_variable(var);
        var = vector_peek_ptr(var_list_node->var_list.list);
    }
}

static void codegen_generate_data_section_part(struct node* node){
    switch(node->type){
        case NODE_TYPE_VARIABLE_LIST:
            codegen_generate_global_variable_list(node);
            break;
        case NODE_TYPE_VARIABLE:
            codegen_generate_global_variable(node);
            break;
        case NODE_TYPE_STRUCT:
            codegen_generate_struct(node);
            break;
        case NODE_TYPE_UNION:
            codegen_generate_union(node);
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

// A byte NASM cannot take inside a quoted character is written as a
// number: a newline or tab as before, and now also the apostrophe, which
// ended the quote ("it's" printed "it"), and every other byte
// outside printable ASCII.
static bool codegen_write_string_char_escaped(char c){
    unsigned char ch = (unsigned char)c;
    if(ch == '\n' || ch == '\t' || ch == '\'' || ch < 32 || ch >= 127){
        asm_push_no_nl("%u, ", (unsigned)ch);
        return true;
    }
    return false;
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

static void codegen_generate_data_section_add_ons(void){
    asm_push("section .data");
    vector_set_peek_pointer(current_process->generator->custom_data_section, 0);
    const char* str = vector_peek_ptr(current_process->generator->custom_data_section);
    while(str){
        asm_push(str);
        str = vector_peek_ptr(current_process->generator->custom_data_section);
    }
}

static void codegen_x86_64_asm_push(const char* ins, ...){
    va_list args;
    va_start(args, ins);
    asm_push_args(ins, args);
    va_end(args);
}

static void codegen_x86_64_ret(struct datatype* dtype, const char* fmt, ...){
    char tmp_buf[200];
    sprintf(tmp_buf, "push %s", fmt);
    va_list args;
    va_start(args, fmt);
    asm_push_args(tmp_buf, args);
    va_end(args);

    assert(current_function);
    stackframe_push(current_function, &(struct stack_frame_element){
        .type      = STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE,
        .name      = "result_value",
        .flags     = STACK_FRAME_ELEMENT_FLAG_HAS_DATATYPE,
        .data.dtype = *dtype,
    });
}

// va_start: the native has put the address of the last named
// parameter in r11 (its home-area slot); store it in the va_list.
static void codegen_x86_64_va_start_store(struct generator* generator, const char* list_address){
    (void)generator;
    asm_push("mov qword [%s], r11", list_address);
}

// va_arg: r11 holds the address of the va_list. Every home-area slot
// is 8 bytes whatever the argument type, so step by at least 8.
static void codegen_x86_64_va_arg_fetch(struct generator* generator, int size){
    struct datatype void_dtype;
    datatype_set_void(&void_dtype);
    void_dtype.pointer_depth++;
    void_dtype.flags |= DATATYPE_FLAG_IS_POINTER;
    asm_push("add qword [r11], %i", align_value(size, DATA_SIZE_DDWORD));
    asm_push("mov rax, qword [r11]");
    generator->ret(&void_dtype, "qword [rax]");
}

static void codegen_x86_64_gen_exp(struct generator* generator, struct node* node, int flags){
    (void)generator;
    codegen_generate_expressionable(node, codegen_history_begin(flags));
}

static void codegen_x86_64_end_exp(struct generator* generator){
    (void)generator;
}

int codegen_x86_64(struct compile_process* process){
    current_process = process;
    x86_64_codegen.compiler = current_process;
    scope_create_root(process);

    // rip-relative [name] for every global access.
    asm_push("default rel");
    vector_set_peek_pointer(process->node_tree_vec, 0);
    codegen_new_scope(0);
    codegen_generate_data_section();

    vector_set_peek_pointer(process->node_tree_vec, 0);
    codegen_generate_root();
    codegen_finish_scope();

    codegen_generate_data_section_add_ons();
    codegen_generate_rod();
    return CODEGEN_ALL_OK;
}
