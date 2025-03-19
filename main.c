#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "helpers/vector.h"
#include "compiler.h"
#include "os.h"

// The driver. Two forms:
//
//   main [options] file...                 gcc-style: -o -c -S -E -I -D -U
//   main [-target=X] in out [object]       the original form, kept exactly
//
// The original form is what the chapter tests, tools/gcc_differential.sh
// and SamOS's tools call, so it keeps its outputs and messages: the asm
// written to `out`, then assembled to `out.o`, then (without `object`)
// linked over `out`. It is recognised when none of -o -c -S -E is given
// and the second file name is not itself an input (.c .o .a or -).

enum {
    MAIN_MODE_LINK,
    MAIN_MODE_OBJECT,       // -c
    MAIN_MODE_ASSEMBLY,     // -S
    MAIN_MODE_PREPROCESS,   // -E
};

// What the link makes.
enum {
    MAIN_LINK_EXECUTABLE,   // the target's default (static, static-pie or PIE)
    MAIN_LINK_DYNAMIC,      // a dynamic PIE, for a program with a .so input
    MAIN_LINK_SHARED,       // -shared
};

static const char* main_link_cmd(const struct target* target, int kind);

static const char* main_prog = "main";
static const char* main_sysroot;
static const char* main_own_includes;
static bool        main_verbose;
static int         main_pid;
// -fPIC or -shared: compile for a shared object; -shared: link one.
static int         main_compile_flags;
static bool        main_shared;

// The asm is written to a temporary file next to the output and renamed
// over it only once compilation succeeds, so a failed compile leaves the
// old output alone. compiler_error() exits from
// deep inside the compiler, so an atexit hook removes the temporaries.
// Every temporary is listed here; the gcc-style form keeps its
// intermediate files in a private directory under $TMPDIR.
static char           main_tmp_output[PATH_MAX];
static struct vector* main_tmp_files;
static char           main_tmp_dir[PATH_MAX];

static void main_remove_tmp_files(void){
    // A forked compile (main_compile_child) exits through here too.
    if(getpid() != main_pid){
        return;
    }
    if(main_tmp_output[0]){
        unlink(main_tmp_output);
    }
    if(main_tmp_files){
        vector_set_peek_pointer(main_tmp_files, 0);
        const char* path = vector_peek_ptr(main_tmp_files);
        while(path){
            unlink(path);
            path = vector_peek_ptr(main_tmp_files);
        }
    }
    if(main_tmp_dir[0]){
        rmdir(main_tmp_dir);
    }
}

static const char* main_track_tmp(const char* path){
    if(!main_tmp_files){
        main_tmp_files = vector_create(sizeof(const char*));
    }
    char* copy = strdup(path);
    vector_push(main_tmp_files, &copy);
    return copy;
}

static void main_error(const char* fmt, ...){
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "%s: error: ", main_prog);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
}

static bool main_same_file(const char* a, const char* b){
    char real_a[PATH_MAX];
    char real_b[PATH_MAX];
    if(!compiler_real_path(a, real_a) || !compiler_real_path(b, real_b)){
        return false;
    }
    return strcmp(real_a, real_b) == 0;
}

static bool main_ends_with(const char* s, const char* suffix){
    size_t len = strlen(s);
    size_t suffix_len = strlen(suffix);
    return len > suffix_len && strcmp(s + len - suffix_len, suffix) == 0;
}

static bool main_is_source(const char* path){
    return S_EQ(path, "-") || main_ends_with(path, ".c");
}

// An assembly file in the target's own syntax (NASM for x86 and x86_64,
// GNU as for aarch64 and riscv64), assembled and linked like a compiled
// source. Not in the original form: there `main in.c out.s` still
// writes the asm to out.s.
static bool main_is_assembly(const char* path){
    return main_ends_with(path, ".s");
}

static bool main_is_shared_library(const char* path){
    return main_ends_with(path, ".so");
}

static bool main_is_linker_input(const char* path){
    return main_ends_with(path, ".o") || main_ends_with(path, ".a") || main_is_shared_library(path);
}

// gcc-multilib (needed by `gcc -m32`) cannot be installed next to the
// aarch64 and riscv64 cross gcc's on Ubuntu, so the x86 target lists
// the i686 cross gcc first and `gcc -m32` as its fallback. The
// commands come from the target; the fallback is used when the first
// word of link_cmd is not on the PATH.
// `kind` picks the target's command for an executable, a program
// linked against a shared library, or a shared object.
static const char* main_link_cmd(const struct target* target, int kind){
    const char* cmd = target->link_cmd;
    const char* fallback = target->link_fallback_cmd;
    if(kind == MAIN_LINK_DYNAMIC && target->link_dynamic_cmd){
        cmd = target->link_dynamic_cmd;
    } else if(kind == MAIN_LINK_SHARED){
        cmd = target->link_shared_cmd;
        fallback = target->link_shared_fallback_cmd;
    }
    if(!fallback){
        return cmd;
    }
    char tool[64];
    sscanf(cmd, "%63s", tool);
    return os_have_command(tool) ? cmd : fallback;
}

