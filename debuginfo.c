#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "compiler.h"
#include "helpers/vector.h"

// the DWARF of -g. The backends call the
// debug_* functions at their statements, blocks, prologues and
// epilogues; each returns at once without COMPILE_PROCESS_DEBUG.
//
//   - line table: GNU as builds .debug_line from `.file` and `.loc`.
//     NASM has nothing like `.loc` (its own -F dwarf maps lines of the
//     .asm, or %line ones, but writes a .debug_info of its own with no
//     C functions in it), so for x86 and x86_64 the line program is
//     written here: a mark before each statement, one row per mark.
//   - call frame information: `.cfi_*` on GNU as (the assembler makes
//     .eh_frame); for NASM a .debug_frame, one FDE per function, its
//     rows placed with DW_CFA_set_loc at marks.
//   - .debug_info on every target: the compile unit, a subprogram per
//     function with its frame register as DW_AT_frame_base, a lexical
//     block per `{ }`, parameters and locals at DW_OP_fbreg offsets,
//     globals and static locals at DW_OP_addr, and their types.
//
// A mark (debug_mark) is a place in the current function's code: a
// `.L` label for GNU as, which never becomes a symbol. NASM writes
// every label to the symbol table, and a debugger or disassembler then
// names the code after it by the label, so for NASM a mark is an EQU
// of the offset from the function's symbol (an absolute symbol, never
// taken for code) and its address is `$function + mark`.
//
// The text is collected in buffers and written by debug_end. Every
// LEB128 number is encoded here, so the two assemblers only see
// plain bytes, words and labels.

#define DEBUG_LINE_MAX 1024

// DWARF 4 tags, attributes, forms and operations used below.
#define DW_TAG_array_type       0x01
#define DW_TAG_formal_parameter 0x05
#define DW_TAG_lexical_block    0x0b
#define DW_TAG_member           0x0d
#define DW_TAG_pointer_type     0x0f
#define DW_TAG_compile_unit     0x11
#define DW_TAG_structure_type   0x13
#define DW_TAG_subroutine_type  0x15
#define DW_TAG_union_type       0x17
#define DW_TAG_subrange_type    0x21
#define DW_TAG_base_type        0x24
#define DW_TAG_subprogram       0x2e
#define DW_TAG_variable         0x34

#define DW_AT_location             0x02
#define DW_AT_name                 0x03
#define DW_AT_byte_size            0x0b
#define DW_AT_stmt_list            0x10
#define DW_AT_low_pc               0x11
#define DW_AT_high_pc              0x12
#define DW_AT_language             0x13
#define DW_AT_comp_dir             0x1b
#define DW_AT_producer             0x25
#define DW_AT_upper_bound          0x2f
#define DW_AT_data_member_location 0x38
#define DW_AT_declaration          0x3c
#define DW_AT_encoding             0x3e
#define DW_AT_external             0x3f
#define DW_AT_frame_base           0x40
#define DW_AT_type                 0x49

#define DW_FORM_addr         0x01
#define DW_FORM_data4        0x06
#define DW_FORM_string       0x08
#define DW_FORM_data1        0x0b
#define DW_FORM_flag         0x0c
#define DW_FORM_udata        0x0f
#define DW_FORM_ref4         0x13
#define DW_FORM_sec_offset   0x17
#define DW_FORM_exprloc      0x18
#define DW_FORM_flag_present 0x19

#define DW_OP_addr  0x03
#define DW_OP_deref 0x06
#define DW_OP_reg0  0x50
#define DW_OP_breg0 0x70
#define DW_OP_fbreg 0x91

#define DW_ATE_boolean       0x02
#define DW_ATE_float         0x04
#define DW_ATE_signed        0x05
#define DW_ATE_signed_char   0x06
#define DW_ATE_unsigned      0x07
#define DW_ATE_unsigned_char 0x08

#define DW_LANG_C99 0x0c

#define DW_LNS_copy         0x01
#define DW_LNS_advance_line 0x03
#define DW_LNS_set_file     0x04
#define DW_LNS_set_prologue_end 0x0a
#define DW_LNE_end_sequence 0x01
#define DW_LNE_set_address  0x02

#define DW_CFA_set_loc            0x01
#define DW_CFA_register           0x09
#define DW_CFA_remember_state     0x0a
#define DW_CFA_restore_state      0x0b
#define DW_CFA_def_cfa            0x0c
#define DW_CFA_def_cfa_register   0x0d
#define DW_CFA_def_cfa_offset     0x0e
#define DW_CFA_offset             0x80
#define DW_CFA_restore            0xc0

// The abbreviation codes, in the order of debug_abbrevs.
#define DEBUG_ABBREV_COMPILE_UNIT       1
#define DEBUG_ABBREV_SUBPROGRAM         2
#define DEBUG_ABBREV_SUBPROGRAM_VOID    3
#define DEBUG_ABBREV_PARAMETER          4
#define DEBUG_ABBREV_VARIABLE           5
#define DEBUG_ABBREV_BLOCK              6
#define DEBUG_ABBREV_BASE_TYPE          7
#define DEBUG_ABBREV_POINTER            8
#define DEBUG_ABBREV_POINTER_VOID       9
#define DEBUG_ABBREV_STRUCT             10
#define DEBUG_ABBREV_UNION              11
#define DEBUG_ABBREV_STRUCT_DECLARATION 12
#define DEBUG_ABBREV_UNION_DECLARATION  13
#define DEBUG_ABBREV_MEMBER             14
#define DEBUG_ABBREV_ARRAY              15
#define DEBUG_ABBREV_SUBRANGE           16
#define DEBUG_ABBREV_SUBRANGE_OPEN      17
#define DEBUG_ABBREV_SUBROUTINE         18

