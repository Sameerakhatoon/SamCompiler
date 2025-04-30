#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "compiler.h"

// Stage 2: a peephole pass over the asm text of
// one file under -O1. The backends write their asm as at -O0 (to a
// temporary file); this pass reads it back, rewrites short runs of
// instructions and writes the result to the real output. A rule only
// looks at neighbouring lines and never across a label or a directive,
// so control can only enter a run at its first line:
//   - push A; ...; pop B is a move done at the push (or nothing), when
//     the lines between neither touch B nor the stack (push/pop on x86
//     and x86_64, str/ldr through [sp] on aarch64, addi sp + sd and
//     ld + addi sp on riscv64);
//   - push x; compute x; move x to y; pop x computes into y, and so does
//     a computation followed by the backends' swap through a temporary;
//   - a value computed into a register only to be moved to another one
//     is computed there, and a constant moved into a register only to be
//     the operand of the next add, sub, compare, logic op or shift
//     becomes that instruction's immediate;
//   - an address computed into a register and only used by the next
//     load or store goes into that load or store;
//   - a value stored to a stack slot and loaded right back is reused,
//     except in a function with a volatile local (opt.c finds them);
//   - a jump to the label right after it goes, and so does the code
//     after an unconditional jump or a return, up to the next label.
// A register is dead when the code after it writes it before reading
// it. That code is scanned forward through labels (falling into a label
// adds no use); a jump, a branch, an instruction this file does not
// know or a directive count as a use, and so do a call and a return
// unless the register is a scratch one they never read. (a
// jump back to a loop's head is followed too.)

enum {
    PH_X86,
    PH_X86_64,
    PH_AARCH64,
    PH_RISCV64,
};

static int ph_arch;

#define PH_MAX_OPS 4
#define PH_OP_LEN  200
#define PH_TEXT    (PH_OP_LEN * 5 + 64)

struct ph_operand {
    char text[PH_OP_LEN];
};

struct ph_ins {
    char              mnem[32];
    int               count;
    struct ph_operand op[PH_MAX_OPS];
};

static char** ph_lines;
static int    ph_count;
static int    ph_capacity;
// Per line: whether it is in a function with a volatile local, and in
// one whose value fits one register (opt.c says).
static bool*  ph_volatile;
static bool*  ph_narrow_return;
static bool   ph_changed;

static bool ph_eq(const char* a, const char* b){
    return strcmp(a, b) == 0;
}

// ---------------------------------------------------------------
// Lines
// ---------------------------------------------------------------

static bool ph_is_comment(const char* line){
    switch(ph_arch){
        case PH_AARCH64: return line[0] == '/' && line[1] == '/';
        case PH_RISCV64: return line[0] == '#';
    }
    return line[0] == ';';
}

// `name:` alone.
static bool ph_is_label(const char* line){
    size_t len = strlen(line);
    return len > 1 && line[len - 1] == ':' && !strpbrk(line, " \t,");
}

static const char* ph_nasm_directives[] = {
    "section", "segment", "global", "extern", "default", "align", "alignb", "bits", "cpu", "common", "static",
    "db", "dw", "dd", "dq", "dt", "do", "dy", "dz", "times", "resb", "resw", "resd", "resq", "rest", "reso", "incbin",
    "equ", 0
};

static bool ph_first_word_is(const char* line, const char* word){
    size_t len = strlen(word);
    return strncmp(line, word, len) == 0 && (line[len] == 0 || line[len] == ' ' || line[len] == '\t');
}

static bool ph_is_instruction(const char* line){
    if(!line || !line[0] || ph_is_comment(line) || ph_is_label(line) || line[0] == '.' || line[0] == '%' || line[0] == '['){
        return false;
    }
    // `name: dd 0`, a label with data after it.
    const char* space = strpbrk(line, " \t");
    if(space && space > line && space[-1] == ':'){
        return false;
    }
    if(ph_arch == PH_X86 || ph_arch == PH_X86_64){
        for(int i = 0; ph_nasm_directives[i]; i++){
            if(ph_first_word_is(line, ph_nasm_directives[i])){
                return false;
            }
        }
    }
    return true;
}

// The next line that is still there, or -1.
static int ph_next(int i){
    if(i < 0){
        return -1;
    }
    for(i++; i < ph_count; i++){
        if(ph_lines[i]){
            return i;
        }
    }
    return -1;
}

static void ph_delete(int i){
    ph_lines[i] = 0;
    ph_changed = true;
}

static void ph_set(int i, const char* text){
    ph_lines[i] = strdup(text);
    ph_changed = true;
}

// A new line before line `at`.
static void ph_insert(int at, const char* text){
    if(ph_count + 1 >= ph_capacity){
        ph_capacity *= 2;
        ph_lines = realloc(ph_lines, sizeof(char*) * ph_capacity);
        ph_volatile = realloc(ph_volatile, sizeof(bool) * ph_capacity);
        ph_narrow_return = realloc(ph_narrow_return, sizeof(bool) * ph_capacity);
    }
    memmove(ph_lines + at + 1, ph_lines + at, sizeof(char*) * (ph_count - at));
    memmove(ph_volatile + at + 1, ph_volatile + at, sizeof(bool) * (ph_count - at));
    memmove(ph_narrow_return + at + 1, ph_narrow_return + at, sizeof(bool) * (ph_count - at));
    ph_count++;
    ph_lines[at] = strdup(text);
    ph_changed = true;
}

// ---------------------------------------------------------------
// Instructions
// ---------------------------------------------------------------

static void ph_trim_copy(char* out, const char* from, const char* to){
    while(from < to && (*from == ' ' || *from == '\t')){
        from++;
    }
    while(to > from && (to[-1] == ' ' || to[-1] == '\t')){
        to--;
    }
    size_t len = (size_t)(to - from);
    if(len >= PH_OP_LEN){
        len = PH_OP_LEN - 1;
    }
    memcpy(out, from, len);
    out[len] = 0;
}

// Mnemonic and operands (split at the commas outside brackets). False
// for anything that is not an instruction or does not fit.
static bool ph_parse(int i, struct ph_ins* ins){
    if(i < 0 || !ph_is_instruction(ph_lines[i])){
        return false;
    }
    const char* line = ph_lines[i];
    if(strlen(line) >= PH_OP_LEN){
        return false;
    }
    const char* p = line;
    size_t len = 0;
    while(*p && *p != ' ' && *p != '\t'){
        if(len + 1 >= sizeof(ins->mnem)){
            return false;
        }
        ins->mnem[len++] = *p++;
    }
    ins->mnem[len] = 0;
    ins->count = 0;
    while(*p == ' ' || *p == '\t'){
        p++;
    }
    if(!*p){
        return true;
    }
    int depth = 0;
    const char* start = p;
    for(;; p++){
        if(*p == '[' || *p == '(' || *p == '{'){
            depth++;
        } else if(*p == ']' || *p == ')' || *p == '}'){
            depth--;
        }
        if(!*p || (*p == ',' && depth == 0)){
            if(ins->count == PH_MAX_OPS){
                return false;
            }
            ph_trim_copy(ins->op[ins->count++].text, start, p);
            if(!*p){
                break;
            }
            start = p + 1;
        }
    }
    return true;
}

static char* ph_format(struct ph_ins* ins){
    static char text[PH_TEXT];
    int len = snprintf(text, sizeof(text), "%s", ins->mnem);
    for(int k = 0; k < ins->count && len < (int)sizeof(text); k++){
        len += snprintf(text + len, sizeof(text) - len, "%s%s", k ? ", " : " ", ins->op[k].text);
    }
    return text;
}

static bool ph_mnem_in(const char* mnem, const char** list){
    for(int i = 0; list[i]; i++){
        if(ph_eq(mnem, list[i])){
            return true;
        }
    }
    return false;
}

static bool ph_is_x86(void){
    return ph_arch == PH_X86 || ph_arch == PH_X86_64;
}

// ---------------------------------------------------------------
// Registers
// ---------------------------------------------------------------

// A register name: its family (all the names of one register) and its
// width (0 the full register, then narrower ones); -1 for anything else.
static const char* ph_x86_names[16][5] = {
    { "rax", "eax", "ax", "al", "ah" },     { "rcx", "ecx", "cx", "cl", "ch" },
    { "rdx", "edx", "dx", "dl", "dh" },     { "rbx", "ebx", "bx", "bl", "bh" },
    { "rsi", "esi", "si", "sil", 0 },       { "rdi", "edi", "di", "dil", 0 },
    { "rbp", "ebp", "bp", "bpl", 0 },       { "rsp", "esp", "sp", "spl", 0 },
    { "r8", "r8d", "r8w", "r8b", 0 },       { "r9", "r9d", "r9w", "r9b", 0 },
    { "r10", "r10d", "r10w", "r10b", 0 },   { "r11", "r11d", "r11w", "r11b", 0 },
    { "r12", "r12d", "r12w", "r12b", 0 },   { "r13", "r13d", "r13w", "r13b", 0 },
    { "r14", "r14d", "r14w", "r14b", 0 },   { "r15", "r15d", "r15w", "r15b", 0 },
};