// pc_includes is found next to this binary (os_self_path follows a
// symlink such as /usr/local/bin/samcc), not in the working directory.
static void main_find_own_includes(void){
    static char own_includes[PATH_MAX];
    if(!os_self_path(own_includes, sizeof(own_includes) - 32)){
        return;
    }
    char* base = (char*)os_base_name(own_includes);
    if(base == own_includes){
        return;
    }
    strcpy(base, "pc_includes");
    main_own_includes = own_includes;
    compiler_set_own_include_dir(own_includes);
}

// a samos target reads its headers from <sysroot>/include, then
// SamCompiler's own pc_includes (stdarg.h, stddef.h, stdint.h,
// stdbool.h). Nothing from /usr/include.
static int main_setup_samos(const struct target* target){
    if(!main_sysroot){
        main_sysroot = getenv("SAMOS_SYSROOT");
    }
    if(!main_sysroot || !main_sysroot[0]){
        fprintf(stderr, "Target %s needs -sysroot=DIR or SAMOS_SYSROOT (DIR/include, DIR/lib)\n", target->name);
        return -1;
    }
    static char sysroot_include[PATH_MAX];
    snprintf(sysroot_include, sizeof(sysroot_include), "%s/include", main_sysroot);
    compiler_add_include_dir(sysroot_include);
    if(main_own_includes){
        compiler_add_include_dir(main_own_includes);
    }
    return 0;
}

static void main_usage(FILE* out){
    fprintf(out,
        "usage: %s [options] file...\n"
        "       %s [-target=T] [options] input.c output [object]\n"
        "\n"
        "Inputs ending in .c are compiled (- reads the source from standard\n"
        "input), .s files (the target's assembly) assembled; .o, .a and .so files\n"
        "go to the linker. Without -c, -S or -E the inputs are compiled,\n"
        "assembled and linked into one executable.\n"
        "\n"
        "  -o FILE          write the output to FILE (default a.out; with -c or -S,\n"
        "                   NAME.o or NAME.s in the current directory for each NAME.c)\n"
        "  -c               compile and assemble, do not link\n"
        "  -S               compile to assembly, do not assemble\n"
        "  -E               preprocess only, to standard output or -o FILE\n"
        "  -I DIR           search DIR for includes, before pc_includes and /usr/include\n"
        "  -D NAME[=VALUE]  define a macro (VALUE defaults to 1)\n"
        "  -U NAME          undefine a macro\n"
        "  -O0, -O, -O1     no optimisation (the default), or constant folding, dead\n"
        "                   code removal and a peephole pass (-O2, -O3, -Os mean -O1)\n"
        "  -fPIC, -fpic     code for a shared object: exported symbols through the GOT\n"
        "  -shared          link a shared object (implies -fPIC); a .so input links\n"
        "                   the program as a dynamic PIE\n"
        "  -g               debug information: DWARF lines, frames and variables\n"
        "  -target=T        x86 (default), x86_64, aarch64, riscv64, samos-x86_64,\n"
        "                   samos-aarch64 or samos-riscv64\n"
        "  -sysroot=DIR     the SamOS sysroot of a samos target (or SAMOS_SYSROOT)\n"
        "  -v               print the assembler and linker commands\n"
        "  -h, --help       print this help\n"
        "\n"
        "The second form is the original interface: it writes the asm to output,\n"
        "assembles it to output.o and links the executable over output; with\n"
        "`object` it stops after the assembler. It is used when none of -o, -c,\n"
        "-S or -E is given and the second file is not itself an input.\n",
        main_prog, main_prog);
}

// Run a tool's command line; returns 0 or the tool's exit status (at
// least 1).
static int main_run(const char* cmd){
    if(main_verbose){
        fprintf(stderr, "%s\n", cmd);
    }
    return os_run_command(cmd);
}

// The arguments the driver was started with, for main_compile_spawned.
static int    main_argc;
static char** main_argv;

struct main_compile_job {
    const char* input;
    const char* output;
    int         flags;
};

static int main_compile_job_run(void* arg){
    struct main_compile_job* job = arg;
    return compile_file(job->input, job->output, job->flags) == COMPILER_FILE_COMPILED_OK ? 0 : 1;
}