// .debug_abbrev: code, tag, has children, (attribute, form) pairs, 0 0.
// Every number is below 128, so each is its own one-byte LEB128.
static const unsigned char debug_abbrevs[] = {
    DEBUG_ABBREV_COMPILE_UNIT, DW_TAG_compile_unit, 1,
        DW_AT_producer, DW_FORM_string, DW_AT_language, DW_FORM_data1,
        DW_AT_name, DW_FORM_string, DW_AT_comp_dir, DW_FORM_string,
        DW_AT_low_pc, DW_FORM_addr, DW_AT_high_pc, DW_FORM_data4,
        DW_AT_stmt_list, DW_FORM_sec_offset, 0, 0,
    DEBUG_ABBREV_SUBPROGRAM, DW_TAG_subprogram, 1,
        DW_AT_name, DW_FORM_string, DW_AT_external, DW_FORM_flag, DW_AT_type, DW_FORM_ref4,
        DW_AT_low_pc, DW_FORM_addr, DW_AT_high_pc, DW_FORM_data4,
        DW_AT_frame_base, DW_FORM_exprloc, 0, 0,
    DEBUG_ABBREV_SUBPROGRAM_VOID, DW_TAG_subprogram, 1,
        DW_AT_name, DW_FORM_string, DW_AT_external, DW_FORM_flag,
        DW_AT_low_pc, DW_FORM_addr, DW_AT_high_pc, DW_FORM_data4,
        DW_AT_frame_base, DW_FORM_exprloc, 0, 0,
    DEBUG_ABBREV_PARAMETER, DW_TAG_formal_parameter, 0,
        DW_AT_name, DW_FORM_string, DW_AT_type, DW_FORM_ref4, DW_AT_location, DW_FORM_exprloc, 0, 0,
    DEBUG_ABBREV_VARIABLE, DW_TAG_variable, 0,
        DW_AT_name, DW_FORM_string, DW_AT_type, DW_FORM_ref4, DW_AT_location, DW_FORM_exprloc, 0, 0,
    DEBUG_ABBREV_BLOCK, DW_TAG_lexical_block, 1,
        DW_AT_low_pc, DW_FORM_addr, DW_AT_high_pc, DW_FORM_data4, 0, 0,
    DEBUG_ABBREV_BASE_TYPE, DW_TAG_base_type, 0,
        DW_AT_name, DW_FORM_string, DW_AT_encoding, DW_FORM_data1, DW_AT_byte_size, DW_FORM_data1, 0, 0,
    DEBUG_ABBREV_POINTER, DW_TAG_pointer_type, 0,
        DW_AT_byte_size, DW_FORM_data1, DW_AT_type, DW_FORM_ref4, 0, 0,
    DEBUG_ABBREV_POINTER_VOID, DW_TAG_pointer_type, 0,
        DW_AT_byte_size, DW_FORM_data1, 0, 0,
    DEBUG_ABBREV_STRUCT, DW_TAG_structure_type, 1,
        DW_AT_name, DW_FORM_string, DW_AT_byte_size, DW_FORM_udata, 0, 0,
    DEBUG_ABBREV_UNION, DW_TAG_union_type, 1,
        DW_AT_name, DW_FORM_string, DW_AT_byte_size, DW_FORM_udata, 0, 0,
    DEBUG_ABBREV_STRUCT_DECLARATION, DW_TAG_structure_type, 0,
        DW_AT_name, DW_FORM_string, DW_AT_declaration, DW_FORM_flag_present, 0, 0,
    DEBUG_ABBREV_UNION_DECLARATION, DW_TAG_union_type, 0,
        DW_AT_name, DW_FORM_string, DW_AT_declaration, DW_FORM_flag_present, 0, 0,
    DEBUG_ABBREV_MEMBER, DW_TAG_member, 0,
        DW_AT_name, DW_FORM_string, DW_AT_type, DW_FORM_ref4, DW_AT_data_member_location, DW_FORM_udata, 0, 0,
    DEBUG_ABBREV_ARRAY, DW_TAG_array_type, 1,
        DW_AT_type, DW_FORM_ref4, 0, 0,
    DEBUG_ABBREV_SUBRANGE, DW_TAG_subrange_type, 0,
        DW_AT_upper_bound, DW_FORM_udata, 0, 0,
    DEBUG_ABBREV_SUBRANGE_OPEN, DW_TAG_subrange_type, 0,
        0, 0,
    DEBUG_ABBREV_SUBROUTINE, DW_TAG_subroutine_type, 0,
        0, 0,
    0,
};

// A growing piece of asm text.
struct debug_text {
    char*  data;
    size_t length;
    size_t capacity;
};

// A type that has a DIE: its key (debug_type_key), the label of its
// DIE, and the type itself, or a subroutine type (a function pointer's
// target, described without its parameters).
#define DEBUG_KIND_TYPE       0
#define DEBUG_KIND_SUBROUTINE 1

struct debug_type {
    char*           key;
    int             label;
    int             kind;
    struct datatype dtype;
};

static struct compile_process* debug_process;
// NASM syntax (x86, x86_64) rather than GNU as.
static bool             debug_nasm;
static int              debug_address_size;
// DWARF numbers of the frame register (DW_AT_frame_base), and for the
// .debug_frame of NASM of the stack pointer and the return address.
static int              debug_frame_register;
static int              debug_sp_register;
static int              debug_ra_register;
static int              debug_label_count;
static int              debug_cu_label;
static int              debug_cie_label;
// The function being generated, and the mark after its last byte; the
// first and the last function, whose ends bound the unit's code.
static const char*      debug_function;
static int              debug_function_end_label;
static const char*      debug_first_function;
static const char*      debug_last_function;
static int              debug_last_end_label;
// The next line row is the first after the prologue.
static bool             debug_prologue_end;
static struct debug_text debug_info;
static struct debug_text debug_types;
// NASM only: the line program's rows, the FDEs, and the CFA program of
// the current function with its size in bytes.
static struct debug_text debug_rows;
static struct debug_text debug_frames;
static struct debug_text debug_fde;
static int              debug_fde_size;
static int              debug_row_file;
static int              debug_row_line;
static struct vector*   debug_files;
static struct vector*   debug_blocks;
static struct vector*   debug_type_list;
static struct vector*   debug_static_symbols;
// The globals the parser made of static locals, waiting for their
// function (debug_function_begin).
static struct vector*   debug_static_globals;

static void debug_text_append(struct debug_text* text, const char* s){
    size_t n = strlen(s);
    if(text->length + n + 1 > text->capacity){
        size_t capacity = text->capacity ? text->capacity * 2 : 4096;
        while(capacity < text->length + n + 1){
            capacity *= 2;
        }
        text->data = realloc(text->data, capacity);
        text->capacity = capacity;
    }
    memcpy(text->data + text->length, s, n + 1);
    text->length += n;
}

static void debug_text_printf(struct debug_text* text, const char* fmt, ...){
    char line[DEBUG_LINE_MAX];
    va_list args;
    va_start(args, fmt);
    vsnprintf(line, sizeof(line), fmt, args);
    va_end(args);
    debug_text_append(text, line);
    debug_text_append(text, "\n");
}

static void debug_text_clear(struct debug_text* text){
    text->length = 0;
    if(text->data){
        text->data[0] = 0;
    }
}

// One line of asm to the output, after whatever the backend wrote.
static void debug_out(const char* fmt, ...){
    va_list args;
    va_start(args, fmt);
    vfprintf(debug_process->ofile, fmt, args);
    va_end(args);
    fprintf(debug_process->ofile, "\n");
}

static void debug_out_text(struct debug_text* text){
    if(text->length){
        fputs(text->data, debug_process->ofile);
    }
}

