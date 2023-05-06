#include <stdio.h>
#include <stdlib.h>
#include "helpers/vector.h"
#include "compiler.h"

static const char* main_x86_link_cmd(void);

// gcc-multilib (needed by `gcc -m32`) cannot be installed next to the
// aarch64 and riscv64 cross gcc's on Ubuntu, so prefer the i686 cross
// gcc when it is there. -static because the cross package ships no
// i386 dynamic loader on the host.
static const char* main_x86_link_cmd(void){
    if(system("command -v i686-linux-gnu-gcc >/dev/null 2>&1") == 0){
        return "i686-linux-gnu-gcc -static";
    }
    return "gcc -m32";
}

// ch143: CLI driver. Reads input/output paths from argv and decides
// whether to run NASM + link (default) or just stop at the .asm.
int main(int argc, char** argv){
    const char* input_file  = "./test.c";
    const char* output_file = "./test";
    const char* option      = "exec";

    if(argc > 1){ input_file  = argv[1]; }
    if(argc > 2){ output_file = argv[2]; }
    if(argc > 3){ option      = argv[3]; }

    int compile_flags = COMPILE_PROCESS_EXECUTE_NASM;
    if(S_EQ(option, "object")){
        compile_flags |= COMPILE_PROCESS_EXPORT_AS_OBJECT;
    }

    int res = compile_file(input_file, output_file, compile_flags);
    if(res == COMPILER_FILE_COMPILED_OK){
        printf("everything compiled fine\n");
    } else if(res == COMPILER_FAILED_WITH_ERRORS){
        printf("Compile failed\n");
        return res;
    } else {
        printf("Unknown response for compile time\n");
        return res;
    }

    if(compile_flags & COMPILE_PROCESS_EXECUTE_NASM){
        char nasm_output_file[64];
        char nasm_cmd[512];
        snprintf(nasm_output_file, sizeof(nasm_output_file), "%s.o", output_file);
        if(compile_flags & COMPILE_PROCESS_EXPORT_AS_OBJECT){
            snprintf(nasm_cmd, sizeof(nasm_cmd),
                "nasm -f elf32 %s -o %s", output_file, nasm_output_file);
        } else {
            snprintf(nasm_cmd, sizeof(nasm_cmd),
                "nasm -f elf32 %s -o %s && %s %s -o %s",
                output_file, nasm_output_file, main_x86_link_cmd(), nasm_output_file, output_file);
        }
        printf("%s\n", nasm_cmd);
        int rc = system(nasm_cmd);
        if(rc != 0){
            printf("Issue assemblign the assembly file with NASM and linking with gcc\n");
            return rc;
        }
    }
    return 0;
}