// Without fork (Windows, or SAMCC_NO_FORK set) the driver runs itself
// again, `--samcc-compile-child FLAGS INPUT OUTPUT` in front of its own
// arguments, so the child sets up the same target, includes and macros
// and compiles that one input.
static int main_compile_spawned(const char* input, const char* output, int flags){
    char self[PATH_MAX];
    if(!os_self_path(self, sizeof(self))){
        main_error("cannot find the driver's own executable to compile %s", input);
        return 1;
    }
    char quoted[PATH_MAX * 4 + 3];
    size_t size = 64 + 3 * sizeof(quoted);
    for(int i = 1; i < main_argc; i++){
        size += strlen(main_argv[i]) * 4 + 4;
    }
    char* cmd = malloc(size);
    os_quote_arg(self, cmd, size);
    snprintf(cmd + strlen(cmd), size - strlen(cmd), " --samcc-compile-child %d", flags);
    for(int i = -2; i < main_argc; i++){
        const char* arg = i == -2 ? input : i == -1 ? output : main_argv[i];
        if(i == 0){
            continue;
        }
        if(!os_quote_arg(arg, quoted, sizeof(quoted))){
            main_error("the argument %s is too long", arg);
            free(cmd);
            return 1;
        }
        strcat(cmd, " ");
        strcat(cmd, quoted);
    }
    fflush(stdout);
    fflush(stderr);
    int rc = os_run_command(cmd);
    free(cmd);
    return rc == 0 ? 0 : 1;
}

// Compile one source in a child process. The compiler keeps global state
// and exits from deep inside on an error, so each input gets a fresh
// process: one bad file cannot take the driver down, and one file's
// state cannot leak into the next.
static int main_compile_child(const char* input, const char* output, int flags){
    struct main_compile_job job;
    job.input = input;
    job.output = output;
    job.flags = flags;
    int rc = os_run_forked(main_compile_job_run, &job);
    if(rc == OS_NO_FORK){
        return main_compile_spawned(input, output, flags);
    }
    return rc;
}

// A temporary next to `dest`, so the finished file can be renamed into
// place (a rename cannot cross file systems).
static int main_reserve_beside(const char* dest, char* out, size_t out_size){
    int written = snprintf(out, out_size, "%s.XXXXXX", dest);
    if(written < 0 || (size_t)written >= out_size){
        main_error("the output path %s is too long", dest);
        return -1;
    }
    if(os_make_temp_file(out) < 0){
        main_error("cannot create a temporary file next to %s: %s", dest, strerror(errno));
        return -1;
    }
    main_track_tmp(out);
    return 0;
}

static int main_install(const char* tmp, const char* dest){
    if(os_replace_file(tmp, dest) != 0){
        main_error("cannot write %s: %s", dest, strerror(errno));
        return -1;
    }
    return 0;
}

// A path in the private temporary directory, unique per input.
static const char* main_tmp_path(int index, const char* suffix){
    if(!main_tmp_dir[0]){
        snprintf(main_tmp_dir, sizeof(main_tmp_dir), "%s/samcc.XXXXXX", os_temp_dir());
        if(os_make_temp_dir(main_tmp_dir) < 0){
            main_error("cannot create a temporary directory: %s", strerror(errno));
            main_tmp_dir[0] = 0;
            return NULL;
        }
    }
    char path[PATH_MAX + 32];
    snprintf(path, sizeof(path), "%s/%d%s", main_tmp_dir, index, suffix);
    return main_track_tmp(path);
}

// NASM gives an undefined symbol no type, and ld rejects a TLS
// relocation against a symbol that is not STT_TLS ("mismatches non-TLS
// reference"). The x86 backends name every extern thread-local variable
// they refer to in a `; tls extern NAME` line of the asm; the symbol of
// each is made STT_TLS in the object NASM wrote. Other objects and
// assemblers are left as they are. Returns nonzero on an error.
static bool main_tls_extern_named(struct vector* names, const char* name){
    for(int i = 0; i < vector_count(names); i++){
        if(S_EQ(*(const char**)vector_at(names, i), name)){
            return true;
        }
    }
    return false;
}