static void debug_setup(struct compile_process* process){
    const char* backend = process->target->backend->name;
    debug_process        = process;
    debug_nasm           = S_EQ(backend, "x86") || S_EQ(backend, "x86_64");
    debug_address_size   = (int)process->target->pointer_size;
    debug_frame_register = S_EQ(backend, "x86") ? 5 : S_EQ(backend, "x86_64") ? 6 : S_EQ(backend, "aarch64") ? 29 : 8;
    debug_sp_register    = S_EQ(backend, "x86") ? 4 : 7;
    debug_ra_register    = S_EQ(backend, "x86") ? 8 : 16;
    debug_label_count    = 0;
    debug_cu_label       = ++debug_label_count;
    debug_cie_label      = ++debug_label_count;
    debug_function       = 0;
    debug_first_function = 0;
    debug_last_function  = 0;
    debug_last_end_label = 0;
    debug_prologue_end   = false;
    debug_fde_size       = 0;
    debug_row_file       = 1;
    debug_row_line       = 1;
    debug_text_clear(&debug_info);
    debug_text_clear(&debug_types);
    debug_text_clear(&debug_rows);
    debug_text_clear(&debug_frames);
    debug_text_clear(&debug_fde);
    debug_files          = vector_create(sizeof(const char*));
    debug_blocks         = vector_create(sizeof(int));
    debug_type_list      = vector_create(sizeof(struct debug_type*));
    debug_static_symbols = vector_create(sizeof(const char*));
    debug_static_globals = vector_create(sizeof(struct node*));
}

// Whether -g is on, and nothing is muted (exp64 works out a type
// without writing anything).
static bool debug_active(struct compile_process* process){
    if(!process || !(process->flags & COMPILE_PROCESS_DEBUG) || !process->ofile || exp64_output_muted()){
        return false;
    }
    if(debug_process != process){
        debug_setup(process);
    }
    return true;
}

static int debug_new_label(void){
    return ++debug_label_count;
}

// A label of this file only: `.L` is never a symbol to GNU as, and a
// `..@` name leaves NASM's scope of `.name` labels alone.
static const char* debug_label(int id){
    static char buffers[4][32];
    static int next;
    char* out = buffers[next++ % 4];
    snprintf(out, sizeof(buffers[0]), debug_nasm ? "..@dbg%d" : ".Ldbg%d", id);
    return out;
}

// A C symbol as the assembler takes it (NASM: `$` marks a name).
static const char* debug_symbol(const char* name){
    static char buffers[4][IDENTIFIER_MAX_LENGTH + 32];
    static int next;
    char* out = buffers[next++ % 4];
    snprintf(out, sizeof(buffers[0]), debug_nasm ? "$%s" : "%s", name);
    return out;
}

// Mark `id` here, in the current function's code (see the top).
static void debug_mark_at(int id){
    if(debug_nasm){
        debug_out("%s equ $ - %s", debug_label(id), debug_symbol(debug_function));
    } else {
        debug_out("%s:", debug_label(id));
    }
}

static int debug_mark(void){
    int id = debug_new_label();
    debug_mark_at(id);
    return id;
}

// The address of a mark in `function`.
static const char* debug_mark_address(int id, const char* function){
    static char buffers[2][IDENTIFIER_MAX_LENGTH + 64];
    static int next;
    char* out = buffers[next++ % 2];
    if(debug_nasm){
        snprintf(out, sizeof(buffers[0]), "%s + %s", debug_symbol(function), debug_label(id));
    } else {
        snprintf(out, sizeof(buffers[0]), "%s", debug_label(id));
    }
    return out;
}

// The bytes from the start of `function` to a mark in it.
static const char* debug_mark_offset(int id, const char* function){
    static char buffers[2][IDENTIFIER_MAX_LENGTH + 64];
    static int next;
    char* out = buffers[next++ % 2];
    if(debug_nasm){
        snprintf(out, sizeof(buffers[0]), "%s", debug_label(id));
    } else {
        snprintf(out, sizeof(buffers[0]), "%s - %s", debug_label(id), debug_symbol(function));
    }
    return out;
}

// The data directive for `size` bytes.
static const char* debug_dir(int size){
    if(debug_nasm){
        return size == 1 ? "db" : size == 2 ? "dw" : size == 4 ? "dd" : "dq";
    }
    return size == 1 ? ".byte" : size == 2 ? ".2byte" : size == 4 ? ".4byte" : ".8byte";
}

static void debug_section(const char* name){
    if(debug_nasm){
        debug_out("section %s noalloc noexec nowrite progbits align=1", name);
    } else {
        debug_out(".section %s,\"\",@progbits", name);
    }
}

static int debug_uleb(unsigned char* out, unsigned long value){
    int n = 0;
    do {
        unsigned char byte = value & 0x7f;
        value >>= 7;
        if(value){
            byte |= 0x80;
        }
        out[n++] = byte;
    } while(value);
    return n;
}

// Signed LEB128. The shift is written as a division of a multiple of
// 128 so it rounds down without relying on >> of a negative number.
static int debug_sleb(unsigned char* out, long value){
    int n = 0;
    while(1){
        unsigned char byte = value & 0x7f;
        value = (value - (value & 0x7f)) / 128;
        if((value == 0 && !(byte & 0x40)) || (value == -1 && (byte & 0x40))){
            out[n++] = byte;
            return n;
        }
        out[n++] = byte | 0x80;
    }
}

static void debug_text_bytes(struct debug_text* text, const unsigned char* bytes, int count){
    char line[DEBUG_LINE_MAX];
    for(int i = 0; i < count; i += 16){
        int n = snprintf(line, sizeof(line), "%s ", debug_dir(1));
        for(int k = i; k < count && k < i + 16; k++){
            n += snprintf(line + n, sizeof(line) - n, k > i ? ",%u" : "%u", (unsigned)bytes[k]);
        }
        debug_text_append(text, line);
        debug_text_append(text, "\n");
    }
}

static void debug_text_byte(struct debug_text* text, int value){
    unsigned char byte = (unsigned char)value;
    debug_text_bytes(text, &byte, 1);
}

static void debug_text_uleb(struct debug_text* text, unsigned long value){
    unsigned char bytes[16];
    debug_text_bytes(text, bytes, debug_uleb(bytes, value));
}

// A DW_FORM_string: the bytes and a NUL, as numbers, so no quoting
// rules of either assembler apply.
static void debug_text_string(struct debug_text* text, const char* s){
    debug_text_bytes(text, (const unsigned char*)s, (int)strlen(s) + 1);
}

// A DW_FORM_ref4: the DIE's offset from the start of the unit.
static void debug_text_ref(struct debug_text* text, int label){
    debug_text_printf(text, "%s %s - %s", debug_dir(4), debug_label(label), debug_label(debug_cu_label));
}

static void debug_text_address(struct debug_text* text, const char* expression){
    debug_text_printf(text, "%s %s", debug_dir(debug_address_size), expression);
}

// ---------------------------------------------------------------
// Types
// ---------------------------------------------------------------

static bool debug_is_array(struct datatype* t){
    return t->flags & DATATYPE_FLAG_IS_ARRAY;
}

static bool debug_is_pointer(struct datatype* t){
    return !debug_is_array(t) && (t->flags & DATATYPE_FLAG_IS_POINTER) && t->pointer_depth > 0;
}

static struct datatype debug_pointee(struct datatype* t){
    struct datatype p = *t;
    p.pointer_depth--;
    if(p.pointer_depth <= 0){
        p.pointer_depth = 0;
        p.flags &= ~DATATYPE_FLAG_IS_POINTER;
    }
    return p;
}

