#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "compiler.h"
#include "helpers/vector.h"

// RISC-V (rv64gc, LP64 psABI) backend, GNU as syntax. Derived
// from codegen_riscv64.c: same stack machine, same function names,
// same callee-owned home area. What changes:
//   - registers: a0 holds the value, a1 the right operand, t0 the
//     address (ebx on i386), t1 the store address. t2 and t3 are
//     scratch for addresses and constants, t4 for the remainder of a
//     struct copy. s0 is the frame pointer, ra the return address.
//   - a push is `addi sp, sp, -16` / `sd a0, 0(sp)`, a pop
//     `ld a0, 0(sp)` / `addi sp, sp, 16`. The psABI keeps sp
//     16-byte aligned, so every slot is 16 bytes, as on aarch64.
//   - memory operands are `offset(base)` with a 12-bit signed
//     offset; anything bigger, and every global, is built in t2
//     first (`li` + `add`, or `lla`).
//   - there are no condition flags: compares produce 0/1 with
//     slt/seqz/snez and branches test a register (beqz/bnez/beq).
//   - int arithmetic uses the *w instructions (addw, subw, mulw,
//     divw, remw, sllw, sraw), which compute on 32 bits and
//     sign-extend the result, so ints stay canonical with no extra
//     instruction.
//   - arguments in a0-a7, spilled by the callee into the home area,
//     so parameter i is at 16+8*i(s0). A struct return pointer is a
//     hidden first argument in a0 and moves the visible arguments to
//     a1 onwards.

static struct compile_process* current_process  = 0;
static struct node*            current_function = 0;

static void codegen_riscv64_asm_push(const char* ins, ...);
static void codegen_riscv64_gen_exp(struct generator* generator, struct node* node, int flags);
static void codegen_riscv64_end_exp(struct generator* generator);
static void codegen_riscv64_entity_address(struct generator* generator, struct resolver_entity* entity, struct generator_entity_address* address_out);
static void codegen_riscv64_ret(struct datatype* dtype, const char* fmt, ...);
static void codegen_riscv64_va_start_store(struct generator* generator, const char* list_address);
static void codegen_riscv64_va_arg_fetch(struct generator* generator, int size);

static const char* codegen_arg_registers[] = { "a0", "a1", "a2", "a3", "a4", "a5", "a6", "a7" };

#define CODEGEN_SLOT 16

struct history;
static struct _riscv64_generator_private {
    struct riscv64_generator_remembered {
        struct history* history;
    } remembered;
} _riscv64_generator_private;

static struct generator riscv64_codegen = {
    .asm_push       = codegen_riscv64_asm_push,
    .gen_exp        = codegen_riscv64_gen_exp,
    .end_exp        = codegen_riscv64_end_exp,
    .entity_address = codegen_riscv64_entity_address,
    .ret            = codegen_riscv64_ret,
    .va_start_store = codegen_riscv64_va_start_store,
    .va_arg_fetch   = codegen_riscv64_va_arg_fetch,
    .private        = &_riscv64_generator_private,
};

enum {
    CODEGEN_ENTITY_RULE_IS_STRUCT_OR_UNION_NON_POINTER = 0b00000001,
    CODEGEN_ENTITY_RULE_IS_FUNCTION_CALL               = 0b00000010,
    CODEGEN_ENTITY_RULE_IS_GET_ADDRESS                 = 0b00000100,
    CODEGEN_ENTITY_RULE_WILL_PEEK_AT_EBX               = 0b00001000,
};