static int main_mark_tls_externs(const char* asm_file, const char* object_file){
    FILE* in = fopen(asm_file, "r");
    if(!in){
        return 0;
    }
    struct vector* names = vector_create(sizeof(const char*));
    char line[IDENTIFIER_MAX_LENGTH + 64];
    while(fgets(line, sizeof(line), in)){
        if(!strncmp(line, "; tls extern ", 13)){
            line[strcspn(line, "\r\n")] = 0;
            char* name = strdup(line + 13);
            vector_push(names, &name);
        }
    }
    fclose(in);
    int rc = 0;
    if(vector_count(names)){
        FILE* f = fopen(object_file, "r+b");
        unsigned char* data = 0;
        long size = 0;
        if(!f || fseek(f, 0, SEEK_END) || (size = ftell(f)) < (long)sizeof(Elf32_Ehdr) || fseek(f, 0, SEEK_SET)
           || !(data = malloc(size)) || fread(data, 1, size, f) != (size_t)size){
            main_error("cannot read %s", object_file);
            rc = 1;
        } else if(data[EI_CLASS] == ELFCLASS64){
            Elf64_Ehdr* eh = (Elf64_Ehdr*)data;
            Elf64_Shdr* sh = (Elf64_Shdr*)(data + eh->e_shoff);
            for(int s = 0; s < eh->e_shnum; s++){
                if(sh[s].sh_type != SHT_SYMTAB){
                    continue;
                }
                Elf64_Sym* sym = (Elf64_Sym*)(data + sh[s].sh_offset);
                const char* strtab = (const char*)(data + sh[sh[s].sh_link].sh_offset);
                for(size_t k = 0; k < sh[s].sh_size / sizeof(Elf64_Sym); k++){
                    if(sym[k].st_shndx == SHN_UNDEF && sym[k].st_name && main_tls_extern_named(names, strtab + sym[k].st_name)){
                        sym[k].st_info = ELF64_ST_INFO(ELF64_ST_BIND(sym[k].st_info), STT_TLS);
                    }
                }
            }
        } else {
            Elf32_Ehdr* eh = (Elf32_Ehdr*)data;
            Elf32_Shdr* sh = (Elf32_Shdr*)(data + eh->e_shoff);
            for(int s = 0; s < eh->e_shnum; s++){
                if(sh[s].sh_type != SHT_SYMTAB){
                    continue;
                }
                Elf32_Sym* sym = (Elf32_Sym*)(data + sh[s].sh_offset);
                const char* strtab = (const char*)(data + sh[sh[s].sh_link].sh_offset);
                for(size_t k = 0; k < sh[s].sh_size / sizeof(Elf32_Sym); k++){
                    if(sym[k].st_shndx == SHN_UNDEF && sym[k].st_name && main_tls_extern_named(names, strtab + sym[k].st_name)){
                        sym[k].st_info = ELF32_ST_INFO(ELF32_ST_BIND(sym[k].st_info), STT_TLS);
                    }
                }
            }
        }
        if(!rc && (fseek(f, 0, SEEK_SET) || fwrite(data, 1, size, f) != (size_t)size)){
            main_error("cannot write %s", object_file);
            rc = 1;
        }
        free(data);
        if(f){
            fclose(f);
        }
    }
    for(int i = 0; i < vector_count(names); i++){
        free(*(char**)vector_at(names, i));
    }
    vector_free(names);
    return rc;
}

static int main_assemble(const struct target* target, const char* asm_file, const char* object_file){
    char quoted_asm[PATH_MAX * 4 + 3];
    char quoted_object[PATH_MAX * 4 + 3];
    char cmd[sizeof(quoted_asm) * 2 + 64];
    if(!os_quote_arg(asm_file, quoted_asm, sizeof(quoted_asm))
       || !os_quote_arg(object_file, quoted_object, sizeof(quoted_object))){
        main_error("the path %s is too long", object_file);
        return 1;
    }
    snprintf(cmd, sizeof(cmd), target->asm_cmd, quoted_asm, quoted_object);
    int rc = main_run(cmd);
    return rc ? rc : main_mark_tls_externs(asm_file, object_file);
}

// `objects` is already quoted and space-separated.
static int main_link(const struct target* target, int kind, const char* objects, const char* output){
    char quoted_output[PATH_MAX * 4 + 3];
    char quoted_sysroot[PATH_MAX * 4 + 3];
    if(!os_quote_arg(output, quoted_output, sizeof(quoted_output))){
        main_error("the output path %s is too long", output);
        return 1;
    }
    size_t size = strlen(objects) + sizeof(quoted_output) + sizeof(quoted_sysroot) + 512;
    char* cmd = malloc(size);
    if(target->samos){
        if(!os_quote_arg(main_sysroot, quoted_sysroot, sizeof(quoted_sysroot))){
            main_error("the sysroot path %s is too long", main_sysroot);
            return 1;
        }
        snprintf(cmd, size, target->link_cmd, quoted_sysroot, objects, quoted_output);
    } else {
        snprintf(cmd, size, main_link_cmd(target, kind), objects, quoted_output);
    }
    int rc = main_run(cmd);
    free(cmd);
    return rc;
}

// NAME.c -> NAME<suffix> in the current directory, as gcc names the
// outputs of -c and -S.
static const char* main_default_output(const char* input, const char* suffix){
    const char* base = os_base_name(input);
    size_t stem = strlen(base);
    const char* dot = strrchr(base, '.');
    if(dot && dot != base){
        stem = (size_t)(dot - base);
    }
    char* out = malloc(stem + strlen(suffix) + 1);
    memcpy(out, base, stem);
    strcpy(out + stem, suffix);
    return out;
}