// All the dimensions of an array are one DIE, so its element is the
// type with every bracket gone.
static struct datatype debug_element(struct datatype* t){
    struct datatype e = *t;
    e.flags &= ~DATATYPE_FLAG_IS_ARRAY;
    e.array.brackets = 0;
    e.array.size = 0;
    return e;
}

static struct vector* debug_brackets(struct datatype* t){
    return t->array.brackets ? t->array.brackets->n_brackets : 0;
}

// A bracket's constant size, or -1 for `[]`, or -2 for a variable
// length.
static long long debug_bracket_size(struct node* bracket){
    struct node* inner = bracket->bracket.inner;
    if(!inner || inner->type == NODE_TYPE_BLANK){
        return -1;
    }
    return inner->type == NODE_TYPE_NUMBER ? (long long)inner->llnum : -2;
}

// A variable length array is a pointer to storage made at run time;
// it gets no DIE.
static bool debug_describable(struct datatype* t){
    struct vector* brackets = debug_is_array(t) ? debug_brackets(t) : 0;
    for(int i = 0; brackets && i < vector_count(brackets); i++){
        if(debug_bracket_size(*(struct node**)vector_at(brackets, i)) == -2){
            return false;
        }
    }
    return true;
}

static void debug_type_key(struct datatype* t, char* out, size_t size){
    if(size < 64){
        snprintf(out, size, "?");
        return;
    }
    if(debug_is_array(t)){
        struct vector* brackets = debug_brackets(t);
        size_t n = snprintf(out, size, "A");
        for(int i = 0; brackets && i < vector_count(brackets) && n + 64 < size; i++){
            n += snprintf(out + n, size - n, "%lld,", debug_bracket_size(*(struct node**)vector_at(brackets, i)));
        }
        struct datatype e = debug_element(t);
        debug_type_key(&e, out + n, size - n);
        return;
    }
    if(t->flags & DATATYPE_FLAG_IS_FUNCTION_POINTER){
        snprintf(out, size, "F");
        return;
    }
    if(debug_is_pointer(t)){
        struct datatype p = debug_pointee(t);
        out[0] = 'P';
        debug_type_key(&p, out + 1, size - 1);
        return;
    }
    if(t->type == DATA_TYPE_STRUCT || t->type == DATA_TYPE_UNION){
        snprintf(out, size, "%c%.200s", t->type == DATA_TYPE_STRUCT ? 'S' : 'U', t->type_str ? t->type_str : "");
        return;
    }
    snprintf(out, size, "B%d,%lu,%d,%.32s", t->type, (unsigned long)t->size, (t->flags & DATATYPE_FLAG_IS_SIGNED) ? 1 : 0,
        t->type_str ? t->type_str : "");
}

// The label of the DIE of a type, which debug_end writes.
static int debug_type_of_kind(int kind, struct datatype* t){
    char key[512];
    if(kind == DEBUG_KIND_SUBROUTINE){
        snprintf(key, sizeof(key), "R");
    } else {
        debug_type_key(t, key, sizeof(key));
    }
    for(int i = 0; i < vector_count(debug_type_list); i++){
        struct debug_type* known = *(struct debug_type**)vector_at(debug_type_list, i);
        if(strcmp(known->key, key) == 0){
            return known->label;
        }
    }
    struct debug_type* entry = calloc(1, sizeof(struct debug_type));
    entry->key = strdup(key);
    entry->label = debug_new_label();
    entry->kind = kind;
    if(t){
        entry->dtype = *t;
    }
    vector_push(debug_type_list, &entry);
    return entry->label;
}

static int debug_type(struct datatype* t){
    return debug_type_of_kind(DEBUG_KIND_TYPE, t);
}

static void debug_emit_base_type(struct datatype* t){
    bool is_signed = t->flags & DATATYPE_FLAG_IS_SIGNED;
    const char* name = "int";
    int encoding = DW_ATE_signed;
    switch(t->type){
        case DATA_TYPE_CHAR:
            name = is_signed ? "char" : "unsigned char";
            encoding = is_signed ? DW_ATE_signed_char : DW_ATE_unsigned_char;
            break;
        case DATA_TYPE_SHORT:
            name = is_signed ? "short" : "unsigned short";
            encoding = is_signed ? DW_ATE_signed : DW_ATE_unsigned;
            break;
        case DATA_TYPE_INTEGER:
            name = is_signed ? "int" : "unsigned int";
            encoding = is_signed ? DW_ATE_signed : DW_ATE_unsigned;
            break;
        case DATA_TYPE_LONG:
            name = is_signed ? "long" : "unsigned long";
            encoding = is_signed ? DW_ATE_signed : DW_ATE_unsigned;
            break;
        case DATA_TYPE_FLOAT:
            name = "float";
            encoding = DW_ATE_float;
            break;
        case DATA_TYPE_DOUBLE:
            name = "double";
            encoding = DW_ATE_float;
            break;
        case DATA_TYPE_LONG_DOUBLE:
            name = "long double";
            encoding = DW_ATE_float;
            break;
    }
    if(t->type_str && S_EQ(t->type_str, "_Bool")){
        name = "_Bool";
        encoding = DW_ATE_boolean;
    }
    debug_text_uleb(&debug_types, DEBUG_ABBREV_BASE_TYPE);
    debug_text_string(&debug_types, name);
    debug_text_byte(&debug_types, encoding);
    debug_text_byte(&debug_types, (int)datatype_size(t));
}

static struct node* debug_struct_node(struct datatype* t){
    struct node* s = t->struct_node;
    if(!s || (s->type == NODE_TYPE_STRUCT ? !s->_struct.body_n : !s->_union.body_n)){
        if(t->type_str){
            s = t->type == DATA_TYPE_UNION ? union_node_for_name(debug_process, t->type_str)
                                           : struct_node_for_name(debug_process, t->type_str);
        }
    }
    return s;
}

static void debug_emit_member(struct node* member, bool is_union){
    if(member->type != NODE_TYPE_VARIABLE || !member->var.name || member->var.bitfield.is_bitfield
       || !debug_describable(&member->var.type)){
        return;
    }
    debug_text_uleb(&debug_types, DEBUG_ABBREV_MEMBER);
    debug_text_string(&debug_types, member->var.name);
    debug_text_ref(&debug_types, debug_type(&member->var.type));
    debug_text_uleb(&debug_types, (unsigned long)(is_union ? 0 : member->var.aoffset));
}