struct history_exp {
    const char* logical_start_op;
    char        logical_end_label[24];
    char        logical_end_label_positive[24];
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

// One expression-stack push: make a 16-byte slot, store the register
// in its low 8 bytes.
static void asm_push_reg(const char* reg, int type, const char* name, int flags, struct datatype* dtype){
    asm_push("addi sp, sp, -16");
    asm_push("sd %s, 0(sp)", reg);
    assert(current_function);
    struct stack_frame_element el = { .type = type, .name = name, .flags = flags };
    if(dtype){
        el.flags |= STACK_FRAME_ELEMENT_FLAG_HAS_DATATYPE;
        el.data.dtype = *dtype;
    }
    stackframe_push(current_function, &el);
}

static void asm_push_ins_push(const char* reg, int stack_entity_type, const char* stack_entity_name){
    asm_push_reg(reg, stack_entity_type, stack_entity_name, 0, 0);
}

static void asm_push_ins_push_with_flags(const char* reg, int stack_entity_type, const char* stack_entity_name, int flags){
    asm_push_reg(reg, stack_entity_type, stack_entity_name, flags, 0);
}

static void asm_push_ins_push_with_data(const char* reg, int stack_entity_type, const char* stack_entity_name, int flags, struct stack_frame_data* data){
    asm_push_reg(reg, stack_entity_type, stack_entity_name, flags, &data->dtype);
}

static int asm_push_ins_pop(const char* reg, int expecting_stack_entity_type, const char* expecting_stack_entity_name){
    asm_push("ld %s, 0(sp)", reg);
    asm_push("addi sp, sp, 16");
    assert(current_function);
    struct stack_frame_element* el = stackframe_back(current_function);
    int flags = el->flags;
    stackframe_pop_expecting(current_function, expecting_stack_entity_type, expecting_stack_entity_name);
    return flags;
}

static int asm_push_ins_pop_or_ignore(const char* reg, int expecting_stack_entity_type, const char* expecting_stack_entity_name){
    if(!stackframe_back_expect(current_function, expecting_stack_entity_type, expecting_stack_entity_name)){
        return STACK_FRAME_ELEMENT_FLAG_ELEMENT_NOT_FOUND;
    }
    return asm_push_ins_pop(reg, expecting_stack_entity_type, expecting_stack_entity_name);
}

// li takes any constant; the assembler expands it to lui/addi/slli
// as needed.
static void codegen_mov_imm(const char* reg, long long value){
    asm_push("li %s, %lld", reg, value);
}

// dst = src + value for any value; addi takes 12 bits signed.
static void codegen_add_imm(const char* dst, const char* src, long long value){
    if(value >= -2048 && value <= 2047){
        asm_push("addi %s, %s, %lld", dst, src, value);
    } else {
        codegen_mov_imm("t3", value);
        asm_push("add %s, %s, t3", dst, src);
    }
}

static void codegen_stack_sp_change(long long change){
    if(change != 0){
        codegen_add_imm("sp", "sp", change);
    }
}

// Split a resolver address such as "s0-8", "s0-16+4", "t0+8",
// "name" or "name+4" into a base (register or symbol) and a byte
// offset.
// The integer registers this backend names in addresses.
static bool codegen_is_register_name(const char* name){
    static const char* regs[] = { "sp", "s0", "a0", "a1", "t0", "t1", "t2", "t3", "t4", 0 };
    for(int i = 0; regs[i]; i++){
        if(S_EQ(name, regs[i])){
            return true;
        }
    }
    return false;
}

struct codegen_address {
    char      base[128];
    long long offset;
    bool      is_register;
};

static void codegen_parse_address(const char* address, struct codegen_address* out){
    memset(out, 0, sizeof(*out));
    size_t i = 0;
    while(address[i] && (i == 0 || (address[i] != '+' && address[i] != '-'))){
        out->base[i] = address[i];
        i++;
    }
    while(address[i]){
        long long sign = address[i] == '-' ? -1 : 1;
        i++;
        long long n = 0;
        while(address[i] >= '0' && address[i] <= '9'){
            n = n * 10 + (address[i] - '0');
            i++;
        }
        out->offset += sign * n;
    }
    out->is_register = codegen_is_register_name(out->base);
}

// reg = the address itself.
static void codegen_address_into(const char* reg, const char* address){
    struct codegen_address a;
    codegen_parse_address(address, &a);
    if(a.is_register){
        if(a.offset == 0){
            if(!S_EQ(reg, a.base)){
                asm_push("mv %s, %s", reg, a.base);
            }
            return;
        }
        codegen_add_imm(reg, a.base, a.offset);
        return;
    }
    // lla: pc-relative auipc + addi, never through the GOT.
    if(a.offset){
        asm_push("lla %s, %s%+lld", reg, a.base, a.offset);
    } else {
        asm_push("lla %s, %s", reg, a.base);
    }
}

// A memory operand `offset(base)`. Register bases with a 12-bit
// signed offset are used directly; anything else is built in t2.
static void codegen_mem_operand(const char* address, char* out, size_t len){
    struct codegen_address a;
    codegen_parse_address(address, &a);
    if(a.is_register && a.offset >= -2048 && a.offset <= 2047){
        snprintf(out, len, "%lld(%s)", a.offset, a.base);
        return;
    }
    codegen_address_into("t2", address);
    snprintf(out, len, "0(t2)");
}



// Load `size` bytes at address into `reg`, sign or zero extended to
// 64 bits.
static void codegen_load_register(const char* reg, const char* address, size_t size, bool is_signed){
    char mem[160];
    codegen_mem_operand(address, mem, sizeof(mem));
    switch(size){
        case DATA_SIZE_BYTE:  asm_push("%s %s, %s", is_signed ? "lb" : "lbu", reg, mem); break;
        case DATA_SIZE_WORD:  asm_push("%s %s, %s", is_signed ? "lh" : "lhu", reg, mem); break;
        case DATA_SIZE_DWORD: asm_push("%s %s, %s", is_signed ? "lw" : "lwu", reg, mem); break;
        default:              asm_push("ld %s, %s", reg, mem); break;
    }
}

// Store the low `size` bytes of `reg` at address.
static void codegen_store_register(const char* reg, const char* address, size_t size){
    char mem[160];
    codegen_mem_operand(address, mem, sizeof(mem));
    switch(size){
        case DATA_SIZE_BYTE:  asm_push("sb %s, %s", reg, mem); break;
        case DATA_SIZE_WORD:  asm_push("sh %s, %s", reg, mem); break;
        case DATA_SIZE_DWORD: asm_push("sw %s, %s", reg, mem); break;
        default:              asm_push("sd %s, %s", reg, mem); break;
    }
}

// The frame record: ra above s0, in one 16-byte slot.
static void asm_push_rbp(void){
    asm_push("addi sp, sp, -16");
    asm_push("sd ra, 8(sp)");
    asm_push("sd s0, 0(sp)");
    stackframe_push(current_function, &(struct stack_frame_element){
        .type = STACK_FRAME_ELEMENT_TYPE_SAVED_BP, .name = "function_entry_saved_ebp",
    });
}

static void codegen_pop_frame_record(void){
    asm_push("ld ra, 8(sp)");
    asm_push("ld s0, 0(sp)");
    asm_push("addi sp, sp, 16");
}

static void asm_pop_rbp(void){
    codegen_pop_frame_record();
    stackframe_pop_expecting(current_function, STACK_FRAME_ELEMENT_TYPE_SAVED_BP, "function_entry_saved_ebp");
}

static void codegen_stack_add_no_compile_time_stack_frame_restore(size_t stack_size){
    codegen_stack_sp_change((long long)stack_size);
}

static void codegen_stack_sub_with_name(size_t stack_size, const char* name){
    if(stack_size != 0){
        stackframe_sub(current_function, STACK_FRAME_ELEMENT_TYPE_UNKNOWN, name, stack_size);
        codegen_stack_sp_change(-(long long)stack_size);
    }
}

static void codegen_stack_sub(size_t stack_size){
    codegen_stack_sub_with_name(stack_size, "literal_stack_change");
}

static void codegen_stack_add_with_name(size_t stack_size, const char* name){
    if(stack_size != 0){
        stackframe_add(current_function, STACK_FRAME_ELEMENT_TYPE_UNKNOWN, name, stack_size);
        codegen_stack_sp_change((long long)stack_size);
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

// GNU as treats every undefined symbol as external, so a prototype
// needs no directive; the comment keeps the listing comparable.
static void codegen_generate_function_prototype(struct node* node){
    codegen_register_function(node, 0);
    asm_push("# extern %s", node->func.name);
}

static void codegen_generate_function_arguments(struct vector* argument_vector){
    vector_set_peek_pointer(argument_vector, 0);
    struct node* current = vector_peek_ptr(argument_vector);
    while(current){
        codegen_new_scope_entity(current, current->var.aoffset, RESOLVER_DEFAULT_ENTITY_FLAG_IS_LOCAL_STACK);
        current = vector_peek_ptr(argument_vector);
    }
}

static void codegen_generate_number_node(struct node* node, struct history* history){
    (void)history;
    codegen_mov_imm("a0", (int)node->llnum);
    struct datatype dtype = datatype_for_numeric();
    asm_push_ins_push_with_data("a0",
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

// `=` stores a0; `+=` loads, adds and stores back, since RISC-V has
// no add-to-memory.
static void codegen_generate_assignment_instruction_for_operator(size_t size, const char* address, const char* op){
    if(S_EQ(op, "=")){
        codegen_store_register("a0", address, size);
    } else if(S_EQ(op, "+=")){
        codegen_load_register("t3", address, size, true);
        asm_push("add a0, t3, a0");
        codegen_store_register("a0", address, size);
    }
}

static struct resolver_default_entity_data* codegen_entity_private(struct resolver_entity* entity){
    return resolver_default_entity_private(entity);
}

static void codegen_riscv64_entity_address(struct generator* generator, struct resolver_entity* entity, struct generator_entity_address* address_out){
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
        asm_push_ins_pop("a0", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
        codegen_generate_assignment_instruction_for_operator(datatype_element_size(&entity->dtype),
            codegen_entity_private(entity)->address, "=");
    }
}

static void codegen_generate_entity_access_start(struct resolver_result* result, struct resolver_entity* root_assignment_entity, struct history* history){
    if(root_assignment_entity->type == RESOLVER_ENTITY_TYPE_UNSUPPORTED){
        codegen_generate_expressionable(root_assignment_entity->node, history);
    } else if(result->flags & RESOLVER_RESULT_FLAG_FIRST_ENTITY_PUSH_VALUE){
        codegen_load_register("a0", result->base.address, datatype_element_size(&root_assignment_entity->dtype),
            root_assignment_entity->dtype.flags & DATATYPE_FLAG_IS_SIGNED);
        asm_push_ins_push_with_data("a0",
            STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value", 0,
            &(struct stack_frame_data){.dtype = root_assignment_entity->dtype});
    } else if(result->flags & RESOLVER_RESULT_FLAG_FIRST_ENTITY_LOAD_TO_EBX){
        if(root_assignment_entity->next && (root_assignment_entity->next->flags & RESOLVER_ENTITY_FLAG_IS_POINTER_ARRAY_ENTITY)){
            codegen_load_register("t0", result->base.address, DATA_SIZE_DDWORD, false);
        } else {
            codegen_address_into("t0", result->base.address);
        }
        asm_push_ins_push_with_data("t0",
            STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value", 0,
            &(struct stack_frame_data){.dtype = root_assignment_entity->dtype});
    }
}

static void codegen_generate_entity_access_for_variable_or_general(struct resolver_result* result, struct resolver_entity* entity){
    (void)result;
    asm_push_ins_pop("t0", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
    if(entity->flags & RESOLVER_ENTITY_FLAG_DO_INDIRECTION){
        asm_push("ld t0, 0(t0)");
    }
    codegen_add_imm("t0", "t0", entity->offset);
    asm_push_ins_push_with_data("t0",
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
        asm_push("ld t0, 0(t0)");
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
    asm_push("# CAST");
}

// reg *= constant, for index scaling; mul has no immediate form.
static void codegen_mul_imm(const char* reg, long long value){
    codegen_mov_imm("t3", value);
    asm_push("mul %s, %s, t3", reg, reg);
}

static void codegen_generate_entity_access_array_bracket_pointer(struct resolver_result* result, struct resolver_entity* entity){
    (void)result;
    asm_push_ins_pop("t0", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
    codegen_generate_expressionable(entity->array.array_index_node, codegen_history_begin(0));
    asm_push_ins_pop("a0", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
    if(datatype_element_size(&entity->dtype) > DATA_SIZE_BYTE){
        codegen_mul_imm("a0", datatype_size_for_array_access(&entity->dtype));
    }
    asm_push("add t0, t0, a0");
    asm_push_ins_push_with_data("t0",
        STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value", 0,
        &(struct stack_frame_data){.dtype = entity->dtype});
}

static void codegen_generate_entity_access_array_bracket(struct resolver_result* result, struct resolver_entity* entity){
    if(entity->flags & RESOLVER_ENTITY_FLAG_IS_POINTER_ARRAY_ENTITY){
        codegen_generate_entity_access_array_bracket_pointer(result, entity);
        return;
    }
    asm_push_ins_pop("t0", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
    codegen_generate_expressionable(entity->array.array_index_node, codegen_history_begin(0));
    asm_push_ins_pop("a0", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
    if(entity->flags & RESOLVER_ENTITY_FLAG_JUST_USE_OFFSET){
        codegen_add_imm("t0", "t0", entity->offset);
    } else {
        codegen_mul_imm("a0", entity->offset);
        asm_push("add t0, t0, a0");
    }
    asm_push_ins_push_with_data("t0",
        STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value", 0,
        &(struct stack_frame_data){.dtype = entity->dtype});
}

static void codegen_generate_entity_access_for_unary_indirection_for_assignment_left_operand(struct resolver_result* result, struct resolver_entity* entity, struct history* history){
    asm_push("# INDIRECTION");
    int flags = asm_push_ins_pop("t0", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
    (void)flags;
    int gen_entity_rules = codegen_entity_rules(result->last_entity, history);
    (void)gen_entity_rules;
    int depth = entity->indirection.depth - 1;
    codegen_apply_unary_access(depth);
    asm_push_ins_push_with_flags("t0",
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

static void codegen_generate_assignment_part(struct node* node, const char* op, struct history* history){
    struct resolver_result* result = resolver_follow(current_process->resolver, node);
    assert(resolver_result_ok(result));
    struct resolver_entity* root_assignment_entity = resolver_result_entity_root(result);
    size_t size = datatype_element_size(&result->last_entity->dtype);
    struct resolver_entity* next_entity = resolver_result_entity_next(root_assignment_entity);
    if(!next_entity){
        if(datatype_is_struct_or_union_non_pointer(&result->last_entity->dtype)){
            codegen_generate_move_struct(&result->last_entity->dtype, result->base.address, 0);
        } else {
            asm_push_ins_pop("a0", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
            codegen_generate_assignment_instruction_for_operator(size, result->base.address, op);
        }
    } else {
        codegen_generate_entity_access_for_assignment_left_operand(result, root_assignment_entity, node, history);
        asm_push_ins_pop("t1", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
        asm_push_ins_pop("a0", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
        if(datatype_is_struct_or_union_non_pointer(&result->last_entity->dtype)){
            compiler_error(current_process, "riscv64: struct assignment through a pointer or member is not supported\n");
        }
        codegen_generate_assignment_instruction_for_operator(size, "t1", op);
    }
}

static void codegen_generate_assignment_expression(struct node* node, struct history* history){
    codegen_generate_expressionable(node->exp.right,
        codegen_history_down(history, EXPRESSION_IS_ASSIGNMENT | IS_RIGHT_OPERAND_OF_ASSIGNMENT));
    codegen_generate_assignment_part(node->exp.left, node->exp.op, history);
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
    // Same truthiness check as codegen_x86.c, so every backend makes
    // the same decisions.
    return res && res->flags && RESPONSE_FLAG_ACKNOWLEDGED;
}

static bool codegen_response_has_entity(struct response* res){
    return codegen_response_acknowledged(res)
        && (res->flags & RESPONSE_FLAG_RESOLVED_ENTITY)
        && res->data.resolved_entity;
}

// Widen the low `size` bytes of a0 back to 64 bits. There are no
// extend instructions in the base ISA for bytes and halfwords, so
// shift up and back down.
static void codegen_reduce_register(size_t size, bool is_signed){
    switch(size){
        case DATA_SIZE_BYTE:
            if(is_signed){ asm_push("slli a0, a0, 56"); asm_push("srai a0, a0, 56"); }
            else         { asm_push("andi a0, a0, 255"); }
            break;
        case DATA_SIZE_WORD:
            asm_push("slli a0, a0, 48");
            asm_push(is_signed ? "srai a0, a0, 48" : "srli a0, a0, 48");
            break;
        case DATA_SIZE_DWORD:
            if(is_signed){ asm_push("sext.w a0, a0"); }
            else         { asm_push("slli a0, a0, 32"); asm_push("srli a0, a0, 32"); }
            break;
        default:
            break;
    }
}

static void codegen_gen_mem_access_get_address(struct node* node, int flags, struct resolver_entity* entity){
    (void)node; (void)flags;
    codegen_address_into("t0", codegen_entity_private(entity)->address);
    asm_push_ins_push_with_flags("t0",
        STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value",
        STACK_FRAME_ELEMENT_FLAG_IS_PUSHED_ADDRESS);
}

static void codegen_gen_mem_access(struct node* node, int flags, struct resolver_entity* entity){
    (void)node;
    if(flags & EXPRESSION_GET_ADDRESS){
        codegen_gen_mem_access_get_address(node, flags, entity);
        return;
    }
    if(datatype_is_struct_or_union_non_pointer(&entity->dtype)){
        codegen_gen_mem_access_get_address(node, 0, entity);
        asm_push_ins_pop("t0", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
        codegen_generate_structure_push_or_return(entity, codegen_history_begin(0), 0);
    } else {
        codegen_load_register("a0", codegen_entity_private(entity)->address, datatype_element_size(&entity->dtype),
            entity->dtype.flags & DATATYPE_FLAG_IS_SIGNED);
        asm_push_ins_push_with_data("a0",
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

static void codegen_generate_unary_indirection(struct node* node, struct history* history){
    int flags = history->flags;
    codegen_response_expect();
    codegen_generate_expressionable(node->unary.operand,
        codegen_history_down(history, flags | EXPRESSION_GET_ADDRESS | EXPRESSION_INDIRECTION));
    struct response* res = codegen_response_pull();
    assert(codegen_response_has_entity(res));
    struct datatype operand_datatype;
    assert(asm_datatype_back(&operand_datatype));
    asm_push_ins_pop("t0", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");

    int depth = node->unary.indirection.depth;
    int real_depth = depth;
    bool loads_value = !(history->flags & EXPRESSION_GET_ADDRESS);
    if(loads_value){
        depth++;
    }
    bool sized_last = loads_value && real_depth == res->data.resolved_entity->dtype.pointer_depth;
    for(int i = 0; i < depth; i++){
        if(sized_last && i == depth - 1){
            codegen_load_register("t0", "t0", datatype_size_no_ptr(&operand_datatype),
                operand_datatype.flags & DATATYPE_FLAG_IS_SIGNED);
        } else {
            asm_push("ld t0, 0(t0)");
        }
    }
    asm_push_ins_push_with_data("t0",
        STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value", 0,
        &(struct stack_frame_data){.dtype = operand_datatype});
    codegen_response_acknowledge(&(struct response){
        .flags = RESPONSE_FLAG_RESOLVED_ENTITY,
        .data.resolved_entity = res->data.resolved_entity,
    });
}

// Full-width instructions for long and pointers, the *w forms otherwise.
static bool codegen_is_wide(struct datatype* dtype){
    return datatype_element_size(dtype) == DATA_SIZE_DDWORD;
}

static void codegen_generate_normal_unary(struct node* node, struct history* history){
    codegen_generate_expressionable(node->unary.operand, history);
    struct datatype last_dtype;
    assert(asm_datatype_back(&last_dtype));
    asm_push_ins_pop("a0", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
    bool wide = codegen_is_wide(&last_dtype);
    if(S_EQ(node->unary.op, "-")){
        asm_push(wide ? "neg a0, a0" : "negw a0, a0");
        asm_push_ins_push_with_data("a0",
            STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value", 0,
            &(struct stack_frame_data){.dtype = last_dtype});
    } else if(S_EQ(node->unary.op, "~")){
        // not of a sign-extended int is still sign-extended
        asm_push("not a0, a0");
        asm_push_ins_push_with_data("a0",
            STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value", 0,
            &(struct stack_frame_data){.dtype = last_dtype});
    } else if(S_EQ(node->unary.op, "*")){
        codegen_generate_unary_indirection(node, history);
    } else if(S_EQ(node->unary.op, "!")){
        asm_push("seqz a0, a0");
        asm_push_ins_push_with_data("a0",
            STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value", 0,
            &(struct stack_frame_data){.dtype = last_dtype});
    }
}

static void codegen_generate_string(struct node* node, struct history* history){
    (void)history;
    const char* label = codegen_register_string(node->sval);
    codegen_address_into("a0", label);
    asm_push_ins_push_with_data("a0",
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
    asm_push_ins_pop("a0", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
    codegen_reduce_register(datatype_size(&node->cast.dtype),
        node->cast.dtype.flags & DATATYPE_FLAG_IS_SIGNED);
    asm_push_ins_push_with_data("a0",
        STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value", 0,
        &(struct stack_frame_data){.dtype = node->cast.dtype});
}

static void codegen_generate_tenary(struct node* node, struct history* history){
    int true_label_id       = codegen_label_count();
    int false_label_id      = codegen_label_count();
    int tenary_end_label_id = codegen_label_count();

    struct datatype last_dtype;
    assert(asm_datatype_back(&last_dtype));
    asm_push_ins_pop("a0", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
    asm_push("beqz a0, .Ltenary_false_%i", false_label_id);
    asm_push(".Ltenary_true_%i:", true_label_id);
    codegen_generate_expressionable(node->tenary.true_node, codegen_history_down(history, 0));
    asm_push_ins_pop_or_ignore("a0", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
    asm_push("j .Ltenary_end_%i", tenary_end_label_id);
    asm_push(".Ltenary_false_%i:", false_label_id);
    codegen_generate_expressionable(node->tenary.false_node, codegen_history_down(history, 0));
    asm_push_ins_pop_or_ignore("a0", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
    asm_push(".Ltenary_end_%i:", tenary_end_label_id);
}

static void codegen_generate_unary(struct node* node, struct history* history){
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

// Read-side `*p` in an access chain. As on x86_64, the last
// step reads a non-pointer value at its real width.
static void codegen_generate_entity_access_for_unary_indirection(struct resolver_result* result, struct resolver_entity* entity, struct history* history){
    (void)history;
    asm_push("# INDIRECTION");
    struct datatype operand_datatype;
    assert(asm_datatype_back(&operand_datatype));
    int flags = asm_push_ins_pop("t0", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
    (void)flags;
    int gen_entity_rules = codegen_entity_rules(result->last_entity, history);
    (void)gen_entity_rules;
    int depth = entity->indirection.depth;
    if(depth > 0 && !(entity->dtype.flags & DATATYPE_FLAG_IS_POINTER)){
        codegen_apply_unary_access(depth - 1);
        codegen_load_register("t0", "t0", datatype_element_size(&entity->dtype),
            entity->dtype.flags & DATATYPE_FLAG_IS_SIGNED);
    } else {
        codegen_apply_unary_access(depth);
    }
    asm_push_ins_push_with_data("t0",
        STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value",
        STACK_FRAME_ELEMENT_FLAG_IS_PUSHED_ADDRESS,
        &(struct stack_frame_data){.dtype = operand_datatype});
}

static void codegen_generate_entity_access_for_unary_get_address(struct resolver_result* result, struct resolver_entity* entity){
    (void)result;
    asm_push_ins_pop("t0", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
    asm_push("# PUSH ADDRESS &");
    asm_push_ins_push_with_data("t0",
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
            asm_push("# NATIVE FUNCTION %s", root_assignment_entity->name);
            struct resolver_entity* func_call_entity = resolver_result_entity_next(root_assignment_entity);
            assert(func_call_entity && func_call_entity->type == RESOLVER_ENTITY_TYPE_FUNCTION_CALL);
            native_func->callbacks.call(&riscv64_codegen, native_func, func_call_entity->func_call_data.arguments);
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

// A function call, same order as on aarch64: callee address kept on
// the stack under the arguments, arguments evaluated last to first,
// then popped into a0-a7 (first argument on top). A struct return
// reserves the result on the caller's stack and passes its address
// as a hidden first argument in a0, so the visible arguments start
// at a1. Every slot is 16 bytes, so sp is 16-byte aligned at the
// jalr.
static void codegen_generate_entity_access_for_function_call(struct resolver_result* result, struct resolver_entity* entity){
    (void)result;
    bool returns_struct = datatype_is_struct_or_union_non_pointer(&entity->dtype);
    int  total_args     = vector_count(entity->func_call_data.arguments);
    int  first_reg      = returns_struct ? 1 : 0;
    int  max_args       = current_process->target->max_reg_args - first_reg;
    if(total_args > max_args){
        compiler_error(current_process, "riscv64: calls with more than %i register arguments are not supported\n", max_args);
    }

    vector_set_flag(entity->func_call_data.arguments, VECTOR_FLAG_PEEK_DECREMENT);
    vector_set_peek_pointer_end(entity->func_call_data.arguments);
    struct node* node = vector_peek_ptr(entity->func_call_data.arguments);

    asm_push_ins_pop("t0", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
    if(returns_struct){
        asm_push("# SUBTRACT ROOM FOR RETURNED STRUCTURE/UNION DATATYPE");
        codegen_stack_sub_with_name(align_value(datatype_size(&entity->dtype), CODEGEN_SLOT), "result_value");
        asm_push("mv a0, sp");
        asm_push_ins_push("a0", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "struct_return_pointer");
    }
    asm_push_ins_push("t0", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "function_call_target");

    while(node){
        size_t before = vector_count(current_function->func.frame.elements);
        codegen_generate_expressionable(node, codegen_history_begin(EXPRESSION_IN_FUNCTION_CALL_ARGUMENTS));
        if(vector_count(current_function->func.frame.elements) != before + 1){
            compiler_error(current_process, "riscv64: an argument must fit one register (no struct arguments by value)\n");
        }
        node = vector_peek_ptr(entity->func_call_data.arguments);
    }

    for(int i = 0; i < total_args; i++){
        asm_push_ins_pop(codegen_arg_registers[first_reg + i], STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
    }
    asm_push_ins_pop("t0", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "function_call_target");
    if(returns_struct){
        asm_push_ins_pop("a0", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "struct_return_pointer");
    }
    asm_push("jalr t0");

    if(returns_struct){
        // The result sits at the top of the stack, where a0 pointed.
        asm_push("mv t0, sp");
        codegen_generate_structure_push(entity, codegen_history_begin(0), 0);
    } else {
        // The psABI returns int sign-extended, but a char or short
        // result still needs narrowing like on the other targets.
        if(!(entity->dtype.flags & DATATYPE_FLAG_IS_POINTER)){
            codegen_reduce_register(datatype_size(&entity->dtype), entity->dtype.flags & DATATYPE_FLAG_IS_SIGNED);
        }
        asm_push_ins_push_with_data("a0",
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

static bool codegen_resolve_node_for_value(struct node* node, struct history* history){
    struct resolver_result* result = 0;
    if(!codegen_resolve_node_return_result(node, history, &result)){
        return false;
    }
    struct datatype dtype;
    assert(asm_datatype_back(&dtype));
    if(datatype_is_struct_or_union_non_pointer(&dtype)){
        codegen_generate_structure_push(result->last_entity, history, 0);
    } else if(!(dtype.flags & DATATYPE_FLAG_IS_POINTER) && dtype.type != DATA_TYPE_VOID){
        asm_push_ins_pop("a0", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
        if(result->flags & RESOLVER_RESULT_FLAG_FINAL_INDIRECTION_REQUIRED_FOR_VALUE){
            codegen_load_register("a0", "a0", datatype_element_size(&dtype), dtype.flags & DATATYPE_FLAG_IS_SIGNED);
        } else {
            codegen_reduce_register(datatype_element_size(&dtype), dtype.flags & DATATYPE_FLAG_IS_SIGNED);
        }
        asm_push_ins_push_with_data("a0",
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



// x0 = x0 <op> x1. int operations use w registers, so they wrap at 32
// bits, and sxtw restores the 64-bit form.
// a0 = a0 <op> a1. int operations use the *w forms, which work on the
// low 32 bits and sign-extend the result. Compares are always full
// width: ints are kept sign-extended, so a 64-bit compare gives the
// 32-bit answer.
static void codegen_gen_math_for_value(int flags, bool wide, bool is_signed){
    const char* w = wide ? "" : "w";
    if(flags & EXPRESSION_IS_ADDITION){
        asm_push("add%s a0, a0, a1", w);
    } else if(flags & EXPRESSION_IS_SUBTRACTION){
        asm_push("sub%s a0, a0, a1", w);
    } else if(flags & EXPRESSION_IS_MULTIPLICATION){
        asm_push("mul%s a0, a0, a1", w);
    } else if(flags & EXPRESSION_IS_DIVISION){
        asm_push("%s%s a0, a0, a1", is_signed ? "div" : "divu", w);
    } else if(flags & EXPRESSION_IS_MODULAS){
        asm_push("%s%s a0, a0, a1", is_signed ? "rem" : "remu", w);
    } else if(flags & EXPRESSION_IS_ABOVE){
        asm_push("slt a0, a1, a0");
    } else if(flags & EXPRESSION_IS_BELOW){
        asm_push("slt a0, a0, a1");
    } else if(flags & EXPRESSION_IS_EQUAL){
        asm_push("xor a0, a0, a1");
        asm_push("seqz a0, a0");
    } else if(flags & EXPRESSION_IS_ABOVE_OR_EQUAL){
        asm_push("slt a0, a0, a1");
        asm_push("xori a0, a0, 1");
    } else if(flags & EXPRESSION_IS_BELOW_OR_EQUAL){
        asm_push("slt a0, a1, a0");
        asm_push("xori a0, a0, 1");
    } else if(flags & EXPRESSION_IS_NOT_EQUAL){
        asm_push("xor a0, a0, a1");
        asm_push("snez a0, a0");
    } else if(flags & EXPRESSION_IS_BITSHIFT_LEFT){
        asm_push("sll%s a0, a0, a1", w);
    } else if(flags & EXPRESSION_IS_BITSHIFT_RIGHT){
        asm_push("sra%s a0, a0, a1", w);
    } else if(flags & EXPRESSION_IS_BITWISE_AND){
        asm_push("and a0, a0, a1");
    } else if(flags & EXPRESSION_IS_BITWISE_OR){
        asm_push("or a0, a0, a1");
    } else if(flags & EXPRESSION_IS_BITWISE_XOR){
        asm_push("xor a0, a0, a1");
    }
}

static void codegen_setup_new_logical_expression(struct history* history, struct node* node){
    int label_index = codegen_label_count();
    sprintf(history->exp.logical_end_label,          ".Lendc_%i",          label_index);
    sprintf(history->exp.logical_end_label_positive, ".Lendc_%i_positive", label_index);
    history->exp.logical_start_op = node->exp.op;
    history->flags |= EXPRESSION_IN_LOGICAL_EXPRESSION;
}

// beqz/bnez test a register and branch; there are no flags.
static void codegen_generate_logical_cmp(const char* op, const char* fail_label, const char* equal_label){
    if(S_EQ(op, "&&"))      asm_push("beqz a0, %s", fail_label);
    else if(S_EQ(op, "||")) asm_push("bnez a0, %s", equal_label);
}

static void codegen_generate_end_labels_for_logical_expression(const char* op, const char* end_label, const char* end_label_positive){
    if(S_EQ(op, "&&")){
        asm_push("# && END CLAUSE");
        asm_push("li a0, 1");
        asm_push("j %s", end_label_positive);
        asm_push("%s:", end_label);
        asm_push("li a0, 0");
        asm_push("%s:", end_label_positive);
    } else if(S_EQ(op, "||")){
        asm_push("# || END CLAUSE");
        asm_push("j %s", end_label);
        asm_push("%s:", end_label_positive);
        asm_push("li a0, 1");
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
    asm_push_ins_pop("a0", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
    codegen_generate_logical_cmp(node->exp.op, history->exp.logical_end_label, history->exp.logical_end_label_positive);
    codegen_generate_expressionable(node->exp.right,
        codegen_history_down(history, history->flags | EXPRESSION_IN_LOGICAL_EXPRESSION));
    if(!is_logical_node(node->exp.right)){
        asm_push_ins_pop("a0", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
        codegen_generate_logical_cmp(node->exp.op, history->exp.logical_end_label, history->exp.logical_end_label_positive);
        codegen_generate_end_labels_for_logical_expression(node->exp.op, history->exp.logical_end_label, history->exp.logical_end_label_positive);
        asm_push_ins_push("a0", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
    }
}

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
        asm_push_ins_pop("a1", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
        struct datatype left_dtype = datatype_for_numeric();
        asm_datatype_back(&left_dtype);
        asm_push_ins_pop("a0", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
        struct datatype* pointer_dtype = datatype_thats_a_pointer(&left_dtype, &right_dtype);
        if(pointer_dtype && datatype_size(datatype_pointer_reduce(pointer_dtype, 1)) > DATA_SIZE_BYTE){
            const char* reg = (pointer_dtype == &right_dtype) ? "a0" : "a1";
            codegen_mul_imm(reg, datatype_size(datatype_pointer_reduce(pointer_dtype, 1)));
        }
        bool wide = codegen_is_wide(&left_dtype) || codegen_is_wide(&right_dtype);
        if(datatype_element_size(&left_dtype) > datatype_element_size(&right_dtype)){
            last_dtype = left_dtype;
        }
        codegen_gen_math_for_value(op_flags, wide, last_dtype.flags & DATATYPE_FLAG_IS_SIGNED);
    }
    asm_push_ins_push_with_data("a0",
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
        compiler_error(current_process, "riscv64: structure too large to copy by value\n");
    }
    return count;
}

#define CODEGEN_MAX_STRUCT_PIECES 256

// Push a struct whose address is in t0, last piece first.
static void codegen_generate_structure_push(struct resolver_entity* entity, struct history* history, int start_pos){
    (void)history;
    asm_push("# STRUCTURE PUSH");
    int offsets[CODEGEN_MAX_STRUCT_PIECES];
    int sizes[CODEGEN_MAX_STRUCT_PIECES];
    int pieces = codegen_struct_pieces(entity->dtype.size, offsets, sizes, CODEGEN_MAX_STRUCT_PIECES);
    for(int i = pieces - 1; i >= start_pos; i--){
        char address[40];
        snprintf(address, sizeof(address), "t0+%i", offsets[i]);
        codegen_load_register("a0", address, sizes[i], false);
        asm_push_ins_push_with_data("a0",
            STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value", 0,
            &(struct stack_frame_data){.dtype = entity->dtype});
    }
    asm_push("# END STRUCTURE PUSH");
    codegen_response_acknowledge(&(struct response){.flags = RESPONSE_FLAG_PUSHED_STRUCTURE});
}

static void codegen_generate_structure_push_or_return(struct resolver_entity* entity, struct history* history, int start_pos){
    codegen_generate_structure_push(entity, history, start_pos);
}

static void codegen_generate_move_struct(struct datatype* dtype, const char* base_address, int offset){
    int offsets[CODEGEN_MAX_STRUCT_PIECES];
    int sizes[CODEGEN_MAX_STRUCT_PIECES];
    int pieces = codegen_struct_pieces(datatype_size(dtype), offsets, sizes, CODEGEN_MAX_STRUCT_PIECES);
    for(int i = 0; i < pieces; i++){
        asm_push_ins_pop("a0", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
        char address[200];
        snprintf(address, sizeof(address), "%s+%i", base_address, offset + offsets[i]);
        codegen_store_register("a0", address, sizes[i]);
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
        stack_adjustment += CODEGEN_SLOT;
        el = asm_stack_peek();
    }
    codegen_stack_add(stack_adjustment);
}

// The struct-return pointer arrived in a0, so it is in the first
// home-area slot.
static void codegen_generate_statement_return_exp(struct node* node){
    codegen_response_expect();
    codegen_generate_expressionable(node->stmt.return_stmt.exp,
        codegen_history_begin(IS_STATEMENT_RETURN));
    struct datatype dtype;
    assert(asm_datatype_back(&dtype));
    if(datatype_is_struct_or_union_non_pointer(&dtype)){
        asm_push("ld t1, 16(s0)");
        codegen_generate_move_struct(&dtype, "t1", 0);
        asm_push("ld a0, 16(s0)");
        return;
    }
    asm_push_ins_pop("a0", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
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
    asm_push_ins_pop("a0", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
    asm_push("beqz a0, .Lif_%i", if_label_id);
    codegen_generate_body(node->stmt.if_stmt.body_node, codegen_history_begin(IS_ALONE_STATEMENT));
    asm_push("j .Lif_end_%i", end_label_id);
    asm_push(".Lif_%i:", if_label_id);
    if(node->stmt.if_stmt.next){
        codegen_generate_else_or_else_if(node->stmt.if_stmt.next, end_label_id);
    }
}

static void codegen_generate_if_stmt(struct node* node){
    int end_label_id = codegen_label_count();
    _codegen_generate_if_stmt(node, end_label_id);
    asm_push(".Lif_end_%i:", end_label_id);
}

static void codegen_generate_while_stmt(struct node* node){
    codegen_begin_entry_exit_point();
    int while_start_id = codegen_label_count();
    int while_end_id   = codegen_label_count();
    asm_push(".Lwhile_start_%i:", while_start_id);
    codegen_generate_expressionable(node->stmt.while_stmt.exp_node, codegen_history_begin(0));
    asm_push_ins_pop("a0", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
    asm_push("beqz a0, .Lwhile_end_%i", while_end_id);
    codegen_generate_body(node->stmt.while_stmt.body_node, codegen_history_begin(IS_ALONE_STATEMENT));
    asm_push("j .Lwhile_start_%i", while_start_id);
    asm_push(".Lwhile_end_%i:", while_end_id);
    codegen_end_entry_exit_point();
}

static void codegen_generate_for_stmt(struct node* node){
    struct for_stmt* fs = &node->stmt.for_stmt;
    int for_loop_start_id = codegen_label_count();
    int for_loop_end_id   = codegen_label_count();
    if(fs->init_node){
        codegen_generate_expressionable(fs->init_node, codegen_history_begin(0));
        asm_push_ins_pop_or_ignore("a0", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
    }
    asm_push("j .Lfor_loop%i", for_loop_start_id);
    codegen_begin_entry_exit_point();
    if(fs->loop_node){
        codegen_generate_expressionable(fs->loop_node, codegen_history_begin(0));
        asm_push_ins_pop_or_ignore("a0", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
    }
    asm_push(".Lfor_loop%i:", for_loop_start_id);
    if(fs->cond_node){
        codegen_generate_expressionable(fs->cond_node, codegen_history_begin(0));
        asm_push_ins_pop_or_ignore("a0", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
        asm_push("beqz a0, .Lfor_loop_end%i", for_loop_end_id);
    }
    if(fs->body_node){
        codegen_generate_body(fs->body_node, codegen_history_begin(IS_ALONE_STATEMENT));
    }
    if(fs->loop_node){
        codegen_generate_expressionable(fs->loop_node, codegen_history_begin(0));
        asm_push_ins_pop_or_ignore("a0", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
    }
    asm_push("j .Lfor_loop%i", for_loop_start_id);
    asm_push(".Lfor_loop_end%i:", for_loop_end_id);
    codegen_end_entry_exit_point();
}

static void codegen_generate_switch_default_stmt(struct node* node){
    (void)node;
    asm_push("# DEFAULT CASE");
    struct code_generator* gen = current_process->generator;
    asm_push(".Lswitch_stmt_%i_case_default:", gen->_switch.current.id);
}

// The compare chain: the case value goes into a1, beq compares two
// registers.
static void codegen_generate_switch_stmt_case_jumps(struct node* node){
    vector_set_peek_pointer(node->stmt.switch_stmt.cases, 0);
    struct parsed_switch_case* sc = vector_peek(node->stmt.switch_stmt.cases);
    while(sc){
        codegen_mov_imm("a1", sc->index);
        asm_push("beq a0, a1, .Lswitch_stmt_%i_case_%i", codegen_switch_id(), sc->index);
        sc = vector_peek(node->stmt.switch_stmt.cases);
    }
    if(node->stmt.switch_stmt.has_default_case){
        asm_push("j .Lswitch_stmt_%i_case_default", codegen_switch_id());
        return;
    }
    codegen_goto_exit_point_maintain_stack(node);
}

static void codegen_generate_switch_stmt(struct node* node){
    codegen_begin_entry_exit_point();
    codegen_begin_switch_statement();
    codegen_generate_expressionable(node->stmt.switch_stmt.exp, codegen_history_begin(0));
    asm_push_ins_pop_or_ignore("a0", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
    codegen_generate_switch_stmt_case_jumps(node);
    codegen_generate_body(node->stmt.switch_stmt.body, codegen_history_begin(IS_ALONE_STATEMENT));
    codegen_end_switch_statement();
    codegen_end_entry_exit_point();
}

static void codegen_generate_goto_stmt(struct node* node){
    asm_push("j .Llabel_%s", node->stmt._goto.label->sval);
}

static void codegen_generate_label(struct node* node){
    asm_push(".Llabel_%s:", node->stmt.label.name->sval);
}

static void codegen_generate_switch_case_stmt(struct node* node){
    struct node* case_exp = node->stmt._case.exp;
    assert(case_exp->type == NODE_TYPE_NUMBER);
    codegen_begin_case_statement(case_exp->llnum);
    asm_push("# CASE %i", (int)case_exp->llnum);
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
    asm_push(".Ldo_while_start_%i:", do_while_start_id);
    codegen_generate_body(node->stmt.do_while_stmt.body_node, codegen_history_begin(IS_ALONE_STATEMENT));
    codegen_generate_expressionable(node->stmt.do_while_stmt.exp_node, codegen_history_begin(0));
    asm_push_ins_pop("a0", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
    asm_push("bnez a0, .Ldo_while_start_%i", do_while_start_id);
    codegen_end_entry_exit_point();
}

static int codegen_home_area_size(void){
    return current_process->target->max_reg_args * DATA_SIZE_DDWORD;
}

// Epilogue after the frame record is popped: drop the home area,
// return through ra.
static void codegen_generate_epilogue_ret(void){
    asm_push("addi sp, sp, %i", codegen_home_area_size());
    asm_push("ret");
}

static void codegen_generate_statement_return(struct node* node){
    if(node->stmt.return_stmt.exp){
        codegen_generate_statement_return_exp(node);
    }
    codegen_stack_add_no_compile_time_stack_frame_restore(
        C_ALIGN(function_node_stack_size(node->binded.function)));
    codegen_pop_frame_record();
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

// Prologue. The home area for the argument registers is allocated
// first, then the frame record goes below it:
//
//   72(s0) a7 ... 16(s0) a0   home area (callee-owned)
//   8(s0)  ra
//   0(s0)  s0 of the caller
//
// so parameter i is at 16+8*i(s0). s0 points at the frame record,
// like x29 on aarch64; the psABI convention of s0 = sp on entry
// would put the parameters at 0(s0) and need a different frame
// header in the parser. A struct return pointer arrives in a0 and so
// lands in the first slot without special handling.
static void codegen_generate_function_with_body(struct node* node){
    codegen_register_function(node, 0);
    asm_push(".globl %s", node->func.name);
    asm_push(".type %s, @function", node->func.name);
    asm_push("# %s function", node->func.name);
    asm_push("%s:", node->func.name);

    asm_push("addi sp, sp, -%i", codegen_home_area_size());
    asm_push_rbp();
    asm_push("mv s0, sp");
    for(int i = 0; i < 8; i++){
        asm_push("sd a%i, %i(s0)", i, 16 + 8 * i);
    }
    codegen_stack_sub(C_ALIGN(function_node_stack_size(node)));
    codegen_new_scope(RESOLVER_DEFAULT_ENTITY_FLAG_IS_LOCAL_STACK);
    codegen_generate_function_arguments(function_node_argument_vec(node));

    codegen_generate_body(node->func.body_n, codegen_history_begin(IS_ALONE_STATEMENT));
    codegen_finish_scope();
    codegen_stack_add(C_ALIGN(function_node_stack_size(node)));
    asm_pop_rbp();
    stackframe_assert_empty(current_function);
    codegen_generate_epilogue_ret();
    asm_push(".size %s, .-%s", node->func.name, node->func.name);
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
    asm_push(".Lexit_point_%i:", ep->id);
    free(ep);
    vector_pop(gen->exit_points);
}

static void codegen_goto_exit_point(struct node* node){
    (void)node;
    struct codegen_exit_point* ep = codegen_current_exit_point();
    asm_push("j .Lexit_point_%i", ep->id);
}

static void codegen_goto_exit_point_maintain_stack(struct node* node){
    (void)node;
    struct codegen_exit_point* ep = codegen_current_exit_point();
    asm_push("j .Lexit_point_%i", ep->id);
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
    asm_push(".Lentry_point_%i:", id);
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
    asm_push("j .Lentry_point_%i", ep->id);
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
    asm_push(".Lswitch_stmt_%i:", switch_stmt_id);
    sd->current.id = switch_stmt_id;
}

static void codegen_end_switch_statement(void){
    struct code_generator* gen = current_process->generator;
    struct generator_switch_stmt* sd = &gen->_switch;
    asm_push(".Lswitch_stmt_%i_end:", sd->current.id);
    memcpy(&sd->current, vector_back(sd->swtiches), sizeof(struct generator_switch_stmt_entity));
    vector_pop(sd->swtiches);
}

static int codegen_switch_id(void){
    struct code_generator* gen = current_process->generator;
    return gen->_switch.current.id;
}

static void codegen_begin_case_statement(int index){
    struct code_generator* gen = current_process->generator;
    asm_push(".Lswitch_stmt_%i_case_%i:", gen->_switch.current.id, index);
}

static void codegen_end_case_statement(void){
}

// GNU as data directive for a scalar of `size` bytes.
static const char* asm_directive_for_size(size_t size){
    switch(size){
        case DATA_SIZE_BYTE:   return ".byte";
        case DATA_SIZE_WORD:   return ".half";
        case DATA_SIZE_DWORD:  return ".word";
        case DATA_SIZE_DDWORD: return ".dword";
    }
    return 0;
}

// Align each global to its own size (8 at most), then the label and
// its value. Sizes that are not a scalar become a zero-filled block.
static void codegen_generate_global_data(const char* name, size_t size, const char* value){
    size_t align = size >= 8 ? 8 : size >= 4 ? 4 : size >= 2 ? 2 : 1;
    asm_push(".balign %lu", (unsigned long)align);
    const char* directive = asm_directive_for_size(size);
    if(directive){
        asm_push("%s: %s %s", name, directive, value);
    } else {
        asm_push("%s: .zero %lu", name, (unsigned long)size);
    }
}

static void codegen_generate_global_variable_for_primitive(struct node* node){
    char value[64];
    if(node->var.val){
        if(node->var.val->type == NODE_TYPE_STRING){
            codegen_generate_global_data(node->var.name, variable_size(node),
                codegen_register_string(node->var.val->sval));
            return;
        }
        snprintf(value, sizeof(value), "%lld", node->var.val->llnum);
    } else {
        snprintf(value, sizeof(value), "0");
    }
    codegen_generate_global_data(node->var.name, variable_size(node), value);
}

static void codegen_generate_global_variable_for_struct(struct node* node){
    if(node->var.val){
        compiler_error(current_process, "We dont yet support values for structures");
        return;
    }
    codegen_generate_global_data(node->var.name, variable_size(node), "0");
}

static void codegen_generate_global_variable_for_union(struct node* node){
    if(node->var.val){
        compiler_error(current_process, "We dont yet support values for unions");
        return;
    }
    codegen_generate_global_data(node->var.name, variable_size(node), "0");
}

static void codegen_generate_variable_for_array(struct node* node){
    if(node->var.val){
        compiler_error(current_process, "We don't support values for arrays yet");
        return;
    }
    codegen_generate_global_data(node->var.name, variable_size(node), "0");
}

static void codegen_generate_global_variable(struct node* node){
    asm_push("# %s %s", node->var.type.type_str, node->var.name);
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
    asm_push(".data");
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
    asm_push(".text");
    // Without the compressed extension in use, instructions are 4
    // bytes and 4-byte aligned.
    asm_push(".p2align 2");
    struct node* node = 0;
    while((node = codegen_node_next()) != 0){
        codegen_generate_root_node(node);
    }
}

// One string as .asciz, with everything outside printable ASCII (and
// the quote and backslash) written as an octal escape.
static void codegen_write_string(struct string_table_element* element){
    char out[4096];
    size_t n = 0;
    for(const char* c = element->str; *c && n < sizeof(out) - 5; c++){
        unsigned char ch = (unsigned char)*c;
        if(ch < 32 || ch >= 127 || ch == '"' || ch == '\\'){
            n += sprintf(out + n, "\\%03o", ch);
        } else {
            out[n++] = ch;
        }
    }
    out[n] = 0;
    asm_push("%s: .asciz \"%s\"", element->label, out);
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
    asm_push(".section .rodata");
    codegen_write_strings();
}

// The natives write their comments in NASM style ("; ..."); RISC-V
// GNU as comments start with `#`.
static void codegen_riscv64_asm_push(const char* ins, ...){
    char line[512];
    va_list args;
    va_start(args, ins);
    vsnprintf(line, sizeof(line), ins, args);
    va_end(args);
    if(line[0] == ';'){
        asm_push("#%s", line + 1);
        return;
    }
    asm_push("%s", line);
}

// ret from a native: fmt names a register to push or is a constant.
static void codegen_riscv64_ret(struct datatype* dtype, const char* fmt, ...){
    char value[64];
    va_list args;
    va_start(args, fmt);
    vsnprintf(value, sizeof(value), fmt, args);
    va_end(args);
    const char* reg = value;
    if(!codegen_is_register_name(value)){
        codegen_mov_imm("a0", atoll(value));
        reg = "a0";
    }
    asm_push_reg(reg, STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value", 0, dtype);
}

// va_start: t0 holds the address of the last named parameter (its
// home-area slot); store it in the va_list.
static void codegen_riscv64_va_start_store(struct generator* generator, const char* list_address){
    (void)generator;
    codegen_store_register("t0", list_address, DATA_SIZE_DDWORD);
}

// va_arg: t0 holds the address of the va_list. Step it one 8-byte
// home-area slot (or more for a bigger type) and load the argument.
static void codegen_riscv64_va_arg_fetch(struct generator* generator, int size){
    struct datatype void_dtype;
    datatype_set_void(&void_dtype);
    void_dtype.pointer_depth++;
    void_dtype.flags |= DATATYPE_FLAG_IS_POINTER;
    asm_push("ld t2, 0(t0)");
    codegen_add_imm("t2", "t2", align_value(size, DATA_SIZE_DDWORD));
    asm_push("sd t2, 0(t0)");
    asm_push("ld a0, 0(t2)");
    generator->ret(&void_dtype, "a0");
}

static void codegen_riscv64_gen_exp(struct generator* generator, struct node* node, int flags){
    (void)generator;
    codegen_generate_expressionable(node, codegen_history_begin(flags));
}

static void codegen_riscv64_end_exp(struct generator* generator){
    (void)generator;
}

int codegen_riscv64(struct compile_process* process){
    current_process = process;
    riscv64_codegen.compiler = current_process;
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
