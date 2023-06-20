#include "compiler.h"

// target descriptors. Every size the front end and the driver
// used to hard-code for 32-bit x86 now comes from one of these. The
// x86 values are exactly the old constants, so the i386 output does
// not move (tests/176-x86-golden.sh).
//
// the three 64-bit targets. All three are LP64: int stays 4
// bytes, long and pointers are 8. A NULL backend means the target is
// described but its code generator has not landed yet; the driver
// refuses it.

// x86 predefines nothing new: the preprocessor unit tests (130, 132-134,
// 147, 148, 157) count the built-in definitions and expect only
// __LINE__. The headers test __LP64__ instead of __i386__.
static const struct target_macro target_x86_macros[] = {
    { 0 },
};

static const struct target_macro target_x86_64_macros[] = {
    { "__x86_64__", 1 },
    { "__LP64__",   1 },
    { 0 },
};

static const struct target_macro target_aarch64_macros[] = {
    { "__aarch64__", 1 },
    { "__LP64__",    1 },
    { 0 },
};

static const struct target_macro target_riscv64_macros[] = {
    { "__riscv",      1 },
    { "__riscv_xlen", 64 },
    { "__LP64__",     1 },
    { 0 },
};

// backends. codegen_x86.c is the original codegen.c renamed.
static const struct codegen_backend codegen_backend_x86 = {
    .name           = "x86",
    .generate       = codegen_x86,
    .frame_register = "ebp",
};

// the x86_64 backend, built from codegen_x86.c.
static const struct codegen_backend codegen_backend_x86_64 = {
    .name           = "x86_64",
    .generate       = codegen_x86_64,
    .frame_register = "rbp",
};

// the AArch64 backend, GNU as syntax.
static const struct codegen_backend codegen_backend_aarch64 = {
    .name           = "aarch64",
    .generate       = codegen_aarch64,
    .frame_register = "x29",
};

// the RISC-V backend, GNU as syntax.
static const struct codegen_backend codegen_backend_riscv64 = {
    .name           = "riscv64",
    .generate       = codegen_riscv64,
    .frame_register = "s0",
};

static const struct target target_x86_desc = {
    .name              = "x86",
    .pointer_size      = DATA_SIZE_DWORD,
    .long_size         = DATA_SIZE_DWORD,
    .stack_slot_size   = DATA_SIZE_DWORD,
    // saved ebp + return eip
    .frame_header_size = DATA_SIZE_DDWORD,
    .arg_slot_size     = DATA_SIZE_DWORD,
    .stack_align       = C_STACK_ALIGNMENT,
    // cdecl: every argument goes on the stack.
    .max_reg_args      = 0,
    .backend           = &codegen_backend_x86,
    .asm_cmd           = "nasm -f elf32 %s -o %s",
    //
    .link_cmd          = "i686-linux-gnu-gcc -static %s -o %s",
    .link_fallback_cmd = "gcc -m32 %s -o %s",
    .run_cmd           = "",
    .macros            = target_x86_macros,
};

static const struct target target_x86_64_desc = {
    .name              = "x86_64",
    .pointer_size      = DATA_SIZE_DDWORD,
    .long_size         = DATA_SIZE_DDWORD,
    // push and pop move 8 bytes in 64-bit mode.
    .stack_slot_size   = DATA_SIZE_DDWORD,
    // saved rbp + return rip
    .frame_header_size = 16,
    .arg_slot_size     = DATA_SIZE_DDWORD,
    .stack_align       = C_STACK_ALIGNMENT,
    // SysV: rdi, rsi, rdx, rcx, r8, r9
    .max_reg_args      = 6,
    .backend           = &codegen_backend_x86_64,
    .asm_cmd           = "nasm -f elf64 %s -o %s",
    // -no-pie: calls take the callee's address with `lea r11, [name]`
    // even for libc functions such as printf. A PIE would have to go
    // through the GOT for those.
    .link_cmd          = "gcc -no-pie %s -o %s",
    .link_fallback_cmd = 0,
    .run_cmd           = "",
    .macros            = target_x86_64_macros,
};

static const struct target target_aarch64_desc = {
    .name              = "aarch64",
    .pointer_size      = DATA_SIZE_DDWORD,
    .long_size         = DATA_SIZE_DDWORD,
    // sp must stay 16-byte aligned whenever it is used to access
    // memory, so every expression push takes a full 16 bytes.
    .stack_slot_size   = 16,
    // saved x29 + x30
    .frame_header_size = 16,
    .arg_slot_size     = DATA_SIZE_DDWORD,
    .stack_align       = C_STACK_ALIGNMENT,
    // AAPCS64: x0-x7
    .max_reg_args      = 8,
    .backend           = &codegen_backend_aarch64,
    .asm_cmd           = "aarch64-linux-gnu-as %s -o %s",
    // -static: qemu-user then needs no aarch64 sysroot.
    .link_cmd          = "aarch64-linux-gnu-gcc -static %s -o %s",
    .link_fallback_cmd = 0,
    .run_cmd           = "qemu-aarch64",
    .macros            = target_aarch64_macros,
};

static const struct target target_riscv64_desc = {
    .name              = "riscv64",
    .pointer_size      = DATA_SIZE_DDWORD,
    .long_size         = DATA_SIZE_DDWORD,
    // The psABI keeps sp 16-byte aligned; pushing 16 at a time keeps
    // that true at every call without extra padding logic.
    .stack_slot_size   = 16,
    // saved s0 + ra
    .frame_header_size = 16,
    .arg_slot_size     = DATA_SIZE_DDWORD,
    .stack_align       = C_STACK_ALIGNMENT,
    // LP64 psABI: a0-a7
    .max_reg_args      = 8,
    .backend           = &codegen_backend_riscv64,
    .asm_cmd           = "riscv64-linux-gnu-as %s -o %s",
    .link_cmd          = "riscv64-linux-gnu-gcc -static %s -o %s",
    .link_fallback_cmd = 0,
    .run_cmd           = "qemu-riscv64",
    .macros            = target_riscv64_macros,
};

static const struct target* target_all[] = {
    &target_x86_desc,
    &target_x86_64_desc,
    &target_aarch64_desc,
    &target_riscv64_desc,
    0,
};

// The datatype helpers (datatype_size and friends) have no
// compile_process to ask, so the target being compiled for is also
// kept here. It defaults to x86 so unit tests that drive the parser
// or resolver directly keep their old 32-bit sizes.
static const struct target* target_current_desc = &target_x86_desc;

const struct target* target_x86(void){
    return &target_x86_desc;
}

// look a target up by its -target= name. NULL if unknown.
const struct target* target_for_name(const char* name){
    for(int i = 0; target_all[i]; i++){
        if(S_EQ(target_all[i]->name, name)){
            return target_all[i];
        }
    }
    return 0;
}

const struct target* target_current(void){
    return target_current_desc;
}

void target_set_current(const struct target* target){
    target_current_desc = target;
}