static void debug_emit_struct(struct datatype* t){
    bool is_union = t->type == DATA_TYPE_UNION;
    struct node* s = debug_struct_node(t);
    struct node* body = 0;
    const char* name = t->type_str;
    if(s){
        body = s->type == NODE_TYPE_STRUCT ? s->_struct.body_n : s->_union.body_n;
        name = s->type == NODE_TYPE_STRUCT ? s->_struct.name : s->_union.name;
    }
    if(!body){
        debug_text_uleb(&debug_types, is_union ? DEBUG_ABBREV_UNION_DECLARATION : DEBUG_ABBREV_STRUCT_DECLARATION);
        debug_text_string(&debug_types, name ? name : "");
        return;
    }
    debug_text_uleb(&debug_types, is_union ? DEBUG_ABBREV_UNION : DEBUG_ABBREV_STRUCT);
    debug_text_string(&debug_types, name ? name : "");
    debug_text_uleb(&debug_types, (unsigned long)datatype_size(t));
    struct vector* statements = body->body.statements;
    for(int i = 0; i < vector_count(statements); i++){
        struct node* member = *(struct node**)vector_at(statements, i);
        if(member->type == NODE_TYPE_VARIABLE_LIST){
            struct vector* list = member->var_list.list;
            for(int j = 0; j < vector_count(list); j++){
                debug_emit_member(*(struct node**)vector_at(list, j), is_union);
            }
        } else {
            debug_emit_member(member, is_union);
        }
    }
    debug_text_byte(&debug_types, 0);
}

static void debug_emit_type(struct debug_type* entry){
    struct datatype* t = &entry->dtype;
    debug_text_printf(&debug_types, "%s:", debug_label(entry->label));
    if(entry->kind == DEBUG_KIND_SUBROUTINE){
        debug_text_uleb(&debug_types, DEBUG_ABBREV_SUBROUTINE);
        return;
    }
    if(debug_is_array(t)){
        struct datatype e = debug_element(t);
        struct vector* brackets = debug_brackets(t);
        debug_text_uleb(&debug_types, DEBUG_ABBREV_ARRAY);
        debug_text_ref(&debug_types, debug_type(&e));
        for(int i = 0; brackets && i < vector_count(brackets); i++){
            long long count = debug_bracket_size(*(struct node**)vector_at(brackets, i));
            if(count > 0){
                debug_text_uleb(&debug_types, DEBUG_ABBREV_SUBRANGE);
                debug_text_uleb(&debug_types, (unsigned long)(count - 1));
            } else {
                debug_text_uleb(&debug_types, DEBUG_ABBREV_SUBRANGE_OPEN);
            }
        }
        debug_text_byte(&debug_types, 0);
        return;
    }
    if(t->flags & DATATYPE_FLAG_IS_FUNCTION_POINTER){
        debug_text_uleb(&debug_types, DEBUG_ABBREV_POINTER);
        debug_text_byte(&debug_types, debug_address_size);
        debug_text_ref(&debug_types, debug_type_of_kind(DEBUG_KIND_SUBROUTINE, 0));
        return;
    }
    if(debug_is_pointer(t)){
        struct datatype p = debug_pointee(t);
        if(datatype_is_void_no_ptr(&p)){
            debug_text_uleb(&debug_types, DEBUG_ABBREV_POINTER_VOID);
            debug_text_byte(&debug_types, debug_address_size);
            return;
        }
        debug_text_uleb(&debug_types, DEBUG_ABBREV_POINTER);
        debug_text_byte(&debug_types, debug_address_size);
        debug_text_ref(&debug_types, debug_type(&p));
        return;
    }
    if(t->type == DATA_TYPE_STRUCT || t->type == DATA_TYPE_UNION){
        debug_emit_struct(t);
        return;
    }
    debug_emit_base_type(t);
}

// ---------------------------------------------------------------
// Variables
// ---------------------------------------------------------------

// A parameter or variable DIE. `address` is a symbol for DW_OP_addr;
// `reg` (not -1) the DWARF number of the register the variable lives in
// at -O1, for DW_OP_reg; otherwise the location is
// DW_OP_fbreg `offset`, then DW_OP_deref when the slot holds the
// object's address.
static void debug_variable(int abbrev, const char* name, struct datatype* type, const char* address, int offset, bool by_reference,
                           int reg){
    debug_text_uleb(&debug_info, abbrev);
    debug_text_string(&debug_info, name);
    debug_text_ref(&debug_info, debug_type(type));
    if(reg >= 0 && reg < 32){
        unsigned char op[2] = { 1, (unsigned char)(DW_OP_reg0 + reg) };
        debug_text_bytes(&debug_info, op, 2);
        return;
    }
    if(address){
        unsigned char op[2] = { (unsigned char)(1 + debug_address_size), DW_OP_addr };
        debug_text_bytes(&debug_info, op, 2);
        debug_text_address(&debug_info, address);
        return;
    }
    unsigned char location[16];
    int n = 1;
    location[n++] = DW_OP_fbreg;
    n += debug_sleb(location + n, offset);
    if(by_reference){
        location[n++] = DW_OP_deref;
    }
    location[0] = (unsigned char)(n - 1);
    debug_text_bytes(&debug_info, location, n);
}

// The parameters are at their frame offsets (var.aoffset) once the
// prologue has stored them; an array parameter is the pointer it
// decays to. `by_reference`: the slot holds the address of a struct
// the caller copied (aarch64 and riscv64, over 16 bytes).
void debug_parameter(struct compile_process* process, struct node* var, bool by_reference){
    if(!debug_active(process) || !var->var.name || !debug_describable(&var->var.type)){
        return;
    }
    struct datatype type = var->var.type;
    if(debug_is_array(&type)){
        type = datatype_array_decay(&type);
    }
    debug_variable(DEBUG_ABBREV_PARAMETER, var->var.name, &type, 0, var->var.aoffset, by_reference, regalloc_dwarf_register(var));
}

void debug_local(struct compile_process* process, struct node* var){
    if(!debug_active(process) || !var->var.name || (var->var.type.flags & DATATYPE_FLAG_IS_STATIC)
       || !debug_describable(&var->var.type)){
        return;
    }
    debug_variable(DEBUG_ABBREV_VARIABLE, var->var.name, &var->var.type, 0, var->var.aoffset, false, regalloc_dwarf_register(var));
}

// A static local lives in a global named `symbol`; it is described in
// its block under its C name, and debug_global leaves the symbol alone.
void debug_static_local(struct compile_process* process, struct node* var, const char* symbol){
    if(!debug_active(process)){
        return;
    }
    char* copy = strdup(symbol);
    vector_push(debug_static_symbols, &copy);
    if(!var->var.name || !debug_describable(&var->var.type)){
        return;
    }
    debug_variable(DEBUG_ABBREV_VARIABLE, var->var.name, &var->var.type, debug_symbol(symbol), 0, false, -1);
}

// A global this file defines. An extern one is described by the file
// that defines it; a thread-local one would need DW_OP_form_tls_address
// and is left out. One the parser made of a static local
// (`__static_<function>_<name>_<N>`) waits for its function.
void debug_global(struct compile_process* process, struct node* var){
    if(!debug_active(process) || !var->var.name){
        return;
    }
    int flags = var->var.type.flags;
    if((flags & (DATATYPE_FLAG_IS_EXTERN | DATATYPE_FLAG_IS_THREAD_LOCAL)) || !debug_describable(&var->var.type)){
        return;
    }
    if(strncmp(var->var.name, "__static_", 9) == 0){
        vector_push(debug_static_globals, &var);
        return;
    }
    for(int i = 0; i < vector_count(debug_static_symbols); i++){
        if(S_EQ(*(const char**)vector_at(debug_static_symbols, i), var->var.name)){
            return;
        }
    }
    debug_variable(DEBUG_ABBREV_VARIABLE, var->var.name, &var->var.type, debug_symbol(var->var.name), 0, false, -1);
}