static bool main_output_is_an_input(const char* output, const char** inputs, int count){
    for(int i = 0; i < count; i++){
        if(!S_EQ(inputs[i], "-") && main_same_file(inputs[i], output)){
            main_error("the output file %s is the input file %s, refusing to overwrite it", output, inputs[i]);
            return true;
        }
    }
    return false;
}

// Append the file at `path` to `out`.
static int main_copy_into(const char* path, FILE* out){
    FILE* in = fopen(path, "rb");
    if(!in){
        main_error("cannot read %s: %s", path, strerror(errno));
        return 1;
    }
    char buf[8192];
    size_t n;
    while((n = fread(buf, 1, sizeof(buf), in)) > 0){
        if(fwrite(buf, 1, n, out) != n){
            fclose(in);
            main_error("cannot write the preprocessed output");
            return 1;
        }
    }
    fclose(in);
    return 0;
}

// -E: each source preprocessed in turn, concatenated.
static int main_preprocess(const char** inputs, int count, const char* output){
    char beside[PATH_MAX];
    FILE* out = stdout;
    if(output){
        if(main_output_is_an_input(output, inputs, count) || main_reserve_beside(output, beside, sizeof(beside)) < 0){
            return 1;
        }
        out = fopen(beside, "wb");
        if(!out){
            main_error("cannot write %s: %s", beside, strerror(errno));
            return 1;
        }
    }
    int rc = 0;
    for(int i = 0; i < count && rc == 0; i++){
        if(!main_is_source(inputs[i])){
            fprintf(stderr, "%s: warning: %s: %s input file unused because %s not done\n", main_prog, inputs[i],
                main_is_assembly(inputs[i]) ? "assembler" : "linker", main_is_assembly(inputs[i]) ? "assembly" : "linking");
            continue;
        }
        const char* tmp = main_tmp_path(i, ".i");
        if(!tmp){
            rc = 1;
            break;
        }
        rc = main_compile_child(inputs[i], tmp, COMPILE_PROCESS_PREPROCESS_ONLY);
        if(rc == 0){
            fflush(out);
            rc = main_copy_into(tmp, out);
        }
    }
    if(output){
        if(fclose(out) != 0 && rc == 0){
            main_error("cannot write %s", output);
            rc = 1;
        }
        if(rc == 0 && main_install(beside, output) < 0){
            rc = 1;
        }
    }
    return rc;
}

// -c and -S: one output per source (and with -c per .s file). A source
// that fails does not stop the others, as with gcc: every file's
// errors are reported, and the status is 1.
static int main_compile_each(const struct target* target, int mode, const char** inputs, int count, const char* output){
    int sources = 0;
    for(int i = 0; i < count; i++){
        sources += main_is_source(inputs[i]) || (mode == MAIN_MODE_OBJECT && main_is_assembly(inputs[i]));
    }
    if(output && sources > 1){
        main_error("cannot specify -o with -c or -S and several source files");
        return 1;
    }
    int failed = 0;
    for(int i = 0; i < count; i++){
        const char* input = inputs[i];
        bool assembly = main_is_assembly(input);
        if(assembly && mode == MAIN_MODE_ASSEMBLY){
            fprintf(stderr, "%s: warning: %s: assembler input file unused because assembly not done\n", main_prog, input);
            continue;
        }
        if(!main_is_source(input) && !assembly){
            fprintf(stderr, "%s: warning: %s: linker input file unused because linking not done\n", main_prog, input);
            continue;
        }
        const char* suffix = mode == MAIN_MODE_OBJECT ? ".o" : ".s";
        if(!output && S_EQ(input, "-")){
            main_error("standard input needs -o with -c or -S");
            return 1;
        }
        const char* dest = output ? output : main_default_output(input, suffix);
        char beside[PATH_MAX];
        if(main_output_is_an_input(dest, inputs, count) || main_reserve_beside(dest, beside, sizeof(beside)) < 0){
            return 1;
        }
        if(mode == MAIN_MODE_ASSEMBLY){
            if(main_compile_child(input, beside, main_compile_flags) != 0){
                failed = 1;
                continue;
            }
        } else {
            const char* asm_file = assembly ? input : main_tmp_path(i, ".s");
            if(!asm_file){
                return 1;
            }
            if(!assembly && main_compile_child(input, asm_file, main_compile_flags) != 0){
                failed = 1;
                continue;
            }
            int rc = main_assemble(target, asm_file, beside);
            if(rc != 0){
                return rc;
            }
        }
        if(main_install(beside, dest) < 0){
            return 1;
        }
    }
    return failed;
}

