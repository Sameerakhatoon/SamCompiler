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

// ch236: forward decls + the global x86 generator v-table. native
// functions live behind this so #include <stdarg.h> can plug into
// codegen without the codegen module knowing anything about them.
// asm_push itself is static in this file (ch143); codegen_asm_push
// is a non-static shim so the v-table can take its address.
void codegen_asm_push(const char* ins, ...);
struct _x86_generator_private* x86_generator_private(struct generator* generator);
void codegen_gen_exp(struct generator* generator, struct node* node, int flags);
void codegen_end_exp(struct generator* generator);
void codegen_entity_address(struct generator* generator, struct resolver_entity* entity, struct generator_entity_address* address_out);
void asm_push_ins_with_datatype(struct datatype* dtype, const char* fmt, ...);
static void codegen_x86_va_start_store(struct generator* generator, const char* list_address, int last_size);
static void codegen_x86_va_arg_fetch(struct generator* generator, int size);

struct history;
struct _x86_generator_private {
    struct x86_generator_remembered {
        struct history* history;
    } remembered;
} _x86_generator_private;

struct generator x86_codegen = {
    .asm_push       = codegen_asm_push,
    .gen_exp        = codegen_gen_exp,
    .end_exp        = codegen_end_exp,
    .entity_address = codegen_entity_address,
    .ret            = asm_push_ins_with_datatype,
    .va_start_store = codegen_x86_va_start_store,
    .va_arg_fetch   = codegen_x86_va_arg_fetch,
    .private        = &_x86_generator_private,
};

// ch154: codegen entity-rule flags for the entity-access dispatchers.
enum {
    CODEGEN_ENTITY_RULE_IS_STRUCT_OR_UNION_NON_POINTER = 0b00000001,
    CODEGEN_ENTITY_RULE_IS_FUNCTION_CALL               = 0b00000010,
    CODEGEN_ENTITY_RULE_IS_GET_ADDRESS                 = 0b00000100,
    CODEGEN_ENTITY_RULE_WILL_PEEK_AT_EBX               = 0b00001000,
};

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

// position independent code. The generator below
// writes absolute addresses (`[$g+4]`, `lea ebx, [$f]`, `mov eax,
// str_3`, `call $f`, `fld qword [x87_const_2]`); every line of .text
// passes through codegen_pic_line, which turns them into PIC:
//
// - esi holds the GOT's address for the whole function (set in the
//   prologue, callee-saved, used by nothing else here).
// - A symbol that binds locally (a backend label, or a C name this file
//   defines, only a `static` one under -fPIC) is reached GOT-relative:
//   `[esi+$g+4 wrt ..gotoff]` (R_386_GOTOFF).
// - Any other C name may be in a shared library, or interposed: edi,
//   the second reserved register, is loaded from its GOT entry first,
//   `mov edi, [esi+$g wrt ..got]`, and the operand becomes `[edi+4]`.
// - `mov reg, label` (an address as an immediate) becomes `lea`.
// - `call $f` to such a symbol becomes `call [esi+$f wrt ..got]`.
//
// Calls never go through the PLT: an i386 PLT entry wants the GOT's
// address in ebx, which every expression here uses as a scratch
// register. A call reads the function's address from the GOT (as gcc
// -fno-plt does) or calls a local function pc-relative, so ebx stays
// free and the code is the same for an executable, a PIE, a static
// link and a shared object.
static bool codegen_in_text = false;

static bool codegen_pic_ident_char(char c){
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')
        || c == '_' || c == '$' || c == '.' || c == '@' || c == '?';
}

static bool codegen_pic_is_register(const char* s, size_t len){
    static const char* registers[] = {
        "eax", "ebx", "ecx", "edx", "esi", "edi", "ebp", "esp",
        "ax", "bx", "cx", "dx", "si", "di", "bp", "sp",
        "al", "ah", "bl", "bh", "cl", "ch", "dl", "dh", 0,
    };
    for(int i = 0; registers[i]; i++){
        if(strlen(registers[i]) == len && strncmp(registers[i], s, len) == 0){
            return true;
        }
    }
    return false;
}

// Whether `symbol` (as written in the asm) is reached GOT-relative.
// Backend labels (str_3, function_call_7, x87_tmp) are always local.
static bool codegen_pic_binds_locally(const char* symbol, size_t len){
    if(symbol[0] != '$'){
        return true;
    }
    char name[IDENTIFIER_MAX_LENGTH + 1];
    if(len - 1 > IDENTIFIER_MAX_LENGTH){
        len = IDENTIFIER_MAX_LENGTH + 1;
    }
    memcpy(name, symbol + 1, len - 1);
    name[len - 1] = 0;
    return symbol_binds_locally(current_process, name);
}

static void codegen_pic_write(const char* line){
    fprintf(current_process->ofile, "%s\n", line);
}

// Rewrite one line of .text (see above) and write it.
static void codegen_pic_line(const char* line){
    const char* start = line;
    while(*start == ' ' || *start == '\t'){
        start++;
    }
    if(*start == ';' || !*start){
        codegen_pic_write(line);
        return;
    }
    char out[1024];
    // `call $f`: a direct call.
    if(strncmp(start, "call $", 6) == 0){
        const char* symbol = start + 5;
        size_t len = strlen(symbol);
        if(!codegen_pic_binds_locally(symbol, len)){
            snprintf(out, sizeof(out), "call [esi+%s wrt ..got]", symbol);
            codegen_pic_write(out);
            return;
        }
        codegen_pic_write(line);
        return;
    }
    // `mov reg, label`: the label's address as an immediate.
    const char* comma = strchr(start, ',');
    if(strncmp(start, "mov ", 4) == 0 && comma && !strchr(start, '[')){
        const char* source = comma + 1;
        while(*source == ' '){
            source++;
        }
        size_t len = 0;
        while(codegen_pic_ident_char(source[len])){
            len++;
        }
        bool is_symbol = len > 0 && !(source[0] >= '0' && source[0] <= '9') && source[0] != '.'
            && source[len] == 0 && !codegen_pic_is_register(source, len);
        if(is_symbol){
            snprintf(out, sizeof(out), "lea %.*s, [%s]", (int)(comma - start - 4), start + 4, source);
            codegen_pic_line(out);
            return;
        }
    }
    // A memory operand based on a symbol: `[sym]`, `[sym+4]`.
    const char* open = strchr(start, '[');
    if(!open){
        codegen_pic_write(line);
        return;
    }
    // a thread-local's offset (`[$t wrt ..tlsie]`, which ld
    // relaxes to a constant in an executable or a static PIE) and the
    // thread pointer (`[gs:0]`) stay as they are.
    if(strstr(open, "wrt ..tlsie]") || strncmp(open, "[gs:", 4) == 0){
        codegen_pic_write(line);
        return;
    }
    const char* symbol = open + 1;
    while(*symbol == ' '){
        symbol++;
    }
    size_t len = 0;
    while(codegen_pic_ident_char(symbol[len])){
        len++;
    }
    if(len == 0 || (symbol[0] >= '0' && symbol[0] <= '9') || codegen_pic_is_register(symbol, len)){
        codegen_pic_write(line);
        return;
    }
    const char* close = strchr(symbol, ']');
    if(!close){
        compiler_error(current_process, "COMPILER BUG: x86 operand without `]`: %s\n", line);
    }
    // Only a constant may follow the symbol: esi (or edi) takes the
    // place of the one base register an operand has.
    const char* rest = symbol + len;
    for(const char* c = rest; c < close; c++){
        if(!strchr("+-0123456789 ", *c)){
            compiler_error(current_process, "COMPILER BUG: x86 symbol operand with a register: %s\n", line);
        }
    }
    if(codegen_pic_binds_locally(symbol, len)){
        snprintf(out, sizeof(out), "%.*s[esi+%.*s wrt ..gotoff]%s",
            (int)(open - line), line, (int)(close - symbol), symbol, close + 1);
    } else {
        snprintf(out, sizeof(out), "mov edi, [esi+%.*s wrt ..got]", (int)len, symbol);
        codegen_pic_write(out);
        snprintf(out, sizeof(out), "%.*s[edi%.*s]%s",
            (int)(open - line), line, (int)(close - rest), rest, close + 1);
    }
    codegen_pic_write(out);
}