// ---------------------------------------------------------------
// Lines
// ---------------------------------------------------------------

// The number of a file in the line table. GNU as is told with `.file`.
static int debug_file_number(const char* filename){
    for(int i = 0; i < vector_count(debug_files); i++){
        if(S_EQ(*(const char**)vector_at(debug_files, i), filename)){
            return i + 1;
        }
    }
    vector_push(debug_files, &filename);
    int number = vector_count(debug_files);
    if(!debug_nasm){
        size_t len = strlen(filename);
        char* quoted = malloc(len * 4 + 1);
        size_t n = 0;
        for(size_t i = 0; i < len; i++){
            unsigned char ch = (unsigned char)filename[i];
            if(ch < 32 || ch >= 127 || ch == '"' || ch == '\\'){
                n += sprintf(quoted + n, "\\%03o", ch);
            } else {
                quoted[n++] = ch;
            }
        }
        quoted[n] = 0;
        debug_out(".file %d \"%s\"", number, quoted);
        free(quoted);
    }
    return number;
}

// The code from here on is the line at `pos`.
static void debug_line(struct compile_process* process, struct pos* pos){
    if(!debug_active(process) || !pos || pos->line <= 0 || !pos->filename){
        return;
    }
    // The first row after the prologue says so: gdb then puts a
    // breakpoint on the function there instead of reading the prologue
    // (on x86_64 it does not know this one and would stop before it).
    bool prologue_end = debug_prologue_end;
    debug_prologue_end = false;
    int file = debug_file_number(pos->filename);
    if(!debug_nasm){
        debug_out(".loc %d %d%s", file, pos->line, prologue_end ? " prologue_end" : "");
        return;
    }
    if(!debug_function){
        return;
    }
    int label = debug_mark();
    unsigned char bytes[32];
    int n = 0;
    if(file != debug_row_file){
        bytes[n++] = DW_LNS_set_file;
        n += debug_uleb(bytes + n, file);
        debug_row_file = file;
    }
    bytes[n++] = 0;
    bytes[n++] = (unsigned char)(1 + debug_address_size);
    bytes[n++] = DW_LNE_set_address;
    debug_text_bytes(&debug_rows, bytes, n);
    debug_text_address(&debug_rows, debug_mark_address(label, debug_function));
    n = 0;
    bytes[n++] = DW_LNS_advance_line;
    n += debug_sleb(bytes + n, pos->line - debug_row_line);
    if(prologue_end){
        bytes[n++] = DW_LNS_set_prologue_end;
    }
    bytes[n++] = DW_LNS_copy;
    debug_text_bytes(&debug_rows, bytes, n);
    debug_row_line = pos->line;
}

static struct pos* debug_pos_of(struct node* node){
    return node && node->type != NODE_TYPE_BLANK && node->pos.line > 0 ? &node->pos : 0;
}

// A statement's line. The parser gives an if, for, while or switch
// node the position where it finished, after the body, so their row
// takes the line of the condition (of a for: the condition, else the
// initialization, else the step) instead. A do-while's code starts
// with its body and a `{ }` has no code of its own: the statements in
// them have their rows.
void debug_statement(struct compile_process* process, struct node* node){
    struct pos* pos = 0;
    switch(node->type){
        case NODE_TYPE_BODY:
        case NODE_TYPE_STATEMENT_DO_WHILE:
            return;
        case NODE_TYPE_STATEMENT_IF:
            pos = debug_pos_of(node->stmt.if_stmt.cond_node);
            break;
        case NODE_TYPE_STATEMENT_FOR:
            pos = debug_pos_of(node->stmt.for_stmt.cond_node);
            if(!pos){
                pos = debug_pos_of(node->stmt.for_stmt.init_node);
            }
            if(!pos){
                pos = debug_pos_of(node->stmt.for_stmt.loop_node);
            }
            break;
        case NODE_TYPE_STATEMENT_WHILE:
            pos = debug_pos_of(node->stmt.while_stmt.exp_node);
            break;
        case NODE_TYPE_STATEMENT_SWITCH:
            pos = debug_pos_of(node->stmt.switch_stmt.exp);
            break;
    }
    debug_line(process, pos ? pos : &node->pos);
}

// ---------------------------------------------------------------
// Call frame information
// ---------------------------------------------------------------

// One CFA rule from here on: a directive for GNU as; for NASM a mark
// and DW_CFA_set_loc to it, then the rule's bytes, in the function's FDE.
static void debug_cfi(struct compile_process* process, const char* directive, const unsigned char* bytes, int count){
    if(!debug_active(process) || !debug_function){
        return;
    }
    if(!debug_nasm){
        debug_out("%s", directive);
        return;
    }
    int label = debug_mark();
    unsigned char set_loc = DW_CFA_set_loc;
    debug_text_bytes(&debug_fde, &set_loc, 1);
    debug_text_address(&debug_fde, debug_mark_address(label, debug_function));
    debug_text_bytes(&debug_fde, bytes, count);
    debug_fde_size += 1 + debug_address_size + count;
}

void debug_cfi_def_cfa(struct compile_process* process, int reg, int offset){
    char directive[64];
    unsigned char bytes[16];
    int n = 0;
    snprintf(directive, sizeof(directive), ".cfi_def_cfa %d, %d", reg, offset);
    bytes[n++] = DW_CFA_def_cfa;
    n += debug_uleb(bytes + n, reg);
    n += debug_uleb(bytes + n, offset);
    debug_cfi(process, directive, bytes, n);
}

void debug_cfi_def_cfa_offset(struct compile_process* process, int offset){
    char directive[64];
    unsigned char bytes[16];
    int n = 0;
    snprintf(directive, sizeof(directive), ".cfi_def_cfa_offset %d", offset);
    bytes[n++] = DW_CFA_def_cfa_offset;
    n += debug_uleb(bytes + n, offset);
    debug_cfi(process, directive, bytes, n);
}

void debug_cfi_def_cfa_register(struct compile_process* process, int reg){
    char directive[64];
    unsigned char bytes[16];
    int n = 0;
    snprintf(directive, sizeof(directive), ".cfi_def_cfa_register %d", reg);
    bytes[n++] = DW_CFA_def_cfa_register;
    n += debug_uleb(bytes + n, reg);
    debug_cfi(process, directive, bytes, n);
}

// `reg` is saved at CFA + offset (offset < 0). The CIE's data alignment
// factor is minus the address size.
void debug_cfi_offset(struct compile_process* process, int reg, int offset){
    char directive[64];
    unsigned char bytes[16];
    int n = 0;
    snprintf(directive, sizeof(directive), ".cfi_offset %d, %d", reg, offset);
    bytes[n++] = (unsigned char)(DW_CFA_offset | reg);
    n += debug_uleb(bytes + n, -offset / (debug_address_size ? debug_address_size : 1));
    debug_cfi(process, directive, bytes, n);
}