#define PH_X86_RAX 0
#define PH_X86_RCX 1
#define PH_X86_RDX 2
#define PH_X86_RSI 4
#define PH_X86_RDI 5
#define PH_X86_RSP 7

// riscv64 integer registers by ABI name, in x-register order.
static const char* ph_riscv_names[32] = {
    "zero", "ra", "sp", "gp", "tp", "t0", "t1", "t2", "s0", "s1", "a0", "a1", "a2", "a3", "a4", "a5",
    "a6", "a7", "s2", "s3", "s4", "s5", "s6", "s7", "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6",
};

#define PH_A64_SP 31
#define PH_RV_SP  2
#define PH_RV_S0  8

static bool ph_is_number(const char* s){
    if(!*s){
        return false;
    }
    for(; *s; s++){
        if(*s < '0' || *s > '9'){
            return false;
        }
    }
    return true;
}

static int ph_family(const char* word, int* width){
    *width = 0;
    switch(ph_arch){
        case PH_X86:
        case PH_X86_64:
            for(int f = 0; f < 16; f++){
                for(int w = 0; w < 5; w++){
                    if(ph_x86_names[f][w] && ph_eq(word, ph_x86_names[f][w])){
                        *width = w;
                        return f;
                    }
                }
            }
            return -1;
        case PH_AARCH64:
            if(ph_eq(word, "sp") || ph_eq(word, "wsp")){
                *width = word[0] == 'w';
                return PH_A64_SP;
            }
            if(ph_eq(word, "fp")){
                return 29;
            }
            if(ph_eq(word, "lr")){
                return 30;
            }
            if((word[0] == 'x' || word[0] == 'w') && ph_is_number(word + 1)){
                int n = (int)strtol(word + 1, 0, 10);
                if(n <= 30){
                    *width = word[0] == 'w';
                    return n;
                }
            }
            // The floating registers, a family each above the integer ones.
            if(strchr("bhsdqv", word[0]) && ph_is_number(word + 1) && (int)strtol(word + 1, 0, 10) < 32){
                *width = 2;
                return 32 + (int)strtol(word + 1, 0, 10);
            }
            return -1;
        case PH_RISCV64:
            if(ph_eq(word, "fp")){
                return PH_RV_S0;
            }
            for(int f = 1; f < 32; f++){
                if(ph_eq(word, ph_riscv_names[f])){
                    return f;
                }
            }
            if(word[0] == 'f' && (word[1] == 't' || word[1] == 's' || word[1] == 'a') && ph_is_number(word + 2)){
                // ft0, fs0, fa0: the floating registers, one family each.
                return 32 + (word[1] == 'a' ? 64 : word[1] == 's' ? 32 : 0) + (int)strtol(word + 2, 0, 10);
            }
            return -1;
    }
    return -1;
}

