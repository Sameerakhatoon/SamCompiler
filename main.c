#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "helpers/vector.h"
#include "compiler.h"

static const char* main_link_cmd(const struct target* target);
static int         main_parse_target(int argc, char** argv, const struct target** target_out);

// gcc-multilib (needed by `gcc -m32`) cannot be installed next to the
// aarch64 and riscv64 cross gcc's on Ubuntu, so the x86 target lists
// the i686 cross gcc first and `gcc -m32` as its fallback. The
// commands come from the target; the fallback is used when the first
// word of link_cmd is not on the PATH.
static const char* main_link_cmd(const struct target* target){
    if(!target->link_fallback_cmd){
        return target->link_cmd;
    }
    char tool[64];
    char check_cmd[128];
    sscanf(target->link_cmd, "%63s", tool);
    snprintf(check_cmd, sizeof(check_cmd), "command -v %s >/dev/null 2>&1", tool);
    if(system(check_cmd) == 0){
        return target->link_cmd;
    }
    return target->link_fallback_cmd;
}

// `-target=NAME` options come before the positional arguments.
// Returns how many argv entries they used, or -1 on a bad name.
static int main_parse_target(int argc, char** argv, const struct target** target_out){
    int used = 0;
    for(int i = 1; i < argc && strncmp(argv[i], "-target=", 8) == 0; i++){
        const char* name = argv[i] + 8;
        const struct target* target = target_for_name(name);
        if(!target){
            fprintf(stderr, "Unknown target %s, expected x86, x86_64, aarch64 or riscv64\n", name);
            return -1;
        }
        *target_out = target;
        used++;
    }
    return used;
}

// ch143: CLI driver. Reads input/output paths from argv and decides
// whether to run NASM + link (default) or just stop at the .asm.
int main(int argc, char** argv){
    const char* input_file  = "./test.c";
    const char* output_file = "./test";
    const char* option      = "exec";

    const struct target* target = target_x86();
    int used = main_parse_target(argc, argv, &target);
    if(used < 0){
        return 1;
    }
    argc -= used;
    argv += used;
    if(!target->backend){
        fprintf(stderr, "Target %s has no code generator yet\n", target->name);
        return 1;
    }
    target_set_current(target);

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
        char nasm_output_file[512];
        char asm_cmd[1024];
        char link_cmd[1024];
        char nasm_cmd[sizeof(asm_cmd) + sizeof(link_cmd) + 8];
        snprintf(nasm_output_file, sizeof(nasm_output_file), "%s.o", output_file);
        snprintf(asm_cmd, sizeof(asm_cmd), target->asm_cmd, output_file, nasm_output_file);
        if(compile_flags & COMPILE_PROCESS_EXPORT_AS_OBJECT){
            snprintf(nasm_cmd, sizeof(nasm_cmd), "%s", asm_cmd);
        } else {
            snprintf(link_cmd, sizeof(link_cmd), main_link_cmd(target), nasm_output_file, output_file);
            snprintf(nasm_cmd, sizeof(nasm_cmd), "%s && %s", asm_cmd, link_cmd);
        }
        printf("%s\n", nasm_cmd);
        int rc = system(nasm_cmd);
        if(rc != 0){
            printf("Issue assemblign the assembly file with NASM and linking with gcc\n");
            return rc;
        }
        // Cross targets do not run on the host directly; say how.
        if(!(compile_flags & COMPILE_PROCESS_EXPORT_AS_OBJECT) && target->run_cmd[0]){
            printf("run with: %s %s\n", target->run_cmd, output_file);
        }
    }
    return 0;
}