void debug_cfi_register(struct compile_process* process, int reg, int in_reg){
    char directive[64];
    unsigned char bytes[16];
    int n = 0;
    snprintf(directive, sizeof(directive), ".cfi_register %d, %d", reg, in_reg);
    bytes[n++] = DW_CFA_register;
    n += debug_uleb(bytes + n, reg);
    n += debug_uleb(bytes + n, in_reg);
    debug_cfi(process, directive, bytes, n);
}

void debug_cfi_restore(struct compile_process* process, int reg){
    char directive[64];
    unsigned char byte = (unsigned char)(DW_CFA_restore | reg);
    snprintf(directive, sizeof(directive), ".cfi_restore %d", reg);
    debug_cfi(process, directive, &byte, 1);
}

// A return in the middle of a function: its epilogue's rules hold until
// the `ret`, the body's rules again after it.
void debug_cfi_remember_state(struct compile_process* process){
    unsigned char byte = DW_CFA_remember_state;
    debug_cfi(process, ".cfi_remember_state", &byte, 1);
}

void debug_cfi_restore_state(struct compile_process* process){
    unsigned char byte = DW_CFA_restore_state;
    debug_cfi(process, ".cfi_restore_state", &byte, 1);
}

// ---------------------------------------------------------------
// Functions and blocks
// ---------------------------------------------------------------

// The static locals of `function` (debug_global kept them): their
// C name is what follows `__static_<function>_` up to the last `_`. A
// longer function name that also matches owns the variable instead
// (`f` and `f_g`).
static bool debug_static_of(struct node* var, const char* function, const char** name, size_t* name_length){
    const char* symbol = var->var.name + 9;
    size_t length = strlen(function);
    if(strncmp(symbol, function, length) != 0 || symbol[length] != '_'){
        return false;
    }
    struct vector* tree = debug_process->node_tree_vec;
    for(int i = 0; i < vector_count(tree); i++){
        struct node* other = *(struct node**)vector_at(tree, i);
        if(other->type == NODE_TYPE_FUNCTION && strlen(other->func.name) > length
           && strncmp(symbol, other->func.name, strlen(other->func.name)) == 0 && symbol[strlen(other->func.name)] == '_'){
            return false;
        }
    }
    *name = symbol + length + 1;
    const char* last = strrchr(*name, '_');
    *name_length = last ? (size_t)(last - *name) : strlen(*name);
    return *name_length > 0;
}

static void debug_static_locals(const char* function){
    for(int i = 0; i < vector_count(debug_static_globals); i++){
        struct node* var = *(struct node**)vector_at(debug_static_globals, i);
        const char* name;
        size_t length;
        if(!debug_static_of(var, function, &name, &length)){
            continue;
        }
        char* c_name = malloc(length + 1);
        memcpy(c_name, name, length);
        c_name[length] = 0;
        debug_variable(DEBUG_ABBREV_VARIABLE, c_name, &var->var.type, debug_symbol(var->var.name), 0, false, -1);
        free(c_name);
    }
}

// Right after the function's label: the subprogram DIE, and the line
// of the declaration for the prologue.
void debug_function_begin(struct compile_process* process, struct node* function){
    if(!debug_active(process)){
        return;
    }
    debug_function = function->func.name;
    debug_function_end_label = debug_new_label();
    if(!debug_first_function){
        debug_first_function = debug_function;
    }
    if(!debug_nasm){
        debug_out(".cfi_startproc");
    }
    debug_text_clear(&debug_fde);
    debug_fde_size = 0;
    debug_line(process, &function->pos);
    debug_prologue_end = true;

    struct datatype* rtype = &function->func.rtype;
    bool is_void = datatype_is_void_no_ptr(rtype);
    debug_text_uleb(&debug_info, is_void ? DEBUG_ABBREV_SUBPROGRAM_VOID : DEBUG_ABBREV_SUBPROGRAM);
    debug_text_string(&debug_info, function->func.name);
    debug_text_byte(&debug_info, (rtype->flags & DATATYPE_FLAG_IS_STATIC) ? 0 : 1);
    if(!is_void){
        debug_text_ref(&debug_info, debug_type(rtype));
    }
    debug_text_address(&debug_info, debug_symbol(function->func.name));
    debug_text_printf(&debug_info, "%s %s", debug_dir(4), debug_mark_offset(debug_function_end_label, debug_function));
    unsigned char frame_base[3] = { 2, (unsigned char)(DW_OP_breg0 + debug_frame_register), 0 };
    debug_text_bytes(&debug_info, frame_base, 3);
    debug_static_locals(debug_function);
}

// After the function's last instruction.
void debug_function_end(struct compile_process* process){
    if(!debug_active(process) || !debug_function){
        return;
    }
    debug_mark_at(debug_function_end_label);
    if(!debug_nasm){
        debug_out(".cfi_endproc");
    } else {
        // The FDE: its length, the CIE's offset, the function's address
        // and size, its CFA program, padded with DW_CFA_nop to a whole
        // number of addresses.
        int start = debug_new_label();
        int end = debug_new_label();
        debug_text_printf(&debug_frames, "%s %s - %s", debug_dir(4), debug_label(end), debug_label(start));
        debug_text_printf(&debug_frames, "%s:", debug_label(start));
        debug_text_printf(&debug_frames, "%s %s", debug_dir(4), debug_label(debug_cie_label));
        debug_text_address(&debug_frames, debug_symbol(debug_function));
        debug_text_printf(&debug_frames, "%s %s", debug_dir(debug_address_size),
            debug_mark_offset(debug_function_end_label, debug_function));
        debug_text_append(&debug_frames, debug_fde.length ? debug_fde.data : "");
        int size = 4 + 4 + 2 * debug_address_size + debug_fde_size;
        while(size % debug_address_size){
            debug_text_byte(&debug_frames, 0);
            size++;
        }
        debug_text_printf(&debug_frames, "%s:", debug_label(end));
    }
    debug_text_byte(&debug_info, 0);
    debug_last_function = debug_function;
    debug_last_end_label = debug_function_end_label;
    debug_function = 0;
    debug_prologue_end = false;
}

// A `{ }`: a lexical block over its code, holding its variables.
void debug_block_begin(struct compile_process* process){
    if(!debug_active(process) || !debug_function){
        return;
    }
    int start = debug_mark();
    debug_new_label();
    vector_push(debug_blocks, &start);
    debug_text_uleb(&debug_info, DEBUG_ABBREV_BLOCK);
    debug_text_address(&debug_info, debug_mark_address(start, debug_function));
    debug_text_printf(&debug_info, "%s %s - %s", debug_dir(4), debug_label(start + 1), debug_label(start));
}