// The default: compile and assemble each source, assemble each .s file,
// then link everything, objects and archives in command-line order, into
// one executable. Every source is compiled even after one fails, so all
// their errors are reported; nothing is linked then.
static int main_build_executable(const struct target* target, const char** inputs, int count, const char* output){
    if(!output){
        output = "a.out";
    }
    if(main_output_is_an_input(output, inputs, count)){
        return 1;
    }
    int kind = main_shared ? MAIN_LINK_SHARED : MAIN_LINK_EXECUTABLE;
    size_t objects_size = 1;
    char* objects = calloc(1, objects_size);
    int failed = 0;
    for(int i = 0; i < count; i++){
        const char* object = inputs[i];
        if(kind == MAIN_LINK_EXECUTABLE && main_is_shared_library(object)){
            kind = MAIN_LINK_DYNAMIC;
        }
        if(main_is_source(inputs[i]) || main_is_assembly(inputs[i])){
            const char* asm_file = main_is_assembly(inputs[i]) ? inputs[i] : main_tmp_path(i, ".s");
            if(!asm_file){
                return 1;
            }
            if(!main_is_assembly(inputs[i]) && main_compile_child(inputs[i], asm_file, main_compile_flags) != 0){
                failed = 1;
                continue;
            }
            if(failed){
                continue;
            }
            object = main_tmp_path(i, ".o");
            if(!object){
                return 1;
            }
            int rc = main_assemble(target, asm_file, object);
            if(rc != 0){
                return rc;
            }
        }
        char quoted[PATH_MAX * 4 + 3];
        if(!os_quote_arg(object, quoted, sizeof(quoted))){
            main_error("the path %s is too long", object);
            return 1;
        }
        objects_size += strlen(quoted) + 1;
        objects = realloc(objects, objects_size);
        if(objects[0]){
            strcat(objects, " ");
        }
        strcat(objects, quoted);
    }
    if(failed){
        free(objects);
        return 1;
    }
    char beside[PATH_MAX];
    if(main_reserve_beside(output, beside, sizeof(beside)) < 0){
        return 1;
    }
    // The linker creates the file itself, with the execute bits.
    unlink(beside);
    int rc = main_link(target, kind, objects, beside);
    free(objects);
    if(rc != 0){
        return rc;
    }
    return main_install(beside, output) < 0 ? 1 : 0;
}