static bool ph_is_word_char(char c){
    return c == '_' || c == '$' || c == '.' || (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

// Whether the text names a register of the family. A symbol spelled
// like a register counts too: unsure means used.
static bool ph_mentions(const char* text, int family){
    char word[PH_OP_LEN];
    const char* p = text;
    while(*p){
        if(!ph_is_word_char(*p)){
            p++;
            continue;
        }
        size_t len = 0;
        while(ph_is_word_char(*p)){
            word[len++] = *p++;
        }
        word[len] = 0;
        int width;
        if(ph_family(word, &width) == family){
            return true;
        }
    }
    return false;
}

static bool ph_ins_mentions(struct ph_ins* ins, int from, int family){
    for(int k = from; k < ins->count; k++){
        if(ph_mentions(ins->op[k].text, family)){
            return true;
        }
    }
    return false;
}

// The register an operand is, or -1.
static int ph_register(const char* text, int* width){
    return ph_family(text, width);
}

// The name of `family` at `width`, or NULL.
static const char* ph_register_name(int family, int width){
    static char name[8];
    switch(ph_arch){
        case PH_X86:
        case PH_X86_64:
            return family >= 0 && family < 16 && width < 5 ? ph_x86_names[family][width] : 0;
        case PH_AARCH64:
            if(family < 0 || family > 30 || width > 1){
                return 0;
            }
            snprintf(name, sizeof(name), "%c%i", width ? 'w' : 'x', family);
            return name;
        case PH_RISCV64:
            return family > 0 && family < 32 ? ph_riscv_names[family] : 0;
    }
    return 0;
}

// The width of a whole integer register: x86 has no 64-bit names.
static int ph_full_width(void){
    return ph_arch == PH_X86 ? 1 : 0;
}

static int ph_sp(void){
    switch(ph_arch){
        case PH_AARCH64: return PH_A64_SP;
        case PH_RISCV64: return PH_RV_SP;
    }
    return PH_X86_RSP;
}

// An operand that is a whole integer register (on x86_64 also its
// 32-bit name, whose writes clear the top half): its family, or -1.
static int ph_whole_register(const char* text){
    int width;
    int family = ph_register(text, &width);
    if(family < 0 || family == ph_sp()){
        return -1;
    }
    switch(ph_arch){
        case PH_X86: return width == 1 ? family : -1;
        case PH_X86_64: return width <= 1 ? family : -1;
        case PH_AARCH64: return family <= 30 && width <= 1 ? family : -1;
        case PH_RISCV64: return family < 32 ? family : -1;
    }
    return -1;
}

// The operand with every register of family `from` renamed to the same
// width of `to`. False when one has no such name.
static bool ph_rename_text(char* text, int from, int to){
    char out[PH_OP_LEN];
    char word[PH_OP_LEN];
    size_t len = 0;
    const char* p = text;
    while(*p){
        if(!ph_is_word_char(*p)){
            if(len + 1 >= sizeof(out)){
                return false;
            }
            out[len++] = *p++;
            continue;
        }
        size_t w = 0;
        while(ph_is_word_char(*p)){
            word[w++] = *p++;
        }
        word[w] = 0;
        int width;
        const char* name = word;
        if(ph_family(word, &width) == from){
            name = ph_register_name(to, width);
            if(!name){
                return false;
            }
        }
        size_t n = strlen(name);
        if(len + n + 1 >= sizeof(out)){
            return false;
        }
        memcpy(out + len, name, n);
        len += n;
    }
    out[len] = 0;
    strcpy(text, out);
    return true;
}

// ---------------------------------------------------------------
// What instructions do
// ---------------------------------------------------------------

// Instructions that write their first operand, read the others and do
// nothing else.
static const char* ph_x86_defs[] = { "mov", "movsx", "movzx", "movsxd", "lea", 0 };
static const char* ph_a64_defs[] = {
    "mov", "movz", "movn", "add", "sub", "mul", "sdiv", "udiv", "neg", "mvn", "and", "orr", "eor", "lsl", "lsr",
    "asr", "sxtw", "sxth", "sxtb", "uxtw", "uxth", "uxtb", "ldr", "ldrsw", "ldrb", "ldrh", "ldrsb", "ldrsh",
    "ldur", "ldursw", "ldurb", "ldurh", "ldursb", "ldursh", "adrp", "adr", "madd", "msub", 0
};
static const char* ph_rv_defs[] = {
    "li", "la", "lla", "mv", "add", "addi", "addw", "addiw", "sub", "subw", "mul", "mulw", "div", "divu", "divw",
    "divuw", "rem", "remu", "remw", "remuw", "and", "andi", "or", "ori", "xor", "xori", "sll", "slli", "sllw",
    "slliw", "srl", "srli", "srlw", "srliw", "sra", "srai", "sraw", "sraiw", "slt", "slti", "sltu", "sltiu",
    "seqz", "snez", "neg", "negw", "not", "sext.w", "lb", "lbu", "lh", "lhu", "lw", "lwu", "ld", "lui", 0
};

static bool ph_is_def(struct ph_ins* ins){
    switch(ph_arch){
        case PH_X86:
        case PH_X86_64: return ins->count == 2 && ph_mnem_in(ins->mnem, ph_x86_defs);
        case PH_AARCH64: return ins->count >= 2 && ph_mnem_in(ins->mnem, ph_a64_defs);
        case PH_RISCV64: return ins->count >= 2 && ph_mnem_in(ins->mnem, ph_rv_defs);
    }
    return false;
}

// `xor eax, eax`: a write that reads nothing.
static bool ph_is_zero_idiom(struct ph_ins* ins){
    return ph_is_x86() && ph_eq(ins->mnem, "xor") && ins->count == 2 && ph_eq(ins->op[0].text, ins->op[1].text);
}

static bool ph_is_call(struct ph_ins* ins){
    const char* m = ins->mnem;
    switch(ph_arch){
        case PH_X86:
        case PH_X86_64: return ph_eq(m, "call");
        case PH_AARCH64: return ph_eq(m, "bl") || ph_eq(m, "blr");
        case PH_RISCV64: return ph_eq(m, "call") || ph_eq(m, "jal") || ph_eq(m, "jalr");
    }
    return false;
}

// Jumps, branches, calls and returns.
static bool ph_is_control(struct ph_ins* ins){
    const char* m = ins->mnem;
    switch(ph_arch){
        case PH_X86:
        case PH_X86_64:
            return m[0] == 'j' || ph_eq(m, "call") || ph_eq(m, "ret") || strncmp(m, "loop", 4) == 0
                || ph_eq(m, "syscall") || ph_eq(m, "int") || ph_eq(m, "iret") || ph_eq(m, "iretq") || ph_eq(m, "hlt");
        case PH_AARCH64:
            return m[0] == 'b' || ph_eq(m, "cbz") || ph_eq(m, "cbnz") || ph_eq(m, "tbz") || ph_eq(m, "tbnz")
                || ph_eq(m, "ret") || ph_eq(m, "svc") || ph_eq(m, "eret");
        case PH_RISCV64:
            return m[0] == 'j' || m[0] == 'b' || ph_eq(m, "call") || ph_eq(m, "tail") || ph_eq(m, "ret")
                || ph_eq(m, "ecall") || ph_eq(m, "ebreak") || ph_eq(m, "mret") || ph_eq(m, "sret");
    }
    return true;
}

// Instructions that use or change a register they do not name: on x86
// the string instructions and rep (rcx, rsi, rdi), cpuid and the like,
// the sign extensions into rdx and the one-operand multiplies and
// divides (rax, rdx), and the stack instructions.
static bool ph_has_hidden_uses(struct ph_ins* ins, int family){
    if(!ph_is_x86()){
        return false;
    }
    const char* m = ins->mnem;
    if(ph_eq(m, "push") || ph_eq(m, "pop") || ph_eq(m, "leave") || ph_eq(m, "enter")){
        return family == PH_X86_RSP || family == 6;
    }
    // cmpxchg8b/16b read edx:eax and ecx:ebx (rbx is family 3) without
    // naming them; a `lock` line (the prefix parses as the mnemonic) may
    // be one. The copy-forward and dead-def rules dropped the
    // `mov ebx, [ebx]` that set the low half of the new value, and the
    // x86 atomics of 8-byte structs stored garbage at -O1.
    if(ph_eq(m, "lock") || ph_eq(m, "cmpxchg8b") || ph_eq(m, "cmpxchg16b")){
        if(family == PH_X86_RAX || family == 3 || family == PH_X86_RCX || family == PH_X86_RDX){
            return true;
        }
    }
    if(family != PH_X86_RAX && family != PH_X86_RCX && family != PH_X86_RDX && family != PH_X86_RSI && family != PH_X86_RDI){
        return false;
    }
    bool string = ins->count == 0 && (strncmp(m, "movs", 4) == 0 || strncmp(m, "stos", 4) == 0 || strncmp(m, "cmps", 4) == 0
                                      || strncmp(m, "scas", 4) == 0 || strncmp(m, "lods", 4) == 0);
    return string || strncmp(m, "rep", 3) == 0 || ph_eq(m, "cpuid") || ph_eq(m, "rdtsc") || ph_eq(m, "rdtscp")
        || ph_eq(m, "rdmsr") || ph_eq(m, "wrmsr") || ph_eq(m, "xgetbv") || ph_eq(m, "rdpmc") || ph_eq(m, "lock")
        || ph_eq(m, "cdq") || ph_eq(m, "cqo") || ph_eq(m, "cdqe") || ph_eq(m, "cwde") || ph_eq(m, "cwd")
        || ph_eq(m, "div") || ph_eq(m, "idiv") || ph_eq(m, "mul") || (ph_eq(m, "imul") && ins->count == 1);
}

// A scratch register a return passes no value in: dead at a `ret`.
static bool ph_dead_at_return(int family){
    switch(ph_arch){
        case PH_X86: return family == PH_X86_RCX;
        case PH_X86_64: return family == PH_X86_RCX || family == PH_X86_RSI || family == PH_X86_RDI || (family >= 8 && family <= 11);
        case PH_AARCH64: return family >= 2 && family <= 17 && family != 8;
        case PH_RISCV64: return (family >= 5 && family <= 7) || family >= 28 || (family >= 12 && family <= 17);
    }
    return false;
}

// The second return register (rdx, x1, a1), dead at the `ret` of a
// function that returns nothing wider than one register.
static bool ph_dead_at_narrow_return(int family){
    switch(ph_arch){
        case PH_X86:
        case PH_X86_64: return family == PH_X86_RDX;
        case PH_AARCH64: return family == 1;
        case PH_RISCV64: return family == 11;
    }
    return false;
}

// A scratch register no call reads (not an argument): dead at a call.
static bool ph_dead_at_call(int family){
    switch(ph_arch){
        case PH_X86_64: return family == 11;
        case PH_AARCH64: return family >= 9 && family <= 17;
        case PH_RISCV64: return (family >= 5 && family <= 7) || family >= 28;
    }
    return false;
}

// A write of the whole register that does not read it.
static bool ph_writes_whole(struct ph_ins* ins, int family){
    bool pop = ph_is_x86() && ph_eq(ins->mnem, "pop") && ins->count == 1;
    if(!(ph_is_def(ins) || ph_is_zero_idiom(ins) || pop) || ph_whole_register(ins->op[0].text) != family){
        return false;
    }
    return ph_is_zero_idiom(ins) || !ph_ins_mentions(ins, 1, family);
}

static bool ph_is_jump_mnem(const char* m);

// A conditional branch to a label: x86 jcc, aarch64 b.cc, cbz, cbnz,
// tbz and tbnz, riscv64 beq and the like.
static bool ph_is_branch(struct ph_ins* ins){
    const char* m = ins->mnem;
    switch(ph_arch){
        case PH_X86:
        case PH_X86_64: return m[0] == 'j' && !ph_eq(m, "jmp") && ins->count == 1;
        case PH_AARCH64: return strncmp(m, "b.", 2) == 0 || ph_eq(m, "cbz") || ph_eq(m, "cbnz") || ph_eq(m, "tbz") || ph_eq(m, "tbnz");
        case PH_RISCV64: return m[0] == 'b' && ins->count >= 2;
    }
    return false;
}

// The line of label `name:` after line i, or -1: only forward, so the
// scans below end.
static int ph_label_after(int i, const char* name){
    size_t len = strlen(name);
    for(int j = ph_next(i); j >= 0; j = ph_next(j)){
        const char* line = ph_lines[j];
        if(strncmp(line, name, len) == 0 && line[len] == ':' && line[len + 1] == 0){
            return j;
        }
    }
    return -1;
}

// the line of label `name:` up to PH_BACK_LINES lines before
// line i, or -1: a loop's backward jump.
#define PH_BACK_LINES 2000

static int ph_label_before(int i, const char* name){
    size_t len = strlen(name);
    for(int j = i - 1, n = 0; j >= 0 && n < PH_BACK_LINES; j--, n++){
        const char* line = ph_lines[j];
        if(line && strncmp(line, name, len) == 0 && line[len] == ':' && line[len + 1] == 0){
            return j;
        }
    }
    return -1;
}

// Whether the register's value may be used after line i. A jump or a
// branch is followed, forward or backward, a few levels
// deep; past that it counts as a use.
static bool ph_live_from(int i, int family, int depth){
    for(int j = ph_next(i); j >= 0; j = ph_next(j)){
        const char* line = ph_lines[j];
        if(ph_is_comment(line) || ph_is_label(line)){
            continue;
        }
        struct ph_ins ins;
        if(!ph_parse(j, &ins)){
            return true;
        }
        if(ph_writes_whole(&ins, family)){
            return false;
        }
        if(ph_ins_mentions(&ins, 0, family) || ph_has_hidden_uses(&ins, family)){
            return true;
        }
        if(ph_eq(ins.mnem, "ret")){
            return !ph_dead_at_return(family) && !(ph_narrow_return[j] && ph_dead_at_narrow_return(family));
        }
        if(ph_is_call(&ins)){
            return !ph_dead_at_call(family);
        }
        bool jump = ph_is_jump_mnem(ins.mnem) && ins.count == 1;
        if(jump || ph_is_branch(&ins)){
            const char* label = ins.op[ins.count - 1].text;
            int target = depth < 4 ? ph_label_after(j, label) : -1;
            if(target < 0 && depth < 4){
                target = ph_label_before(j, label);
            }
            if(target < 0 || ph_live_from(target, family, depth + 1)){
                return true;
            }
            if(jump){
                return false;
            }
            continue;
        }
        if(ph_is_control(&ins)){
            return true;
        }
    }
    return true;
}

static bool ph_live_after(int i, int family){
    return ph_live_from(i, family, 0);
}

// An instruction that may sit between a push and its pop: it does not
// touch the stack pointer, the stack or the flow.
static bool ph_stack_neutral(struct ph_ins* ins){
    if(ph_is_control(ins) || ph_has_hidden_uses(ins, ph_sp()) || ph_ins_mentions(ins, 0, ph_sp())){
        return false;
    }
    return !ph_is_x86() || !ph_has_hidden_uses(ins, PH_X86_RAX);
}

// A register-to-register move of whole registers: dst and src families.
static bool ph_is_move(struct ph_ins* ins, int* dst, int* src){
    bool mnem = ph_arch == PH_RISCV64 ? ph_eq(ins->mnem, "mv") : ph_eq(ins->mnem, "mov");
    if(!mnem || ins->count != 2){
        return false;
    }
    int wd, ws;
    *dst = ph_register(ins->op[0].text, &wd);
    *src = ph_register(ins->op[1].text, &ws);
    if(*dst < 0 || *src < 0 || *dst == ph_sp() || *src == ph_sp() || wd != ph_full_width() || ws != ph_full_width()){
        return false;
    }
    return ph_arch != PH_AARCH64 || (*dst <= 30 && *src <= 30);
}

static void ph_move_text(char* text, int dst, int src){
    const char* d = ph_register_name(dst, ph_full_width());
    char dname[8];
    snprintf(dname, sizeof(dname), "%s", d);
    snprintf(text, PH_TEXT, "%s %s, %s", ph_arch == PH_RISCV64 ? "mv" : "mov", dname, ph_register_name(src, ph_full_width()));
}

// ---------------------------------------------------------------
// Pushes and pops of the expression stack
// ---------------------------------------------------------------

// A push of one integer value starting at line i: the operand, and the
// push's last line.
static bool ph_push_at(int i, char* value, int* last){
    struct ph_ins a;
    if(!ph_parse(i, &a)){
        return false;
    }
    switch(ph_arch){
        case PH_X86:
        case PH_X86_64:
            if(!ph_eq(a.mnem, "push") || a.count != 1){
                return false;
            }
            strcpy(value, a.op[0].text);
            *last = i;
            return true;
        case PH_AARCH64:
            if(!ph_eq(a.mnem, "str") || a.count != 2 || !ph_eq(a.op[1].text, "[sp, #-16]!") || a.op[0].text[0] != 'x'){
                return false;
            }
            strcpy(value, a.op[0].text);
            *last = i;
            return true;
        case PH_RISCV64: {
            struct ph_ins b;
            int j = ph_next(i);
            if(!ph_eq(a.mnem, "addi") || a.count != 3 || !ph_eq(a.op[0].text, "sp") || !ph_eq(a.op[1].text, "sp")
               || !ph_eq(a.op[2].text, "-16") || !ph_parse(j, &b) || !ph_eq(b.mnem, "sd") || b.count != 2
               || !ph_eq(b.op[1].text, "0(sp)")){
                return false;
            }
            strcpy(value, b.op[0].text);
            *last = j;
            return true;
        }
    }
    return false;
}

// A pop into an integer register starting at line i: the register and
// the pop's last line.
static bool ph_pop_at(int i, char* reg, int* last){
    struct ph_ins a;
    if(!ph_parse(i, &a)){
        return false;
    }
    switch(ph_arch){
        case PH_X86:
        case PH_X86_64:
            if(!ph_eq(a.mnem, "pop") || a.count != 1){
                return false;
            }
            strcpy(reg, a.op[0].text);
            *last = i;
            return true;
        case PH_AARCH64:
            if(!ph_eq(a.mnem, "ldr") || a.count != 3 || !ph_eq(a.op[1].text, "[sp]") || !ph_eq(a.op[2].text, "#16")
               || a.op[0].text[0] != 'x'){
                return false;
            }
            strcpy(reg, a.op[0].text);
            *last = i;
            return true;
        case PH_RISCV64: {
            struct ph_ins b;
            int j = ph_next(i);
            if(!ph_eq(a.mnem, "ld") || a.count != 2 || !ph_eq(a.op[1].text, "0(sp)") || !ph_parse(j, &b)
               || !ph_eq(b.mnem, "addi") || b.count != 3 || !ph_eq(b.op[0].text, "sp") || !ph_eq(b.op[1].text, "sp")
               || !ph_eq(b.op[2].text, "16")){
                return false;
            }
            strcpy(reg, a.op[0].text);
            *last = j;
            return true;
        }
    }
    return false;
}

static void ph_delete_range(int first, int last){
    for(int k = first; k >= 0 && k <= last; k = ph_next(k)){
        ph_delete(k);
    }
}

// The instructions from line `first` to before `end` all write register
// `from` (each its first operand, whole), read it only after the first
// one, and touch neither `to` nor the stack pointer: rename `from` to
// `to` in them. Nothing is changed unless all of them qualify.
static bool ph_rename_run(int first, int end, int from, int to){
    struct ph_ins ins;
    int n = 0;
    for(int j = first; j >= 0 && j != end; j = ph_next(j)){
        if(!ph_parse(j, &ins) || ++n > 4){
            return false;
        }
        bool zero = ph_is_zero_idiom(&ins);
        if(!(ph_is_def(&ins) || zero) || ph_whole_register(ins.op[0].text) != from
           || ph_ins_mentions(&ins, 0, to) || ph_ins_mentions(&ins, 0, ph_sp())){
            return false;
        }
        // movk keeps the other bits of its register: it reads it.
        if(j == first && !zero && (ph_ins_mentions(&ins, 1, from) || ph_eq(ins.mnem, "movk"))){
            return false;
        }
    }
    if(!n || end < 0){
        return false;
    }
    for(int j = first; j != end; j = ph_next(j)){
        ph_parse(j, &ins);
        for(int k = 0; k < ins.count; k++){
            if(!ph_rename_text(ins.op[k].text, from, to)){
                return false;
            }
        }
        ph_set(j, ph_format(&ins));
    }
    return true;
}

// ---------------------------------------------------------------
// Rules for every target
// ---------------------------------------------------------------

static bool ph_is_jump_mnem(const char* m){
    switch(ph_arch){
        case PH_X86:
        case PH_X86_64: return ph_eq(m, "jmp");
        case PH_AARCH64: return ph_eq(m, "b");
        case PH_RISCV64: return ph_eq(m, "j");
    }
    return false;
}

// `jmp L` with L among the labels right after it.
static bool ph_jump_to_next(int i, struct ph_ins* ins){
    if(!ph_is_jump_mnem(ins->mnem) || ins->count != 1){
        return false;
    }
    char want[PH_OP_LEN + 2];
    snprintf(want, sizeof(want), "%s:", ins->op[0].text);
    for(int j = ph_next(i); j >= 0 && ph_is_label(ph_lines[j]); j = ph_next(j)){
        if(ph_eq(ph_lines[j], want)){
            ph_delete(i);
            return true;
        }
    }
    return false;
}

// Code after an unconditional jump or a return, up to the next label
// or directive, cannot run.
static bool ph_dead_after(int i, struct ph_ins* ins){
    bool ends = ph_eq(ins->mnem, "ret") || (ph_is_jump_mnem(ins->mnem) && ins->count == 1);
    if(!ends){
        return false;
    }
    bool any = false;
    for(int j = ph_next(i); j >= 0 && ph_is_instruction(ph_lines[j]); j = ph_next(j)){
        ph_delete(j);
        any = true;
    }
    return any;
}

// push A; S; pop B, where S leaves the stack and B alone: `mov B, A` in
// place of the push (nothing when A is B and S leaves it alone too).
static bool ph_push_pop(int i){
    char value[PH_OP_LEN];
    char reg[PH_OP_LEN];
    int push_last, pop_last;
    if(!ph_push_at(i, value, &push_last)){
        return false;
    }
    int k = ph_next(push_last);
    int between = 0;
    while(k >= 0 && !ph_pop_at(k, reg, &pop_last)){
        struct ph_ins c;
        if(++between > 4 || !ph_parse(k, &c) || !ph_stack_neutral(&c)){
            return false;
        }
        k = ph_next(k);
    }
    if(k < 0){
        return false;
    }
    int b = ph_whole_register(reg);
    int wa;
    int a = ph_register(value, &wa);
    if(b < 0 || ph_mentions(value, ph_sp())){
        return false;
    }
    // The lines between must not touch B (nor A, when A is B).
    for(int j = ph_next(push_last); j != k; j = ph_next(j)){
        if(ph_mentions(ph_lines[j], b)){
            return false;
        }
    }
    char text[PH_TEXT];
    if(a == b && ph_eq(value, reg)){
        ph_delete_range(i, push_last);
    } else if(a >= 0){
        if(wa != ph_full_width()){
            return false;
        }
        ph_move_text(text, b, a);
        ph_delete_range(ph_next(i), push_last);
        ph_set(i, text);
    } else if(ph_is_x86()){
        // An immediate or a memory operand, read at the push.
        snprintf(text, sizeof(text), "mov %s, %s", reg, value);
        ph_set(i, text);
    } else {
        return false;
    }
    ph_delete_range(k, pop_last);
    return true;
}

// push A; S (computes A); mov T, A; pop A -> S computing T.
static bool ph_push_compute_pop(int i){
    char value[PH_OP_LEN];
    char reg[PH_OP_LEN];
    int push_last, pop_last;
    if(!ph_push_at(i, value, &push_last)){
        return false;
    }
    int acc = ph_whole_register(value);
    int first = ph_next(push_last);
    int k = first;
    for(int n = 0; n < 5 && k >= 0 && acc >= 0; n++){
        struct ph_ins c;
        int dst, src;
        if(!ph_parse(k, &c)){
            return false;
        }
        if(ph_is_move(&c, &dst, &src) && src == acc){
            int p = ph_next(k);
            if(k != first && dst != acc && ph_pop_at(p, reg, &pop_last) && ph_eq(reg, value)
               && ph_rename_run(first, k, acc, dst)){
                ph_delete_range(i, push_last);
                ph_delete(k);
                ph_delete_range(p, pop_last);
                return true;
            }
            return false;
        }
        if(!ph_is_def(&c) && !ph_is_zero_idiom(&c)){
            return false;
        }
        k = ph_next(k);
    }
    return false;
}

// S (computes A); mov T, A; mov A, B; mov B, T -> mov A, B; S computing
// B (and mov T, B when T is still needed).
static bool ph_compute_swap(int i){
    struct ph_ins c;
    if(!ph_parse(i, &c) || !ph_is_def(&c)){
        return false;
    }
    int acc = ph_whole_register(c.op[0].text);
    if(acc < 0){
        return false;
    }
    int k = i;
    for(int n = 0; n < 5 && k >= 0; n++){
        int t, a2, b, a3, b3, t3;
        if(!ph_parse(k, &c)){
            return false;
        }
        if(ph_is_move(&c, &t, &a2) && a2 == acc && k != i){
            struct ph_ins m2, m3;
            int k2 = ph_next(k);
            int k3 = ph_next(k2);
            if(!ph_parse(k2, &m2) || !ph_parse(k3, &m3) || !ph_is_move(&m2, &a3, &b) || a3 != acc
               || !ph_is_move(&m3, &b3, &t3) || b3 != b || t3 != t || t == acc || b == acc || t == b){
                return false;
            }
            bool keep_t = ph_live_after(k3, t);
            if(!ph_rename_run(i, k, acc, b)){
                return false;
            }
            char text[PH_TEXT];
            if(keep_t){
                ph_move_text(text, t, b);
                ph_set(k, text);
            } else {
                ph_delete(k);
            }
            ph_delete(k2);
            ph_delete(k3);
            ph_move_text(text, acc, b);
            ph_insert(i, text);
            return true;
        }
        if(!ph_is_def(&c)){
            return false;
        }
        k = ph_next(k);
    }
    return false;
}

// def R; mov D, R -> def D, when R is dead after the move.
static bool ph_def_move(int i, struct ph_ins* a){
    int j = ph_next(i);
    struct ph_ins b;
    int d, r;
    if(!ph_is_def(a) || !ph_parse(j, &b) || !ph_is_move(&b, &d, &r)){
        return false;
    }
    if(ph_whole_register(a->op[0].text) != r || d == r || ph_ins_mentions(a, 0, d) || ph_ins_mentions(a, 1, r)
       || ph_eq(a->mnem, "movk") || ph_live_after(j, r)){
        return false;
    }
    if(!ph_rename_text(a->op[0].text, r, d)){
        return false;
    }
    ph_set(i, ph_format(a));
    ph_delete(j);
    return true;
}

// `mov rax, rax`, `mov x0, x0`, `mv a0, a0` (not the 32-bit forms, which
// clear the top half).
static bool ph_self_move(int i, struct ph_ins* a){
    int d, s;
    if(ph_is_move(a, &d, &s) && d == s && ph_eq(a->op[0].text, a->op[1].text) && ph_arch != PH_X86){
        ph_delete(i);
        return true;
    }
    return false;
}

// an instruction with an operand whose base register it
// writes back (aarch64 `[sp, #-16]!` and `[sp], #16`).
static bool ph_writes_back(struct ph_ins* ins){
    if(ph_arch != PH_AARCH64){
        return false;
    }
    for(int k = 0; k < ins->count; k++){
        if(strchr(ins->op[k].text, '!')){
            return true;
        }
    }
    return ins->count == 3 && ins->op[1].text[0] == '[';
}

static bool ph_has_memory(struct ph_ins* ins){
    for(int k = 0; k < ins->count; k++){
        if(strchr(ins->op[k].text, '[') || strchr(ins->op[k].text, '(')){
            return true;
        }
    }
    return false;
}

// def R, with R dead after it, does nothing (a value the
// stack machine worked out and nobody uses, such as the old value of a
// statement's x++). A load stays in a function with a volatile local.
static bool ph_dead_def(int i, struct ph_ins* a){
    if(!ph_is_def(a) || ph_writes_back(a) || (ph_volatile[i] && ph_has_memory(a))){
        return false;
    }
    int r = ph_whole_register(a->op[0].text);
    if(r < 0 || ph_live_after(i, r)){
        return false;
    }
    ph_delete(i);
    return true;
}

// Instructions whose first operand is only read.
static const char* ph_x86_reads_first[] = { "cmp", "test", "push", 0 };
static const char* ph_a64_reads_first[] = { "cmp", "cmn", "tst", "str", "strb", "strh", "stur", "sturb", "sturh",
                                            "cbz", "cbnz", "tbz", "tbnz", 0 };
static const char* ph_rv_reads_first[] = { "sb", "sh", "sw", "sd", "beq", "bne", "blt", "bge", "bltu", "bgeu",
                                           "bgt", "ble", "bgtu", "bleu", "beqz", "bnez", "bltz", "bgez", "blez", "bgtz", 0 };
// And some that write an operand after the first.
static const char* ph_writes_others[] = { "xchg", "xadd", "cmpxchg", "cmpxchg8b", "cmpxchg16b", "ldp", "ldpsw",
                                          "ldxp", "ldaxp", "stxp", "stlxp", 0 };

// mov A, B; S; mov B, A, where S neither names B nor jumps
// and A dies after: S working on B. A variable's register (B) copied out
// to be changed by the expression code and copied back.
static bool ph_move_run(int i, struct ph_ins* a){
    int a_reg, b_reg;
    if(!ph_is_move(a, &a_reg, &b_reg) || a_reg == b_reg || (ph_arch == PH_X86 && b_reg > 3)){
        return false;
    }
    int lines[8];
    struct ph_ins run[8];
    int n = 0;
    int j = ph_next(i);
    struct ph_ins c;
    while(ph_parse(j, &c)){
        int d, s;
        if(ph_is_move(&c, &d, &s) && d == b_reg && s == a_reg){
            break;
        }
        if(n == 8 || ph_is_control(&c) || ph_ins_mentions(&c, 0, b_reg) || ph_mentions(c.mnem, b_reg)
           || ph_has_hidden_uses(&c, a_reg) || ph_has_hidden_uses(&c, b_reg)){
            return false;
        }
        for(int k = 0; k < c.count; k++){
            if(!ph_rename_text(c.op[k].text, a_reg, b_reg)){
                return false;
            }
        }
        lines[n] = j;
        run[n++] = c;
        j = ph_next(j);
    }
    if(!n || !ph_parse(j, &c) || ph_live_after(j, a_reg)){
        return false;
    }
    for(int k = 0; k < n; k++){
        ph_set(lines[k], ph_format(&run[k]));
    }
    ph_delete(i);
    ph_delete(j);
    return true;
}

// mov A, B; I, where I reads A and A dies there -> I reading
// B. Mostly a variable's register (B) copied to the expression
// registers only to be read once.
static bool ph_copy_forward(int i, struct ph_ins* a){
    int dst, src;
    // On x86 only eax to ebx have byte names.
    if(!ph_is_move(a, &dst, &src) || dst == src || (ph_arch == PH_X86 && src > 3)){
        return false;
    }
    // I is the first instruction after it that names A, past up to three
    // that touch neither A nor B nor the flow.
    int j = ph_next(i);
    struct ph_ins b;
    for(int n = 0; ph_parse(j, &b) && !ph_ins_mentions(&b, 0, dst); n++){
        if(n == 3 || ph_is_control(&b) || ph_ins_mentions(&b, 0, src) || ph_has_hidden_uses(&b, dst) || ph_has_hidden_uses(&b, src)){
            return false;
        }
        j = ph_next(j);
    }
    if(!ph_parse(j, &b) || !ph_ins_mentions(&b, 0, dst) || ph_writes_back(&b) || ph_mnem_in(b.mnem, ph_writes_others)
       || ph_has_hidden_uses(&b, dst) || ph_has_hidden_uses(&b, src) || ph_mentions(b.mnem, dst)){
        return false;
    }
    if(ph_is_control(&b) && !ph_is_branch(&b)){
        return false;
    }
    const char** reads_first = ph_is_x86() ? ph_x86_reads_first : ph_arch == PH_AARCH64 ? ph_a64_reads_first : ph_rv_reads_first;
    int width;
    if(b.count > 0 && ph_register(b.op[0].text, &width) == dst && !ph_mnem_in(b.mnem, reads_first)){
        return false;
    }
    if(ph_live_after(j, dst)){
        return false;
    }
    for(int k = 0; k < b.count; k++){
        if(!ph_rename_text(b.op[k].text, dst, src)){
            return false;
        }
    }
    ph_set(j, ph_format(&b));
    ph_delete(i);
    return true;
}

static bool ph_parse_integer(const char* text, long long* value){
    const char* p = text;
    if(ph_is_x86() && strncmp(p, "dword ", 6) == 0){
        p += 6;
    }
    if(*p == '#'){
        p++;
    }
    char* end;
    *value = strtoll(p, &end, 0);
    return end != p && *end == 0;
}

// ---------------------------------------------------------------
// x86 and x86_64
// ---------------------------------------------------------------

static bool ph_x86_memory_plain(const char* address){
    return address[0] == '[' && !strstr(address, "fs:") && !strstr(address, "gs:") && !strstr(address, "tls");
}

// "dword [rbp-8]" -> size "dword", address "rbp-8". False when the
// operand is not memory.
static bool ph_x86_split_memory(const char* operand, char* size, char* address){
    const char* open = strchr(operand, '[');
    size_t len = strlen(operand);
    if(!open || operand[len - 1] != ']'){
        return false;
    }
    ph_trim_copy(size, operand, open);
    ph_trim_copy(address, open + 1, operand + len - 1);
    return true;
}

static bool ph_x86_frame_slot(const char* address){
    return (strncmp(address, "rbp", 3) == 0 || strncmp(address, "ebp", 3) == 0)
        && (address[3] == '+' || address[3] == '-') && ph_is_number(address + 4);
}

static const char* ph_x86_imm_ops[] = { "add", "sub", "and", "or", "xor", "cmp", 0 };
static const char* ph_x86_shifts[] = { "shl", "shr", "sar", "sal", 0 };

static bool ph_x86_rules(int i, struct ph_ins* a){
    int j = ph_next(i);
    struct ph_ins b;
    if(!ph_parse(j, &b)){
        return false;
    }
    int wa, wb;
    int sp = PH_X86_RSP;
    char size[PH_OP_LEN];
    char inner[PH_OP_LEN];
    char text[PH_TEXT];

    // lea R, [M]; mov|movsx|movzx|movsxd R', size [R] -> ... [M].
    if(ph_eq(a->mnem, "lea") && a->count == 2 && b.count == 2 && ph_mnem_in(b.mnem, ph_x86_defs) && !ph_eq(b.mnem, "lea")){
        int fa = ph_register(a->op[0].text, &wa);
        int fd = ph_register(b.op[0].text, &wb);
        if(fa >= 0 && wa <= 1 && fd == fa && ph_x86_split_memory(b.op[1].text, size, inner) && ph_register(inner, &wb) == fa
           && ph_x86_memory_plain(a->op[1].text)){
            snprintf(text, sizeof(text), "%s %s, %s%s%s", b.mnem, b.op[0].text, size, size[0] ? " " : "", a->op[1].text);
            ph_set(j, text);
            ph_delete(i);
            return true;
        }
    }
    // lea R, [M]; mov size [R], V -> mov size [M], V when R dies there.
    if(ph_eq(a->mnem, "lea") && a->count == 2 && ph_eq(b.mnem, "mov") && b.count == 2 && ph_x86_memory_plain(a->op[1].text)){
        int fa = ph_register(a->op[0].text, &wa);
        if(fa >= 0 && wa <= 1 && ph_x86_split_memory(b.op[0].text, size, inner) && ph_register(inner, &wb) == fa
           && !ph_mentions(b.op[1].text, fa) && !ph_live_after(j, fa)){
            snprintf(text, sizeof(text), "mov %s%s%s, %s", size, size[0] ? " " : "", a->op[1].text, b.op[1].text);
            ph_set(j, text);
            ph_delete(i);
            return true;
        }
    }

    // mov rcx, rax; <compute rax>; xchg rax, rcx -> <compute rcx>.
    if(ph_eq(a->mnem, "mov") && a->count == 2){
        int to = ph_register(a->op[0].text, &wa);
        int acc = ph_register(a->op[1].text, &wb);
        if(to >= 0 && acc >= 0 && wa == ph_full_width() && wb == ph_full_width() && to != acc && to != sp && acc != sp){
            int k = j;
            for(int n = 0; n < 5 && k >= 0; n++){
                struct ph_ins c;
                if(!ph_parse(k, &c)){
                    break;
                }
                if(ph_eq(c.mnem, "xchg") && c.count == 2){
                    bool same = (ph_eq(c.op[0].text, a->op[1].text) && ph_eq(c.op[1].text, a->op[0].text))
                             || (ph_eq(c.op[0].text, a->op[0].text) && ph_eq(c.op[1].text, a->op[1].text));
                    if(same && k != j && ph_rename_run(j, k, acc, to)){
                        ph_delete(i);
                        ph_delete(k);
                        return true;
                    }
                    break;
                }
                if(!ph_is_def(&c) && !ph_is_zero_idiom(&c)){
                    break;
                }
                k = ph_next(k);
            }
        }
    }

    // def B; def A; xchg A, B -> def A; def B.
    if(ph_is_def(a) && ph_is_def(&b)){
        struct ph_ins c;
        int k = ph_next(j);
        int fb = ph_whole_register(a->op[0].text);
        int fa = ph_whole_register(b.op[0].text);
        if(fa >= 0 && fb >= 0 && fa != fb && ph_parse(k, &c) && ph_eq(c.mnem, "xchg") && c.count == 2
           && ph_whole_register(c.op[0].text) >= 0 && ph_whole_register(c.op[1].text) >= 0
           && ((ph_whole_register(c.op[0].text) == fa && ph_whole_register(c.op[1].text) == fb)
               || (ph_whole_register(c.op[0].text) == fb && ph_whole_register(c.op[1].text) == fa))
           && !ph_ins_mentions(a, 0, fa) && !ph_ins_mentions(a, 1, fb) && !ph_ins_mentions(&b, 0, fb) && !ph_ins_mentions(&b, 1, fa)
           && ph_rename_text(a->op[0].text, fb, fa) && ph_rename_text(b.op[0].text, fa, fb)){
            ph_set(i, ph_format(a));
            ph_set(j, ph_format(&b));
            ph_delete(k);
            return true;
        }
    }

    // mov R, imm; op X, R -> op X, imm when R dies there.
    long long imm;
    if(ph_eq(a->mnem, "mov") && a->count == 2 && ph_parse_integer(a->op[1].text, &imm) && b.count == 2
       && imm >= -2147483647LL - 1 && imm <= 2147483647LL){
        int r = ph_whole_register(a->op[0].text);
        int wr;
        ph_register(a->op[0].text, &wr);
        int fx = ph_register(b.op[0].text, &wa);
        int fo = ph_register(b.op[1].text, &wb);
        // `mov ecx, -1` leaves 0xffffffff in rcx, which no 64-bit
        // immediate form gives.
        if(ph_arch == PH_X86_64 && wr == 1 && wb == 0 && imm < 0){
            fo = -1;
        }
        bool op = ph_mnem_in(b.mnem, ph_x86_imm_ops) && wb <= 1;
        bool shift = ph_mnem_in(b.mnem, ph_x86_shifts) && wb == 3 && imm >= 0 && imm <= 255;
        if(r >= 0 && fo == r && fx != r && !ph_mentions(b.op[0].text, r) && (op || shift) && !ph_live_after(j, r)){
            snprintf(text, sizeof(text), "%s %s, %lld", b.mnem, b.op[0].text, imm);
            ph_set(j, text);
            ph_delete(i);
            return true;
        }
    }

    // A store to a stack slot and a load of it right after.
    if(!ph_volatile[i] && ph_eq(a->mnem, "mov") && a->count == 2 && b.count >= 1){
        char slot[PH_OP_LEN];
        int fv = ph_register(a->op[1].text, &wa);
        if(fv >= 0 && fv != sp && ph_x86_split_memory(a->op[0].text, size, slot) && ph_x86_frame_slot(slot)){
            // push dword [slot] -> push V.
            if(ph_eq(b.mnem, "push") && b.count == 1 && ph_eq(b.op[0].text, a->op[0].text) && wa == ph_full_width()
               && ph_eq(size, ph_arch == PH_X86 ? "dword" : "qword")){
                snprintf(text, sizeof(text), "push %s", a->op[1].text);
                ph_set(j, text);
                return true;
            }
            if(b.count == 2 && ph_eq(b.op[1].text, a->op[0].text)){
                int fd = ph_register(b.op[0].text, &wb);
                text[0] = 0;
                if(fd >= 0 && ph_eq(b.mnem, "mov") && wb == wa){
                    if(fd == fv){
                        ph_delete(j);
                        return true;
                    }
                    snprintf(text, sizeof(text), "mov %s, %s", b.op[0].text, a->op[1].text);
                } else if(fd >= 0 && ph_eq(b.mnem, "movsxd") && wa == 1){
                    snprintf(text, sizeof(text), "movsxd %s, %s", b.op[0].text, a->op[1].text);
                } else if(fd >= 0 && (ph_eq(b.mnem, "movsx") || ph_eq(b.mnem, "movzx")) && wa >= 2 && wa <= 3){
                    snprintf(text, sizeof(text), "%s %s, %s", b.mnem, b.op[0].text, a->op[1].text);
                }
                if(text[0]){
                    ph_set(j, text);
                    return true;
                }
            }
        }
    }
    return false;
}

// ---------------------------------------------------------------
// aarch64
// ---------------------------------------------------------------

// "[x29, #-8]" -> base family 29, offset -8. False for anything else.
static bool ph_a64_slot(const char* operand, int* base, long* offset){
    char inner[PH_OP_LEN];
    size_t len = strlen(operand);
    if(operand[0] != '[' || operand[len - 1] != ']'){
        return false;
    }
    ph_trim_copy(inner, operand + 1, operand + len - 1);
    char* comma = strchr(inner, ',');
    *offset = 0;
    if(comma){
        *comma = 0;
        const char* imm = comma + 1;
        while(*imm == ' '){
            imm++;
        }
        if(*imm != '#'){
            return false;
        }
        char* end;
        *offset = strtol(imm + 1, &end, 10);
        if(*end){
            return false;
        }
    }
    int width;
    *base = ph_register(inner, &width);
    return *base >= 0 && width == 0;
}

static const char* ph_a64_loads[] = { "ldr", "ldrsw", "ldrb", "ldrh", "ldrsb", "ldrsh", 0 };
static const char* ph_a64_stores[] = { "str", "strb", "strh", 0 };

static bool ph_a64_rules(int i, struct ph_ins* a){
    int j = ph_next(i);
    struct ph_ins b;
    if(!ph_parse(j, &b)){
        return false;
    }
    int wa, wb;
    int sp = PH_A64_SP;
    char text[PH_TEXT];

    // str d0, [sp, #-16]!; ldr d1, [sp], #16 -> fmov d1, d0.
    if(ph_eq(a->mnem, "str") && a->count == 2 && ph_eq(a->op[1].text, "[sp, #-16]!")
       && ph_eq(b.mnem, "ldr") && b.count == 3 && ph_eq(b.op[1].text, "[sp]") && ph_eq(b.op[2].text, "#16")){
        const char* from = a->op[0].text;
        const char* to = b.op[0].text;
        if(from[0] == to[0] && (from[0] == 'd' || from[0] == 's')){
            if(ph_eq(from, to)){
                ph_delete(i);
            } else {
                snprintf(text, sizeof(text), "fmov %s, %s", to, from);
                ph_set(i, text);
            }
            ph_delete(j);
            return true;
        }
    }

    // add|sub R, x29|sp, #N; ldr|str V, [R] -> ldr|str V, [x29, #+-N].
    if((ph_eq(a->mnem, "add") || ph_eq(a->mnem, "sub")) && a->count == 3 && a->op[2].text[0] == '#'
       && b.count == 2 && (ph_mnem_in(b.mnem, ph_a64_loads) || ph_mnem_in(b.mnem, ph_a64_stores))){
        int r = ph_register(a->op[0].text, &wa);
        int base = ph_register(a->op[1].text, &wb);
        char* end;
        long n = strtol(a->op[2].text + 1, &end, 10);
        int inner_base;
        long inner_offset;
        if(*end == 0 && r >= 0 && r <= 30 && wa == 0 && (base == 29 || base == sp) && wb == 0
           && ph_a64_slot(b.op[1].text, &inner_base, &inner_offset) && inner_base == r && inner_offset == 0){
            long offset = ph_eq(a->mnem, "sub") ? -n : n;
            int wv;
            int fv = ph_register(b.op[0].text, &wv);
            bool load = ph_mnem_in(b.mnem, ph_a64_loads);
            // The unscaled form takes -256..255 for any size.
            if(offset >= -256 && offset <= 255 && fv >= 0
               && ((load && fv == r) || (fv != r && !ph_live_after(j, r)))){
                snprintf(text, sizeof(text), "%s %s, [%s, #%li]", b.mnem, b.op[0].text, a->op[1].text, offset);
                ph_set(j, text);
                ph_delete(i);
                return true;
            }
        }
    }

    // mov R, #N; add|sub|cmp ..., R -> ..., #N when R dies there.
    long long imm;
    if(ph_eq(a->mnem, "mov") && a->count == 2 && ph_parse_integer(a->op[1].text, &imm) && imm > -4096 && imm < 4096){
        int r = ph_whole_register(a->op[0].text);
        bool arith = (ph_eq(b.mnem, "add") || ph_eq(b.mnem, "sub")) && b.count == 3;
        bool cmp = ph_eq(b.mnem, "cmp") && b.count == 2;
        int last = arith ? 2 : 1;
        int wl;
        int wr;
        ph_register(a->op[0].text, &wr);
        // `mov w1, #-1` leaves 0xffffffff in x1, which no 64-bit
        // immediate form gives.
        bool widened = wr == 1 && imm < 0;
        if(r >= 0 && (arith || cmp) && ph_register(b.op[last].text, &wl) == r && !(widened && wl == 0)
           && (arith ? !ph_mentions(b.op[1].text, r) : !ph_mentions(b.op[0].text, r))){
            int fd = arith ? ph_register(b.op[0].text, &wb) : -1;
            if(fd == r || !ph_live_after(j, r)){
                const char* m = b.mnem;
                long long n = imm;
                if(n < 0){
                    m = ph_eq(m, "add") ? "sub" : ph_eq(m, "sub") ? "add" : "cmn";
                    n = -n;
                }
                if(arith){
                    snprintf(text, sizeof(text), "%s %s, %s, #%lld", m, b.op[0].text, b.op[1].text, n);
                } else {
                    snprintf(text, sizeof(text), "%s %s, #%lld", m, b.op[0].text, n);
                }
                ph_set(j, text);
                ph_delete(i);
                return true;
            }
        }
    }

    // str V, [x29, #N]; ldr D, [x29, #N] -> reuse V.
    if(!ph_volatile[i] && ph_eq(a->mnem, "str") && a->count == 2 && b.count == 2 && ph_eq(a->op[1].text, b.op[1].text)){
        int base;
        long offset;
        int fv = ph_register(a->op[0].text, &wa);
        int fd = ph_register(b.op[0].text, &wb);
        if(fv >= 0 && fv < 31 && fd >= 0 && fd < 31 && ph_a64_slot(a->op[1].text, &base, &offset) && base == 29){
            text[0] = 0;
            if(ph_eq(b.mnem, "ldr") && wa == wb){
                if(fv == fd && wa == 0){
                    ph_delete(j);
                    return true;
                }
                // A 32-bit reload also clears the top of the register.
                snprintf(text, sizeof(text), "mov %s, %s", b.op[0].text, a->op[0].text);
            } else if(ph_eq(b.mnem, "ldrsw") && wa == 1 && wb == 0){
                snprintf(text, sizeof(text), "sxtw %s, %s", b.op[0].text, a->op[0].text);
            }
            if(text[0]){
                ph_set(j, text);
                return true;
            }
        }
    }
    return false;
}

// ---------------------------------------------------------------
// riscv64
// ---------------------------------------------------------------

// "-8(s0)" -> base s0, offset -8.
static bool ph_rv_slot(const char* operand, int* base, long* offset){
    char* end;
    *offset = strtol(operand, &end, 10);
    if(end == operand || *end != '('){
        return false;
    }
    char inner[PH_OP_LEN];
    size_t len = strlen(end);
    if(end[len - 1] != ')'){
        return false;
    }
    ph_trim_copy(inner, end + 1, end + len - 1);
    int width;
    *base = ph_register(inner, &width);
    return *base > 0;
}

static const char* ph_rv_loads[] = { "lb", "lbu", "lh", "lhu", "lw", "lwu", "ld", 0 };
static const char* ph_rv_stores[] = { "sb", "sh", "sw", "sd", 0 };

// Register-register operations and their immediate forms (a sub becomes
// an add of the negated constant); shifts take 0..63 (0..31 for the w
// forms).
static const char* ph_rv_reg_ops[] = { "add", "addw", "and", "or", "xor", "slt", "sltu", "sub", "subw",
                                       "sll", "sllw", "srl", "srlw", "sra", "sraw", 0 };
static const char* ph_rv_imm_ops[] = { "addi", "addiw", "andi", "ori", "xori", "slti", "sltiu", "addi", "addiw",
                                       "slli", "slliw", "srli", "srliw", "srai", "sraiw", 0 };

static bool ph_rv_rules(int i, struct ph_ins* a){
    struct ph_ins b;
    int j = ph_next(i);
    int wa, wb;
    char text[PH_TEXT];

    // addi sp, sp, -16; fsd fA, 0(sp); fld fB, 0(sp); addi sp, sp, 16
    // -> fmv.d fB, fA.
    if(ph_eq(a->mnem, "addi") && a->count == 3 && ph_eq(a->op[0].text, "sp") && ph_eq(a->op[1].text, "sp")
       && ph_eq(a->op[2].text, "-16") && ph_parse(j, &b) && ph_eq(b.mnem, "fsd") && b.count == 2 && ph_eq(b.op[1].text, "0(sp)")){
        struct ph_ins c, d;
        int k = ph_next(j);
        int l = ph_next(k);
        if(ph_parse(k, &c) && ph_eq(c.mnem, "fld") && c.count == 2 && ph_eq(c.op[1].text, "0(sp)")
           && ph_parse(l, &d) && ph_eq(d.mnem, "addi") && d.count == 3 && ph_eq(d.op[0].text, "sp")
           && ph_eq(d.op[1].text, "sp") && ph_eq(d.op[2].text, "16")){
            if(ph_eq(b.op[0].text, c.op[0].text)){
                ph_delete(i);
            } else {
                snprintf(text, sizeof(text), "fmv.d %s, %s", c.op[0].text, b.op[0].text);
                ph_set(i, text);
            }
            ph_delete(j);
            ph_delete(k);
            ph_delete(l);
            return true;
        }
    }

    if(!ph_parse(j, &b)){
        return false;
    }

    // addi R, BASE, N; load|store V, 0(R) -> load|store V, N(BASE).
    if(ph_eq(a->mnem, "addi") && a->count == 3 && b.count == 2
       && (ph_mnem_in(b.mnem, ph_rv_loads) || ph_mnem_in(b.mnem, ph_rv_stores))){
        int r = ph_register(a->op[0].text, &wa);
        int base = ph_register(a->op[1].text, &wb);
        char* end;
        long n = strtol(a->op[2].text, &end, 10);
        int inner_base;
        long inner_offset;
        if(*end == 0 && end != a->op[2].text && r > 0 && r < 32 && base > 0 && r != base && n >= -2048 && n <= 2047
           && ph_rv_slot(b.op[1].text, &inner_base, &inner_offset) && inner_base == r && inner_offset == 0){
            int fv = ph_register(b.op[0].text, &wb);
            bool load = ph_mnem_in(b.mnem, ph_rv_loads);
            if(fv > 0 && ((load && fv == r) || (fv != r && !ph_live_after(j, r)))){
                snprintf(text, sizeof(text), "%s %s, %li(%s)", b.mnem, b.op[0].text, n, a->op[1].text);
                ph_set(j, text);
                ph_delete(i);
                return true;
            }
        }
    }

    // li R, N; op D, S, R -> opi D, S, N when R dies there.
    long long imm;
    if(ph_eq(a->mnem, "li") && a->count == 2 && ph_parse_integer(a->op[1].text, &imm) && b.count == 3){
        int r = ph_whole_register(a->op[0].text);
        int op = -1;
        for(int k = 0; ph_rv_reg_ops[k]; k++){
            if(ph_eq(b.mnem, ph_rv_reg_ops[k])){
                op = k;
            }
        }
        int s1 = ph_register(b.op[1].text, &wa);
        int s2 = ph_register(b.op[2].text, &wb);
        int fd = ph_register(b.op[0].text, &wb);
        const char* source = 0;
        bool commutes = op >= 0 && op <= 4;
        if(r > 0 && op >= 0 && s2 == r && s1 != r){
            source = b.op[1].text;
        } else if(r > 0 && commutes && s1 == r && s2 != r){
            source = b.op[2].text;
        }
        bool is_sub = op == 7 || op == 8;
        bool is_shift = op >= 9;
        bool is_w = op >= 0 && b.mnem[strlen(b.mnem) - 1] == 'w';
        long long n = is_sub ? -imm : imm;
        bool fits = is_shift ? (n >= 0 && n <= (is_w ? 31 : 63)) : (n >= -2048 && n <= 2047);
        if(source && fits && (fd == r || !ph_live_after(j, r))){
            snprintf(text, sizeof(text), "%s %s, %s, %lld", ph_rv_imm_ops[op], b.op[0].text, source, n);
            ph_set(j, text);
            ph_delete(i);
            return true;
        }
    }

    // sw|sd V, N(s0); lw|ld D, N(s0) -> reuse V.
    if(!ph_volatile[i] && a->count == 2 && b.count == 2 && ph_eq(a->op[1].text, b.op[1].text)
       && ((ph_eq(a->mnem, "sw") && ph_eq(b.mnem, "lw")) || (ph_eq(a->mnem, "sd") && ph_eq(b.mnem, "ld")))){
        int base;
        long offset;
        int fv = ph_register(a->op[0].text, &wa);
        int fd = ph_register(b.op[0].text, &wb);
        if(fv > 0 && fv < 32 && fd > 0 && fd < 32 && ph_rv_slot(a->op[1].text, &base, &offset) && base == PH_RV_S0){
            if(ph_eq(b.mnem, "lw")){
                snprintf(text, sizeof(text), "sext.w %s, %s", b.op[0].text, a->op[0].text);
            } else if(fv == fd){
                ph_delete(j);
                return true;
            } else {
                snprintf(text, sizeof(text), "mv %s, %s", b.op[0].text, a->op[0].text);
            }
            ph_set(j, text);
            return true;
        }
    }
    return false;
}

// ---------------------------------------------------------------
// Driver
// ---------------------------------------------------------------

static void ph_split(char* text){
    ph_capacity = 1024;
    ph_lines = malloc(sizeof(char*) * ph_capacity);
    ph_count = 0;
    char* line = text;
    while(*line){
        char* end = strchr(line, '\n');
        if(end){
            *end = 0;
        }
        if(ph_count + 1 >= ph_capacity){
            ph_capacity *= 2;
            ph_lines = realloc(ph_lines, sizeof(char*) * ph_capacity);
        }
        ph_lines[ph_count++] = line;
        if(!end){
            break;
        }
        line = end + 1;
    }
}

// Which lines belong to a function with a volatile local, or to one
// that returns a single register: the backends start each function with
// a `<comment> NAME function` line.
static void ph_mark_functions(void){
    ph_volatile = calloc(ph_capacity, sizeof(bool));
    ph_narrow_return = calloc(ph_capacity, sizeof(bool));
    bool current = false;
    bool narrow = false;
    for(int i = 0; i < ph_count; i++){
        const char* line = ph_lines[i];
        if(ph_is_comment(line)){
            const char* name = line + (ph_arch == PH_AARCH64 ? 2 : 1);
            while(*name == ' '){
                name++;
            }
            const char* space = strchr(name, ' ');
            if(space && ph_eq(space, " function")){
                char function[PH_OP_LEN];
                ph_trim_copy(function, name, space);
                current = opt_function_has_volatile(function);
                narrow = opt_function_returns_narrow(function);
            }
        }
        ph_volatile[i] = current;
        ph_narrow_return[i] = narrow;
    }
}

static char* ph_read_all(FILE* in){
    size_t capacity = 65536;
    size_t len = 0;
    char* text = malloc(capacity);
    size_t got;
    while((got = fread(text + len, 1, capacity - len - 1, in)) > 0){
        len += got;
        if(capacity - len < 2){
            capacity *= 2;
            text = realloc(text, capacity);
        }
    }
    text[len] = 0;
    return text;
}

static void ph_line(int i){
    struct ph_ins ins;
    if(!ph_lines[i] || !ph_parse(i, &ins)){
        return;
    }
    if(ph_jump_to_next(i, &ins) || ph_dead_after(i, &ins) || ph_self_move(i, &ins)
       || ph_push_pop(i) || ph_push_compute_pop(i) || ph_compute_swap(i) || ph_def_move(i, &ins)
       || ph_dead_def(i, &ins) || ph_copy_forward(i, &ins) || ph_move_run(i, &ins)){
        return;
    }
    switch(ph_arch){
        case PH_X86:
        case PH_X86_64:
            ph_x86_rules(i, &ins);
            break;
        case PH_AARCH64:
            ph_a64_rules(i, &ins);
            break;
        case PH_RISCV64:
            ph_rv_rules(i, &ins);
            break;
    }
}

void peephole_run(const char* target_name, FILE* in, FILE* out){
    if(strstr(target_name, "x86_64")){
        ph_arch = PH_X86_64;
    } else if(strstr(target_name, "aarch64")){
        ph_arch = PH_AARCH64;
    } else if(strstr(target_name, "riscv64")){
        ph_arch = PH_RISCV64;
    } else {
        ph_arch = PH_X86;
    }
    char* text = ph_read_all(in);
    ph_split(text);
    ph_mark_functions();
    // Every rule leaves fewer instructions (one inserts a line but takes
    // away two), so this ends; one rule's result is often another's
    // start, so it runs until nothing changes, 64 passes at most.
    for(int pass = 0; pass < 64; pass++){
        ph_changed = false;
        for(int i = 0; i < ph_count; i++){
            ph_line(i);
        }
        if(!ph_changed){
            break;
        }
    }
    for(int i = 0; i < ph_count; i++){
        if(ph_lines[i]){
            fputs(ph_lines[i], out);
            fputc('\n', out);
        }
    }
}