static void asm_push_args(const char* ins, va_list args){
    // The asm goes to the output file only. It used to be echoed to
    // stdout on every compile as well.
    if(!current_process->ofile){
        return;
    }
    if(!codegen_in_text){
        vfprintf(current_process->ofile, ins, args);
        fprintf(current_process->ofile, "\n");
        return;
    }
    char line[1024];
    int written = vsnprintf(line, sizeof(line), ins, args);
    if(written < 0 || (size_t)written >= sizeof(line)){
        compiler_error(current_process, "COMPILER BUG: x86 instruction longer than %i bytes\n", (int)sizeof(line));
    }
    codegen_pic_line(line);
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
    if(current_process->ofile){
        va_list args;
        va_start(args, ins);
        vfprintf(current_process->ofile, ins, args);
        va_end(args);
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

// ch187: queue a line to be flushed into the .data section on its
// own pass at the end of codegen. Used for per-call function
// pointer slots so we can `call [slot]` after stashing the target
// before pushing arguments.
static void codegen_data_section_add(const char* data, ...){
    va_list args;
    va_start(args, data);
    char* new_data = malloc(256);
    vsprintf(new_data, data, args);
    va_end(args);
    vector_push(current_process->generator->custom_data_section, &new_data);
}

// ch160: pop variant that no-ops when the top ledger element isn't
// the expected one. Used by the for-loop emitter so init/cond/loop
// expressionables that don't push a value don't crash the pop.
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

// ch156: emit `pop ebp` WITHOUT touching the stack-frame ledger.
// Used by `return` so the runtime stack pops without making the
// compile-time ledger think we've left the function.
static void asm_pop_ebp_no_stack_frame_restore(void){
    asm_push("pop ebp");
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

// A C name as NASM must see it. `$` marks an identifier, so a name that
// is also a register or keyword (`gs`, `eax`, `abs`) works; rdefault.c
// writes the same prefix into the addresses (backend symbol_prefix).
static const char* codegen_symbol(const char* name){
    // `$`, a name of up to IDENTIFIER_MAX_LENGTH and the NUL, or a
    // thread-local alias (tls_alias) of one.
    static char buffers[4][IDENTIFIER_MAX_LENGTH + 32];
    static int next;
    char* out = buffers[next++ % 4];
    snprintf(out, sizeof(buffers[0]), "$%s", name);
    return out;
}

static void codegen_generate_function_prototype(struct node* node){
    codegen_register_function(node, 0);
    // A prototype of a function defined further down is not external;
    // NASM rejects a symbol that is both extern and defined.
    if(!function_defined_in_tree(current_process, node->func.name)){
        asm_push("extern %s", codegen_symbol(node->func.name));
    }
}

static void codegen_generate_function_arguments(struct vector* argument_vector){
    vector_set_peek_pointer(argument_vector, 0);
    struct node* current = vector_peek_ptr(argument_vector);
    while(current){
        codegen_new_scope_entity(current, current->var.aoffset, RESOLVER_DEFAULT_ENTITY_FLAG_IS_LOCAL_STACK);
        debug_parameter(current_process, current, false);
        current = vector_peek_ptr(argument_vector);
    }
}

// ch138: NUMBER -> push dword <literal>. The stack-frame ledger
// gets a PUSHED_VALUE / "result_value" element with a numeric flag
// and an int dtype attached.
// The constant is pushed as its 32-bit pattern; its type (int, unsigned
// int, long, unsigned long) comes from the value and suffix.
static void codegen_generate_number_node(struct node* node, struct history* history){
    (void)history;
    struct datatype dtype = datatype_for_number_node(node);
    asm_push_ins_push_with_data("dword %i",
        STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value",
        STACK_FRAME_ELEMENT_FLAG_IS_NUMERICAL,
        &(struct stack_frame_data){.dtype = dtype},
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
// ch170: forward decl for the string literal emitter.
static void codegen_generate_string(struct node* node, struct history* history);
// ch171: forward decls for the parens emitter + the inheritable-flag
// stripper (real impl lives further down with the other ch142 helpers).
static void codegen_generate_exp_parenthesis_node(struct node* node, struct history* history);
static int  codegen_remove_uninheritable_flags(int flags);
// ch172: forward decl for the ternary emitter.
static void codegen_generate_tenary(struct node* node, struct history* history);
// ch173: forward decl for the cast emitter.
static void codegen_generate_cast(struct node* node, struct history* history);
// ch147: forward decl - real impl lives in the label-system block below.
static int  codegen_label_count(void);
// ch151: forward decl for the UNARY codegen used by the dispatcher
// (real impl lives further down).
static void codegen_generate_unary(struct node* node, struct history* history);
// ch154: forward decl for asm_datatype_back (real impl lives below).
static bool asm_datatype_back(struct datatype* dtype_out);
// ch157: forward decl for the body emitter so the if-stmt helpers
// can call it before it's defined.
static void codegen_generate_body(struct node* node, struct history* history);
// ch158: forward decls for the entry/exit point helpers (real impls
// live in the label-system block below).
static void codegen_begin_entry_exit_point(void);
static void codegen_register_entry_point(int entry_point_id);
static void codegen_begin_exit_point(void);
static void codegen_end_entry_exit_point(void);
// ch161: forward decl for the goto-exit-point helper.
static void codegen_goto_exit_point(struct node* node);
// ch166: forward decl for the maintain-stack variant.
static void codegen_goto_exit_point_maintain_stack(struct node* node);
// ch166/167: forward decls for the switch bookkeeping helpers.
static void codegen_begin_switch_statement(void);
static void codegen_end_switch_statement(void);
static int  codegen_switch_id(void);
static void codegen_begin_case_statement(long long index);
static void codegen_end_case_statement(void);
static void codegen_generate_switch_default_stmt(struct node* node);
// ch162: forward decl for the goto-entry-point helper.
static void codegen_goto_entry_point(struct node* node);

// ch150: forward decls for the structure helpers (real impls live
// further down).
static void codegen_generate_structure_push_or_return(struct resolver_entity* entity, struct history* history, int start_pos);
static void codegen_generate_structure_push(struct resolver_entity* entity, struct history* history, int start_pos);
static void codegen_generate_move_struct(struct datatype* dtype, const char* base_address, int offset);

// A21: floating point (the x87 section below).
static bool x87_wanted(struct node* node);
static void x87_push(struct node* node, int flags);
static void x87_effect(struct node* node);
static void x87_return(struct node* node, struct datatype* rtype);
static void x87_initialize(struct node* value, struct datatype* type, const char* address);
static bool x87_is_floating(struct datatype* t);
static struct datatype x87_type(struct node* node);

// ch138: dispatch by node type. Sets EXPRESSION_IS_NOT_ROOT_NODE for
// nested calls so the recursive walk knows it's deeper than the top.
// A21: an expression with floating point in it goes to the x87 code,
// unless only its address is wanted.
static void codegen_generate_expressionable(struct node* node, struct history* history){
    compiler_check_stack(current_process);
    if(node->pos.line){
        current_process->pos = node->pos;
    }
    if(!(history->flags & EXPRESSION_GET_ADDRESS) && x87_wanted(node)){
        x87_push(node, history->flags);
        return;
    }
    if(codegen_is_exp_root(history)){
        history->flags |= EXPRESSION_IS_NOT_ROOT_NODE;
    }
    switch(node->type){
        case NODE_TYPE_NUMBER:
            codegen_generate_number_node(node, history);
            break;
        // ch170: string literal in expression position.
        case NODE_TYPE_STRING:
            codegen_generate_string(node, history);
            break;
        // ch145: identifiers take the value-load path; (sub)expressions
        // route back through the expression dispatcher.
        case NODE_TYPE_IDENTIFIER:
            codegen_generate_identifier(node, history);
            break;
        case NODE_TYPE_EXPRESSION:
            codegen_generate_exp_node(node, history);
            break;
        // ch171: `(expr)` - strip uninheritable flags then walk inner.
        case NODE_TYPE_EXPRESSION_PARENTHESES:
            codegen_generate_exp_parenthesis_node(node, history);
            break;
        // ch151: UNARY in expression position (e.g. `&x`).
        case NODE_TYPE_UNARY:
            codegen_generate_unary(node, history);
            break;
        // ch172: ternary.
        case NODE_TYPE_TENARY:
            codegen_generate_tenary(node, history);
            break;
        // ch173: cast.
        case NODE_TYPE_CAST:
            codegen_generate_cast(node, history);
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

// ch236: native generator hook - fill out a generator_entity_address
// view of a resolver_entity (stack-vs-data, offset, address, base).
void codegen_entity_address(struct generator* generator, struct resolver_entity* entity, struct generator_entity_address* address_out){
    struct resolver_default_entity_data* data = codegen_entity_private(entity);
    address_out->address      = data->address;
    address_out->base_address = data->base_address;
    address_out->is_stack     = data->flags & RESOLVER_DEFAULT_ENTITY_FLAG_IS_LOCAL_STACK;
    address_out->offset       = data->offset;
}

// ch138: a local variable declaration. Register it as a scope entity
// (stack-resident) and, if it has an initializer, evaluate the RHS
// onto the stack and store it at the variable's address.
static void codegen_generate_scope_variable(struct node* node){
    struct resolver_entity* entity = codegen_new_scope_entity(node, node->var.aoffset, RESOLVER_DEFAULT_ENTITY_FLAG_IS_LOCAL_STACK);
    debug_local(current_process, node);
    if(node->var.val && !datatype_is_struct_or_union_non_pointer(&entity->dtype)
       && (x87_wanted(node->var.val) || (current_process->uses_floating && x87_is_floating(&entity->dtype)))){
        x87_initialize(node->var.val, &entity->dtype, codegen_entity_private(entity)->address);
        return;
    }
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
static void codegen_generate_entity_access_for_unary_indirection_for_assignment_left_operand(struct resolver_result* result, struct resolver_entity* entity, struct history* history);
static void codegen_generate_entity_access_for_unary_get_address(struct resolver_result* result, struct resolver_entity* entity);
// ch155: forward decl - shared between LHS and read-side dispatchers.
static void codegen_generate_entity_access_for_unsupported(struct resolver_result* result, struct resolver_entity* entity);
// ch174: forward decls - shared between LHS and read-side dispatchers.
static void codegen_generate_entity_access_for_cast(struct resolver_result* result, struct resolver_entity* entity);
static void codegen_generate_entity_access_array_bracket(struct resolver_result* result, struct resolver_entity* entity);

// ch154: codegen rules for an entity in the access chain. Used by
// the indirection / get-address paths.
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

// ch154: chain of `mov ebx, [ebx]` for `depth` dereferences.
static void codegen_apply_unary_access(int depth){
    for(int i = 0; i < depth; i++){
        asm_push("mov ebx, [ebx]");
    }
}

static void codegen_generate_entity_access_for_entity_for_assignment_left_operand(struct resolver_result* result, struct resolver_entity* entity, struct history* history){
    (void)history;
    switch(entity->type){
        // ch174: array bracket on LHS.
        case RESOLVER_ENTITY_TYPE_ARRAY_BRACKET:
            codegen_generate_entity_access_array_bracket(result, entity);
            break;
        case RESOLVER_ENTITY_TYPE_VARIABLE:
        case RESOLVER_ENTITY_TYPE_GENERAL:
            codegen_generate_entity_access_for_variable_or_general(result, entity);
            break;
        case RESOLVER_ENTITY_TYPE_FUNCTION_CALL:
            // ch148
            codegen_generate_entity_access_for_function_call(result, entity);
            break;
        // ch154: unary indirection / get-address on the LHS.
        case RESOLVER_ENTITY_TYPE_UNARY_INDIRECTION:
            codegen_generate_entity_access_for_unary_indirection_for_assignment_left_operand(result, entity, history);
            break;
        case RESOLVER_ENTITY_TYPE_UNARY_GET_ADDRESS:
            codegen_generate_entity_access_for_unary_get_address(result, entity);
            break;
        // ch155: UNSUPPORTED falls back to emitting the wrapped node.
        case RESOLVER_ENTITY_TYPE_UNSUPPORTED:
            codegen_generate_entity_access_for_unsupported(result, entity);
            break;
        // ch174: cast - just a `; CAST` comment marker.
        case RESOLVER_ENTITY_TYPE_CAST:
            codegen_generate_entity_access_for_cast(result, entity);
            break;
        default:
            compiler_error(current_process, "COMPILER BUG: unexpected entity type in assignment LHS\n");
    }
}

// ch155: UNSUPPORTED entity in the access chain - hand the wrapped
// AST node off to the regular expressionable emitter.
static void codegen_generate_entity_access_for_unsupported(struct resolver_result* result, struct resolver_entity* entity){
    (void)result;
    codegen_generate_expressionable(entity->node, codegen_history_begin(0));
}

// ch174: cast on the access chain - just a comment marker (the
// resolver entity carries the dtype, no extra emit needed). A pointer
// on the stack takes the cast's type, so an indirection after it reads
// that type: `*(struct s*)p` with a `void* p` read a void and never
// pushed the struct (va_arg of a struct type).
static void codegen_generate_entity_access_for_cast(struct resolver_result* result, struct resolver_entity* entity){
    (void)result;
    asm_push("; CAST");
    struct stack_frame_element* top = stackframe_back(current_function);
    if(top && (top->flags & STACK_FRAME_ELEMENT_FLAG_HAS_DATATYPE) && (entity->dtype.flags & DATATYPE_FLAG_IS_POINTER)){
        top->data.dtype = entity->dtype;
    }
}

// ch174: pointer-array bracket access. Pop the base into ebx,
// evaluate the index, pop into eax, scale by element size (skip if
// byte-sized), add ebx + eax, push.
static bool codegen_indirection_operand_is_address(struct resolver_result* result, struct resolver_entity* entity);

static void codegen_generate_entity_access_array_bracket_pointer(struct resolver_result* result, struct resolver_entity* entity){
    asm_push_ins_pop("ebx", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
    // `a[i][j]` with `char** a` (or `char* v[2]`), `sp->name[i]`: the
    // entity before left the address where the pointer to index is
    // kept (a[i], the member). The load was missing, so a[1][0] and
    // sp->name[0] read the bytes of the pointer. A root entity was
    // already loaded by codegen_generate_entity_access_start.
    if(entity->prev && entity->prev != resolver_result_entity_root(result)
       && codegen_indirection_operand_is_address(result, entity)){
        asm_push("mov ebx, [ebx]");
    }
    codegen_generate_expressionable(entity->array.array_index_node, codegen_history_begin(0));
    asm_push_ins_pop("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
    if(datatype_element_size(&entity->dtype) > DATA_SIZE_BYTE){
        asm_push("imul eax, %i", (int)datatype_size_for_array_access(&entity->dtype));
    }
    asm_push("add ebx, eax");
    asm_push_ins_push_with_data("ebx",
        STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value", 0,
        &(struct stack_frame_data){.dtype = entity->dtype});
}

// ch174: array-bracket access on a non-pointer-array entity. Two
// cases: JUST_USE_OFFSET (constant index already folded into the
// entity's offset) -> add the offset; otherwise scale the runtime
// index by the per-element offset.
static void codegen_generate_entity_access_array_bracket(struct resolver_result* result, struct resolver_entity* entity){
    if(entity->flags & RESOLVER_ENTITY_FLAG_IS_POINTER_ARRAY_ENTITY){
        codegen_generate_entity_access_array_bracket_pointer(result, entity);
        return;
    }
    asm_push_ins_pop("ebx", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
    codegen_generate_expressionable(entity->array.array_index_node, codegen_history_begin(0));
    asm_push_ins_pop("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
    if(entity->flags & RESOLVER_ENTITY_FLAG_JUST_USE_OFFSET){
        asm_push("add ebx, %i", entity->offset);
    } else {
        asm_push("imul eax, %i", entity->offset);
        asm_push("add ebx, eax");
    }
    asm_push_ins_push_with_data("ebx",
        STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value", 0,
        &(struct stack_frame_data){.dtype = entity->dtype});
}

// What the access chain pushed before a `*`: the address where the
// pointer is kept (after a member, an element, or a root loaded as an
// address) or the pointer's value (a root pushed as a value, a call,
// a cast, a generated expression).
static bool codegen_indirection_operand_is_address(struct resolver_result* result, struct resolver_entity* entity){
    struct resolver_entity* prev = entity->prev;
    if(!prev){
        return false;
    }
    if(prev == resolver_result_entity_root(result)){
        if(prev->type == RESOLVER_ENTITY_TYPE_UNSUPPORTED){
            return false;
        }
        return !(result->flags & RESOLVER_RESULT_FLAG_FIRST_ENTITY_PUSH_VALUE);
    }
    // A member or element that is an array is used by its address, which
    // is already its value; only a pointer kept there needs the load.
    bool keeps_pointer = (prev->dtype.flags & DATATYPE_FLAG_IS_POINTER)
        && !(prev->dtype.flags & DATATYPE_FLAG_IS_ARRAY);
    switch(prev->type){
        case RESOLVER_ENTITY_TYPE_VARIABLE:
        case RESOLVER_ENTITY_TYPE_GENERAL:
            return keeps_pointer;
        case RESOLVER_ENTITY_TYPE_ARRAY_BRACKET:
            return keeps_pointer || resolver_entity_is_pointer_array_element(prev);
        default:
            return false;
    }
}

// ch154: LHS unary indirection. Pop the address, walk
// `depth - 1` dereferences (the last one is the store, handled
// upstream), push tagged IS_PUSHED_ADDRESS.
static void codegen_generate_entity_access_for_unary_indirection_for_assignment_left_operand(struct resolver_result* result, struct resolver_entity* entity, struct history* history){
    asm_push("; INDIRECTION");
    struct datatype operand_datatype;
    bool has_type = asm_datatype_back(&operand_datatype);
    int flags = asm_push_ins_pop("ebx", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
    (void)flags;
    int gen_entity_rules = codegen_entity_rules(result->last_entity, history);
    (void)gen_entity_rules;
    // The store wants the address of `*p`, which is p's value: one load
    // fewer than the depth, plus one when the chain left the address
    // where p is kept (`*w->p = 9`, `(*bp->p).y = 30`) rather than p's
    // value (`*p = 9`, `(*q).y = 20`). The result flag that stood for
    // this also comes from a member access after the `*`.
    int depth = entity->indirection.depth - 1;
    if(codegen_indirection_operand_is_address(result, entity)){
        depth++;
    }
    // A subscript or another `*` after it (`(*pp)[i] = c`, `*(*pp) = c`,
    // stores through the pointer `*pp` holds, so that pointer
    // is loaded too; the store went to the bytes after *pp's own address.
    // On i386 a thread-local pointer is such a chain (`name[i] = c` is
    // `(*(char**)tls_address)[i] = c`).
    bool through_result = entity->next && (entity->next->type == RESOLVER_ENTITY_TYPE_ARRAY_BRACKET
        || entity->next->type == RESOLVER_ENTITY_TYPE_UNARY_INDIRECTION);
    if(through_result && (entity->dtype.flags & DATATYPE_FLAG_IS_POINTER) && !(entity->dtype.flags & DATATYPE_FLAG_IS_ARRAY)){
        depth++;
    }
    codegen_apply_unary_access(depth);
    // The address carries the type stored there, for a store whose
    // resolver type is unknown: `*(p + 5) = 7` through a generated
    // pointer expression emitted `mov (null) [edx]`.
    if(has_type && entity->prev && entity->prev->type == RESOLVER_ENTITY_TYPE_UNSUPPORTED
       && (operand_datatype.flags & DATATYPE_FLAG_IS_POINTER)){
        struct datatype stored = *datatype_pointer_reduce(&operand_datatype, entity->indirection.depth);
        stored.flags &= ~DATATYPE_FLAG_IS_ARRAY;
        asm_push_ins_push_with_data("ebx",
            STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value",
            STACK_FRAME_ELEMENT_FLAG_IS_PUSHED_ADDRESS, &(struct stack_frame_data){.dtype = stored});
        return;
    }
    asm_push_ins_push_with_flags("ebx",
        STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value",
        STACK_FRAME_ELEMENT_FLAG_IS_PUSHED_ADDRESS);
}

// The type stored at the address the access walk left on the stack, when
// the resolver could not work it out (a store through a generated
// pointer expression): the type that address was pushed with.
static void codegen_store_type_from_stack(struct datatype* dtype){
    struct datatype pushed;
    if(datatype_element_size(dtype) == 0 && asm_datatype_back(&pushed)){
        *dtype = pushed;
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
static int  codegen_set_flag_for_operator(const char* op);
static void codegen_gen_math_for_value(const char* reg, const char* value, int flags, bool is_signed);
static void codegen_reduce_register(const char* reg, size_t size, bool is_signed);
static bool asm_datatype_back(struct datatype* dtype_out);

enum {
    // Push the assignment's value: it is an operand, as in `a = b = 3`.
    CODEGEN_ASSIGN_VALUE_NEEDED = 0b001,
    // `x++` / `x--`: the value is the operand before the step.
    CODEGEN_ASSIGN_POSTFIX      = 0b010,
    // ++ / --: there is no right operand on the stack, the step is 1.
    CODEGEN_ASSIGN_STEP_ONE     = 0b100,
};

// eax = the `size`-byte value at [address], sign or zero extended.
static void codegen_load_eax(const char* address, size_t size, bool is_signed){
    if(size == DATA_SIZE_BYTE || size == DATA_SIZE_WORD){
        asm_push("%s eax, %s [%s]", is_signed ? "movsx" : "movzx",
            size == DATA_SIZE_BYTE ? "byte" : "word", address);
        return;
    }
    asm_push("mov eax, [%s]", address);
}

// Push eax as the value of an assignment, narrowed to the stored type
// (the value of `c = 300` with a char c is 44).
static void codegen_push_assignment_value(struct datatype* dtype){
    codegen_reduce_register("eax", datatype_element_size(dtype), dtype->flags & DATATYPE_FLAG_IS_SIGNED);
    asm_push_ins_push_with_data("eax",
        STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value", 0,
        &(struct stack_frame_data){.dtype = *dtype});
}

// [address] = eax at the width of dtype. An _Atomic object
// takes an xchg (locked by itself: the sequentially consistent store)
// from ecx, so eax keeps the value of the assignment.
static void codegen_store_maybe_atomic(struct datatype* dtype, const char* mov_type, const char* address){
    size_t size = datatype_element_size(dtype);
    if(datatype_is_atomic(dtype)){
        asm_push("mov ecx, eax");
        asm_push("xchg %s [%s], %s", mov_type, address, codegen_sub_register("ecx", size));
        return;
    }
    asm_push("mov %s [%s], %s", mov_type, address, codegen_sub_register("eax", size));
}

// an _Atomic struct, union or long double of `size` bytes
// copied atomically between the object at ecx and the buffer at edx
// (into the object when `store`), with `order`. A lock-free size is one
// access: a mov (xchg for a seq_cst store), 8 bytes through the x87 as
// gcc -m32 does it (fild and fistp move any 64 bits) with mfence after
// a seq_cst store. Any other size is libatomic's generic __atomic_load
// or __atomic_store (size, object, buffer, order). eax, ecx and edx are
// clobbered.
static void codegen_x86_atomic_copy(bool store, int size, int order){
    if(size == DATA_SIZE_BYTE || size == DATA_SIZE_WORD || size == DATA_SIZE_DWORD){
        const char* keyword = size == DATA_SIZE_BYTE ? "byte" : size == DATA_SIZE_WORD ? "word" : "dword";
        const char* eax = codegen_sub_register("eax", size);
        if(!store){
            asm_push("mov %s, %s [ecx]", eax, keyword);
            asm_push("mov %s [edx], %s", keyword, eax);
            return;
        }
        asm_push("mov %s, %s [edx]", eax, keyword);
        asm_push("%s %s [ecx], %s", order == ATOMIC_ORDER_SEQ_CST ? "xchg" : "mov", keyword, eax);
        return;
    }
    if(size == DATA_SIZE_DDWORD){
        asm_push("fild qword [%s]", store ? "edx" : "ecx");
        asm_push("fistp qword [%s]", store ? "ecx" : "edx");
        if(store && order == ATOMIC_ORDER_SEQ_CST){
            asm_push("mfence");
        }
        return;
    }
    const char* symbol = store ? "$__atomic_store" : "$__atomic_load";
    asm_push("extern %s", symbol);
    asm_push("push %i", order);
    asm_push("push edx");
    asm_push("push ecx");
    asm_push("push %i", size);
    asm_push("call %s", symbol);
    asm_push("add esp, 16");
}

// push the value of the _Atomic struct or union of type dtype
// at ebx, read atomically (seq_cst), as codegen_generate_structure_push
// pushes a plain one: room for its dwords, then the copy into it.
static void codegen_x86_atomic_structure_push(struct datatype* dtype){
    int size = (int)datatype_size(dtype);
    asm_push("; ATOMIC STRUCTURE PUSH");
    for(int i = align_value(size, DATA_SIZE_DWORD) / DATA_SIZE_DWORD; i > 0; i--){
        asm_push_ins_push_with_data("dword 0", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value", 0,
            &(struct stack_frame_data){.dtype = *dtype});
    }
    asm_push("mov ecx, ebx");
    asm_push("mov edx, esp");
    codegen_x86_atomic_copy(false, size, ATOMIC_ORDER_SEQ_CST);
    asm_push("; END ATOMIC STRUCTURE PUSH");
}

// the struct value on top of the stack stored atomically
// (seq_cst) to the _Atomic struct or union at `address`, then popped.
static void codegen_x86_atomic_move_struct(struct datatype* dtype, const char* address){
    int size = (int)datatype_size(dtype);
    asm_push("lea ecx, [%s]", address);
    asm_push("mov edx, esp");
    codegen_x86_atomic_copy(true, size, ATOMIC_ORDER_SEQ_CST);
    for(int i = align_value(size, DATA_SIZE_DWORD) / DATA_SIZE_DWORD; i > 0; i--){
        asm_push_ins_pop("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
    }
}

// ch140: `lhs = value`, the value already on the stack. Single-entity
// path: store directly into the resolved base address. Multi-entity
// path: compute the address via the access walker, pop the addr (edx)
// and the rhs (eax), then mov. The value push is only for an
// assignment that is itself an operand (`a = b = 3`).
static void codegen_generate_plain_assignment(struct node* node, struct resolver_result* result, struct history* history, int assign_flags){
    struct resolver_entity* root_assignment_entity = resolver_result_entity_root(result);
    struct datatype* dtype = &result->last_entity->dtype;
    const char* reg_to_use = "eax";
    const char* mov_type   = codegen_byte_word_or_dword_or_ddword(datatype_element_size(dtype), &reg_to_use);
    bool is_struct = datatype_is_struct_or_union_non_pointer(dtype);
    if(is_struct && (assign_flags & CODEGEN_ASSIGN_VALUE_NEEDED)){
        compiler_error(current_process, "x86: the value of a struct assignment cannot be used\n");
    }
    if(!resolver_result_entity_next(root_assignment_entity)){
        if(is_struct){
            // ch150: struct-by-value assignment - pop chunks and store.
            codegen_generate_move_struct(dtype, result->base.address, 0);
            return;
        }
        asm_push_ins_pop("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
        codegen_store_maybe_atomic(dtype, mov_type, result->base.address);
    } else {
        codegen_generate_entity_access_for_assignment_left_operand(result, root_assignment_entity, node, history);
        struct datatype stored = *dtype;
        codegen_store_type_from_stack(&stored);
        if(stored.size != dtype->size || stored.flags != dtype->flags){
            *dtype = stored;
            reg_to_use = "eax";
            mov_type = codegen_byte_word_or_dword_or_ddword(datatype_element_size(dtype), &reg_to_use);
        }
        asm_push_ins_pop("edx", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
        if(is_struct){
            // `*p = s`, `o.inner = s`: the struct's dwords are under the
            // address on the stack. This emitted a scalar mov with no
            // valid size (`ddword`, `null`).
            codegen_generate_move_struct(dtype, "edx", 0);
            return;
        }
        asm_push_ins_pop("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
        codegen_store_maybe_atomic(dtype, mov_type, "edx");
    }
    if(assign_flags & CODEGEN_ASSIGN_VALUE_NEEDED){
        codegen_push_assignment_value(dtype);
    }
}

// `a op= v`, `++a`, `a++` on an _Atomic a at [address], the
// operand in ecx: one atomic read-modify-write (C11 6.5.2.4, 6.5.16.2).
// A lock cmpxchg loop works out `old op v` as the plain operator does
// and stores it only if a still holds old, else tries again with what
// it found. The stack is used for old across the arithmetic (which
// takes edx), outside the ledger: it is popped before anything else.
static void codegen_generate_atomic_read_modify_write(struct datatype* dtype, struct datatype* value_dtype, const char* op,
                                                      const char* address, int assign_flags){
    static int count;
    int label = ++count;
    size_t size = datatype_element_size(dtype);
    bool lhs_signed = dtype->flags & DATATYPE_FLAG_IS_SIGNED;
    const char* arith = assignment_arithmetic_operator(op);
    struct datatype_binary types;
    datatype_binary_types(dtype, value_dtype, arith, &types);
    if(types.scale_right > 1){
        asm_push("imul ecx, %zu", types.scale_right);
    }
    const char* reg_to_use = "edx";
    const char* mov_type   = codegen_byte_word_or_dword_or_ddword(size, &reg_to_use);
    codegen_load_eax(address, size, lhs_signed);
    asm_push(".atomic_rmw_%i:", label);
    codegen_reduce_register("eax", size, lhs_signed);
    asm_push("push eax");
    asm_push("push ecx");
    codegen_gen_math_for_value("eax", "ecx", codegen_set_flag_for_operator(arith), types.operation.flags & DATATYPE_FLAG_IS_SIGNED);
    asm_push("pop ecx");
    asm_push("mov edx, eax");
    asm_push("pop eax");
    asm_push("lock cmpxchg %s [%s], %s", mov_type, address, reg_to_use);
    asm_push("jne .atomic_rmw_%i", label);
    if(assign_flags & CODEGEN_ASSIGN_POSTFIX){
        asm_push_ins_push_with_data("eax",
            STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value", 0,
            &(struct stack_frame_data){.dtype = *dtype});
        return;
    }
    if(assign_flags & CODEGEN_ASSIGN_VALUE_NEEDED){
        asm_push("mov eax, edx");
        codegen_push_assignment_value(dtype);
    }
}

// `lhs op= value`, `++lhs`, `lhs++`: load, compute, store. A non-simple
// lhs address is kept in ebx because mul/div clobber edx and shifts
// take their count from cl.
static void codegen_generate_read_modify_write(struct node* node, struct resolver_result* result, const char* op, struct history* history, int assign_flags){
    struct resolver_entity* root_assignment_entity = resolver_result_entity_root(result);
    struct datatype dtype = result->last_entity->dtype;
    if(datatype_is_struct_or_union_non_pointer(&dtype)){
        compiler_error(current_process, "x86: %s on a struct is not valid\n", op);
    }
    struct datatype value_dtype = datatype_for_numeric();
    if(!(assign_flags & CODEGEN_ASSIGN_STEP_ONE)){
        asm_datatype_back(&value_dtype);
    }

    const char* address = result->base.address;
    if(resolver_result_entity_next(root_assignment_entity)){
        codegen_generate_entity_access_for_assignment_left_operand(result, root_assignment_entity, node, history);
        codegen_store_type_from_stack(&dtype);
        asm_push_ins_pop("ebx", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
        address = "ebx";
    }
    if(assign_flags & CODEGEN_ASSIGN_STEP_ONE){
        asm_push("mov ecx, 1");
    } else {
        asm_push_ins_pop("ecx", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
    }

    size_t size = datatype_element_size(&dtype);
    bool lhs_signed = dtype.flags & DATATYPE_FLAG_IS_SIGNED;
    if(datatype_is_atomic(&dtype)){
        codegen_generate_atomic_read_modify_write(&dtype, &value_dtype, op, address, assign_flags);
        return;
    }
    codegen_load_eax(address, size, lhs_signed);
    if(assign_flags & CODEGEN_ASSIGN_POSTFIX){
        asm_push_ins_push_with_data("eax",
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
        asm_push("imul ecx, %zu", types.scale_right);
    }
    codegen_gen_math_for_value("eax", "ecx", op_flags, types.operation.flags & DATATYPE_FLAG_IS_SIGNED);

    const char* reg_to_use = "eax";
    const char* mov_type   = codegen_byte_word_or_dword_or_ddword(size, &reg_to_use);
    asm_push("mov %s [%s], %s", mov_type, address, reg_to_use);
    if((assign_flags & CODEGEN_ASSIGN_VALUE_NEEDED) && !(assign_flags & CODEGEN_ASSIGN_POSTFIX)){
        codegen_push_assignment_value(&dtype);
    }
}

static struct node* codegen_access_conditional(struct node* node);
static struct node* codegen_access_with_operand(struct node* path, struct node* conditional, struct node* operand);

// A copy of `like` (its position and bindings) made a unary `op` of
// `operand`.
static struct node* codegen_unary_of(struct node* like, const char* op, struct node* operand){
    struct node* unary = malloc(sizeof(struct node));
    memcpy(unary, like, sizeof(struct node));
    unary->type = NODE_TYPE_UNARY;
    memset(&unary->unary, 0, sizeof(unary->unary));
    unary->unary.op      = op;
    unary->unary.operand = operand;
    if(op_is_indirection(op)){
        unary->unary.indirection.depth = 1;
    }
    return unary;
}

// a member through an address, `(&x)->m`, is `x.m`. The
// resolver walked `&x` as an address and then `->` through it as a
// pointer it had loaded, and the program read or wrote at x's contents
// (`(&p->in)->t = 1`, and `(c ? &p->in : &q->in)->t` once a conditional
// is taken apart). A copy of the access chain `node` with each such `->`
// made a `.`, or node itself when it has none.
static struct node* codegen_access_through_address(struct node* node){
    if(node->type == NODE_TYPE_EXPRESSION_PARENTHESES){
        struct node* inner = codegen_access_through_address(node->parenthesis.exp);
        if(inner == node->parenthesis.exp){
            return node;
        }
        struct node* copy = malloc(sizeof(struct node));
        memcpy(copy, node, sizeof(struct node));
        copy->parenthesis.exp = inner;
        return copy;
    }
    if(node->type != NODE_TYPE_EXPRESSION || !(is_access_node(node) || is_array_node(node))){
        return node;
    }
    struct node* base = node->exp.left;
    while(base->type == NODE_TYPE_EXPRESSION_PARENTHESES){
        base = base->parenthesis.exp;
    }
    bool through_address = S_EQ(node->exp.op, "->") && base->type == NODE_TYPE_UNARY && op_is_address(base->unary.op);
    struct node* left = codegen_access_through_address(through_address ? base->unary.operand : node->exp.left);
    if(!through_address && left == node->exp.left){
        return node;
    }
    struct node* copy = malloc(sizeof(struct node));
    memcpy(copy, node, sizeof(struct node));
    if(through_address){
        copy->exp.op = ".";
    }
    copy->exp.left = left;
    return copy;
}

// an assignment to a member or element of a conditional,
// `(c ? p : q)->m = v`, is one to `*(c ? &p->m : &q->m)`: the resolver
// has no entity for a conditional, so the store had no object and the
// backend crashed (the driver said nothing and exited with 1). It is an
// lvalue only through a pointer (`->`, `[]`); a member of a struct
// value, `(c ? s : t).m`, is not one, as in gcc. Any other left operand
// is returned as it is.
static struct node* codegen_conditional_lvalue(struct node* node, const char* op){
    struct node* conditional = codegen_access_conditional(node);
    if(!conditional){
        return node;
    }
    bool through_pointer = false;
    for(struct node* path = node; path != conditional; ){
        if(path->type == NODE_TYPE_EXPRESSION_PARENTHESES){
            path = path->parenthesis.exp;
            continue;
        }
        through_pointer |= S_EQ(path->exp.op, "->") || is_array_node(path);
        path = path->exp.left;
    }
    if(!through_pointer){
        compiler_error(current_process, "The left operand of %s is a member of a conditional's value, which is not an lvalue\n", op);
    }
    struct node* tenary = malloc(sizeof(struct node));
    memcpy(tenary, conditional->exp.right, sizeof(struct node));
    tenary->tenary.true_node  = codegen_unary_of(node, "&",
        codegen_access_through_address(codegen_access_with_operand(node, conditional, conditional->exp.right->tenary.true_node)));
    tenary->tenary.false_node = codegen_unary_of(node, "&",
        codegen_access_through_address(codegen_access_with_operand(node, conditional, conditional->exp.right->tenary.false_node)));
    struct node* addresses = malloc(sizeof(struct node));
    memcpy(addresses, conditional, sizeof(struct node));
    addresses->exp.right = tenary;
    struct node* parentheses = malloc(sizeof(struct node));
    memcpy(parentheses, node, sizeof(struct node));
    parentheses->type = NODE_TYPE_EXPRESSION_PARENTHESES;
    memset(&parentheses->parenthesis, 0, sizeof(parentheses->parenthesis));
    parentheses->parenthesis.exp = addresses;
    return codegen_unary_of(node, "*", parentheses);
}

static void codegen_generate_assignment_part(struct node* node, const char* op, struct history* history, int assign_flags){
    node = codegen_conditional_lvalue(codegen_access_through_address(node), op);
    struct resolver_result* result = resolver_follow(current_process->resolver, node);
    // No entity at the end is no object to store to (it crashed below).
    if(!resolver_result_ok(result) || !result->last_entity){
        compiler_error(current_process, "The left operand of %s cannot be assigned to\n", op);
    }
    // `*(c ? a : b) = v`: the indirection of an expression the resolver
    // does not follow had no type, and the store came out as `mov (null)
    // [edx], (null)`, which only the assembler refused.
    // x87_type gives any expression its C type.
    if(datatype_element_size(&result->last_entity->dtype) == 0){
        struct datatype t = x87_type(node);
        if(datatype_element_size(&t) != 0){
            result->last_entity->dtype = t;
        }
    }
    if(S_EQ(op, "=")){
        codegen_generate_plain_assignment(node, result, history, assign_flags);
        return;
    }
    // A statement `x += n` on a non-pointer keeps its single instruction.
    struct resolver_entity* root_assignment_entity = resolver_result_entity_root(result);
    struct datatype* dtype = &result->last_entity->dtype;
    // on an _Atomic object (not a _Bool) the add is locked.
    bool atomic = datatype_is_atomic(dtype);
    if(S_EQ(op, "+=") && !(assign_flags & CODEGEN_ASSIGN_VALUE_NEEDED)
       && !(dtype->flags & DATATYPE_FLAG_IS_POINTER)
       && !datatype_is_struct_or_union_non_pointer(dtype)
       && !(atomic && (dtype->flags & DATATYPE_FLAG_IS_BOOL))){
        const char* reg_to_use = "eax";
        const char* mov_type   = codegen_byte_word_or_dword_or_ddword(datatype_element_size(dtype), &reg_to_use);
        const char* add = atomic ? "lock add" : "add";
        if(!resolver_result_entity_next(root_assignment_entity)){
            asm_push_ins_pop("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
            asm_push("%s %s [%s], %s", add, mov_type, result->base.address, reg_to_use);
        } else {
            codegen_generate_entity_access_for_assignment_left_operand(result, root_assignment_entity, node, history);
            struct datatype stored = *dtype;
            codegen_store_type_from_stack(&stored);
            reg_to_use = "eax";
            mov_type = codegen_byte_word_or_dword_or_ddword(datatype_element_size(&stored), &reg_to_use);
            asm_push_ins_pop("edx", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
            asm_push_ins_pop("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
            asm_push("%s %s [edx], %s", add, mov_type, reg_to_use);
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
    if(entity->type == RESOLVER_ENTITY_TYPE_FUNCTION){
        // A function name used as a value is its address, a function
        // pointer; this loaded the function's first instruction bytes.
        asm_push("lea ebx, [%s]", codegen_entity_private(entity)->address);
        asm_push_ins_push_with_data("ebx",
            STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value", 0,
            &(struct stack_frame_data){.dtype = entity->dtype});
    } else if(entity->dtype.flags & DATATYPE_FLAG_IS_ARRAY){
        // An array used as a value is the address of its first element
        // (loading here used to read a[0]).
        asm_push("lea ebx, [%s]", codegen_entity_private(entity)->address);
        struct datatype decayed = datatype_decay(&entity->dtype);
        asm_push_ins_push_with_data("ebx",
            STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value", 0,
            &(struct stack_frame_data){.dtype = decayed});
    } else if(datatype_is_struct_or_union_non_pointer(&entity->dtype)){
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

// ch154: `*p` / `**p`. Pull the operand as an address (push it),
// pop into ebx, do `depth` `mov ebx, [ebx]` steps. The final dtype
// gets a sign/zero extension if depth fully matches the operand's
// pointer depth.
static void codegen_generate_unary_indirection(struct node* node, struct history* history){
    const char* reg_to_use = "ebx";
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
    if(!(history->flags & EXPRESSION_GET_ADDRESS)){
        depth++;
    }
    for(int i = 0; i < depth; i++){
        asm_push("mov %s, [%s]", reg_to_use, reg_to_use);
    }
    if(real_depth == res->data.resolved_entity->dtype.pointer_depth){
        codegen_reduce_register(reg_to_use, datatype_size_no_ptr(&operand_datatype),
            operand_datatype.flags & DATATYPE_FLAG_IS_SIGNED);
    }
    // A loaded value has the pointed-to type. Tagging it with the
    // operand's pointer type made `*p + 1` scale the 1.
    struct datatype result_dtype = operand_datatype;
    if(!(history->flags & EXPRESSION_GET_ADDRESS)){
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

// ch154: `-x`, `~x`, `*p` (the last hands off to indirection). `!`
// and other unaries land in later chapters.
static void codegen_generate_normal_unary(struct node* node, struct history* history){
    codegen_generate_expressionable(node->unary.operand, history);
    struct datatype last_dtype;
    assert(asm_datatype_back(&last_dtype));
    asm_push_ins_pop("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
    // `-` and `~` produce the promoted operand type (a char becomes int).
    struct datatype promoted = datatype_promote(&last_dtype);
    if(S_EQ(node->unary.op, "-")){
        asm_push("neg eax");
        asm_push_ins_push_with_data("eax",
            STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value", 0,
            &(struct stack_frame_data){.dtype = promoted});
    } else if(S_EQ(node->unary.op, "~")){
        asm_push("not eax");
        asm_push_ins_push_with_data("eax",
            STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value", 0,
            &(struct stack_frame_data){.dtype = promoted});
    } else if(S_EQ(node->unary.op, "*")){
        codegen_generate_unary_indirection(node, history);
    } else if(S_EQ(node->unary.op, "!")){
        // ch222: logical not. sete on flags after cmp, zero-extend, push.
        asm_push("cmp eax, 0");
        asm_push("sete al");
        asm_push("movzx eax, al");
        // `!x` is an int whatever x is (a pointer stayed a pointer).
        struct datatype result = datatype_for_numeric();
        result.flags &= ~DATATYPE_FLAG_IS_LITERAL;
        asm_push_ins_push_with_data("eax",
            STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value", 0,
            &(struct stack_frame_data){.dtype = result});
    }
}

// ch170: emit a mov of an arbitrary value into a register.
static void codegen_gen_mov_for_value(const char* reg, const char* value, const char* datatype, int flags){
    (void)datatype; (void)flags;
    asm_push("mov %s, %s", reg, value);
}

// ch170: NODE_TYPE_STRING in expression position. Register the
// string literal in .rodata, mov its label into eax, push as the
// typed result_value with dtype = `const char*`.
static void codegen_generate_string(struct node* node, struct history* history){
    const char* label = codegen_register_string(node->sval);
    codegen_gen_mov_for_value("eax", label, "dword", history->flags);
    asm_push_ins_push_with_data("eax",
        STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value", 0,
        &(struct stack_frame_data){.dtype = datatype_for_string()});
}

// ch171: NODE_TYPE_EXPRESSION_PARENTHESES - thin pass-through that
// strips uninheritable flags before walking the inner expression.
static void codegen_generate_exp_parenthesis_node(struct node* node, struct history* history){
    codegen_generate_expressionable(node->parenthesis.exp,
        codegen_history_down(history, codegen_remove_uninheritable_flags(history->flags)));
}

// ch173: `(T) operand`. Resolve the cast (which already pushed a
// value via the resolver path if possible); otherwise walk the
// operand. Then pop into eax, reduce-register to the target dtype's
// width with sign/zero extension, and re-push tagged with the new
// dtype.
static void codegen_generate_cast(struct node* node, struct history* history){
    if(!codegen_resolve_node_for_value(node, history)){
        codegen_generate_expressionable(node->cast.operand, history);
    }
    asm_push_ins_pop("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
    // `(void)x` (assert's `cond ? (void)0 : ...`) keeps no value to
    // narrow; its size 0 wrote `movsx eax, (null)`.
    if(node->cast.dtype.type != DATA_TYPE_VOID || (node->cast.dtype.flags & DATATYPE_FLAG_IS_POINTER)){
        codegen_reduce_register("eax", datatype_size(&node->cast.dtype),
            node->cast.dtype.flags & DATATYPE_FLAG_IS_SIGNED);
    }
    asm_push_ins_push_with_data("eax",
        STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value", 0,
        &(struct stack_frame_data){.dtype = node->cast.dtype});
}

// ch172: ternary `cond ? T : F`. The cond was already evaluated and
// is on top of the stack. Pop it, compare against 0, emit T or F
// branch, store the chosen value via the usual result_value push.
static void codegen_generate_tenary(struct node* node, struct history* history){
    int true_label_id       = codegen_label_count();
    int false_label_id      = codegen_label_count();
    int tenary_end_label_id = codegen_label_count();

    struct datatype last_dtype;
    assert(asm_datatype_back(&last_dtype));
    asm_push_ins_pop("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
    asm_push("cmp eax, 0");
    asm_push("je .tenary_false_%i", false_label_id);
    asm_push(".tenary_true_%i:", true_label_id);
    codegen_generate_expressionable(node->tenary.true_node, codegen_history_down(history, 0));
    asm_push_ins_pop_or_ignore("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
    asm_push("jmp .tenary_end_%i", tenary_end_label_id);
    asm_push(".tenary_false_%i:", false_label_id);
    codegen_generate_expressionable(node->tenary.false_node, codegen_history_down(history, 0));
    asm_push_ins_pop_or_ignore("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
    asm_push(".tenary_end_%i:", tenary_end_label_id);
}

// `c ? x : y` whose operands are struct values, the parentheses
// around it taken off. The resolver has no entity for a conditional.
static bool codegen_struct_conditional(struct node* node, struct datatype* t_out){
    while(node->type == NODE_TYPE_EXPRESSION_PARENTHESES){
        node = node->parenthesis.exp;
    }
    if(node->type != NODE_TYPE_EXPRESSION || !S_EQ(node->exp.op, "?")
       || node->exp.right->type != NODE_TYPE_TENARY){
        return false;
    }
    struct datatype t = x87_type(node);
    t.flags &= ~DATATYPE_FLAG_IS_ARRAY;
    if(!datatype_is_struct_or_union_non_pointer(&t)){
        return false;
    }
    *t_out = t;
    return true;
}

// a call of __atomic_load_n or __atomic_exchange_n on an
// _Atomic struct or union (as <stdatomic.h> makes them) pushes the
// struct's dwords, like a struct-valued conditional; its type.
static bool codegen_atomic_struct_value(struct node* node, struct datatype* t_out){
    while(node->type == NODE_TYPE_EXPRESSION_PARENTHESES){
        node = node->parenthesis.exp;
    }
    if(!is_parentheses_node(node) || node->exp.left->type != NODE_TYPE_IDENTIFIER){
        return false;
    }
    const struct atomic_builtin* b = atomic_builtin_get(node->exp.left->sval);
    if(!b || !(b->kind == ATOMIC_BUILTIN_LOAD || (b->kind == ATOMIC_BUILTIN_EXCHANGE && strncmp(b->name, "__sync_", 7)))){
        return false;
    }
    struct node* first = node->exp.right;
    if(first->type == NODE_TYPE_EXPRESSION_PARENTHESES){
        first = first->parenthesis.exp;
    }
    while(first->type == NODE_TYPE_EXPRESSION && S_EQ(first->exp.op, ",")){
        first = first->exp.left;
    }
    struct datatype t = x87_type(first);
    if(!(t.flags & DATATYPE_FLAG_IS_POINTER) || t.pointer_depth != 1){
        return false;
    }
    t.pointer_depth = 0;
    t.flags &= ~DATATYPE_FLAG_IS_POINTER;
    if(!datatype_is_struct_or_union_non_pointer(&t)){
        return false;
    }
    *t_out = t;
    return true;
}

// One operand of a struct-valued conditional, copied into the room
// reserved under it. The operand leaves its dwords on the stack (a
// variable, a call's result, a nested conditional) or, for a struct
// reached through a pointer, its address.
static void codegen_struct_conditional_operand(struct node* operand, struct history* history, int pieces){
    struct vector* elements = current_function->func.frame.elements;
    int before = vector_count(elements);
    codegen_generate_expressionable(operand, codegen_history_down(history, 0));
    int above = vector_count(elements) - before;
    struct stack_frame_element* top = stackframe_back(current_function);
    if(above == 1 && pieces != 1 && (top->flags & STACK_FRAME_ELEMENT_FLAG_IS_PUSHED_ADDRESS)){
        asm_push_ins_pop("ebx", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
        for(int i = 0; i < pieces; i++){
            asm_push("mov eax, [ebx+%i]", i * DATA_SIZE_DWORD);
            asm_push("mov [esp+%i], eax", i * DATA_SIZE_DWORD);
        }
        return;
    }
    if(above != pieces){
        compiler_error(current_process, "COMPILER BUG: x86 struct conditional operand left %i dwords, not %i\n", above, pieces);
    }
    for(int i = 0; i < pieces; i++){
        asm_push("mov eax, [esp+%i]", i * DATA_SIZE_DWORD);
        asm_push("mov [esp+%i], eax", (above + i) * DATA_SIZE_DWORD);
    }
    codegen_stack_add(above * DATA_SIZE_DWORD);
}

// a struct-valued conditional. Each operand used to leave its
// own dwords (a call's result its own room), and the ledger, which
// cannot tell that only one branch ran, counted both after the join:
// wrong values and a stack off by a struct. The room is made before the
// condition and each branch copies its value into it, as exp64_ternary
// does; afterwards the room is the value, laid out as pushed dwords.
static void codegen_generate_struct_conditional(struct node* node, struct datatype* t, struct history* history){
    while(node->type == NODE_TYPE_EXPRESSION_PARENTHESES){
        node = node->parenthesis.exp;
    }
    int pieces = align_value(datatype_size(t), DATA_SIZE_DWORD) / DATA_SIZE_DWORD;
    int false_label_id = codegen_label_count();
    int end_label_id   = codegen_label_count();
    asm_push("; STRUCT CONDITIONAL");
    codegen_stack_sub_with_name(pieces * DATA_SIZE_DWORD, "result_value");
    codegen_generate_expressionable(node->exp.left, codegen_history_down(history, 0));
    asm_push_ins_pop("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
    asm_push("cmp eax, 0");
    asm_push("je .tenary_false_%i", false_label_id);
    codegen_struct_conditional_operand(node->exp.right->tenary.true_node, history, pieces);
    asm_push("jmp .tenary_end_%i", end_label_id);
    asm_push(".tenary_false_%i:", false_label_id);
    codegen_struct_conditional_operand(node->exp.right->tenary.false_node, history, pieces);
    asm_push(".tenary_end_%i:", end_label_id);
    struct vector* elements = current_function->func.frame.elements;
    for(int i = 0; i < pieces; i++){
        struct stack_frame_element* el = vector_at(elements, vector_count(elements) - 1 - i);
        el->type       = STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE;
        el->flags     |= STACK_FRAME_ELEMENT_FLAG_HAS_DATATYPE;
        el->data.dtype = *t;
    }
    codegen_response_acknowledge(&(struct response){.flags = RESPONSE_FLAG_PUSHED_STRUCTURE});
}

// The conditional at the start of a member or element access
// (`(c ? s : t).m`, `(c ? p : q)[i]`), or NULL.
static struct node* codegen_access_conditional(struct node* node){
    bool access = false;
    while(true){
        if(node->type == NODE_TYPE_EXPRESSION_PARENTHESES){
            node = node->parenthesis.exp;
        } else if(node->type == NODE_TYPE_EXPRESSION && (is_access_node(node) || is_array_node(node))){
            node = node->exp.left;
            access = true;
        } else {
            break;
        }
    }
    if(!access || node->type != NODE_TYPE_EXPRESSION || !S_EQ(node->exp.op, "?")
       || node->exp.right->type != NODE_TYPE_TENARY){
        return 0;
    }
    return node;
}

// A copy of the access `path` with `operand` in place of the
// conditional at its start.
static struct node* codegen_access_with_operand(struct node* path, struct node* conditional, struct node* operand){
    if(path == conditional){
        return operand;
    }
    struct node* copy = malloc(sizeof(struct node));
    memcpy(copy, path, sizeof(struct node));
    if(path->type == NODE_TYPE_EXPRESSION_PARENTHESES){
        copy->parenthesis.exp = codegen_access_with_operand(path->parenthesis.exp, conditional, operand);
    } else {
        copy->exp.left = codegen_access_with_operand(path->exp.left, conditional, operand);
    }
    return copy;
}

// `(c ? s : t).m` is `c ? s.m : t.m`: only one operand is
// evaluated either way, and the member of each is something the
// resolver can follow. The resolver had no root for the conditional,
// which crashed and then was an error.
static struct node* codegen_access_of_conditional(struct node* node, struct node* conditional){
    struct node* tenary = malloc(sizeof(struct node));
    memcpy(tenary, conditional->exp.right, sizeof(struct node));
    tenary->tenary.true_node  = codegen_access_with_operand(node, conditional, conditional->exp.right->tenary.true_node);
    tenary->tenary.false_node = codegen_access_with_operand(node, conditional, conditional->exp.right->tenary.false_node);
    struct node* rewritten = malloc(sizeof(struct node));
    memcpy(rewritten, conditional, sizeof(struct node));
    rewritten->exp.right = tenary;
    return rewritten;
}

// ch151/154: NODE_TYPE_UNARY codegen dispatch.
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
// ch154: read-side unary indirection on the access chain. Pop the
// address, walk `depth` `[ebx]` dereferences, push tagged
// IS_PUSHED_ADDRESS.
static void codegen_generate_entity_access_for_unary_indirection(struct resolver_result* result, struct resolver_entity* entity, struct history* history){
    (void)history;
    asm_push("; INDIRECTION");
    struct datatype operand_datatype;
    assert(asm_datatype_back(&operand_datatype));
    int flags = asm_push_ins_pop("ebx", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
    (void)flags;
    // Book passes `result` here where the signature wants `history`;
    // we pass the actual history so the type-check holds.
    int gen_entity_rules = codegen_entity_rules(result->last_entity, history);
    (void)gen_entity_rules;
    int depth = entity->indirection.depth;
    // `*v[i]` with `int* v[2]`: the chain left the element's address,
    // and the element is the pointer to follow.
    if(resolver_entity_is_pointer_array_element(entity->prev)){
        asm_push("mov ebx, [ebx]");
    }
    // The pushed value has the pointed-to type, so a following `+ 1`
    // is not scaled as pointer arithmetic.
    struct datatype result_dtype = *datatype_pointer_reduce(&operand_datatype, depth);
    result_dtype.flags &= ~DATATYPE_FLAG_IS_ARRAY;
    // A struct (`s = *p`, `(*p).m`, `*(struct s*)p`) is wanted by its
    // address, which the caller pushes the pieces from or adds the
    // member's offset to; the last load read the struct's first dword
    // as that address.
    if(datatype_is_struct_or_union_non_pointer(&result_dtype)){
        depth--;
        // Under `&` (`&(*q).y`) the root was pushed as the address where
        // q is kept, not q's value, so the struct's address is one load
        // further.
        if(entity->prev == resolver_result_entity_root(result)
           && codegen_indirection_operand_is_address(result, entity)){
            depth++;
        }
    }
    codegen_apply_unary_access(depth);
    asm_push_ins_push_with_data("ebx",
        STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value",
        STACK_FRAME_ELEMENT_FLAG_IS_PUSHED_ADDRESS,
        &(struct stack_frame_data){.dtype = result_dtype});
}

// ch154: read-side unary get-address. Pop ebx (the address from the
// access chain), comment `; PUSH ADDRESS &`, push it back as the
// result value with the entity's dtype.
static void codegen_generate_entity_access_for_unary_get_address(struct resolver_result* result, struct resolver_entity* entity){
    (void)result;
    asm_push_ins_pop("ebx", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
    asm_push("; PUSH ADDRESS &");
    asm_push_ins_push_with_data("ebx",
        STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value", 0,
        &(struct stack_frame_data){.dtype = entity->dtype});
}

static void codegen_generate_entity_access_for_entity(struct resolver_result* result, struct resolver_entity* entity, struct history* history){
    switch(entity->type){
        // ch174: array bracket on read path.
        case RESOLVER_ENTITY_TYPE_ARRAY_BRACKET:
            codegen_generate_entity_access_array_bracket(result, entity);
            break;
        case RESOLVER_ENTITY_TYPE_VARIABLE:
        case RESOLVER_ENTITY_TYPE_GENERAL:
            codegen_generate_entity_access_for_variable_or_general(result, entity);
            break;
        // ch148: function-call entity.
        case RESOLVER_ENTITY_TYPE_FUNCTION_CALL:
            codegen_generate_entity_access_for_function_call(result, entity);
            break;
        // ch154: unary indirection / get-address on read path.
        case RESOLVER_ENTITY_TYPE_UNARY_INDIRECTION:
            codegen_generate_entity_access_for_unary_indirection(result, entity, history);
            break;
        case RESOLVER_ENTITY_TYPE_UNARY_GET_ADDRESS:
            codegen_generate_entity_access_for_unary_get_address(result, entity);
            break;
        // ch155 / ch174: unsupported + cast.
        case RESOLVER_ENTITY_TYPE_UNSUPPORTED:
            codegen_generate_entity_access_for_unsupported(result, entity);
            break;
        case RESOLVER_ENTITY_TYPE_CAST:
            codegen_generate_entity_access_for_cast(result, entity);
            break;
        default:
            // todo: other entity kinds land in later chapters.
            break;
    }
}

static void codegen_generate_entity_access(struct resolver_result* result, struct resolver_entity* root_assignment_entity, struct node* top_most_node, struct history* history){
    (void)top_most_node;
    // The resolver has no entity for some roots: `(c ? s : t).m`, a
    // member of a conditional expression, crashed here.
    if(!root_assignment_entity){
        compiler_error(current_process, "x86: this expression is not supported (a member of a conditional expression?)\n");
    }
    // ch237: native function dispatch. When the root entity is a
    // RESOLVER_ENTITY_TYPE_NATIVE_FUNCTION, pull the registered
    // native_function and hand off to its callback with the call
    // arguments from the next entity in the chain.
    if(root_assignment_entity->type == RESOLVER_ENTITY_TYPE_NATIVE_FUNCTION){
        struct native_function* native_func = native_function_get(current_process, root_assignment_entity->name);
        if(native_func){
            asm_push("; NATIVE FUNCTION %s", root_assignment_entity->name);
            struct resolver_entity* func_call_entity = resolver_result_entity_next(root_assignment_entity);
            assert(func_call_entity && func_call_entity->type == RESOLVER_ENTITY_TYPE_FUNCTION_CALL);
            native_func->callbacks.call(&x86_codegen, native_func, func_call_entity->func_call_data.arguments);
            return;
        }
    }
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

// The bytes a call's arguments take. The resolver counts an argument it
// cannot follow as one dword, and it follows no conditional: a struct
// one takes its struct's dwords.
static size_t codegen_call_arguments_size(struct resolver_entity* entity){
    size_t size = entity->func_call_data.stack_size;
    struct vector* arguments = entity->func_call_data.arguments;
    for(int i = 0; i < vector_count(arguments); i++){
        struct datatype t;
        if(codegen_struct_conditional(*(struct node**)vector_at(arguments, i), &t)
           || codegen_atomic_struct_value(*(struct node**)vector_at(arguments, i), &t)){
            size += align_value(datatype_size(&t), DATA_SIZE_DWORD) - DATA_SIZE_DWORD;
        }
    }
    return size;
}

// ch148: function call code. Iterate args last-to-first (cdecl push
// order), then `call ecx` (callee address sits in ecx), pop args
// off the stack, push the eax return value as our result.
static void codegen_generate_entity_access_for_function_call(struct resolver_result* result, struct resolver_entity* entity){
    (void)result;
    size_t arguments_size = codegen_call_arguments_size(entity);
    vector_set_flag(entity->func_call_data.arguments, VECTOR_FLAG_PEEK_DECREMENT);
    vector_set_peek_pointer_end(entity->func_call_data.arguments);
    struct node* node = vector_peek_ptr(entity->func_call_data.arguments);
    // ch187: stash the callee into a freshly-allocated .data slot
    // before pushing arguments. Otherwise an argument that is itself
    // a function call would clobber ecx between here and the call.
    int function_call_label_id = codegen_label_count();
    codegen_data_section_add("function_call_%i: dd 0", function_call_label_id);
    asm_push_ins_pop("ebx", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
    // A call through a function pointer: the chain left the pointer's
    // address, so load the pointer itself.
    if(entity->func_call_data.through_pointer && !entity->func_call_data.callee_is_value){
        asm_push("mov ebx, [ebx]");
    }
    asm_push("mov dword [function_call_%i], ebx", function_call_label_id);
    size_t slot_elements = 0;
    if(datatype_is_struct_or_union_non_pointer(&entity->dtype)){
        // ch150: caller allocates the return slot; a pointer to it is
        // the hidden first argument the callee will write to.
        asm_push("; SUBTRACT ROOM FOR RETURNED STRUCTURE/UNION DATATYPE");
        codegen_stack_sub_with_name(align_value(datatype_size(&entity->dtype), DATA_SIZE_DWORD), "result_value");
        slot_elements = vector_count(current_function->func.frame.elements);
    }
    // The i386 psABI wants esp 16-byte aligned at the call.
    // ebp is 8 below a multiple of 16 (return address and saved ebp),
    // so the bytes below ebp at the call must be 8 mod 16. The pad goes
    // under the arguments.
    size_t returns_struct = datatype_is_struct_or_union_non_pointer(&entity->dtype);
    size_t call_bytes = arguments_size + (returns_struct ? DATA_SIZE_DWORD : 0);
    size_t below_ebp = (vector_count(current_function->func.frame.elements) - 1) * DATA_SIZE_DWORD + call_bytes;
    size_t pad = (C_STACK_ALIGNMENT + 8 - below_ebp % C_STACK_ALIGNMENT) % C_STACK_ALIGNMENT;
    codegen_stack_sub_with_name(pad, "call_alignment_pad");
    size_t elements_before_arguments = vector_count(current_function->func.frame.elements);
    while(node){
        codegen_generate_expressionable(node, codegen_history_begin(EXPRESSION_IN_FUNCTION_CALL_ARGUMENTS));
        node = vector_peek_ptr(entity->func_call_data.arguments);
    }
    if((vector_count(current_function->func.frame.elements) - elements_before_arguments) * DATA_SIZE_DWORD
       != arguments_size){
        compiler_error(current_process, "COMPILER BUG: x86 call arguments do not match their stack size\n");
    }
    if(datatype_is_struct_or_union_non_pointer(&entity->dtype)){
        // the hidden pointer is the first argument, so it has
        // to be pushed last, where the callee reads it ([ebp+8]).
        // ch150 pushed it before the arguments, which put it after the
        // last one and crashed. The slot sits just above the
        // arguments pushed since it was reserved.
        size_t arg_bytes = (vector_count(current_function->func.frame.elements) - slot_elements) * DATA_SIZE_DWORD;
        asm_push("lea ebx, [esp+%i]", (int)arg_bytes);
        asm_push_ins_push("ebx", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
    }
    asm_push("call [function_call_%i]", function_call_label_id);
    if(returns_struct){
        // The callee removed the hidden pointer with `ret 4`.
        stackframe_pop_expecting(current_function, STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
    }
    codegen_stack_add(arguments_size);
    codegen_stack_add_with_name(pad, "call_alignment_pad");
    if(datatype_is_struct_or_union_non_pointer(&entity->dtype)){
        // The callee wrote the struct into the room reserved before the
        // arguments, which is now on top of the stack and laid out as
        // pushed pieces are (first piece lowest). It is the value as it
        // stands. Pushing a second copy left the room behind, under the
        // copy: as a call argument it shifted every later argument
        // (`f(g(), k, h())`).
        struct vector* elements = current_function->func.frame.elements;
        int pieces = align_value(datatype_size(&entity->dtype), DATA_SIZE_DWORD) / DATA_SIZE_DWORD;
        for(int i = 0; i < pieces; i++){
            struct stack_frame_element* el = vector_at(elements, vector_count(elements) - 1 - i);
            el->type       = STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE;
            el->flags     |= STACK_FRAME_ELEMENT_FLAG_HAS_DATATYPE;
            el->data.dtype = entity->dtype;
        }
        if(entity != result->last_entity){
            // `f().m`: the member access after the call takes the
            // struct's address, not its first piece. The struct moves to
            // this call's own .data copy, so the room does not stay on
            // the stack under the rest of the expression (`1 + f().m`).
            // The member is read straight away, before anything else
            // can call here again.
            codegen_data_section_add("struct_result_%i: times %i db 0", function_call_label_id, pieces * DATA_SIZE_DWORD);
            for(int i = 0; i < pieces; i++){
                asm_push("mov eax, [esp+%i]", i * DATA_SIZE_DWORD);
                asm_push("mov [struct_result_%i+%i], eax", function_call_label_id, i * DATA_SIZE_DWORD);
            }
            codegen_stack_add(pieces * DATA_SIZE_DWORD);
            asm_push("mov ebx, struct_result_%i", function_call_label_id);
            asm_push_ins_push_with_data("ebx",
                STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value", 0,
                &(struct stack_frame_data){.dtype = entity->dtype});
            return;
        }
        codegen_response_acknowledge(&(struct response){.flags = RESPONSE_FLAG_PUSHED_STRUCTURE});
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
    if(is_access_node(node)){
        // the chain would go on from a struct value an atomic
        // builtin pushed; the resolver drops the member and nothing here
        // follows it.
        struct node* left = node->exp.left;
        while(left->type == NODE_TYPE_EXPRESSION_PARENTHESES){
            left = left->parenthesis.exp;
        }
        if(is_parentheses_node(left) && left->exp.left->type == NODE_TYPE_IDENTIFIER && atomic_builtin_get(left->exp.left->sval)){
            compiler_error(current_process, "x86: a member of the struct value of %s is not supported; assign the value to a variable first\n",
                left->exp.left->sval);
        }
    }
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
        && !(result->flags & RESOLVER_RESULT_FLAG_FIRST_ENTITY_PUSH_VALUE)
        && !resolver_entity_is_struct_element(result->last_entity);
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
    // ch153: peek the dtype off the top of the ledger and post-process
    // the pushed value:
    //   - struct/union value: push the structure proper.
    //   - non-pointer primitive: pop -> optional final indirection ->
    //     reduce-register (sign/zero extend) -> push as the typed
    //     result_value.
    struct datatype dtype;
    // A struct used before its definition left no typed value (found by
    // tests/322 once its seeds moved with the atomics tests): an error,
    // not an assert.
    if(!asm_datatype_back(&dtype)){
        compiler_error(current_process, "x86: this expression has no value of a known type (a struct used before its definition?)\n");
    }
    if(codegen_result_is_array_address(result, &dtype)){
        // The chain left an array's address (a whole array, or a row of
        // a multi-dimensional one): the value is that address, as a
        // pointer to the element type.
        asm_push_ins_pop("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
        struct datatype decayed = datatype_decay(&dtype);
        asm_push_ins_push_with_data("eax",
            STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value", 0,
            &(struct stack_frame_data){.dtype = decayed});
    } else if(codegen_result_is_pointer_load(result, &dtype)){
        asm_push_ins_pop("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
        asm_push("mov eax, [eax]");
        struct datatype element = dtype;
        element.flags &= ~DATATYPE_FLAG_IS_ARRAY;
        asm_push_ins_push_with_data("eax",
            STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value", 0,
            &(struct stack_frame_data){.dtype = element});
    } else if(datatype_is_struct_or_union_non_pointer(&dtype)){
        // The chain left the struct's address (`t[1]`, `o.inner`); a
        // call already pushed the pieces. Pushing from ebx without
        // popping read whatever it held and left the address behind.
        // The pushed type says how big: through a cast
        // (`*(struct s*)p`, va_arg of a struct) the last entity is the
        // indirection, whose own type is not the struct's.
        if(result->last_entity->type != RESOLVER_ENTITY_TYPE_FUNCTION_CALL){
            asm_push_ins_pop("ebx", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
            struct resolver_entity pushed = *result->last_entity;
            pushed.dtype = dtype;
            codegen_generate_structure_push(&pushed, history, 0);
        }
    } else if(!(dtype.flags & DATATYPE_FLAG_IS_POINTER) && dtype.type != DATA_TYPE_VOID){
        // a void result (va_start, va_end, a void call) has no
        // value to widen;
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
        // An unsigned remainder clears edx; cdq sign-extended a dividend
        // above 0x7fffffff and div then trapped.
        asm_push("mov ecx, %s", value);
        if(is_signed){ asm_push("cdq"); asm_push("idiv ecx"); }
        else         { asm_push("xor edx, edx"); asm_push("div ecx"); }
        asm_push("mov eax, edx");
    } else if(flags & EXPRESSION_IS_ABOVE){
        codegen_gen_cmp(value, is_signed ? "setg" : "seta");
    } else if(flags & EXPRESSION_IS_BELOW){
        codegen_gen_cmp(value, is_signed ? "setl" : "setb");
    } else if(flags & EXPRESSION_IS_EQUAL){
        codegen_gen_cmp(value, "sete");
    } else if(flags & EXPRESSION_IS_ABOVE_OR_EQUAL){
        codegen_gen_cmp(value, is_signed ? "setge" : "setae");
    } else if(flags & EXPRESSION_IS_BELOW_OR_EQUAL){
        codegen_gen_cmp(value, is_signed ? "setle" : "setbe");
    } else if(flags & EXPRESSION_IS_NOT_EQUAL){
        codegen_gen_cmp(value, "setne");
    } else if(flags & EXPRESSION_IS_BITSHIFT_LEFT){
        value = codegen_sub_register(value, DATA_SIZE_BYTE);
        asm_push("sal %s, %s", reg, value);
    } else if(flags & EXPRESSION_IS_BITSHIFT_RIGHT){
        value = codegen_sub_register(value, DATA_SIZE_BYTE);
        asm_push("%s %s, %s", is_signed ? "sar" : "shr", reg, value);
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

// Any non-zero operand makes `||` true. The book's `jg` counted a
// negative one as false, so `-1 || 0` was 0.
static void codegen_generate_logical_cmp_or(const char* reg, const char* equal_label){
    asm_push("cmp %s, 0", reg);
    asm_push("jne %s", equal_label);
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
        // The 0/1 result is an int. Pushed without a type, a
        // `return a || b;` asserted on the missing datatype.
        asm_push_ins_push_with_data("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value", 0,
            &(struct stack_frame_data){.dtype = datatype_for_int()});
    }
}

// eax = eax / n for a byte difference that is an exact multiple of n
// (a pointer difference).
static void codegen_divide_exact(size_t n){
    if((n & (n - 1)) == 0){
        int shift = 0;
        while(((size_t)1 << shift) < n){
            shift++;
        }
        asm_push("sar eax, %i", shift);
        return;
    }
    asm_push("mov ecx, %zu", n);
    asm_push("cdq");
    asm_push("idiv ecx");
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
        // ch146: pointer arithmetic - scale the integer side of `p + n`
        // by sizeof(*p) so it advances n elements. datatype_binary_types
        // also gives the type the operation runs in (signed or not) and
        // the result's type; the signedness used to come from the right
        // operand alone, and every operator was scaled, comparisons too.
        struct datatype_binary types;
        datatype_binary_types(&left_dtype, &right_dtype, node->exp.op, &types);
        if(types.scale_left > 1){
            asm_push("imul eax, %zu", types.scale_left);
        }
        if(types.scale_right > 1){
            asm_push("imul ecx, %zu", types.scale_right);
        }
        codegen_gen_math_for_value("eax", "ecx", op_flags, types.operation.flags & DATATYPE_FLAG_IS_SIGNED);
        if(types.pointer_difference > 1){
            codegen_divide_exact(types.pointer_difference);
        }
        last_dtype = types.result;
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
    struct datatype conditional_type;
    if(codegen_struct_conditional(node, &conditional_type)){
        codegen_generate_struct_conditional(node, &conditional_type, history);
        return;
    }
    struct node* direct = codegen_access_through_address(node);
    if(direct != node){
        codegen_generate_expressionable(direct, history);
        return;
    }
    struct node* conditional = codegen_access_conditional(node);
    if(conditional){
        codegen_generate_expressionable(codegen_access_of_conditional(node, conditional), history);
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

// ---------------------------------------------------------------
// A21: floating point, on the x87 FPU.
// ---------------------------------------------------------------
//
// The resolver chains above know integers only. An expression in which
// floating point takes part (a float or double value, or an operator,
// cast or call with such an operand) is generated here instead, from
// its C type as exp64 does it on the 64-bit targets: a floating value
// ends in st0, any other in eax, and nothing is left on the stack. The
// integer parts inside it (an index, a pointer, an int operand) go
// through codegen_generate_expressionable as before, and the address
// of a float or double object is the value of `&object` there. A
// program without floating point never comes here
// (compile_process.uses_floating), so its code is unchanged.
//
// cdecl: a float or double argument is 4 or 8 bytes on the stack, a
// float passed to `...` or without a prototype is a double, and the
// result comes back in st0. The x87 stack is empty at every call: a
// value that waits for another operand is spilled to the stack. Each
// result is rounded to its type (float or double) through memory, as
// the SSE and FP units of the 64-bit targets round it; gcc -m32 keeps
// x87 extended precision between operations, so a result can differ
// in the last bit.

static struct datatype x87_type(struct node* node);
static struct datatype x87_value(struct node* node);
static bool            x87_involves(struct node* node);
static struct datatype x87_call_emit(struct node* node, bool struct_result);

// a long double is the x87's own 80-bit format, 12 bytes in
// memory (a tword and two bytes of padding); it is never rounded.
static bool x87_is_floating(struct datatype* t){
    return datatype_is_floating(t) || datatype_is_long_double(t);
}

static bool x87_is_long(struct datatype* t){
    return datatype_is_long_double(t);
}

static bool x87_is_comparison(const char* op){
    return S_EQ(op, "==") || S_EQ(op, "!=") || S_EQ(op, "<") || S_EQ(op, ">")
        || S_EQ(op, "<=") || S_EQ(op, ">=");
}

// The operands of a call, the comma operators taken apart.
static void x87_flatten_arguments(struct node* node, struct vector* out){
    if(!node || node->type == NODE_TYPE_BLANK){
        return;
    }
    if(node->type == NODE_TYPE_EXPRESSION && S_EQ(node->exp.op, ",")){
        x87_flatten_arguments(node->exp.left, out);
        x87_flatten_arguments(node->exp.right, out);
        return;
    }
    vector_push(out, &node);
}

static struct vector* x87_call_arguments(struct node* node){
    struct vector* args = vector_create(sizeof(struct node*));
    struct node* right = node->exp.right;
    if(right->type == NODE_TYPE_EXPRESSION_PARENTHESES){
        right = right->parenthesis.exp;
    }
    x87_flatten_arguments(right, args);
    return args;
}

static struct node* x87_strip(struct node* node){
    while(node->type == NODE_TYPE_EXPRESSION_PARENTHESES){
        node = node->parenthesis.exp;
    }
    return node;
}

// ---- Types: what x87_value will give, worked out without output.

static struct datatype x87_deref(struct datatype* t){
    if(t->flags & DATATYPE_FLAG_IS_ARRAY){
        struct datatype e = *t;
        struct vector* brackets = t->array.brackets ? t->array.brackets->n_brackets : 0;
        if(!brackets || vector_count(brackets) <= 1){
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
    if(t->flags & DATATYPE_FLAG_IS_FUNCTION_POINTER){
        return *t;
    }
    return *datatype_pointer_reduce(t, 1);
}

// The member `name` of the struct or union t, or NULL when there is
// none (the integer generator reports that).
static struct node* x87_member(struct datatype* t, const char* name){
    if(!datatype_is_struct_or_union(t) || !t->type_str){
        return 0;
    }
    struct symbol* sym = symresolver_get_tag(current_process, t->type_str);
    if(!sym || sym->type != SYMBOL_TYPE_NODE){
        return 0;
    }
    struct node* su = sym->data;
    if(!node_is_struct_or_union(su) || !su->_struct.body_n){
        return 0;
    }
    return struct_member_named(su->_struct.body_n->body.statements, name);
}

static struct datatype x87_member_type(struct datatype* t, const char* name){
    struct node* member = x87_member(t, name);
    return member ? member->var.type : datatype_for_int();
}

// The function a call names, or NULL for a call through a pointer.
static struct resolver_entity* x87_called_function(struct node* callee){
    callee = x87_strip(callee);
    if(callee->type != NODE_TYPE_IDENTIFIER || resolver_get_variable(0, current_process->resolver, callee->sval)){
        return 0;
    }
    return resolver_get_function(0, current_process->resolver, callee->sval);
}

// The type of parameter i of the function a call names, or of the
// prototype of the function pointer it calls through; false
// past the parameters or without a prototype.
static bool x87_param_type(struct node* callee, int i, struct datatype* out){
    struct resolver_entity* function = x87_called_function(callee);
    if(function){
        struct vector* params = function->node->func.args.vector;
        if(!params || i >= vector_count(params)){
            return false;
        }
        *out = (*(struct node**)vector_at(params, i))->var.type;
        return true;
    }
    struct datatype t = x87_type(callee);
    struct function_prototype* prototype = (t.flags & DATATYPE_FLAG_IS_FUNCTION_POINTER) ? t.function_params : 0;
    if(!prototype || i >= vector_count(prototype->types)){
        return false;
    }
    *out = *(struct datatype*)vector_at(prototype->types, i);
    return true;
}

// a call of __atomic_load_n or __atomic_exchange_n on an
// _Atomic long double, whose value is that long double (in st0, from
// x87_atomic_ld_call); *t its type.
static bool x87_atomic_ld_value(struct node* node, struct datatype* t){
    node = x87_strip(node);
    if(!is_parentheses_node(node) || x87_strip(node->exp.left)->type != NODE_TYPE_IDENTIFIER){
        return false;
    }
    const struct atomic_builtin* b = atomic_builtin_get(x87_strip(node->exp.left)->sval);
    if(!b || !(b->kind == ATOMIC_BUILTIN_LOAD || (b->kind == ATOMIC_BUILTIN_EXCHANGE && strncmp(b->name, "__sync_", 7)))){
        return false;
    }
    struct vector* args = x87_call_arguments(node);
    if(vector_count(args) != b->args){
        return false;
    }
    struct datatype pt = x87_type(*(struct node**)vector_at(args, 0));
    if(!(pt.flags & (DATATYPE_FLAG_IS_POINTER | DATATYPE_FLAG_IS_ARRAY))){
        return false;
    }
    struct datatype e = x87_deref(&pt);
    if(!x87_is_long(&e)){
        return false;
    }
    e.atomic_level = 0;
    *t = e;
    return true;
}

static struct datatype x87_call_type(struct node* node){
    struct datatype atomic;
    if(x87_atomic_ld_value(node, &atomic)){
        return atomic;
    }
    struct resolver_entity* function = x87_called_function(node->exp.left);
    if(function){
        return function->dtype;
    }
    struct datatype t = x87_type(node->exp.left);
    if(!(t.flags & DATATYPE_FLAG_IS_FUNCTION_POINTER)){
        return datatype_for_int();
    }
    t = *datatype_pointer_reduce(&t, 1);
    t.flags &= ~DATATYPE_FLAG_IS_FUNCTION_POINTER;
    return t;
}

static struct datatype x87_expression_type(struct node* node){
    const char* op = node->exp.op;
    if(is_node_assignment(node)){
        return x87_type(node->exp.left);
    }
    if(S_EQ(op, ",")){
        return x87_type(node->exp.right);
    }
    if(is_logical_operator(op) || x87_is_comparison(op)){
        return datatype_for_int();
    }
    if(S_EQ(op, "?")){
        struct node* tenary = node->exp.right;
        if(tenary->type != NODE_TYPE_TENARY){
            return datatype_for_int();
        }
        struct datatype yes = x87_type(tenary->tenary.true_node);
        struct datatype no  = x87_type(tenary->tenary.false_node);
        if(x87_is_floating(&yes) || x87_is_floating(&no)){
            struct datatype_binary types;
            datatype_binary_types(&yes, &no, "+", &types);
            return types.result;
        }
        return yes;
    }
    if(is_parentheses_node(node)){
        return x87_call_type(node);
    }
    if(is_array_node(node)){
        struct datatype t = x87_type(node->exp.left);
        return x87_deref(&t);
    }
    if(is_access_node(node)){
        struct datatype t = x87_type(node->exp.left);
        if(node->exp.right->type != NODE_TYPE_IDENTIFIER){
            return datatype_for_int();
        }
        return x87_member_type(&t, node->exp.right->sval);
    }
    struct datatype left = x87_type(node->exp.left);
    struct datatype right = x87_type(node->exp.right);
    struct datatype_binary types;
    datatype_binary_types(&left, &right, op, &types);
    return types.result;
}

static struct datatype x87_type(struct node* node){
    switch(node->type){
        case NODE_TYPE_NUMBER:
            return datatype_for_number_node(node);
        case NODE_TYPE_STRING:
            return datatype_for_string();
        case NODE_TYPE_IDENTIFIER: {
            struct resolver_entity* entity = resolver_get_variable(0, current_process->resolver, node->sval);
            if(entity){
                return entity->dtype;
            }
            entity = resolver_get_function(0, current_process->resolver, node->sval);
            if(entity){
                struct datatype t = entity->dtype;
                t.flags |= DATATYPE_FLAG_IS_POINTER | DATATYPE_FLAG_IS_FUNCTION_POINTER;
                t.pointer_depth++;
                return t;
            }
            return datatype_for_int();
        }
        case NODE_TYPE_EXPRESSION_PARENTHESES:
            return x87_type(node->parenthesis.exp);
        case NODE_TYPE_CAST:
            return node->cast.dtype;
        case NODE_TYPE_UNARY: {
            const char* op = node->unary.op;
            struct datatype t = x87_type(node->unary.operand);
            if(op_is_indirection(op)){
                for(int i = 0; i < node->unary.indirection.depth; i++){
                    t = x87_deref(&t);
                }
                return t;
            }
            if(op_is_address(op)){
                t.flags &= ~DATATYPE_FLAG_IS_ARRAY;
                t.flags |= DATATYPE_FLAG_IS_POINTER;
                t.pointer_depth++;
                return t;
            }
            if(S_EQ(op, "!")){
                return datatype_for_int();
            }
            if(is_increment_operator(op)){
                return t;
            }
            return datatype_promote(&t);
        }
        case NODE_TYPE_EXPRESSION:
            return x87_expression_type(node);
    }
    return datatype_for_int();
}

// ---- Which nodes are generated here.

static bool x87_type_is_floating(struct node* node){
    struct datatype t = x87_type(node);
    return x87_is_floating(&t);
}

// Floating point takes part in `node` itself: its value is floating, or
// an operand of its operator (or an argument or parameter of its call)
// is. `&x` of a double x is a pointer and stays with the integer code.
static bool x87_involves(struct node* node){
    switch(node->type){
        case NODE_TYPE_NUMBER:
            return node->num.flags & NUMBER_FLAG_FLOATING;
        case NODE_TYPE_IDENTIFIER:
            return x87_type_is_floating(node);
        case NODE_TYPE_EXPRESSION_PARENTHESES:
            return x87_involves(node->parenthesis.exp);
        case NODE_TYPE_CAST:
            return x87_is_floating(&node->cast.dtype) || x87_type_is_floating(node->cast.operand);
        case NODE_TYPE_UNARY:
            if(op_is_address(node->unary.op)){
                return false;
            }
            return x87_type_is_floating(node) || x87_type_is_floating(node->unary.operand);
        case NODE_TYPE_EXPRESSION:
            break;
        default:
            return false;
    }
    if(x87_type_is_floating(node)){
        return true;
    }
    const char* op = node->exp.op;
    if(is_parentheses_node(node)){
        struct vector* args = x87_call_arguments(node);
        for(int i = 0; i < vector_count(args); i++){
            if(x87_type_is_floating(*(struct node**)vector_at(args, i))){
                return true;
            }
        }
        struct datatype param;
        for(int i = 0; x87_param_type(node->exp.left, i, &param); i++){
            if(x87_is_floating(&param)){
                return true;
            }
        }
        return false;
    }
    if(S_EQ(op, "?")){
        struct node* tenary = node->exp.right;
        return x87_type_is_floating(node->exp.left)
            || (tenary->type == NODE_TYPE_TENARY
                && (x87_type_is_floating(tenary->tenary.true_node) || x87_type_is_floating(tenary->tenary.false_node)));
    }
    if(S_EQ(op, ",")){
        return x87_involves(node->exp.left) || x87_involves(node->exp.right);
    }
    if(is_array_node(node) || is_access_node(node)){
        return false;
    }
    return x87_type_is_floating(node->exp.left) || x87_type_is_floating(node->exp.right);
}

static bool x87_wanted(struct node* node){
    return current_process->uses_floating && x87_involves(node);
}

// ---- Instructions.

// The constant 1.0f in .data, for ++ and --. The scratch of a
// conversion or a rounding (a value, the control words of a truncation)
// is stack room below esp, outside the ledger and released straight
// away: it was a qword and two words in .data, shared by
// every thread, so threads converting at once got each other's values.
static void x87_scratch(void){
    static bool emitted;
    if(!emitted){
        codegen_data_section_add("x87_one: dd 0x3f800000");
        emitted = true;
    }
}

static const char* x87_size_keyword(int size){
    return size == DATA_SIZE_DWORD ? "dword" : size == DATA_SIZE_DDWORD ? "qword" : "tword";
}

// Room on the stack, in the ledger like every push.
static void x87_reserve(int bytes){
    codegen_stack_sub_with_name(bytes, "x87_spill");
}

static void x87_release(int bytes){
    codegen_stack_add_with_name(bytes, "x87_spill");
}

// st0 to new stack room of the bytes a value of the floating type t is
// spilled in (exact for a float too): a qword, or a long double's 12.
static int x87_spill_size(struct datatype* t){
    return x87_is_long(t) ? (int)t->size : DATA_SIZE_DDWORD;
}

static void x87_spill_as(struct datatype* t){
    x87_reserve(x87_spill_size(t));
    asm_push("fstp %s [esp]", x87_size_keyword(x87_spill_size(t)));
}

// st0 rounded to a float or double; a long double is left as it is.
static void x87_round(int size){
    if(size > DATA_SIZE_DDWORD){
        return;
    }
    x87_scratch();
    asm_push("sub esp, 8");
    asm_push("fstp %s [esp]", x87_size_keyword(size));
    asm_push("fld %s [esp]", x87_size_keyword(size));
    asm_push("add esp, 8");
}

// eax = st0 != 0, st0 popped. A NaN is true, -0.0 false.
static void x87_truth_of_st0(void){
    asm_push("fldz");
    asm_push("fucomip st0, st1");
    asm_push("fstp st0");
    asm_push("setne al");
    asm_push("setp dl");
    asm_push("or al, dl");
    asm_push("movzx eax, al");
}

// eax narrowed to an integer type of 1 or 2 bytes, extended back.
static void x87_reduce(struct datatype* t){
    size_t size = datatype_element_size(t);
    if(size == DATA_SIZE_BYTE || size == DATA_SIZE_WORD){
        codegen_reduce_register("eax", size, t->flags & DATATYPE_FLAG_IS_SIGNED);
    }
}

// The value of type `from` (st0 or eax) as type `to` (C11 6.3.1.4,
// 6.3.1.5). A floating value truncates toward zero into an integer
// (rounding control set to chop for the fistp), a _Bool is its test
// against 0. An unsigned int goes through a zero-extended qword.
static void x87_convert(struct datatype* from, struct datatype* to){
    bool from_floating = x87_is_floating(from);
    if(to->type == DATA_TYPE_VOID && !(to->flags & DATATYPE_FLAG_IS_POINTER)){
        if(from_floating){
            asm_push("fstp st0");
        }
        return;
    }
    if(x87_is_floating(to)){
        if(!from_floating){
            if(datatype_is_struct_or_union_non_pointer(from) || (from->flags & DATATYPE_FLAG_IS_POINTER)){
                compiler_error(current_process, "This value cannot become a floating value\n");
            }
            x87_scratch();
            asm_push("sub esp, 8");
            asm_push("mov [esp], eax");
            if(!(from->flags & DATATYPE_FLAG_IS_SIGNED) && datatype_element_size(from) == DATA_SIZE_DWORD){
                asm_push("mov dword [esp+4], 0");
                asm_push("fild qword [esp]");
            } else {
                asm_push("fild dword [esp]");
            }
            asm_push("add esp, 8");
            if(to->size == DATA_SIZE_DWORD){
                x87_round(DATA_SIZE_DWORD);
            }
        } else if(from->size > to->size){
            x87_round(to->size);
        }
        return;
    }
    if(!from_floating){
        x87_reduce(to);
        return;
    }
    if(to->flags & DATATYPE_FLAG_IS_BOOL){
        x87_truth_of_st0();
        return;
    }
    if((to->flags & DATATYPE_FLAG_IS_POINTER) || datatype_is_struct_or_union_non_pointer(to)){
        compiler_error(current_process, "A floating value cannot become this type\n");
    }
    x87_scratch();
    asm_push("sub esp, 12");
    asm_push("fnstcw [esp+8]");
    asm_push("mov ax, [esp+8]");
    asm_push("or ax, 0x0c00");
    asm_push("mov [esp+10], ax");
    asm_push("fldcw [esp+10]");
    asm_push("fistp qword [esp]");
    asm_push("fldcw [esp+8]");
    asm_push("mov eax, [esp]");
    asm_push("add esp, 12");
    x87_reduce(to);
}

// ---- Values.

// An integer value from the integer generator, in eax.
static struct datatype x87_integer(struct node* node){
    codegen_generate_expressionable(node, codegen_history_begin(EXPRESSION_IS_NOT_ROOT_NODE));
    struct datatype t = datatype_for_int();
    asm_datatype_back(&t);
    if(datatype_is_struct_or_union_non_pointer(&t)){
        compiler_error(current_process, "x86: a struct value cannot be used with floating point here\n");
    }
    asm_push_ins_pop("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
    return t;
}

// eax = the address of the object `node` designates. A member and an
// element are worked out here from the address or pointer under them;
// a name, or anything else, is `&node` from the integer generator.
static void x87_address(struct node* node){
    node = x87_strip(node);
    if(node->type == NODE_TYPE_UNARY && op_is_indirection(node->unary.op) && node->unary.indirection.depth == 1){
        x87_value(node->unary.operand);
        return;
    }
    if(is_access_node(node) && node->exp.right->type == NODE_TYPE_IDENTIFIER){
        struct datatype t = x87_type(node->exp.left);
        struct node* member = x87_member(&t, node->exp.right->sval);
        if(!member){
            compiler_error(current_process, "%s has no member %s\n", t.type_str ? t.type_str : "this", node->exp.right->sval);
        }
        if(S_EQ(node->exp.op, "->")){
            x87_value(node->exp.left);
        } else {
            x87_address(node->exp.left);
        }
        int offset = t.type == DATA_TYPE_UNION ? 0 : member->var.aoffset;
        if(offset){
            asm_push("add eax, %i", offset);
        }
        return;
    }
    if(is_array_node(node)){
        struct datatype t = x87_type(node->exp.left);
        struct datatype element = x87_deref(&t);
        if(t.flags & DATATYPE_FLAG_IS_ARRAY){
            x87_address(node->exp.left);
        } else {
            x87_value(node->exp.left);
        }
        x87_reserve(DATA_SIZE_DWORD);
        asm_push("mov [esp], eax");
        struct node* index = node->exp.right->type == NODE_TYPE_BRACKET ? node->exp.right->bracket.inner : node->exp.right;
        x87_value(index);
        asm_push("imul eax, %i", (int)datatype_size(&element));
        asm_push("add eax, [esp]");
        x87_release(DATA_SIZE_DWORD);
        return;
    }
    struct node* address = calloc(1, sizeof(struct node));
    address->type          = NODE_TYPE_UNARY;
    address->unary.op      = "&";
    address->unary.operand = node;
    address->binded        = node->binded;
    address->pos           = node->pos;
    x87_integer(address);
}

// st0 or eax = the object of type t at the address in eax.
static void x87_load(struct datatype* t){
    if(x87_is_long(t) && datatype_is_atomic(t)){
        // 12 bytes, not lock-free: libatomic, through room
        // on the stack.
        x87_reserve((int)t->size);
        asm_push("mov ecx, eax");
        asm_push("mov edx, esp");
        codegen_x86_atomic_copy(false, (int)t->size, ATOMIC_ORDER_SEQ_CST);
        asm_push("fld tword [esp]");
        x87_release((int)t->size);
        return;
    }
    if(x87_is_floating(t)){
        asm_push("fld %s [eax]", x87_size_keyword(t->size));
        return;
    }
    codegen_load_eax("eax", datatype_element_size(t), t->flags & DATATYPE_FLAG_IS_SIGNED);
}

// The object at the address in ecx = the value of type t (st0, kept as
// the expression's value, or eax).
// a store to an _Atomic object is sequentially consistent: a
// floating one is followed by mfence, an integer one is an xchg from
// edx (eax keeps the value).
static void x87_store(struct datatype* t){
    if(x87_is_long(t) && datatype_is_atomic(t)){
        // the value to room on the stack (and back to st0),
        // then libatomic's store from there.
        x87_reserve((int)t->size);
        asm_push("fstp tword [esp]");
        asm_push("fld tword [esp]");
        asm_push("mov edx, esp");
        codegen_x86_atomic_copy(true, (int)t->size, ATOMIC_ORDER_SEQ_CST);
        x87_release((int)t->size);
        return;
    }
    if(x87_is_long(t)){
        // fst has no tword form.
        asm_push("fstp tword [ecx]");
        asm_push("fld tword [ecx]");
        return;
    }
    if(x87_is_floating(t)){
        asm_push("fst %s [ecx]", x87_size_keyword(t->size));
        if(datatype_is_atomic(t)){
            asm_push("mfence");
        }
        return;
    }
    const char* reg = datatype_is_atomic(t) ? "edx" : "eax";
    const char* keyword = codegen_byte_word_or_dword_or_ddword(datatype_element_size(t), &reg);
    if(datatype_is_atomic(t)){
        asm_push("mov edx, eax");
        asm_push("xchg %s [ecx], %s", keyword, reg);
        return;
    }
    asm_push("mov %s [ecx], %s", keyword, reg);
}

static void x87_constant(struct node* node, struct datatype* t){
    int id = codegen_label_count();
    if(x87_is_long(t)){
        // the tword from the constant's spelling.
        unsigned char bytes[16];
        long_double_from_text(node->num.text, bytes);
        unsigned int words[3];
        for(int w = 0; w < 3; w++){
            words[w] = bytes[4 * w] | bytes[4 * w + 1] << 8 | bytes[4 * w + 2] << 16 | (unsigned int)bytes[4 * w + 3] << 24;
        }
        codegen_data_section_add("x87_const_%i: dd %u, %u, %u", id, words[0], words[1], words[2]);
        asm_push("fld tword [x87_const_%i]", id);
        return;
    }
    unsigned long long bits = floating_bits(number_node_double(node), t->size);
    codegen_data_section_add("x87_const_%i: %s %llu", id, t->size == DATA_SIZE_DWORD ? "dd" : "dq", bits);
    asm_push("fld %s [x87_const_%i]", x87_size_keyword(t->size), id);
}

// `node` as type t: st0 for a floating t.
static void x87_operand_as(struct node* node, struct datatype* t){
    struct datatype v = x87_value(node);
    x87_convert(&v, t);
}

// eax = 0 when `node` is false.
static void x87_truth(struct node* node){
    struct datatype t = x87_value(node);
    if(x87_is_floating(&t)){
        x87_truth_of_st0();
    }
}

static bool x87_is_struct_call(struct node* node);

static void x87_effect(struct node* node){
    if(x87_is_struct_call(node)){
        struct datatype rtype = x87_call_emit(x87_strip(node), true);
        codegen_stack_add_with_name(align_value(datatype_size(&rtype), DATA_SIZE_DWORD), "result_value");
        return;
    }
    struct datatype t = x87_value(node);
    if(x87_is_floating(&t)){
        asm_push("fstp st0");
    }
}

static const char* x87_arith_ins(const char* op){
    if(S_EQ(op, "+")) return "fadd";
    if(S_EQ(op, "-")) return "fsub";
    if(S_EQ(op, "*")) return "fmul";
    if(S_EQ(op, "/")) return "fdiv";
    compiler_error(current_process, "The operator %s does not take floating operands\n", op);
    return 0;
}

// Both operands in the common type c, spilled: the right one at [esp],
// the left at [esp+8] (at [esp+12] for a long double).
static void x87_operands(struct node* left, struct node* right, struct datatype* c){
    x87_operand_as(left, c);
    x87_spill_as(c);
    x87_operand_as(right, c);
    x87_spill_as(c);
}

static struct datatype x87_common(struct node* left, struct node* right){
    struct datatype l = x87_type(left);
    struct datatype r = x87_type(right);
    if((l.flags & DATATYPE_FLAG_IS_POINTER) || (r.flags & DATATYPE_FLAG_IS_POINTER)
       || datatype_is_struct_or_union_non_pointer(&l) || datatype_is_struct_or_union_non_pointer(&r)){
        compiler_error(current_process, "Invalid operands: a floating value with a pointer or struct\n");
    }
    struct datatype_binary types;
    datatype_binary_types(&l, &r, "+", &types);
    return types.operation;
}

static struct datatype x87_arith(struct node* node){
    struct datatype c = x87_common(node->exp.left, node->exp.right);
    const char* ins = x87_arith_ins(node->exp.op);
    x87_operands(node->exp.left, node->exp.right, &c);
    int spill = x87_spill_size(&c);
    if(x87_is_long(&c)){
        // No tword operand for the arithmetic: st1 op st0, popped.
        asm_push("fld tword [esp+%i]", spill);
        asm_push("fld tword [esp]");
        asm_push("%sp st1, st0", ins);
    } else {
        asm_push("fld qword [esp+8]");
        asm_push("%s qword [esp]", ins);
    }
    x87_release(2 * spill);
    x87_round(c.size);
    return c;
}

// fucomip sets ZF, PF and CF like ucomisd: an unordered pair (a NaN)
// sets all three, so `<` loads the operands the other way round and
// asks for `above`.
static struct datatype x87_compare(struct node* node){
    const char* op = node->exp.op;
    struct datatype c = x87_common(node->exp.left, node->exp.right);
    x87_operands(node->exp.left, node->exp.right, &c);
    bool swap = S_EQ(op, "<") || S_EQ(op, "<=");
    int spill = x87_spill_size(&c);
    const char* keyword = x87_size_keyword(spill);
    asm_push("fld %s [esp+%i]", keyword, swap ? spill : 0);
    asm_push("fld %s [esp+%i]", keyword, swap ? 0 : spill);
    asm_push("fucomip st0, st1");
    asm_push("fstp st0");
    if(S_EQ(op, "==") || S_EQ(op, "!=")){
        bool eq = S_EQ(op, "==");
        asm_push(eq ? "sete al" : "setne al");
        asm_push(eq ? "setnp dl" : "setp dl");
        asm_push(eq ? "and al, dl" : "or al, dl");
    } else {
        asm_push((S_EQ(op, "<") || S_EQ(op, ">")) ? "seta al" : "setae al");
    }
    asm_push("movzx eax, al");
    // The flags are read: `add esp` may change them now.
    x87_release(2 * spill);
    return datatype_for_int();
}

// `a op= v`, `++a`, `a++` on an _Atomic float or double a,
// one atomic read-modify-write as gcc -m32 makes it: a compare-and-swap
// loop. The old value's bits are read, `old op v` is worked out on the
// x87 and rounded to a's type, and a lock cmpxchg (cmpxchg8b for a
// double) stores the new bits only if a still holds the old ones, else
// the loop retries with what it found there. cmpxchg8b compares edx:eax
// and stores ecx:ebx, so the address goes in esi, saved around it (esi
// holds the GOT under -fPIC). The room on the stack: [esp] the new bits,
// [esp+8] the old ones, then the operand spilled as a qword (none for
// ++ and --), then the address.
static void codegen_x86_atomic_cas_memory(int size, int success, int failure);
static void codegen_x86_atomic_exchange_memory(int size, int order);

// st0 = __atomic_load_n(p, mo) or __atomic_exchange_n(p, v, mo)
// on the _Atomic long double *p (of type t): through room on the stack
// and libatomic. The room: [esp] the result, then the value given to an
// exchange, then the address.
static struct datatype x87_atomic_ld_call(struct node* node, struct datatype* t){
    struct vector* args = x87_call_arguments(node);
    const struct atomic_builtin* b = atomic_builtin_get(x87_strip(node->exp.left)->sval);
    bool exchange = b->kind == ATOMIC_BUILTIN_EXCHANGE;
    int size = (int)t->size;
    struct node* order_node = *(struct node**)vector_at(args, b->args - 1);
    bool constant;
    int order = atomic_order(current_process, order_node, exchange ? ATOMIC_ACCESS_RMW : ATOMIC_ACCESS_LOAD, &constant);
    if(!constant){
        x87_effect(order_node);
    }
    x87_value(*(struct node**)vector_at(args, 0));
    x87_reserve(DATA_SIZE_DWORD);
    asm_push("mov [esp], eax");
    if(exchange){
        x87_operand_as(*(struct node**)vector_at(args, 1), t);
        x87_spill_as(t);
    }
    x87_reserve(size);
    if(exchange){
        asm_push("mov ecx, [esp+%i]", 2 * size);
        asm_push("lea edx, [esp+%i]", size);
        asm_push("mov ebx, esp");
        codegen_x86_atomic_exchange_memory(size, order);
    } else {
        asm_push("mov ecx, [esp+%i]", size);
        asm_push("mov edx, esp");
        codegen_x86_atomic_copy(false, size, order);
    }
    asm_push("fld tword [esp]");
    x87_release(size + (exchange ? size : 0) + DATA_SIZE_DWORD);
    return *t;
}

// the same on an _Atomic long double (12 bytes, not lock-free):
// libatomic's load and compare-exchange around the x87 arithmetic, as
// gcc -m32 does. The room: [esp] the new value, [esp+12] the old one,
// then the operand as a long double (none for ++ and --), then the
// address. A failed compare-exchange leaves what it found in the old
// value for the next try.
static struct datatype x87_atomic_ld_update(struct node* object, struct node* operand, const char* ins, bool postfix){
    struct datatype lt = x87_type(object);
    int size = (int)lt.size;
    int label = codegen_label_count();
    x87_scratch();
    x87_address(object);
    x87_reserve(DATA_SIZE_DWORD);
    asm_push("mov [esp], eax");
    int spill = 0;
    if(operand){
        x87_operand_as(operand, &lt);
        x87_spill_as(&lt);
        spill = x87_spill_size(&lt);
    }
    x87_reserve(2 * size);
    int address = 2 * size + spill;
    asm_push("mov ecx, [esp+%i]", address);
    asm_push("lea edx, [esp+%i]", size);
    codegen_x86_atomic_copy(false, size, ATOMIC_ORDER_SEQ_CST);
    asm_push(".x87_atomic_%i:", label);
    asm_push("fld tword [esp+%i]", size);
    if(operand){
        asm_push("fld tword [esp+%i]", 2 * size);
        asm_push("%sp st1, st0", ins);
    } else {
        asm_push("%s dword [x87_one]", ins);
    }
    asm_push("fstp tword [esp]");
    asm_push("mov ecx, [esp+%i]", address);
    asm_push("lea edx, [esp+%i]", size);
    asm_push("mov ebx, esp");
    codegen_x86_atomic_cas_memory(size, ATOMIC_ORDER_SEQ_CST, ATOMIC_ORDER_SEQ_CST);
    asm_push("test eax, eax");
    asm_push("jz .x87_atomic_%i", label);
    asm_push("fld tword [esp+%i]", postfix ? size : 0);
    x87_release(2 * size + spill + DATA_SIZE_DWORD);
    return lt;
}

static struct datatype x87_atomic_update(struct node* object, struct node* operand, const char* ins, bool postfix){
    struct datatype lt = x87_type(object);
    struct datatype c = lt;
    if(x87_is_long(&lt)){
        return x87_atomic_ld_update(object, operand, ins, postfix);
    }
    if(operand){
        c = x87_common(object, operand);
    }
    if(!x87_is_floating(&lt) || x87_is_long(&lt) || x87_is_long(&c)){
        compiler_error(current_process, "x86: ++, -- or a compound assignment with a floating operand on an _Atomic object is only supported for a float or double object\n");
    }
    int size = lt.size;
    const char* keyword = x87_size_keyword(size);
    int label = codegen_label_count();
    x87_scratch();
    x87_address(object);
    x87_reserve(DATA_SIZE_DWORD);
    asm_push("mov [esp], eax");
    int spill = 0;
    if(operand){
        x87_operand_as(operand, &c);
        x87_spill_as(&c);
        spill = x87_spill_size(&c);
    }
    x87_reserve(2 * DATA_SIZE_DDWORD);
    int address = 2 * DATA_SIZE_DDWORD + spill;
    asm_push("mov ecx, [esp+%i]", address);
    asm_push("mov eax, [ecx]");
    if(size == DATA_SIZE_DDWORD){
        asm_push("mov edx, [ecx+4]");
    }
    asm_push(".x87_atomic_%i:", label);
    asm_push("mov [esp+8], eax");
    if(size == DATA_SIZE_DDWORD){
        asm_push("mov [esp+12], edx");
    }
    asm_push("fld %s [esp+8]", keyword);
    if(operand){
        x87_convert(&lt, &c);
        asm_push("%s qword [esp+16]", ins);
        x87_round(c.size);
        x87_convert(&c, &lt);
    } else {
        asm_push("%s dword [x87_one]", ins);
        x87_round(size);
    }
    asm_push("fstp %s [esp]", keyword);
    asm_push("mov ecx, [esp+%i]", address);
    asm_push("mov eax, [esp+8]");
    if(size == DATA_SIZE_DWORD){
        asm_push("mov edx, [esp]");
        asm_push("lock cmpxchg dword [ecx], edx");
    } else {
        asm_push("mov edx, [esp+12]");
        asm_push("push esi");
        asm_push("mov esi, ecx");
        asm_push("mov ebx, [esp+4]");
        asm_push("mov ecx, [esp+8]");
        asm_push("lock cmpxchg8b [esi]");
        asm_push("pop esi");
    }
    asm_push("jne .x87_atomic_%i", label);
    // The value of the expression: the new value, or the old one for
    // a++ and a--.
    asm_push("fld %s [esp+%i]", keyword, postfix ? 8 : 0);
    x87_release(2 * DATA_SIZE_DDWORD + spill + DATA_SIZE_DWORD);
    return lt;
}

// `a = b` and `a op= b`, a or b floating. The address of a is worked
// out once.
static struct datatype x87_assign(struct node* node){
    const char* op = node->exp.op;
    struct datatype lt = x87_type(node->exp.left);
    if(S_EQ(op, "=")){
        x87_operand_as(node->exp.right, &lt);
        int bytes = x87_is_floating(&lt) ? x87_spill_size(&lt) : DATA_SIZE_DWORD;
        if(x87_is_floating(&lt)){
            x87_spill_as(&lt);
        } else {
            x87_reserve(bytes);
            asm_push("mov [esp], eax");
        }
        x87_address(node->exp.left);
        asm_push("mov ecx, eax");
        if(x87_is_floating(&lt)){
            asm_push("fld %s [esp]", x87_size_keyword(bytes));
        } else {
            asm_push("mov eax, [esp]");
        }
        x87_release(bytes);
        x87_store(&lt);
        x87_reduce(&lt);
        return lt;
    }
    char arith_op[4] = {0};
    strncpy(arith_op, op, strlen(op) - 1);
    const char* ins = x87_arith_ins(arith_op);
    if(datatype_is_atomic(&lt)){
        return x87_atomic_update(node->exp.left, node->exp.right, ins, false);
    }
    struct datatype c = x87_common(node->exp.left, node->exp.right);
    x87_address(node->exp.left);
    x87_reserve(DATA_SIZE_DWORD);
    asm_push("mov [esp], eax");
    x87_operand_as(node->exp.right, &c);
    x87_spill_as(&c);
    int spill = x87_spill_size(&c);
    asm_push("mov eax, [esp+%i]", spill);
    x87_load(&lt);
    x87_convert(&lt, &c);
    if(x87_is_long(&c)){
        asm_push("fld tword [esp]");
        asm_push("%sp st1, st0", ins);
    } else {
        asm_push("%s qword [esp]", ins);
    }
    x87_round(c.size);
    x87_release(spill);
    x87_convert(&c, &lt);
    asm_push("mov ecx, [esp]");
    x87_release(DATA_SIZE_DWORD);
    x87_store(&lt);
    if(!x87_is_floating(&lt)){
        x87_reduce(&lt);
    }
    return lt;
}

// `++x`, `x++`, `--x`, `x--` of a float or double x: the old value
// stays on the stack for the postfix forms (x + 1 - 1 need not be x).
static struct datatype x87_increment(struct node* node){
    struct datatype lt = x87_type(node->unary.operand);
    bool postfix = node->unary.flags & UNARY_FLAG_IS_POSTFIX;
    if(datatype_is_atomic(&lt)){
        return x87_atomic_update(node->unary.operand, 0, S_EQ(node->unary.op, "++") ? "fadd" : "fsub", postfix);
    }
    x87_scratch();
    x87_address(node->unary.operand);
    x87_reserve(DATA_SIZE_DWORD);
    asm_push("mov [esp], eax");
    x87_load(&lt);
    int spill = x87_spill_size(&lt);
    if(postfix){
        asm_push("fld st0");
        x87_spill_as(&lt);
    }
    asm_push("%s dword [x87_one]", S_EQ(node->unary.op, "++") ? "fadd" : "fsub");
    x87_round(lt.size);
    if(postfix){
        asm_push("mov ecx, [esp+%i]", spill);
    } else {
        asm_push("mov ecx, [esp]");
    }
    x87_store(&lt);
    if(postfix){
        asm_push("fstp st0");
        asm_push("fld %s [esp]", x87_size_keyword(spill));
        x87_release(spill);
    }
    x87_release(DATA_SIZE_DWORD);
    return lt;
}

static struct datatype x87_unary(struct node* node, struct datatype* t){
    const char* op = node->unary.op;
    if(is_increment_operator(op)){
        return x87_increment(node);
    }
    if(op_is_indirection(op)){
        x87_address(node);
        x87_load(t);
        return *t;
    }
    if(S_EQ(op, "!")){
        x87_truth(node->unary.operand);
        asm_push("cmp eax, 0");
        asm_push("sete al");
        asm_push("movzx eax, al");
        return datatype_for_int();
    }
    struct datatype v = x87_value(node->unary.operand);
    if(S_EQ(op, "-")){
        asm_push("fchs");
    } else if(!S_EQ(op, "+")){
        compiler_error(current_process, "The operator %s does not take a floating operand\n", op);
    }
    return v;
}

static struct datatype x87_logical(struct node* node){
    bool is_and = S_EQ(node->exp.op, "&&");
    int id = codegen_label_count();
    x87_truth(node->exp.left);
    asm_push("cmp eax, 0");
    asm_push("%s .x87_short_%i", is_and ? "je" : "jne", id);
    x87_truth(node->exp.right);
    asm_push("cmp eax, 0");
    asm_push("%s .x87_short_%i", is_and ? "je" : "jne", id);
    asm_push("mov eax, %i", is_and ? 1 : 0);
    asm_push("jmp .x87_end_%i", id);
    asm_push(".x87_short_%i:", id);
    asm_push("mov eax, %i", is_and ? 0 : 1);
    asm_push(".x87_end_%i:", id);
    return datatype_for_int();
}

static struct datatype x87_ternary(struct node* node, struct datatype* t){
    struct node* tenary = node->exp.right;
    int id = codegen_label_count();
    x87_truth(node->exp.left);
    asm_push("cmp eax, 0");
    asm_push("je .x87_no_%i", id);
    x87_operand_as(tenary->tenary.true_node, t);
    asm_push("jmp .x87_end_%i", id);
    asm_push(".x87_no_%i:", id);
    x87_operand_as(tenary->tenary.false_node, t);
    asm_push(".x87_end_%i:", id);
    return *t;
}

// A call with floating arguments, parameters or result, cdecl: the
// arguments pushed last first as their types (a double 8 bytes, a
// struct as its dwords), esp 16-byte aligned at the call, a floating
// result in st0. A struct result is written through the hidden pointer
// into room reserved under the arguments, which is left on the stack
// as the integer generator leaves a struct value (x87_push).
static struct datatype x87_call_emit(struct node* node, bool struct_result){
    struct node* callee = x87_strip(node->exp.left);
    struct resolver_entity* function = x87_called_function(callee);
    if(!struct_result && callee->type == NODE_TYPE_IDENTIFIER && atomic_builtin_get(callee->sval)
       && native_function_get(current_process, callee->sval)
       && !resolver_get_variable(0, current_process->resolver, callee->sval)){
        // an atomic builtin with a long double operand is the
        // integer generator's (codegen_x86_native_atomic), which works
        // the operand out on the x87 itself; its result in eax.
        struct native_function* native = native_function_get(current_process, callee->sval);
        native->callbacks.call(&x86_codegen, native, x87_call_arguments(node));
        struct datatype t = datatype_for_int();
        asm_datatype_back(&t);
        asm_push_ins_pop("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
        return t;
    }
    struct datatype rtype = x87_call_type(node);
    if(callee->type == NODE_TYPE_IDENTIFIER && !function
       && !resolver_get_variable(0, current_process->resolver, callee->sval)){
        compiler_error(current_process, "%s is not declared\n", callee->sval);
    }
    if(datatype_is_struct_or_union_non_pointer(&rtype) != struct_result){
        compiler_error(current_process, "x86: a struct call result with floating point arguments can only be assigned or passed on\n");
    }
    struct vector* args = x87_call_arguments(node);
    int count = vector_count(args);
    struct datatype* types = calloc(count + 1, sizeof(struct datatype));
    size_t bytes = 0;
    for(int i = 0; i < count; i++){
        struct node* arg = *(struct node**)vector_at(args, i);
        struct datatype t = x87_type(arg);
        struct datatype param;
        if(x87_param_type(callee, i, &param)){
            t = param;
        } else if(x87_is_floating(&t)){
            // The default argument promotions: a float is a double; a
            // long double stays one.
            if(!x87_is_long(&t)){
                t = datatype_floating(DATA_SIZE_DDWORD);
            }
        } else if(t.flags & DATATYPE_FLAG_IS_ARRAY){
            t = datatype_decay(&t);
        } else {
            t = datatype_promote(&t);
        }
        types[i] = t;
        if(datatype_is_struct_or_union_non_pointer(&t)){
            bytes += align_value(datatype_size(&t), DATA_SIZE_DWORD);
        } else {
            bytes += x87_is_floating(&t) ? t.size : DATA_SIZE_DWORD;
        }
    }
    int slot = 0;
    if(!function){
        slot = codegen_label_count();
        codegen_data_section_add("function_call_%i: dd 0", slot);
        x87_value(callee);
        asm_push("mov dword [function_call_%i], eax", slot);
    }
    int room = struct_result ? align_value(datatype_size(&rtype), DATA_SIZE_DWORD) : 0;
    codegen_stack_sub_with_name(room, "result_value");
    size_t hidden = struct_result ? DATA_SIZE_DWORD : 0;
    // ebp is 8 below a multiple of 16, so the bytes below it at the
    // call must be 8 mod 16 (as codegen_generate_entity_access_for_function_call).
    size_t below_ebp = (vector_count(current_function->func.frame.elements) - 1) * DATA_SIZE_DWORD + bytes + hidden;
    size_t pad = (C_STACK_ALIGNMENT + 8 - below_ebp % C_STACK_ALIGNMENT) % C_STACK_ALIGNMENT;
    codegen_stack_sub_with_name(pad, "call_alignment_pad");
    for(int i = count - 1; i >= 0; i--){
        struct node* arg = *(struct node**)vector_at(args, i);
        if(datatype_is_struct_or_union_non_pointer(&types[i])){
            codegen_generate_expressionable(arg, codegen_history_begin(EXPRESSION_IS_NOT_ROOT_NODE | EXPRESSION_IN_FUNCTION_CALL_ARGUMENTS));
            continue;
        }
        x87_operand_as(arg, &types[i]);
        if(x87_is_floating(&types[i])){
            x87_reserve(types[i].size);
            asm_push("fstp %s [esp]", x87_size_keyword(types[i].size));
        } else {
            x87_reserve(DATA_SIZE_DWORD);
            asm_push("mov [esp], eax");
        }
    }
    if(struct_result){
        // The hidden pointer: the room above the arguments and the pad.
        asm_push("lea eax, [esp+%i]", (int)(bytes + pad));
        x87_reserve(DATA_SIZE_DWORD);
        asm_push("mov [esp], eax");
    }
    if(function){
        // `$name` like every other call: the PIC pass sends a
        // `call $f` to a symbol from another object through the GOT; a
        // bare `call f` stayed a text relocation, which a PIE refuses.
        asm_push("call %s", codegen_symbol(function->name));
    } else {
        asm_push("call [function_call_%i]", slot);
    }
    if(struct_result){
        // The callee removed the hidden pointer with `ret 4`.
        stackframe_pop(current_function);
    }
    x87_release(bytes);
    codegen_stack_add_with_name(pad, "call_alignment_pad");
    free(types);
    if(struct_result){
        struct vector* elements = current_function->func.frame.elements;
        for(int i = 0; i < room / DATA_SIZE_DWORD; i++){
            struct stack_frame_element* el = vector_at(elements, vector_count(elements) - 1 - i);
            el->type       = STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE;
            el->flags     |= STACK_FRAME_ELEMENT_FLAG_HAS_DATATYPE;
            el->data.dtype = rtype;
        }
        codegen_response_acknowledge(&(struct response){.flags = RESPONSE_FLAG_PUSHED_STRUCTURE});
        return rtype;
    }
    if(x87_is_floating(&rtype)){
        x87_round(rtype.size);
    } else if(!(rtype.flags & DATATYPE_FLAG_IS_POINTER)){
        x87_reduce(&rtype);
    }
    return rtype;
}

static struct datatype x87_expression(struct node* node, struct datatype* t){
    const char* op = node->exp.op;
    if(is_node_assignment(node)){
        return x87_assign(node);
    }
    if(S_EQ(op, ",")){
        x87_effect(node->exp.left);
        return x87_value(node->exp.right);
    }
    if(is_logical_operator(op)){
        return x87_logical(node);
    }
    if(S_EQ(op, "?")){
        return x87_ternary(node, t);
    }
    if(is_parentheses_node(node)){
        struct datatype atomic;
        if(x87_atomic_ld_value(node, &atomic)){
            return x87_atomic_ld_call(node, &atomic);
        }
        return x87_call_emit(node, false);
    }
    if(is_array_node(node) || is_access_node(node)){
        // `(c ? s : t).d` as `c ? s.d : t.d`.
        struct node* conditional = codegen_access_conditional(node);
        if(conditional){
            return x87_value(codegen_access_of_conditional(node, conditional));
        }
        x87_address(node);
        x87_load(t);
        return *t;
    }
    if(x87_is_comparison(op)){
        return x87_compare(node);
    }
    return x87_arith(node);
}

// The value of `node`: st0 when it is floating, else eax.
static struct datatype x87_value(struct node* node){
    node = x87_strip(node);
    if(!x87_involves(node)){
        return x87_integer(node);
    }
    struct datatype t = x87_type(node);
    switch(node->type){
        case NODE_TYPE_NUMBER:
            x87_constant(node, &t);
            return t;
        case NODE_TYPE_IDENTIFIER:
            x87_address(node);
            x87_load(&t);
            return t;
        case NODE_TYPE_CAST: {
            struct datatype from = x87_value(node->cast.operand);
            x87_convert(&from, &node->cast.dtype);
            return node->cast.dtype;
        }
        case NODE_TYPE_UNARY:
            return x87_unary(node, &t);
        case NODE_TYPE_EXPRESSION:
            return x87_expression(node, &t);
    }
    compiler_error(current_process, "x86: cannot generate this floating point expression\n");
    return t;
}

static bool x87_is_struct_call(struct node* node){
    node = x87_strip(node);
    if(node->type != NODE_TYPE_EXPRESSION || !is_parentheses_node(node)){
        return false;
    }
    struct datatype t = x87_call_type(node);
    return datatype_is_struct_or_union_non_pointer(&t);
}

// codegen_generate_expressionable's way in: the value pushed as the
// integer generator pushes one, a struct a call returns as its dwords.
// A floating value reaches an integer operator only as a condition
// (`if (d)`), so it is pushed as its truth, or as an argument of a call
// the integer generator makes (one inside a member chain, `f(x).m`),
// pushed as its bytes, unconverted.
static void x87_push(struct node* node, int flags){
    if(x87_is_struct_call(node)){
        x87_call_emit(x87_strip(node), true);
        return;
    }
    struct datatype t = x87_value(node);
    if(x87_is_floating(&t) && (flags & EXPRESSION_IN_FUNCTION_CALL_ARGUMENTS)){
        codegen_stack_sub_with_name(t.size, "result_value");
        asm_push("fstp %s [esp]", x87_size_keyword(t.size));
        return;
    }
    if(x87_is_floating(&t)){
        x87_truth_of_st0();
        t = datatype_for_int();
    }
    asm_push_ins_push_with_data("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value", 0,
        &(struct stack_frame_data){.dtype = t});
}

// `return e;` from a function returning rtype, either floating: a
// floating result is left in st0.
static void x87_return(struct node* node, struct datatype* rtype){
    x87_operand_as(node, rtype);
}

// A local's initializer, either side floating: stored at `address`.
static void x87_initialize(struct node* value, struct datatype* type, const char* address){
    x87_operand_as(value, type);
    asm_push("lea ecx, [%s]", address);
    x87_store(type);
    if(x87_is_floating(type)){
        asm_push("fstp st0");
    }
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
    if(datatype_is_atomic(&entity->dtype) && start_pos == 0){
        codegen_x86_atomic_structure_push(&entity->dtype);
        codegen_response_acknowledge(&(struct response){.flags = RESPONSE_FLAG_PUSHED_STRUCTURE});
        return;
    }
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
    if(datatype_is_atomic(dtype) && offset == 0){
        codegen_x86_atomic_move_struct(dtype, base_address);
        return;
    }
    size_t structure_size = align_value(datatype_size(dtype), DATA_SIZE_DWORD);
    int pops = structure_size / DATA_SIZE_DWORD;
    // the last dword of a struct whose size is not a multiple
    // of 4 (`struct { char c[5]; }`) is stored as the struct's own bytes;
    // the whole dword overwrote the variable next to it.
    int tail = datatype_size(dtype) % DATA_SIZE_DWORD;
    for(int i = 0; i < pops; i++){
        asm_push_ins_pop("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
        char fmt[10];
        int chunk_offset = offset + (i * DATA_SIZE_DWORD);
        codegen_plus_or_minus_string_for_value(fmt, chunk_offset, sizeof(fmt));
        if(i == pops - 1 && tail){
            if(tail == 1){
                asm_push("mov [%s%s], al", base_address, fmt);
            } else {
                asm_push("mov [%s%s], ax", base_address, fmt);
            }
            if(tail == 3){
                codegen_plus_or_minus_string_for_value(fmt, chunk_offset + 2, sizeof(fmt));
                asm_push("shr eax, 16");
                asm_push("mov [%s%s], al", base_address, fmt);
            }
            continue;
        }
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

// ch156: walk the return value as an expression. Struct/union return
// goes via the hidden caller-allocated slot pointed to by [ebp+8];
// other types pop into eax.
static void codegen_generate_statement_return_exp(struct node* node){
    struct datatype* rtype = &node->binded.function->func.rtype;
    if(!datatype_is_struct_or_union_non_pointer(rtype)
       && (x87_wanted(node->stmt.return_stmt.exp) || (current_process->uses_floating && x87_is_floating(rtype)))){
        // A21: a float or double result goes back in st0.
        x87_return(node->stmt.return_stmt.exp, rtype);
        return;
    }
    codegen_response_expect();
    codegen_generate_expressionable(node->stmt.return_stmt.exp,
        codegen_history_begin(IS_STATEMENT_RETURN));
    struct datatype dtype;
    assert(asm_datatype_back(&dtype));
    if(datatype_is_struct_or_union_non_pointer(&dtype)){
        asm_push("mov edx, [ebp+8]");
        codegen_generate_move_struct(&dtype, "edx", 0);
        asm_push("mov eax, [ebp+8]");
        return;
    }
    asm_push_ins_pop("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
}

// ch157 fwd decl - the if-statement helpers need to refer to each
// other before they're defined.
static void _codegen_generate_if_stmt(struct node* node, int end_label_id);

// ch157: emit the body of an `else` block.
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

// ch157: condition -> pop eax -> cmp/je .if_N -> body -> jmp end ->
// .if_N: -> recurse into else/else-if chain.
static void _codegen_generate_if_stmt(struct node* node, int end_label_id){
    int if_label_id = codegen_label_count();
    codegen_generate_expressionable(node->stmt.if_stmt.cond_node, codegen_history_begin(0));
    asm_push_ins_pop("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
    asm_push("cmp eax, 0");
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

// ch158: while loop. Open an entry/exit point so break/continue can
// hook in later, then emit the canonical
//   .while_start_N:
//     <cond>; pop eax; cmp eax, 0; je .while_end_M
//     <body>
//     jmp .while_start_N
//   .while_end_M:
// pattern.
static void codegen_generate_while_stmt(struct node* node){
    codegen_begin_entry_exit_point();
    int while_start_id = codegen_label_count();
    int while_end_id   = codegen_label_count();
    asm_push(".while_start_%i:", while_start_id);
    codegen_generate_expressionable(node->stmt.while_stmt.exp_node, codegen_history_begin(0));
    asm_push_ins_pop("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
    asm_push("cmp eax, 0");
    asm_push("je .while_end_%i", while_end_id);
    codegen_generate_body(node->stmt.while_stmt.body_node, codegen_history_begin(IS_ALONE_STATEMENT));
    asm_push("jmp .while_start_%i", while_start_id);
    asm_push(".while_end_%i:", while_end_id);
    codegen_end_entry_exit_point();
}

// ch160/163: for loop.
//   init
//   jmp .for_loop<N>             ; skip incrementer on first pass
//   .entry_point_M:              ; continue lands here, then runs
//     incrementer                ;   the incrementer too
//   .for_loop<N>:                ; tail jump + first-pass land here
//     cond? cmp + je .for_loop_end<M>
//     body
//     incrementer (again)         ; book ships incrementer twice -
//                                 ;   once on continue, once on tail
//     jmp .for_loop<N>
//   .for_loop_end<M>:
// Any of the four parts may be missing.
// A for-loop's init and step are evaluated for their effect, like a
// statement: an assignment or ++/-- there pushes no value.
static void codegen_generate_statement(struct node* node, struct history* history);

static void codegen_generate_for_effect(struct node* node){
    if(node->type == NODE_TYPE_VARIABLE || node->type == NODE_TYPE_VARIABLE_LIST){
        codegen_generate_statement(node, codegen_history_begin(0));
        return;
    }
    if(x87_wanted(node)){
        x87_effect(node);
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
    asm_push_ins_pop_or_ignore("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
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
    // ch163: skip past the entry-point incrementer on the first pass.
    asm_push("jmp .for_loop%i", for_loop_start_id);
    codegen_begin_entry_exit_point();
    if(fs->loop_node){
        codegen_generate_for_effect(fs->loop_node);
    }
    asm_push(".for_loop%i:", for_loop_start_id);
    if(fs->cond_node){
        codegen_generate_expressionable(fs->cond_node, codegen_history_begin(0));
        asm_push_ins_pop_or_ignore("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
        asm_push("cmp eax, 0");
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

// ch166: switch `default:` label.
static void codegen_generate_switch_default_stmt(struct node* node){
    (void)node;
    asm_push("; DEFAULT CASE");
    struct code_generator* gen = current_process->generator;
    asm_push(".switch_stmt_%i_case_default:", gen->_switch.current.id);
}

// ch166: jump table built before the body. For each parsed case
// emit cmp+je; default falls through to the body, otherwise jmp to
// the exit point.
static void codegen_generate_switch_stmt_case_jumps(struct node* node){
    vector_set_peek_pointer(node->stmt.switch_stmt.cases, 0);
    struct parsed_switch_case* sc = vector_peek(node->stmt.switch_stmt.cases);
    while(sc){
        asm_push("cmp eax, %i", (int)sc->index);
        asm_push("je .switch_stmt_%i_case_%s", codegen_switch_id(), codegen_case_label(sc->index));
        sc = vector_peek(node->stmt.switch_stmt.cases);
    }
    if(node->stmt.switch_stmt.has_default_case){
        // ch186 fixed the ch166 typo - `codegen_switch_id` without
        // `()` was passing the function pointer to printf and
        // producing a bogus label id.
        asm_push("jmp .switch_stmt_%i_case_default", codegen_switch_id());
        return;
    }
    codegen_goto_exit_point_maintain_stack(node);
}

static void codegen_generate_switch_stmt(struct node* node){
    codegen_begin_entry_exit_point();
    codegen_begin_switch_statement();
    codegen_generate_expressionable(node->stmt.switch_stmt.exp, codegen_history_begin(0));
    asm_push_ins_pop_or_ignore("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
    codegen_generate_switch_stmt_case_jumps(node);
    codegen_generate_body(node->stmt.switch_stmt.body, codegen_history_begin(IS_ALONE_STATEMENT));
    codegen_end_switch_statement();
    codegen_end_entry_exit_point();
}

// ch168: `goto LABEL;` - emits `jmp ..@label_<name>`.
static void codegen_generate_goto_stmt(struct node* node){
    asm_push("jmp ..@label_%s", node->stmt._goto.label->sval);
}

// ch169: `LABEL:` statement - emits the asm label.
static void codegen_generate_label(struct node* node){
    // a `..@` label is not a NASM non-local label, so the
    // `.name` labels after it (an if's, a loop's) keep the function's
    // scope; `label_f_x:` started a new one and left them undefined.
    asm_push("..@label_%s:", node->stmt.label.name->sval);
}

// ch167: `case N:` - emits the `.switch_stmt_<id>_case_N:` label
// and a comment marker. Body statements follow normally.
static void codegen_generate_switch_case_stmt(struct node* node){
    struct node* case_exp = node->stmt._case.exp;
    assert(case_exp->type == NODE_TYPE_NUMBER);
    codegen_begin_case_statement((long long)case_exp->llnum);
    asm_push("; CASE %i", (int)case_exp->llnum);
    codegen_end_case_statement();
}

// ch161: `break;` - jumps to the innermost exit point (the loop's
// .while_end / .for_loop_end / etc.).
static void codegen_generate_break_stmt(struct node* node){
    codegen_goto_exit_point(node);
}

// ch162: `continue;` - jumps to the innermost entry point. NOTE:
// inside a for loop this currently skips the incrementer; the book
// ships the same bug with a `#warning`. We'll fix it in a later
// gotcha when we have a real failing test.
static void codegen_generate_continue_stmt(struct node* node){
    codegen_goto_entry_point(node);
}

// ch159: do/while - body first, then condition; jump back to start
// when condition is non-zero.
static void codegen_generate_do_while_stmt(struct node* node){
    // `continue` goes to the condition, so the entry point is
    // placed after the body, not at the start (same label numbers).
    int entry_point_id = codegen_label_count();
    codegen_register_entry_point(entry_point_id);
    codegen_begin_exit_point();
    int do_while_start_id = codegen_label_count();
    asm_push(".do_while_start_%i:", do_while_start_id);
    codegen_generate_body(node->stmt.do_while_stmt.body_node, codegen_history_begin(IS_ALONE_STATEMENT));
    asm_push(".entry_point_%i:", entry_point_id);
    codegen_generate_expressionable(node->stmt.do_while_stmt.exp_node, codegen_history_begin(0));
    asm_push_ins_pop("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
    asm_push("cmp eax, 0");
    asm_push("jne .do_while_start_%i", do_while_start_id);
    codegen_end_entry_exit_point();
}

// A function that returns a struct or union removes the hidden result
// pointer itself, `ret 4`, as gcc does on i386.
static void codegen_generate_ret(struct node* function){
    debug_cfi_def_cfa(current_process, 4, 4);
    debug_cfi_restore(current_process, 5);
    if(datatype_is_struct_or_union_non_pointer(&function->func.rtype)){
        asm_push("ret 4");
    } else {
        asm_push("ret");
    }
}

// ch156: real `return [expr];` emit. Drops the frame WITHOUT touching
// the compile-time ledger (the ledger only finalises at the function
// epilogue assertion), then `pop ebp; ret`.
static void codegen_generate_statement_return(struct node* node){
    if(node->stmt.return_stmt.exp){
        codegen_generate_statement_return_exp(node);
    }
    // ebx was saved just below the locals. esp may be lower
    // than the locals here (a return inside a call's arguments or a
    // switch), so the frame is dropped from ebp.
    size_t locals_size = C_ALIGN(function_node_stack_size(node->binded.function));
    asm_push("mov ebx, [ebp-%i]", (int)locals_size + DATA_SIZE_DWORD);
    asm_push("mov esi, [ebp-%i]", (int)locals_size + 2 * DATA_SIZE_DWORD);
    asm_push("mov edi, [ebp-%i]", (int)locals_size + 3 * DATA_SIZE_DWORD);
    debug_cfi_remember_state(current_process);
    asm_push("mov esp, ebp");
    asm_pop_ebp_no_stack_frame_restore();
    codegen_generate_ret(node->binded.function);
    debug_cfi_restore_state(current_process);
}

static void codegen_generate_statement(struct node* node, struct history* history){
    compiler_check_stack(current_process);
    (void)history;
    // A backend error names the statement's line, not the end of the file.
    if(node->pos.line){
        current_process->pos = node->pos;
    }
    debug_statement(current_process, node);
    bool is_expression = node->type == NODE_TYPE_EXPRESSION || node->type == NODE_TYPE_UNARY
        || node->type == NODE_TYPE_CAST || node->type == NODE_TYPE_EXPRESSION_PARENTHESES;
    if(is_expression && x87_wanted(node)){
        x87_effect(node);
        codegen_discard_unused_stack();
        return;
    }
    switch(node->type){
        case NODE_TYPE_EXPRESSION:
            codegen_generate_exp_node(node, codegen_history_begin(history->flags));
            break;
        // ch156: a unary in statement position (e.g. `*p = ...` mixed
        // in with normal exp statements) needs its own dispatch entry.
        case NODE_TYPE_UNARY:
            codegen_generate_unary(node, codegen_history_begin(history->flags));
            break;
        // `(b = a);`, `((b) = (a));`: the parentheses add nothing to a
        // statement. This case was dropped once with the ++/-- lowering,
        // and the statement then generated no code at all.
        case NODE_TYPE_EXPRESSION_PARENTHESES:
            codegen_generate_statement(node->parenthesis.exp, history);
            return;
        // `(void)f(x);`: a cast only shapes the value, which a statement
        // discards; the operand still runs. It generated nothing.
        case NODE_TYPE_CAST:
            codegen_generate_statement(node->cast.operand, history);
            return;
        case NODE_TYPE_VARIABLE:
            codegen_generate_scope_variable(node);
            break;
        // ch181: comma-separated local var list - emit each var.
        case NODE_TYPE_VARIABLE_LIST: {
            assert(node->type == NODE_TYPE_VARIABLE_LIST);
            vector_set_peek_pointer(node->var_list.list, 0);
            struct node* v = vector_peek_ptr(node->var_list.list);
            while(v){
                codegen_generate_scope_variable(v);
                v = vector_peek_ptr(node->var_list.list);
            }
            break;
        }
        // ch156: return statement.
        case NODE_TYPE_STATEMENT_RETURN:
            codegen_generate_statement_return(node);
            break;
        // ch157: if statement.
        case NODE_TYPE_STATEMENT_IF:
            codegen_generate_if_stmt(node);
            break;
        // ch158: while loop.
        case NODE_TYPE_STATEMENT_WHILE:
            codegen_generate_while_stmt(node);
            break;
        // ch159: do/while loop.
        case NODE_TYPE_STATEMENT_DO_WHILE:
            codegen_generate_do_while_stmt(node);
            break;
        // ch160: for loop.
        case NODE_TYPE_STATEMENT_FOR:
            codegen_generate_for_stmt(node);
            break;
        // ch161: break statement.
        case NODE_TYPE_STATEMENT_BREAK:
            codegen_generate_break_stmt(node);
            break;
        // ch162: continue statement.
        case NODE_TYPE_STATEMENT_CONTINUE:
            codegen_generate_continue_stmt(node);
            break;
        // ch166: switch statement.
        case NODE_TYPE_STATEMENT_SWITCH:
            codegen_generate_switch_stmt(node);
            break;
        // ch167: case and default labels.
        case NODE_TYPE_STATEMENT_CASE:
            codegen_generate_switch_case_stmt(node);
            break;
        case NODE_TYPE_STATEMENT_DEFAULT:
            codegen_generate_switch_default_stmt(node);
            break;
        // ch168: goto statement.
        case NODE_TYPE_STATEMENT_GOTO:
            codegen_generate_goto_stmt(node);
            break;
        // ch169: label.
        case NODE_TYPE_LABEL:
            codegen_generate_label(node);
            break;
        case NODE_TYPE_BODY:
            // A compound statement `{ ... }` inside a function.
            codegen_generate_body(node, codegen_history_begin(IS_ALONE_STATEMENT));
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
    debug_block_begin(current_process);
    codegen_generate_stack_scope(node->body.statements, node->body.size, history);
    debug_block_end(current_process);
}

static void codegen_generate_function_with_body(struct node* node){
    codegen_register_function(node, 0);
    // A static function stays local to the object.
    // Under -fPIC the line came first (codegen_pic_function_globals).
    if(!(node->func.rtype.flags & DATATYPE_FLAG_IS_STATIC) && !(current_process->flags & COMPILE_PROCESS_PIC)){
        asm_push("global %s", codegen_symbol(node->func.name));
    }
    asm_push("; %s function", node->func.name);
    asm_push("%s:", codegen_symbol(node->func.name));
    debug_function_begin(current_process, node);

    // the CFA is esp at the call, ebp + 8 from here on.
    asm_push_ebp();
    debug_cfi_def_cfa_offset(current_process, 8);
    debug_cfi_offset(current_process, 5, -8);
    asm_push("mov ebp, esp");
    debug_cfi_def_cfa_register(current_process, 5);
    codegen_stack_sub(C_ALIGN(function_node_stack_size(node)));
    // cdecl makes ebx callee-saved, and every expression here uses it.
    // It goes in the slot below the locals.
    asm_push_ins_push("ebx", STACK_FRAME_ELEMENT_TYPE_UNKNOWN, "function_entry_saved_ebx");
    // esi = the GOT's address, edi = a symbol's address
    // read from the GOT; both callee-saved, kept under ebx. call/pop
    // gives the address of .pic_base, and R_386_GOTPC adds the distance
    // from there to the GOT.
    asm_push_ins_push("esi", STACK_FRAME_ELEMENT_TYPE_UNKNOWN, "function_entry_saved_esi");
    asm_push_ins_push("edi", STACK_FRAME_ELEMENT_TYPE_UNKNOWN, "function_entry_saved_edi");
    asm_push("call .pic_base");
    asm_push(".pic_base:");
    asm_push("pop esi");
    asm_push("add esi, _GLOBAL_OFFSET_TABLE_+$$-.pic_base wrt ..gotpc");
    codegen_new_scope(RESOLVER_DEFAULT_ENTITY_FLAG_IS_LOCAL_STACK);
    codegen_generate_function_arguments(function_node_argument_vec(node));

    codegen_generate_body(node->func.body_n, codegen_history_begin(IS_ALONE_STATEMENT));
    codegen_finish_scope();
    asm_push_ins_pop("edi", STACK_FRAME_ELEMENT_TYPE_UNKNOWN, "function_entry_saved_edi");
    asm_push_ins_pop("esi", STACK_FRAME_ELEMENT_TYPE_UNKNOWN, "function_entry_saved_esi");
    asm_push_ins_pop("ebx", STACK_FRAME_ELEMENT_TYPE_UNKNOWN, "function_entry_saved_ebx");
    codegen_stack_add(C_ALIGN(function_node_stack_size(node)));
    asm_pop_ebp();
    stackframe_assert_empty(current_function);
    codegen_generate_ret(node);
    debug_function_end(current_process);
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

// ch166: switch-case jump-table variant. Currently identical to the
// plain goto_exit_point; the book reserves it for future stack-
// frame maintenance (hence the name).
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

// ch164: switch-statement bookkeeping. Stack the outer switch onto
// `swtiches` and emit a fresh `.switch_stmt_<id>:` label.
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

static void codegen_begin_case_statement(long long index){
    struct code_generator* gen = current_process->generator;
    asm_push(".switch_stmt_%i_case_%s:", gen->_switch.current.id, codegen_case_label(index));
}

static void codegen_end_case_statement(void){
    // No-op (placeholder).
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
    // an _Atomic global is aligned to its size, as in gcc, so
    // a plain load or store of it (a double's fld and fst too) never
    // straddles a cache line and stays one access. Other globals keep
    // the packed layout the locked golden output has. The
    // alignment gcc gives it (that of its size from 2 to 16 bytes, a
    // 12-byte long double's 4).
    if(datatype_is_atomic(&node->var.type) && datatype_alignment(&node->var.type) > DATA_SIZE_BYTE){
        asm_push("align %lu, db 0", (unsigned long)datatype_alignment(&node->var.type));
    }
    if(node->var.val){
        if(node->var.val->type == NODE_TYPE_STRING){
            // ch112: register the literal in the string table and
            // emit the label as the variable's value (so the global
            // holds the address of the string in .rodata).
            const char* label = codegen_register_string(node->var.val->sval);
            asm_push("%s: %s %s",
                codegen_symbol(node->var.name),
                asm_keyword_for_size(variable_size(node), tmp_buf),
                label);
        } else {
            // ch111: emit the integer initializer literal (A21: the
            // bits of a floating one).
            asm_push("%s: %s %lld",
                codegen_symbol(node->var.name),
                asm_keyword_for_size(variable_size(node), tmp_buf),
                initializer_number_bits(node->var.val));
        }
    } else {
        asm_push("%s: %s 0",
            codegen_symbol(node->var.name),
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
    asm_push("%s: %s 0", codegen_symbol(node->var.name),
        asm_keyword_for_size(variable_size(node), tmp_buf));
}

// ch178: same shape as the struct emitter; unions get a single
// uninitialized blob sized to the union's largest member.
static void codegen_generate_global_variable_for_union(struct node* node){
    if(node->var.val){
        compiler_error(current_process, "We dont yet support values for unions");
        return;
    }
    char tmp_buf[256];
    asm_push("%s: %s 0", codegen_symbol(node->var.name),
        asm_keyword_for_size(variable_size(node), tmp_buf));
}

// ch179: global array variable - `name: <kw> 0` sized by
// `variable_size` (which factors in array.size). Initializers
// still unsupported.
static void codegen_generate_variable_for_array(struct node* node){
    if(node->var.val){
        compiler_error(current_process, "We don't support values for arrays yet");
        return;
    }
    char tmp_buf[256];
    asm_push("%s: %s 0", codegen_symbol(node->var.name),
        asm_keyword_for_size(variable_size(node), tmp_buf));
}

// A global with a brace initializer (or an address): its bytes, leaf by
// leaf, from codegen_emit_initializer.
static const char* codegen_init_nasm_size(size_t size){
    switch(size){
        case DATA_SIZE_BYTE:  return "db";
        case DATA_SIZE_WORD:  return "dw";
        case DATA_SIZE_DWORD: return "dd";
        default:              return "dq";
    }
}

static void codegen_init_value(size_t size, long long value){
    asm_push("%s %lld", codegen_init_nasm_size(size), value);
}

// Under -fPIC every exported function is declared global, and typed,
// before the data: `wrt ..sym` in an initializer (below) needs the
// symbol to be global already.
static void codegen_pic_function_globals(void){
    if(!(current_process->flags & COMPILE_PROCESS_PIC)){
        return;
    }
    struct vector* tree = current_process->node_tree_vec;
    for(int i = 0; i < vector_count(tree); i++){
        struct node* node = *(struct node**)vector_at(tree, i);
        if(node->type == NODE_TYPE_FUNCTION && node->func.body_n && !(node->func.rtype.flags & DATATYPE_FLAG_IS_STATIC)){
            asm_push("global %s:function", codegen_symbol(node->func.name));
        }
    }
}

// NASM writes a reference to a symbol of this file as one to its
// section; under -fPIC an exported symbol's must name the symbol
// (`wrt ..sym`), so the word holds the definition that wins.
static void codegen_init_symbol(size_t size, const char* symbol){
    bool by_symbol = (current_process->flags & COMPILE_PROCESS_PIC) && symbol_defined_in_tree(current_process, symbol)
        && !symbol_binds_locally(current_process, symbol);
    asm_push("%s %s%s", codegen_init_nasm_size(size), codegen_symbol(symbol), by_symbol ? " wrt ..sym" : "");
}

static void codegen_init_zero(size_t count){
    asm_push("times %lu db 0", (unsigned long)count);
}

// A global's symbol is exported unless it is static. In a
// shared object it is also typed and sized: a program that uses it
// gets a copy relocation, which copies that many bytes.
static void codegen_global_label(struct node* node){
    if(node->var.type.flags & DATATYPE_FLAG_IS_STATIC){
        return;
    }
    if(current_process->flags & COMPILE_PROCESS_PIC){
        asm_push("global %s:data %lu", codegen_symbol(node->var.name), (unsigned long)variable_size(node));
        return;
    }
    asm_push("global %s", codegen_symbol(node->var.name));
}

static void codegen_generate_global_initializer(struct node* node){
    codegen_global_label(node);
    asm_push("align %lu, db 0", (unsigned long)variable_alignment(node));
    asm_push("%s:", codegen_symbol(node->var.name));
    codegen_emit_initializer(node, &(struct initializer_emitter){
        .value        = codegen_init_value,
        .symbol       = codegen_init_symbol,
        .zero         = codegen_init_zero,
        .string_label = codegen_register_string,
    });
}

// a thread-local variable's initial image goes to .tdata, or
// to .tbss without an initializer; each thread gets a copy of it. The
// symbols of a tls section are STT_TLS. Accesses go through a hidden
// global alias (tls_alias), which NASM takes in a TLS reference also
// when the variable is static. The data section continues after it.
static void codegen_generate_thread_local(struct node* node){
    unsigned long align = variable_alignment(node);
    const char* alias = codegen_symbol(tls_alias(current_process, node->var.name));
    asm_push(node->var.val ? "section .tdata progbits alloc write tls" : "section .tbss nobits alloc write tls");
    asm_push(node->var.val ? "align %lu, db 0" : "alignb %lu", align);
    asm_push("global %s:hidden", alias);
    asm_push("%s:", alias);
    if(!node->var.val){
        codegen_global_label(node);
        asm_push("%s: resb %lu", codegen_symbol(node->var.name), (unsigned long)variable_size(node));
    } else if(node->var.val->type == NODE_TYPE_INITIALIZER){
        codegen_generate_global_initializer(node);
    } else {
        codegen_global_label(node);
        if(datatype_is_struct_or_union_non_pointer(&node->var.type) || (node->var.type.flags & DATATYPE_FLAG_IS_ARRAY)){
            compiler_error(current_process, "A struct, union or array global needs a { } initializer\n");
        }
        codegen_generate_global_variable_for_primitive(node);
    }
    asm_push("section .data");
}

static void codegen_generate_global_variable(struct node* node){
    debug_global(current_process, node);
    asm_push("; %s %s", node->var.type.type_str, node->var.name);
    if((node->var.type.flags & DATATYPE_FLAG_IS_THREAD_LOCAL) && !(node->var.type.flags & DATATYPE_FLAG_IS_EXTERN)){
        codegen_generate_thread_local(node);
        codegen_new_scope_entity(node, 0, 0);
        return;
    }
    if(node->var.val && node->var.val->type == NODE_TYPE_INITIALIZER){
        codegen_generate_global_initializer(node);
        codegen_new_scope_entity(node, 0, 0);
        return;
    }
    // `extern int g;` names another object's variable; it
    // gets no storage here, and no extern when this file defines it too.
    if(node->var.type.flags & DATATYPE_FLAG_IS_EXTERN){
        if(!variable_defined_in_tree(current_process, node->var.name)){
            asm_push("extern %s", codegen_symbol(node->var.name));
            // the driver makes it STT_TLS in the object.
            if(node->var.type.flags & DATATYPE_FLAG_IS_THREAD_LOCAL){
                asm_push("; tls extern %s", node->var.name);
            }
        }
        codegen_new_scope_entity(node, 0, 0);
        return;
    }
    // _Alignas, of the variable or a member of its type.
    if(variable_alignas(node)){
        asm_push("align %lu, db 0", (unsigned long)variable_alignas(node));
    }
    codegen_global_label(node);
    // ch179: arrays go through their own emit path and also register
    // a scope entity so the resolver can find them.
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
        case DATA_TYPE_FLOAT:
        case DATA_TYPE_DOUBLE:
        case DATA_TYPE_LONG_DOUBLE:
            codegen_generate_global_variable_for_primitive(node);
            break;
        case DATA_TYPE_STRUCT:
            codegen_generate_global_variable_for_struct(node);
            break;
        // ch178: union global.
        case DATA_TYPE_UNION:
            codegen_generate_global_variable_for_union(node);
            break;
    }
    // ch180: every global VAR also registers a scope entity so the
    // resolver can find it during expression codegen.
    assert(node->type == NODE_TYPE_VARIABLE);
    codegen_new_scope_entity(node, 0, 0);
}

// ch149: top-level struct definition with an attached variable
// (`struct foo { ... } v;`) emits the variable; the body itself was
// already captured by the parser.
static void codegen_generate_struct(struct node* node){
    if(node->flags & NODE_FLAG_HAS_VARIABLE_COMBINED){
        codegen_generate_global_variable(node->_struct.var);
    }
}

// ch178: union counterpart.
static void codegen_generate_union(struct node* node){
    if(node->flags & NODE_FLAG_HAS_VARIABLE_COMBINED){
        codegen_generate_global_variable(node->_union.var);
    }
}

// ch181: comma-separated global var list - emit each var.
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
        // ch149: top-level struct (`struct foo {...} v;`) emits its
        // attached variable into .data.
        case NODE_TYPE_STRUCT:
            codegen_generate_struct(node);
            break;
        // ch178: same for top-level unions.
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
    codegen_in_text = true;
    struct node* node = 0;
    while((node = codegen_node_next()) != 0){
        codegen_generate_root_node(node);
    }
    codegen_in_text = false;
}

// ch110: write escape-sequence chars as their decimal ASCII value
// (`'\n'` -> `10`). Returns true if c was handled.
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

// ch187: re-open `.data` and flush every `codegen_data_section_add`
// line. Runs after `.text` so the function-call slots' labels are
// known by the time the assembler resolves them.
static void codegen_generate_data_section_add_ons(void){
    asm_push("section .data");
    vector_set_peek_pointer(current_process->generator->custom_data_section, 0);
    const char* str = vector_peek_ptr(current_process->generator->custom_data_section);
    while(str){
        asm_push(str);
        str = vector_peek_ptr(current_process->generator->custom_data_section);
    }
}

// ch236: native generator helpers - drive expression codegen
// through the v-table so native functions can recurse back in.
struct _x86_generator_private* x86_generator_private(struct generator* generator){
    return generator->private;
}

void codegen_asm_push(const char* ins, ...){
    va_list args;
    va_start(args, ins);
    asm_push_args(ins, args);
    va_end(args);
}

// ch237: native-function return helper exposed via the v-table.
// Emits `push <fmt>` and tags the stackframe element with the dtype
// so subsequent stackframe walks know what's there.
void asm_push_ins_with_datatype(struct datatype* dtype, const char* fmt, ...){
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

// the target-specific halves of the stdarg natives (see
// preprocessor/static-includes/stdarg.c).
// cdecl arguments are 4-byte slots in order, a struct its size rounded
// up, so the list is the address of the next one: va_start starts it
// past the last named parameter, and va_arg gives the address it holds
// (the va_arg macro reads the type from there) and steps it past the
// argument. va_arg used to step first and read after, which skipped
// into the middle of an argument wider than the last named one, and
// it left the va_list's address on the stack under its result, where
// the `+` of `total + va_arg(...)` took it as its left operand.
static void codegen_x86_va_start_store(struct generator* generator, const char* list_address, int last_size){
    generator->asm_push("add ebx, %i", (int)align_value(last_size, DATA_SIZE_DWORD));
    generator->asm_push("mov dword [%s], ebx", list_address);
}

static void codegen_x86_va_arg_fetch(struct generator* generator, int size){
    asm_push_ins_pop("ebx", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
    generator->asm_push("mov dword eax, [ebx]");
    generator->asm_push("add dword [ebx], %i", (int)align_value(size, DATA_SIZE_DWORD));
    struct datatype void_dtype;
    datatype_set_void(&void_dtype);
    void_dtype.pointer_depth++;
    void_dtype.flags |= DATATYPE_FLAG_IS_POINTER;
    generator->ret(&void_dtype, "eax");
}

// the stack-pointer builtins behind variable-length arrays.
// The parser puts each in a statement of its own, `save = ...`, `a =
// ...` or a call, so nothing of the expression stack is under esp when
// it moves. The stack stays 16-byte aligned.
void codegen_x86_native_vla_save(struct generator* generator, struct native_function* func, struct vector* arguments){
    (void)func; (void)arguments;
    struct datatype dtype = datatype_for_int();
    dtype.flags &= ~DATATYPE_FLAG_IS_SIGNED;
    generator->asm_push("mov eax, esp");
    generator->ret(&dtype, "eax");
}

void codegen_x86_native_vla_alloc(struct generator* generator, struct native_function* func, struct vector* arguments){
    (void)func;
    vector_set_peek_pointer(arguments, 0);
    generator->gen_exp(generator, vector_peek_ptr(arguments), 0);
    asm_push_ins_pop("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
    generator->asm_push("add eax, 15");
    generator->asm_push("and eax, -16");
    generator->asm_push("sub esp, eax");
    generator->asm_push("mov eax, esp");
    struct datatype dtype;
    datatype_set_void(&dtype);
    dtype.pointer_depth++;
    dtype.flags |= DATATYPE_FLAG_IS_POINTER;
    generator->ret(&dtype, "eax");
}

void codegen_x86_native_vla_restore(struct generator* generator, struct native_function* func, struct vector* arguments){
    (void)func;
    vector_set_peek_pointer(arguments, 0);
    generator->gen_exp(generator, vector_peek_ptr(arguments), 0);
    asm_push_ins_pop("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
    generator->asm_push("mov esp, eax");
    struct datatype dtype;
    datatype_set_void(&dtype);
    generator->ret(&dtype, "0");
}

// the i386 halves of gcc's atomic builtins (exp64_atomic_builtin
// is the 64-bit one). The arguments are pushed by gen_exp and popped
// into ebx (the object's address), ecx and edx. A load is a mov, a store
// or exchange an xchg (locked by itself), + and - a lock xadd, anything
// else a lock cmpxchg loop. i386 has no 8-byte integer here (long long is
// 4 bytes), so 1, 2 and 4 byte objects, and 8-byte ones (a double) for
// the generic forms (codegen_x86_atomic_generic8).
static int codegen_x86_atomic_count;

static struct node* codegen_x86_atomic_argument(struct vector* arguments, int i){
    return *(struct node**)vector_at(arguments, i);
}

// Generate argument i and pop it into reg.
static void codegen_x86_atomic_pop_argument(struct generator* generator, struct vector* arguments, int i, const char* reg){
    generator->gen_exp(generator, codegen_x86_atomic_argument(arguments, i), 0);
    asm_push_ins_pop(reg, STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
}

// The memory order arguments from `first` on: one that is not a
// constant is evaluated, for its side effects. The order of
// the last one, for the access (seq_cst when it is not a constant). x86
// is TSO: only a store and a fence weaker than seq_cst differ (a mov,
// and nothing); loads are movs and read-modify-writes locked whatever
// the order.
static int codegen_x86_atomic_orders(struct generator* generator, struct vector* arguments, int first, int access){
    int order = ATOMIC_ORDER_SEQ_CST;
    for(int i = first; i < vector_count(arguments); i++){
        bool constant;
        order = atomic_order(current_process, codegen_x86_atomic_argument(arguments, i), access, &constant);
        if(!constant){
            codegen_x86_atomic_pop_argument(generator, arguments, i, "ecx");
        }
    }
    return order;
}

// The type of the object a pointer of type pt points to, not atomic.
static struct datatype codegen_x86_atomic_object(struct datatype* pt, const char* name){
    if(!(pt->flags & (DATATYPE_FLAG_IS_POINTER | DATATYPE_FLAG_IS_ARRAY))){
        compiler_error(current_process, "The first argument of %s must be a pointer\n", name);
    }
    struct datatype t = *pt;
    if(t.flags & DATATYPE_FLAG_IS_ARRAY){
        t.flags &= ~DATATYPE_FLAG_IS_ARRAY;
    } else if(--t.pointer_depth <= 0){
        t.pointer_depth = 0;
        t.flags &= ~DATATYPE_FLAG_IS_POINTER;
    }
    t.atomic_level = 0;
    if(t.type == DATA_TYPE_VOID && !t.pointer_depth){
        compiler_error(current_process, "%s needs an integer or pointer object\n", name);
    }
    return t;
}

// exchange the _Atomic object of `size` bytes at ecx with the
// value at edx, its old value to ebx's buffer; compare-exchange it with
// the expected value at edx (which a failure overwrites with what was
// there) and the desired one at ebx, eax = 0 or 1. Lock-free sizes are
// xchg, lock cmpxchg and lock cmpxchg8b (with esi and edi saved around
// it); any other size libatomic's __atomic_exchange and
// __atomic_compare_exchange. Every general register but esi, edi and
// ebp is clobbered.
static void codegen_x86_atomic_exchange_memory(int size, int order){
    if(size == DATA_SIZE_BYTE || size == DATA_SIZE_WORD || size == DATA_SIZE_DWORD){
        const char* keyword = size == DATA_SIZE_BYTE ? "byte" : size == DATA_SIZE_WORD ? "word" : "dword";
        const char* eax = codegen_sub_register("eax", size);
        asm_push("mov %s, %s [edx]", eax, keyword);
        asm_push("xchg %s [ecx], %s", keyword, eax);
        asm_push("mov %s [ebx], %s", keyword, eax);
        return;
    }
    if(size == DATA_SIZE_DDWORD){
        int label = ++codegen_x86_atomic_count;
        asm_push("push esi");
        asm_push("push edi");
        asm_push("mov esi, ecx");
        asm_push("mov edi, ebx");
        asm_push("mov ebx, [edx]");
        asm_push("mov ecx, [edx+4]");
        asm_push("mov eax, [esi]");
        asm_push("mov edx, [esi+4]");
        asm_push(".atomic_%i:", label);
        asm_push("lock cmpxchg8b [esi]");
        asm_push("jne .atomic_%i", label);
        asm_push("mov [edi], eax");
        asm_push("mov [edi+4], edx");
        asm_push("pop edi");
        asm_push("pop esi");
        return;
    }
    asm_push("extern $__atomic_exchange");
    asm_push("push %i", order);
    asm_push("push ebx");
    asm_push("push edx");
    asm_push("push ecx");
    asm_push("push %i", size);
    asm_push("call $__atomic_exchange");
    asm_push("add esp, 20");
}

static void codegen_x86_atomic_cas_memory(int size, int success, int failure){
    int label = ++codegen_x86_atomic_count;
    if(size == DATA_SIZE_BYTE || size == DATA_SIZE_WORD || size == DATA_SIZE_DWORD){
        const char* keyword = size == DATA_SIZE_BYTE ? "byte" : size == DATA_SIZE_WORD ? "word" : "dword";
        const char* eax = codegen_sub_register("eax", size);
        if(size == DATA_SIZE_DWORD){
            asm_push("mov ebx, [ebx]");
        } else {
            asm_push("movzx ebx, %s [ebx]", keyword);
        }
        asm_push("mov %s, %s [edx]", eax, keyword);
        asm_push("lock cmpxchg %s [ecx], %s", keyword, size == DATA_SIZE_BYTE ? "bl" : size == DATA_SIZE_WORD ? "bx" : "ebx");
        asm_push("je .atomic_%i", label);
        asm_push("mov %s [edx], %s", keyword, eax);
        asm_push(".atomic_%i:", label);
        asm_push("sete al");
        asm_push("movzx eax, al");
        return;
    }
    if(size == DATA_SIZE_DDWORD){
        asm_push("push esi");
        asm_push("push edi");
        asm_push("mov esi, ecx");
        asm_push("mov edi, edx");
        asm_push("mov ecx, [ebx+4]");
        asm_push("mov ebx, [ebx]");
        asm_push("mov eax, [edi]");
        asm_push("mov edx, [edi+4]");
        asm_push("lock cmpxchg8b [esi]");
        asm_push("je .atomic_%i", label);
        asm_push("mov [edi], eax");
        asm_push("mov [edi+4], edx");
        asm_push(".atomic_%i:", label);
        asm_push("pop edi");
        asm_push("pop esi");
        asm_push("sete al");
        asm_push("movzx eax, al");
        return;
    }
    asm_push("extern $__atomic_compare_exchange");
    asm_push("push %i", failure);
    asm_push("push %i", success);
    asm_push("push ebx");
    asm_push("push edx");
    asm_push("push ecx");
    asm_push("push %i", size);
    asm_push("call $__atomic_compare_exchange");
    asm_push("add esp, 24");
    asm_push("movzx eax, al");
}

// the builtins on an _Atomic struct, union or long double, the
// object's address pushed: the generic forms (values by address) on any
// of them, and the _n forms <stdatomic.h> uses (values by value: a
// struct's dwords on the stack, as a struct value is here) on a struct
// or union. A long double by value is not done on i386 (its value is
// the x87's, which these do not reach).
static void codegen_x86_atomic_memory(struct generator* generator, const struct atomic_builtin* b, struct vector* arguments,
                                      struct datatype* t){
    int kind = b->kind;
    int size = (int)datatype_size(t);
    int words = align_value(size, DATA_SIZE_DWORD) / DATA_SIZE_DWORD;
    struct datatype void_type;
    datatype_set_void(&void_type);
    struct datatype int_type = datatype_for_int();
    bool by_value = kind == ATOMIC_BUILTIN_LOAD || kind == ATOMIC_BUILTIN_STORE
                 || (kind == ATOMIC_BUILTIN_EXCHANGE && strncmp(b->name, "__sync_", 7)) || kind == ATOMIC_BUILTIN_CAS;
    bool generic = kind == ATOMIC_BUILTIN_GENERIC_LOAD || kind == ATOMIC_BUILTIN_GENERIC_STORE
                || kind == ATOMIC_BUILTIN_GENERIC_EXCHANGE || kind == ATOMIC_BUILTIN_GENERIC_CAS;
    if(!generic && !by_value){
        compiler_error(current_process, "%s needs an integer or pointer object\n", b->name);
    }
    bool ld = !datatype_is_struct_or_union_non_pointer(t);
    if(by_value && ld && (kind == ATOMIC_BUILTIN_LOAD || kind == ATOMIC_BUILTIN_EXCHANGE)){
        // x87_atomic_call does these where a long double is wanted.
        compiler_error(current_process, "x86: %s on an _Atomic long double is only supported for a floating value\n", b->name);
    }
    // The orders: those that are not constants are evaluated here.
    bool constant;
    int order, failure = ATOMIC_ORDER_SEQ_CST;
    if(kind == ATOMIC_BUILTIN_CAS || kind == ATOMIC_BUILTIN_GENERIC_CAS){
        order = atomic_order(current_process, codegen_x86_atomic_argument(arguments, 4), ATOMIC_ACCESS_RMW, &constant);
        failure = atomic_order(current_process, codegen_x86_atomic_argument(arguments, 5), ATOMIC_ACCESS_LOAD, &constant);
        codegen_x86_atomic_orders(generator, arguments, 3, ATOMIC_ACCESS_RMW);
    } else {
        int access = kind == ATOMIC_BUILTIN_LOAD || kind == ATOMIC_BUILTIN_GENERIC_LOAD ? ATOMIC_ACCESS_LOAD
                   : kind == ATOMIC_BUILTIN_STORE || kind == ATOMIC_BUILTIN_GENERIC_STORE ? ATOMIC_ACCESS_STORE
                   : ATOMIC_ACCESS_RMW;
        order = codegen_x86_atomic_orders(generator, arguments, b->args - 1, access);
    }
    switch(kind){
        case ATOMIC_BUILTIN_GENERIC_LOAD:
        case ATOMIC_BUILTIN_GENERIC_STORE:
            codegen_x86_atomic_pop_argument(generator, arguments, 1, "edx");
            asm_push_ins_pop("ecx", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
            codegen_x86_atomic_copy(kind == ATOMIC_BUILTIN_GENERIC_STORE, size, order);
            generator->ret(&void_type, "0");
            return;
        case ATOMIC_BUILTIN_GENERIC_EXCHANGE:
        case ATOMIC_BUILTIN_GENERIC_CAS:
            generator->gen_exp(generator, codegen_x86_atomic_argument(arguments, 1), 0);
            generator->gen_exp(generator, codegen_x86_atomic_argument(arguments, 2), 0);
            asm_push_ins_pop("ebx", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
            asm_push_ins_pop("edx", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
            asm_push_ins_pop("ecx", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
            if(kind == ATOMIC_BUILTIN_GENERIC_EXCHANGE){
                codegen_x86_atomic_exchange_memory(size, order);
                generator->ret(&void_type, "0");
                return;
            }
            codegen_x86_atomic_cas_memory(size, order, failure);
            generator->ret(&int_type, "eax");
            return;
        case ATOMIC_BUILTIN_LOAD:
            asm_push_ins_pop("ebx", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
            codegen_x86_atomic_structure_push(t);
            codegen_response_acknowledge(&(struct response){.flags = RESPONSE_FLAG_PUSHED_STRUCTURE});
            return;
        case ATOMIC_BUILTIN_STORE:
            if(ld){
                // The value through the x87, spilled as a long double.
                x87_operand_as(codegen_x86_atomic_argument(arguments, 1), t);
                x87_spill_as(t);
                asm_push("mov ecx, [esp+%i]", size);
                asm_push("mov edx, esp");
                codegen_x86_atomic_copy(true, size, order);
                x87_release(size);
                asm_push_ins_pop("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
                generator->ret(&void_type, "0");
                return;
            }
            generator->gen_exp(generator, codegen_x86_atomic_argument(arguments, 1), 0);
            asm_push("mov ecx, [esp+%i]", DATA_SIZE_DWORD * words);
            asm_push("mov edx, esp");
            codegen_x86_atomic_copy(true, size, order);
            for(int i = 0; i <= words; i++){
                asm_push_ins_pop("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
            }
            generator->ret(&void_type, "0");
            return;
        case ATOMIC_BUILTIN_EXCHANGE:
            // [A], the result's room, the value: the old value goes to
            // the room, which then moves down over A.
            for(int i = 0; i < words; i++){
                asm_push_ins_push_with_data("dword 0", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value", 0,
                    &(struct stack_frame_data){.dtype = *t});
            }
            generator->gen_exp(generator, codegen_x86_atomic_argument(arguments, 1), 0);
            asm_push("mov ecx, [esp+%i]", 2 * DATA_SIZE_DWORD * words);
            asm_push("mov edx, esp");
            asm_push("lea ebx, [esp+%i]", DATA_SIZE_DWORD * words);
            codegen_x86_atomic_exchange_memory(size, order);
            for(int i = 0; i < words; i++){
                asm_push_ins_pop("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
            }
            for(int i = words - 1; i >= 0; i--){
                asm_push("mov eax, [esp+%i]", DATA_SIZE_DWORD * i);
                asm_push("mov [esp+%i], eax", DATA_SIZE_DWORD * (i + 1));
            }
            asm_push_ins_pop("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
            codegen_response_acknowledge(&(struct response){.flags = RESPONSE_FLAG_PUSHED_STRUCTURE});
            return;
        case ATOMIC_BUILTIN_CAS:
            generator->gen_exp(generator, codegen_x86_atomic_argument(arguments, 1), 0);
            if(ld){
                x87_operand_as(codegen_x86_atomic_argument(arguments, 2), t);
                x87_spill_as(t);
                asm_push("mov ecx, [esp+%i]", size + DATA_SIZE_DWORD);
                asm_push("mov edx, [esp+%i]", size);
                asm_push("mov ebx, esp");
                codegen_x86_atomic_cas_memory(size, order, failure);
                x87_release(size);
                asm_push_ins_pop("ecx", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
                asm_push_ins_pop("ecx", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
                generator->ret(&int_type, "eax");
                return;
            }
            generator->gen_exp(generator, codegen_x86_atomic_argument(arguments, 2), 0);
            asm_push("mov ecx, [esp+%i]", DATA_SIZE_DWORD * (words + 1));
            asm_push("mov edx, [esp+%i]", DATA_SIZE_DWORD * words);
            asm_push("mov ebx, esp");
            codegen_x86_atomic_cas_memory(size, order, failure);
            for(int i = 0; i < words + 2; i++){
                asm_push_ins_pop("ecx", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
            }
            generator->ret(&int_type, "eax");
            return;
    }
    compiler_error(current_process, "BUG: atomic builtin %s\n", b->name);
}

// __atomic_always_lock_free and __atomic_is_lock_free, as
// exp64_atomic_lock_free: 1 for a constant size of 1, 2, 4 or 8 bytes
// (8 through cmpxchg8b, as gcc -m32 says), else 0 for the first and
// libatomic's answer for the second.
static void codegen_x86_atomic_lock_free(struct generator* generator, const struct atomic_builtin* b, struct vector* arguments){
    struct datatype int_type = datatype_for_int();
    struct node* size = codegen_x86_atomic_argument(arguments, 0);
    struct node* p = codegen_x86_atomic_argument(arguments, 1);
    bool always = S_EQ(b->name, "__atomic_always_lock_free");
    struct constant c;
    if(constant_fold(current_process, size, &c) && constant_is_integer(&c)){
        struct constant pc;
        bool aligned = true;
        if(c.value && constant_fold(current_process, p, &pc) && constant_is_integer(&pc)){
            aligned = pc.value % c.value == 0;
        }
        if((atomic_size_is_lock_free((long long)c.value) && aligned) || always){
            asm_push("mov eax, %i", atomic_size_is_lock_free((long long)c.value) && aligned);
            generator->ret(&int_type, "eax");
            return;
        }
    } else if(always){
        compiler_error(current_process, "The size given to __atomic_always_lock_free must be a constant\n");
    }
    generator->gen_exp(generator, p, 0);
    generator->gen_exp(generator, size, 0);
    asm_push("extern $__atomic_is_lock_free");
    asm_push("call $__atomic_is_lock_free");
    asm_push_ins_pop("ecx", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
    asm_push_ins_pop("ecx", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
    asm_push("movzx eax, al");
    generator->ret(&int_type, "eax");
}

// the generic builtins (values by address) on an 8-byte object,
// a double: gcc -m32's sequences. A load or store is one qword access
// through the x87 (fild and fistp move any 64 bits unchanged; the x87
// stack is empty here), a store followed by mfence; an exchange or a
// compare-exchange is lock cmpxchg8b, which compares edx:eax and stores
// ecx:ebx, so the object's address goes in esi, saved around it (esi
// holds the GOT under -fPIC).
static void codegen_x86_atomic_generic8(struct generator* generator, int kind, int label, int order){
    struct datatype void_type;
    datatype_set_void(&void_type);
    struct datatype int_type = datatype_for_int();
    switch(kind){
        case ATOMIC_BUILTIN_GENERIC_LOAD:
            asm_push_ins_pop("ecx", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
            asm_push_ins_pop("ebx", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
            asm_push("fild qword [ebx]");
            asm_push("fistp qword [ecx]");
            generator->ret(&void_type, "0");
            return;
        case ATOMIC_BUILTIN_GENERIC_STORE:
            asm_push_ins_pop("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
            asm_push_ins_pop("ebx", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
            asm_push("fild qword [eax]");
            asm_push("fistp qword [ebx]");
            if(order == ATOMIC_ORDER_SEQ_CST){
                asm_push("mfence");
            }
            generator->ret(&void_type, "0");
            return;
    }
    // An exchange: ret, val, obj; a compare-exchange: desired, expected,
    // obj. eax and edx start as the object's value read plainly; the
    // first cmpxchg8b checks it.
    bool cas = kind == ATOMIC_BUILTIN_GENERIC_CAS;
    asm_push_ins_pop("edx", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
    asm_push_ins_pop("ecx", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
    asm_push_ins_pop("ebx", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
    asm_push("push esi");
    asm_push("push edi");
    asm_push("mov esi, ebx");
    if(cas){
        // edi = expected, which a failure overwrites with what was there.
        asm_push("mov edi, ecx");
        asm_push("mov ebx, [edx]");
        asm_push("mov ecx, [edx+4]");
        asm_push("mov eax, [edi]");
        asm_push("mov edx, [edi+4]");
        asm_push("lock cmpxchg8b [esi]");
        asm_push("je .atomic_%i", label);
        asm_push("mov [edi], eax");
        asm_push("mov [edi+4], edx");
        asm_push(".atomic_%i:", label);
        asm_push("pop edi");
        asm_push("pop esi");
        asm_push("sete al");
        asm_push("movzx eax, al");
        generator->ret(&int_type, "eax");
        return;
    }
    // edi = ret, which gets the old value.
    asm_push("mov edi, edx");
    asm_push("mov ebx, [ecx]");
    asm_push("mov ecx, [ecx+4]");
    asm_push("mov eax, [esi]");
    asm_push("mov edx, [esi+4]");
    asm_push(".atomic_%i:", label);
    asm_push("lock cmpxchg8b [esi]");
    asm_push("jne .atomic_%i", label);
    asm_push("mov [edi], eax");
    asm_push("mov [edi+4], edx");
    asm_push("pop edi");
    asm_push("pop esi");
    generator->ret(&void_type, "0");
}

static int codegen_x86_atomic_size(struct datatype* t){
    return t->flags & DATATYPE_FLAG_IS_POINTER ? DATA_SIZE_DWORD : (int)datatype_size(t);
}

// eax = eax op ecx for the fetch-and-op operators.
static void codegen_x86_atomic_op(const char* op, const char* reg, const char* value){
    if(S_EQ(op, "nand")){
        asm_push("and %s, %s", reg, value);
        asm_push("not %s", reg);
        return;
    }
    const char* ins = S_EQ(op, "+") ? "add" : S_EQ(op, "-") ? "sub" : S_EQ(op, "&") ? "and" : S_EQ(op, "|") ? "or" : "xor";
    asm_push("%s %s, %s", ins, reg, value);
}

void codegen_x86_native_atomic(struct generator* generator, struct native_function* func, struct vector* arguments){
    const struct atomic_builtin* b = atomic_builtin_get(func->name);
    if(vector_count(arguments) != b->args){
        compiler_error(current_process, "%s expects %i arguments\n", b->name, b->args);
    }
    int kind = b->kind;
    struct datatype void_type;
    datatype_set_void(&void_type);
    struct datatype int_type = datatype_for_int();
    if(kind == ATOMIC_BUILTIN_FENCE || kind == ATOMIC_BUILTIN_SIGNAL_FENCE){
        int order = codegen_x86_atomic_orders(generator, arguments, 0, ATOMIC_ACCESS_FENCE);
        if(kind == ATOMIC_BUILTIN_FENCE && order == ATOMIC_ORDER_SEQ_CST){
            asm_push("mfence");
        }
        generator->ret(&void_type, "0");
        return;
    }
    if(kind == ATOMIC_BUILTIN_LOCK_FREE){
        codegen_x86_atomic_lock_free(generator, b, arguments);
        return;
    }
    // The pointer stays pushed while the operands are worked out.
    generator->gen_exp(generator, codegen_x86_atomic_argument(arguments, 0), 0);
    struct datatype pt = datatype_for_numeric();
    asm_datatype_back(&pt);
    struct datatype t = codegen_x86_atomic_object(&pt, b->name);
    if(kind == ATOMIC_BUILTIN_TEST_AND_SET || kind == ATOMIC_BUILTIN_CLEAR){
        t = datatype_for_int();
        t.size = DATA_SIZE_BYTE;
        t.type = DATA_TYPE_CHAR;
        t.flags &= ~DATATYPE_FLAG_IS_SIGNED;
    } else if(datatype_is_struct_or_union_non_pointer(&t) || x87_is_long(&t)){
        codegen_x86_atomic_memory(generator, b, arguments, &t);
        return;
    }
    int size = codegen_x86_atomic_size(&t);
    bool generic = kind == ATOMIC_BUILTIN_GENERIC_LOAD || kind == ATOMIC_BUILTIN_GENERIC_STORE
                || kind == ATOMIC_BUILTIN_GENERIC_EXCHANGE || kind == ATOMIC_BUILTIN_GENERIC_CAS;
    if(size > DATA_SIZE_DWORD && !(generic && size == DATA_SIZE_DDWORD)){
        compiler_error(current_process, "x86: %s on an object of %i bytes is not supported (no 8-byte integers on i386)\n",
            b->name, size);
    }
    bool is_signed = (t.flags & DATATYPE_FLAG_IS_SIGNED) && !(t.flags & DATATYPE_FLAG_IS_POINTER);
    const char* keyword = size == DATA_SIZE_BYTE ? "byte" : size == DATA_SIZE_WORD ? "word" : "dword";
    const char* eax = codegen_sub_register("eax", size);
    const char* edx = codegen_sub_register("edx", size);
    int label = ++codegen_x86_atomic_count;
    // The operands after the pointer: an __atomic builtin ends in its
    // memory order (a compare-exchange in weak and two orders), a
    // __sync one has none.
    int operands = b->args - (!strncmp(b->name, "__sync_", 7) ? 0
                 : kind == ATOMIC_BUILTIN_CAS || kind == ATOMIC_BUILTIN_GENERIC_CAS ? 3 : 1);
    for(int i = 1; i < operands; i++){
        generator->gen_exp(generator, codegen_x86_atomic_argument(arguments, i), 0);
    }
    int access = kind == ATOMIC_BUILTIN_LOAD || kind == ATOMIC_BUILTIN_GENERIC_LOAD ? ATOMIC_ACCESS_LOAD
               : kind == ATOMIC_BUILTIN_STORE || kind == ATOMIC_BUILTIN_GENERIC_STORE || kind == ATOMIC_BUILTIN_CLEAR
                 ? ATOMIC_ACCESS_STORE : ATOMIC_ACCESS_RMW;
    int order = codegen_x86_atomic_orders(generator, arguments, operands, access);
    if(kind == ATOMIC_BUILTIN_RELEASE){
        order = ATOMIC_ORDER_RELEASE;
    }
    // a store weaker than seq_cst is a mov.
    const char* store = order == ATOMIC_ORDER_SEQ_CST ? "xchg" : "mov";
    if(size == DATA_SIZE_DDWORD){
        codegen_x86_atomic_generic8(generator, kind, label, order);
        return;
    }
    switch(kind){
        case ATOMIC_BUILTIN_LOAD:
            asm_push_ins_pop("ebx", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
            codegen_load_eax("ebx", size, is_signed);
            generator->ret(&t, "eax");
            return;
        case ATOMIC_BUILTIN_STORE:
        case ATOMIC_BUILTIN_EXCHANGE:
        case ATOMIC_BUILTIN_GENERIC_STORE:
            asm_push_ins_pop("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
            asm_push_ins_pop("ebx", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
            if(kind == ATOMIC_BUILTIN_GENERIC_STORE){
                codegen_load_eax("eax", size, is_signed);
            }
            asm_push("%s %s [ebx], %s", kind == ATOMIC_BUILTIN_EXCHANGE ? "xchg" : store, keyword, eax);
            if(kind != ATOMIC_BUILTIN_EXCHANGE){
                generator->ret(&void_type, "0");
                return;
            }
            codegen_reduce_register("eax", size, is_signed);
            generator->ret(&t, "eax");
            return;
        case ATOMIC_BUILTIN_CLEAR:
        case ATOMIC_BUILTIN_RELEASE:
        case ATOMIC_BUILTIN_TEST_AND_SET:
            asm_push_ins_pop("ebx", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
            asm_push("mov eax, %i", kind == ATOMIC_BUILTIN_TEST_AND_SET ? 1 : 0);
            asm_push("%s %s [ebx], %s", kind == ATOMIC_BUILTIN_TEST_AND_SET ? "xchg" : store, keyword, eax);
            if(kind != ATOMIC_BUILTIN_TEST_AND_SET){
                generator->ret(&void_type, "0");
                return;
            }
            asm_push("test al, al");
            asm_push("setne al");
            asm_push("movzx eax, al");
            generator->ret(&int_type, "eax");
            return;
        case ATOMIC_BUILTIN_FETCH_OP:
        case ATOMIC_BUILTIN_OP_FETCH:
            asm_push_ins_pop("ecx", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
            asm_push_ins_pop("ebx", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
            if(S_EQ(b->op, "+") || S_EQ(b->op, "-")){
                asm_push("mov eax, ecx");
                if(S_EQ(b->op, "-")){
                    asm_push("neg eax");
                }
                asm_push("lock xadd %s [ebx], %s", keyword, eax);
            } else {
                codegen_load_eax("ebx", size, false);
                asm_push(".atomic_%i:", label);
                asm_push("mov edx, eax");
                codegen_x86_atomic_op(b->op, "edx", "ecx");
                asm_push("lock cmpxchg %s [ebx], %s", keyword, edx);
                asm_push("jne .atomic_%i", label);
            }
            if(kind == ATOMIC_BUILTIN_OP_FETCH){
                codegen_x86_atomic_op(b->op, "eax", "ecx");
            }
            codegen_reduce_register("eax", size, is_signed);
            generator->ret(&t, "eax");
            return;
        case ATOMIC_BUILTIN_CAS_VALUE:
        case ATOMIC_BUILTIN_CAS_BOOL:
            asm_push_ins_pop("edx", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
            asm_push_ins_pop("eax", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
            asm_push_ins_pop("ebx", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
            asm_push("lock cmpxchg %s [ebx], %s", keyword, edx);
            if(kind == ATOMIC_BUILTIN_CAS_BOOL){
                asm_push("sete al");
                asm_push("movzx eax, al");
                generator->ret(&int_type, "eax");
                return;
            }
            codegen_reduce_register("eax", size, is_signed);
            generator->ret(&t, "eax");
            return;
        case ATOMIC_BUILTIN_CAS:
        case ATOMIC_BUILTIN_GENERIC_CAS:
            // The weak form is the strong one. A failure writes what was
            // there to *expected; mov keeps cmpxchg's ZF for the result.
            asm_push_ins_pop("edx", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
            asm_push_ins_pop("ecx", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
            asm_push_ins_pop("ebx", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
            if(kind == ATOMIC_BUILTIN_GENERIC_CAS){
                asm_push("mov %s, %s [edx]", edx, keyword);
            }
            asm_push("mov %s, %s [ecx]", eax, keyword);
            asm_push("lock cmpxchg %s [ebx], %s", keyword, edx);
            asm_push("je .atomic_%i", label);
            asm_push("mov %s [ecx], %s", keyword, eax);
            asm_push(".atomic_%i:", label);
            asm_push("sete al");
            asm_push("movzx eax, al");
            generator->ret(&int_type, "eax");
            return;
        case ATOMIC_BUILTIN_GENERIC_LOAD:
            asm_push_ins_pop("ecx", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
            asm_push_ins_pop("ebx", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
            asm_push("mov %s, %s [ebx]", eax, keyword);
            asm_push("mov %s [ecx], %s", keyword, eax);
            generator->ret(&void_type, "0");
            return;
        case ATOMIC_BUILTIN_GENERIC_EXCHANGE:
            asm_push_ins_pop("edx", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
            asm_push_ins_pop("ecx", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
            asm_push_ins_pop("ebx", STACK_FRAME_ELEMENT_TYPE_PUSHED_VALUE, "result_value");
            asm_push("mov %s, %s [ecx]", eax, keyword);
            asm_push("xchg %s [ebx], %s", keyword, eax);
            asm_push("mov %s [edx], %s", keyword, eax);
            generator->ret(&void_type, "0");
            return;
    }
    compiler_error(current_process, "BUG: atomic builtin %s\n", b->name);
}

// the address of this thread's copy of the variable a name
// stands for here (parser_thread_local_object): its offset from the
// thread pointer out of the GOT (initial-exec, `wrt ..tlsie`, which ld
// turns into local-exec for an executable that defines it), plus the
// thread pointer at gs:0. One this file defines is reached through its
// hidden alias (tls_alias). A local of the same name that hides it is
// not thread-local: its address.
void codegen_x86_native_tls_address(struct generator* generator, struct native_function* func, struct vector* arguments){
    (void)func;
    struct node* name = codegen_x86_atomic_argument(arguments, 0);
    struct resolver_entity* entity = resolver_get_variable(0, current_process->resolver, name->sval);
    if(!entity){
        compiler_error(current_process, "%s is not declared\n", name->sval);
    }
    struct resolver_default_entity_data* data = codegen_entity_private(entity);
    struct datatype void_pointer;
    datatype_set_void(&void_pointer);
    void_pointer.pointer_depth = 1;
    void_pointer.flags |= DATATYPE_FLAG_IS_POINTER;
    if(!(entity->dtype.flags & DATATYPE_FLAG_IS_THREAD_LOCAL) || (data->flags & RESOLVER_DEFAULT_ENTITY_FLAG_IS_LOCAL_STACK)){
        asm_push("lea eax, [%s]", data->address);
        generator->ret(&void_pointer, "eax");
        return;
    }
    const char* symbol = name->sval;
    bool here = variable_defined_in_tree(current_process, symbol);
    asm_push("mov eax, [%s wrt ..tlsie]", codegen_symbol(here ? tls_alias(current_process, symbol) : symbol));
    asm_push("add eax, [gs:0]");
    generator->ret(&void_pointer, "eax");
}

void codegen_gen_exp(struct generator* generator, struct node* node, int flags){
    // ch238: switch from history_down to history_begin since the
    // remembered.history slot is always NULL at native-callback
    // entry. history_begin synthesizes a fresh top-level history.
    codegen_generate_expressionable(node, codegen_history_begin(flags));
}

void codegen_end_exp(struct generator* generator){
    // currently a no-op; reserved for future bookkeeping.
}

int codegen_x86(struct compile_process* process){
    current_process = process;
    // ch236: bind the global x86 v-table to the active compile_process.
    x86_codegen.compiler = current_process;
    scope_create_root(process);

    vector_set_peek_pointer(process->node_tree_vec, 0);
    codegen_new_scope(0);
    // The functions find the GOT through it.
    asm_push("extern _GLOBAL_OFFSET_TABLE_");
    codegen_pic_function_globals();
    codegen_generate_data_section();

    vector_set_peek_pointer(process->node_tree_vec, 0);
    codegen_generate_root();
    codegen_finish_scope();

    // ch187: flush queued .data lines (per-call function pointer
    // slots etc.) before .rodata.
    codegen_generate_data_section_add_ons();
    codegen_generate_rod();
    debug_end(current_process);
    // The stack is not executable (the linker warns otherwise).
    asm_push("section .note.GNU-stack noalloc noexec nowrite progbits");
    return CODEGEN_ALL_OK;
}
