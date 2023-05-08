#include "compiler.h"

// target descriptors. Every size the front end and the driver
// used to hard-code for 32-bit x86 now comes from one of these. The
// x86 values are exactly the old constants, so the i386 output does
// not move (tests/176-x86-golden.sh).

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
    .backend           = codegen,
    .asm_cmd           = "nasm -f elf32 %s -o %s",
    //
    .link_cmd          = "i686-linux-gnu-gcc -static %s -o %s",
    .link_fallback_cmd = "gcc -m32 %s -o %s",
    .run_cmd           = "",
};

// The datatype helpers (datatype_size and friends) have no
// compile_process to ask, so the target being compiled for is also
// kept here. It defaults to x86 so unit tests that drive the parser
// or resolver directly keep their old 32-bit sizes.
static const struct target* target_current_desc = &target_x86_desc;

const struct target* target_x86(void){
    return &target_x86_desc;
}

const struct target* target_current(void){
    return target_current_desc;
}

void target_set_current(const struct target* target){
    target_current_desc = target;
}