// The original form: `in out [object]`, unchanged.
static int main_legacy(const struct target* target, const char* input_file, const char* output_file, bool object){
    int compile_flags = COMPILE_PROCESS_EXECUTE_NASM | main_compile_flags;
    if(object){
        compile_flags |= COMPILE_PROCESS_EXPORT_AS_OBJECT;
    }

    if(main_same_file(input_file, output_file)){
        fprintf(stderr, "The output file %s is the input file, refusing to overwrite it\n", output_file);
        return 1;
    }
    int written = snprintf(main_tmp_output, sizeof(main_tmp_output), "%s.XXXXXX", output_file);
    if(written < 0 || (size_t)written >= sizeof(main_tmp_output)){
        fprintf(stderr, "The output path %s is too long\n", output_file);
        main_tmp_output[0] = 0;
        return 1;
    }
    if(os_make_temp_file(main_tmp_output) < 0){
        fprintf(stderr, "Cannot create a temporary file next to %s\n", output_file);
        main_tmp_output[0] = 0;
        return 1;
    }

    int res = compile_file(input_file, main_tmp_output, compile_flags);
    if(res == COMPILER_FILE_COMPILED_OK){
        if(os_replace_file(main_tmp_output, output_file) != 0){
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

    char nasm_output_file[PATH_MAX];
    char quoted_output[PATH_MAX * 4 + 3];
    char quoted_object[PATH_MAX * 4 + 3];
    char asm_cmd[sizeof(quoted_output) * 2 + 64];
    char link_cmd[sizeof(quoted_output) * 6 + 256];
    char nasm_cmd[sizeof(asm_cmd) + sizeof(link_cmd) + 8];
    snprintf(nasm_output_file, sizeof(nasm_output_file), "%s.o", output_file);
    if(!os_quote_arg(output_file, quoted_output, sizeof(quoted_output)) ||
       !os_quote_arg(nasm_output_file, quoted_object, sizeof(quoted_object))){
        fprintf(stderr, "The output path %s is too long\n", output_file);
        return 1;
    }
    snprintf(asm_cmd, sizeof(asm_cmd), target->asm_cmd, quoted_output, quoted_object);
    if(object){
        snprintf(nasm_cmd, sizeof(nasm_cmd), "%s", asm_cmd);
    } else {
        if(target->samos){
            char quoted_sysroot[PATH_MAX * 4 + 3];
            if(!os_quote_arg(main_sysroot, quoted_sysroot, sizeof(quoted_sysroot))){
                fprintf(stderr, "The sysroot path %s is too long\n", main_sysroot);
                return 1;
            }
            snprintf(link_cmd, sizeof(link_cmd), target->link_cmd, quoted_sysroot, quoted_object, quoted_output);
        } else {
            snprintf(link_cmd, sizeof(link_cmd), main_link_cmd(target, MAIN_LINK_EXECUTABLE), quoted_object, quoted_output);
        }
        snprintf(nasm_cmd, sizeof(nasm_cmd), "%s && %s", asm_cmd, link_cmd);
    }
    printf("%s\n", nasm_cmd);
    fflush(stdout);
    // Assembled, the object's extern thread-locals marked, then linked.
    int rc = os_run_command(asm_cmd);
    if(rc == 0 && main_mark_tls_externs(output_file, nasm_output_file)){
        rc = 1;
    }
    if(rc == 0 && !object){
        rc = os_run_command(link_cmd);
    }
    if(rc != 0){
        printf("Issue assemblign the assembly file with NASM and linking with gcc\n");
        return rc;
    }
    // Cross targets do not run on the host directly; say how.
    if(!object && target->run_cmd[0]){
        printf("run with: %s %s\n", target->run_cmd, output_file);
    }
    return 0;
}

// "-D NAME=VALUE" -> "#define NAME VALUE", "-D NAME" -> "#define NAME 1".
static int main_add_define(const char* arg){
    const char* equals = strchr(arg, '=');
    size_t name_len = equals ? (size_t)(equals - arg) : strlen(arg);
    if(name_len == 0){
        main_error("-D needs a macro name");
        return -1;
    }
    const char* value = equals ? equals + 1 : "1";
    size_t size = name_len + strlen(value) + 16;
    char* line = malloc(size);
    snprintf(line, size, "#define %.*s %s", (int)name_len, arg, value);
    compiler_add_cli_macro_line(line);
    return 0;
}

static int main_add_undef(const char* arg){
    size_t size = strlen(arg) + 16;
    char* line = malloc(size);
    snprintf(line, size, "#undef %s", arg);
    compiler_add_cli_macro_line(line);
    return 0;
}

// An option that takes a value as `-XVALUE` or `-X VALUE`. Returns the
// value, or NULL (with a message) when it is missing.
static const char* main_option_value(int argc, char** argv, int* i, const char* option){
    const char* arg = argv[*i];
    if(arg[2]){
        return arg + 2;
    }
    if(*i + 1 >= argc || !argv[*i + 1][0]){
        main_error("%s needs an argument", option);
        return NULL;
    }
    *i += 1;
    return argv[*i];
}

int main(int argc, char** argv){
    main_pid = getpid();
    main_prog = os_base_name(argv[0]);
    atexit(main_remove_tmp_files);

    // main_compile_spawned's child: compile one input with the options
    // that follow, then stop.
    const char* child_input = NULL;
    const char* child_output = NULL;
    int child_flags = 0;
    if(argc >= 5 && S_EQ(argv[1], "--samcc-compile-child")){
        child_flags = (int)strtol(argv[2], NULL, 10);
        child_input = argv[3];
        child_output = argv[4];
        argv[4] = argv[0];
        argv += 4;
        argc -= 4;
    }
    main_argc = argc;
    main_argv = argv;

    if(argc < 2){
        main_usage(stderr);
        return 1;
    }

    const struct target* target = target_x86();
    const char* output = NULL;
    int mode = MAIN_MODE_LINK;
    bool mode_given = false;
    const char** files = calloc(argc, sizeof(const char*));
    int file_count = 0;
    bool options_done = false;

    // Options may come anywhere, before or after the file names.
    for(int i = 1; i < argc; i++){
        const char* arg = argv[i];
        if(options_done || arg[0] != '-' || S_EQ(arg, "-")){
            files[file_count++] = arg;
        } else if(S_EQ(arg, "--")){
            options_done = true;
        } else if(S_EQ(arg, "-h") || S_EQ(arg, "--help")){
            main_usage(stdout);
            return 0;
        } else if(strncmp(arg, "-target=", 8) == 0 || strncmp(arg, "--target=", 9) == 0){
            const char* name = strchr(arg, '=') + 1;
            target = target_for_name(name);
            if(!target){
                fprintf(stderr, "Unknown target %s, expected x86, x86_64, aarch64, riscv64, "
                    "samos-x86_64, samos-aarch64 or samos-riscv64\n", name);
                return 1;
            }
        } else if(strncmp(arg, "-sysroot=", 9) == 0 || strncmp(arg, "--sysroot=", 10) == 0){
            main_sysroot = strchr(arg, '=') + 1;
        } else if(strncmp(arg, "-I", 2) == 0){
            const char* dir = main_option_value(argc, argv, &i, "-I");
            if(!dir){
                return 1;
            }
            compiler_add_include_dir(dir);
        } else if(strncmp(arg, "-D", 2) == 0){
            const char* def = main_option_value(argc, argv, &i, "-D");
            if(!def || main_add_define(def) < 0){
                return 1;
            }
        } else if(strncmp(arg, "-U", 2) == 0){
            const char* name = main_option_value(argc, argv, &i, "-U");
            if(!name || main_add_undef(name) < 0){
                return 1;
            }
        } else if(strncmp(arg, "-o", 2) == 0){
            output = main_option_value(argc, argv, &i, "-o");
            if(!output){
                return 1;
            }
        } else if(S_EQ(arg, "-c") || S_EQ(arg, "-S") || S_EQ(arg, "-E")){
            // The earliest stop wins, as in gcc: -E over -S over -c.
            int wanted = arg[1] == 'c' ? MAIN_MODE_OBJECT : arg[1] == 'S' ? MAIN_MODE_ASSEMBLY : MAIN_MODE_PREPROCESS;
            if(wanted > mode){
                mode = wanted;
            }
            mode_given = true;
        } else if(S_EQ(arg, "-v")){
            main_verbose = true;
        } else if(S_EQ(arg, "-O0")){
            main_compile_flags &= ~COMPILE_PROCESS_OPTIMIZE;
        } else if(S_EQ(arg, "-O") || S_EQ(arg, "-O1") || S_EQ(arg, "-O2") || S_EQ(arg, "-O3") || S_EQ(arg, "-Os")){
            // one level; the higher ones ask for no more.
            main_compile_flags |= COMPILE_PROCESS_OPTIMIZE;
        } else if(S_EQ(arg, "-fPIC") || S_EQ(arg, "-fpic")){
            main_compile_flags |= COMPILE_PROCESS_PIC;
        } else if(S_EQ(arg, "-shared")){
            main_shared = true;
            main_compile_flags |= COMPILE_PROCESS_PIC;
        } else if(S_EQ(arg, "-g") || S_EQ(arg, "-ggdb")){
            // the DWARF is in the asm itself, so
            // the assembler and linker commands stay as they are.
            main_compile_flags |= COMPILE_PROCESS_DEBUG;
        } else if(S_EQ(arg, "-g0")){
            main_compile_flags &= ~COMPILE_PROCESS_DEBUG;
        } else {
            main_error("unknown option %s (see %s --help)", arg, main_prog);
            return 1;
        }
    }

    if(!target->backend){
        fprintf(stderr, "Target %s has no code generator yet\n", target->name);
        return 1;
    }
    if(target->samos && (main_shared || (main_compile_flags & COMPILE_PROCESS_PIC))){
        main_error("target %s has no shared objects (-shared, -fPIC)", target->name);
        return 1;
    }
    target_set_current(target);
    main_find_own_includes();
    if(target->samos && main_setup_samos(target) < 0){
        return 1;
    }
    if(child_input){
        return compile_file(child_input, child_output, child_flags) == COMPILER_FILE_COMPILED_OK ? 0 : 1;
    }

    if(file_count == 0){
        main_error("no input files");
        return 1;
    }

    // The original form: `in out` or `in out object`. A trailing
    // `object` only exists in that form, so it decides even when `out`
    // ends in .o (`in.c out.o object`, which the tests use).
    bool legacy = !output && !mode_given
        && ((file_count == 3 && S_EQ(files[2], "object") && main_is_source(files[0]))
            || ((file_count == 2 || file_count == 3)
                && !main_is_source(files[1]) && !main_is_linker_input(files[1])));
    if(legacy){
        if(file_count == 3 && !S_EQ(files[2], "object")){
            main_error("unknown mode %s: the third argument of `%s in out [object]` can only be object", files[2], main_prog);
            return 1;
        }
        return main_legacy(target, files[0], files[1], file_count == 3);
    }

    int stdin_count = 0;
    for(int i = 0; i < file_count; i++){
        const char* file = files[i];
        if(!main_is_source(file) && !main_is_assembly(file) && !main_is_linker_input(file)){
            main_error("%s: unknown file type (expected .c, .s, .o, .a, .so or -)", file);
            return 1;
        }
        if(S_EQ(file, "-")){
            if(++stdin_count > 1){
                main_error("standard input (-) given more than once");
                return 1;
            }
        } else if(access(file, R_OK) != 0){
            main_error("%s: %s", file, strerror(errno));
            return 1;
        }
    }
    if(child_input){
        return compile_file(child_input, child_output, child_flags) == COMPILER_FILE_COMPILED_OK ? 0 : 1;
    }

    switch(mode){
        case MAIN_MODE_PREPROCESS:
            return main_preprocess(files, file_count, output);
        case MAIN_MODE_OBJECT:
        case MAIN_MODE_ASSEMBLY:
            return main_compile_each(target, mode, files, file_count, output);
    }
    return main_build_executable(target, files, file_count, output);
}
