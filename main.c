#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include "helpers/vector.h"
#include "compiler.h"

static const char* main_link_cmd(const struct target* target);
static int         main_parse_target(int argc, char** argv, const struct target** target_out);

// The asm is written to a temporary file next to the output and renamed
// over it only once compilation succeeds, so a failed compile leaves the
// old output alone. compiler_error() exits from
// deep inside the compiler, so an atexit hook removes the temporary.
static char main_tmp_output[PATH_MAX];

static void main_remove_tmp_output(void){
    if(main_tmp_output[0]){
        unlink(main_tmp_output);
    }
}

static bool main_same_file(const char* a, const char* b){
    char real_a[PATH_MAX];
    char real_b[PATH_MAX];
    if(!realpath(a, real_a) || !realpath(b, real_b)){
        return false;
    }
    return strcmp(real_a, real_b) == 0;
}

// Single-quote `in` for /bin/sh so a path reaches the assembler and linker
// as one argument and is never run as shell syntax (E2).
static bool main_shell_quote(const char* in, char* out, size_t out_size){
    size_t used = 0;
    if(out_size < 3){
        return false;
    }
    out[used++] = '\'';
    for(const char* c = in; *c; c++){
        const char* piece = *c == '\'' ? "'\\''" : 0;
        size_t piece_len = piece ? strlen(piece) : 1;
        if(used + piece_len + 2 > out_size){
            return false;
        }
        if(piece){
            memcpy(out + used, piece, piece_len);
        } else {
            out[used] = *c;
        }
        used += piece_len;
    }
    out[used++] = '\'';
    out[used] = 0;
    return true;
}

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

    if(main_same_file(input_file, output_file)){
        fprintf(stderr, "The output file %s is the input file, refusing to overwrite it\n", output_file);
        return 1;
    }
    int written = snprintf(main_tmp_output, sizeof(main_tmp_output), "%s.XXXXXX", output_file);
    if(written < 0 || (size_t)written >= sizeof(main_tmp_output)){
        fprintf(stderr, "The output path %s is too long\n", output_file);
        return 1;
    }
    int tmp_fd = mkstemp(main_tmp_output);
    if(tmp_fd < 0){
        fprintf(stderr, "Cannot create a temporary file next to %s\n", output_file);
        main_tmp_output[0] = 0;
        return 1;
    }
    // mkstemp creates the file as 0600; give the asm the usual 0666 & ~umask.
    mode_t mask = umask(0);
    umask(mask);
    fchmod(tmp_fd, 0666 & ~mask);
    close(tmp_fd);
    atexit(main_remove_tmp_output);

    int res = compile_file(input_file, main_tmp_output, compile_flags);
    if(res == COMPILER_FILE_COMPILED_OK){
        if(rename(main_tmp_output, output_file) != 0){
            fprintf(stderr, "Cannot write %s\n", output_file);
            return 1;
        }
        main_tmp_output[0] = 0;
        printf("everything compiled fine\n");
    } else if(res == COMPILER_FAILED_WITH_ERRORS){
        printf("Compile failed\n");
        return res;
    } else {
        printf("Unknown response for compile time\n");
        return res;
    }

    if(compile_flags & COMPILE_PROCESS_EXECUTE_NASM){
        char nasm_output_file[PATH_MAX];
        char quoted_output[PATH_MAX * 4 + 3];
        char quoted_object[PATH_MAX * 4 + 3];
        char asm_cmd[sizeof(quoted_output) * 2 + 64];
        char link_cmd[sizeof(quoted_output) * 2 + 64];
        char nasm_cmd[sizeof(asm_cmd) + sizeof(link_cmd) + 8];
        snprintf(nasm_output_file, sizeof(nasm_output_file), "%s.o", output_file);
        if(!main_shell_quote(output_file, quoted_output, sizeof(quoted_output)) ||
           !main_shell_quote(nasm_output_file, quoted_object, sizeof(quoted_object))){
            fprintf(stderr, "The output path %s is too long\n", output_file);
            return 1;
        }
        snprintf(asm_cmd, sizeof(asm_cmd), target->asm_cmd, quoted_output, quoted_object);
        if(compile_flags & COMPILE_PROCESS_EXPORT_AS_OBJECT){
            snprintf(nasm_cmd, sizeof(nasm_cmd), "%s", asm_cmd);
        } else {
            snprintf(link_cmd, sizeof(link_cmd), main_link_cmd(target), quoted_object, quoted_output);
            snprintf(nasm_cmd, sizeof(nasm_cmd), "%s && %s", asm_cmd, link_cmd);
        }
        printf("%s\n", nasm_cmd);
        int rc = system(nasm_cmd);
        if(rc != 0){
            printf("Issue assemblign the assembly file with NASM and linking with gcc\n");
            // system() returns a wait status; 256 truncated to an exit
            // code would be 0, so report the tool's own status (or 1).
            if(rc > 0 && WIFEXITED(rc) && WEXITSTATUS(rc) != 0){
                return WEXITSTATUS(rc);
            }
            return 1;
        }
        // Cross targets do not run on the host directly; say how.
        if(!(compile_flags & COMPILE_PROCESS_EXPORT_AS_OBJECT) && target->run_cmd[0]){
            printf("run with: %s %s\n", target->run_cmd, output_file);
        }
    }
    return 0;
}