void debug_block_end(struct compile_process* process){
    if(!debug_active(process) || !debug_function || !vector_count(debug_blocks)){
        return;
    }
    int start = *(int*)vector_back(debug_blocks);
    vector_pop(debug_blocks);
    debug_mark_at(start + 1);
    debug_text_byte(&debug_info, 0);
}

// ---------------------------------------------------------------
// The sections
// ---------------------------------------------------------------

static void debug_write_bytes(const unsigned char* bytes, int count){
    struct debug_text text = {0};
    debug_text_bytes(&text, bytes, count);
    debug_out_text(&text);
    free(text.data);
}

// NASM: the line program (DWARF 3), each row a DW_LNE_set_address to
// a statement's mark, then its line; the sequence ends after the last
// function.
static void debug_write_line_program(void){
    int unit_start = debug_new_label();
    int unit_end = debug_new_label();
    int header_start = debug_new_label();
    int header_end = debug_new_label();
    debug_out("%s %s - %s", debug_dir(4), debug_label(unit_end), debug_label(unit_start));
    debug_out("%s:", debug_label(unit_start));
    debug_out("%s 3", debug_dir(2));
    debug_out("%s %s - %s", debug_dir(4), debug_label(header_end), debug_label(header_start));
    debug_out("%s:", debug_label(header_start));
    // minimum_instruction_length, default_is_stmt, line_base (-5),
    // line_range, opcode_base, the standard opcodes' operand counts,
    // and no include_directories.
    static const unsigned char header[] = { 1, 1, 0xfb, 14, 13, 0, 1, 1, 1, 1, 0, 0, 0, 1, 0, 0, 1, 0 };
    debug_write_bytes(header, sizeof(header));
    struct debug_text files = {0};
    for(int i = 0; i < vector_count(debug_files); i++){
        // Name, directory 0 (the compilation directory), no time or size.
        debug_text_string(&files, *(const char**)vector_at(debug_files, i));
        unsigned char rest[3] = { 0, 0, 0 };
        debug_text_bytes(&files, rest, 3);
    }
    debug_text_byte(&files, 0);
    debug_out_text(&files);
    free(files.data);
    debug_out("%s:", debug_label(header_end));
    debug_out_text(&debug_rows);
    if(debug_last_function){
        unsigned char set_address[3] = { 0, (unsigned char)(1 + debug_address_size), DW_LNE_set_address };
        debug_write_bytes(set_address, 3);
        debug_out("%s %s", debug_dir(debug_address_size), debug_mark_address(debug_last_end_label, debug_last_function));
    }
    unsigned char end_sequence[3] = { 0, 1, DW_LNE_end_sequence };
    debug_write_bytes(end_sequence, 3);
    debug_out("%s:", debug_label(unit_end));
}

// NASM: .debug_frame, the CIE (at entry the CFA is the stack pointer
// plus one address, the return address just below it), then the FDEs.
static void debug_write_frames(void){
    debug_section(".debug_frame");
    int start = debug_new_label();
    int end = debug_new_label();
    debug_out("%s:", debug_label(debug_cie_label));
    debug_out("%s %s - %s", debug_dir(4), debug_label(end), debug_label(start));
    debug_out("%s:", debug_label(start));
    unsigned char cie[32];
    int n = 0;
    cie[n++] = 0xff;
    cie[n++] = 0xff;
    cie[n++] = 0xff;
    cie[n++] = 0xff;
    cie[n++] = 1;                           // version
    cie[n++] = 0;                           // augmentation ""
    cie[n++] = 1;                           // code_alignment_factor
    n += debug_sleb(cie + n, -debug_address_size);
    cie[n++] = (unsigned char)debug_ra_register;
    cie[n++] = DW_CFA_def_cfa;
    cie[n++] = (unsigned char)debug_sp_register;
    cie[n++] = (unsigned char)debug_address_size;
    cie[n++] = (unsigned char)(DW_CFA_offset | debug_ra_register);
    cie[n++] = 1;
    while((4 + n) % debug_address_size){
        cie[n++] = 0;
    }
    debug_write_bytes(cie, n);
    debug_out("%s:", debug_label(end));
    debug_out_text(&debug_frames);
}

// At the end of the asm: .debug_abbrev, .debug_info, .debug_line
// (for GNU as only its first label, which DW_AT_stmt_list names; the
// assembler appends the line program) and for NASM .debug_frame.
void debug_end(struct compile_process* process){
    if(!debug_active(process)){
        return;
    }

    // Writing a type can name more types; the list grows as it goes.
    for(int i = 0; i < vector_count(debug_type_list); i++){
        debug_emit_type(*(struct debug_type**)vector_at(debug_type_list, i));
    }

    int abbrev = debug_new_label();
    int info_start = debug_new_label();
    int info_end = debug_new_label();
    int line = debug_new_label();
    debug_section(".debug_abbrev");
    debug_out("%s:", debug_label(abbrev));
    debug_write_bytes(debug_abbrevs, sizeof(debug_abbrevs));

    debug_section(".debug_info");
    debug_out("%s:", debug_label(debug_cu_label));
    debug_out("%s %s - %s", debug_dir(4), debug_label(info_end), debug_label(info_start));
    debug_out("%s:", debug_label(info_start));
    debug_out("%s 4", debug_dir(2));
    debug_out("%s %s", debug_dir(4), debug_label(abbrev));
    debug_out("%s %d", debug_dir(1), debug_address_size);
    char comp_dir[PATH_MAX];
    // compiler_real_path (helper.c) keeps the platform calls in one place.
    if(!compiler_real_path(".", comp_dir)){
        snprintf(comp_dir, sizeof(comp_dir), ".");
    }
    const char* name = process->cfile.abs_path;
    if(!name){
        name = vector_count(debug_files) ? *(const char**)vector_at(debug_files, 0) : "";
    }
    struct debug_text unit = {0};
    debug_text_uleb(&unit, DEBUG_ABBREV_COMPILE_UNIT);
    debug_text_string(&unit, "SamCompiler");
    debug_text_byte(&unit, DW_LANG_C99);
    debug_text_string(&unit, name);
    debug_text_string(&unit, comp_dir);
    // The code is the functions, from the first one's symbol to the
    // last one's end (the same section, so the difference is a number).
    if(debug_first_function){
        debug_text_address(&unit, debug_symbol(debug_first_function));
        debug_text_printf(&unit, "%s %s - %s", debug_dir(4),
            debug_mark_address(debug_last_end_label, debug_last_function), debug_symbol(debug_first_function));
    } else {
        debug_text_address(&unit, "0");
        debug_text_printf(&unit, "%s 0", debug_dir(4));
    }
    debug_text_printf(&unit, "%s %s", debug_dir(4), debug_label(line));
    debug_out_text(&unit);
    free(unit.data);
    debug_out_text(&debug_info);
    debug_out_text(&debug_types);
    debug_out("%s 0", debug_dir(1));
    debug_out("%s:", debug_label(info_end));

    debug_section(".debug_line");
    debug_out("%s:", debug_label(line));
    if(debug_nasm){
        debug_write_line_program();
        debug_write_frames();
    }
}
