#include <stdio.h>
#include <stdlib.h>
#include "compiler.h"
#include "helpers/vector.h"

// ch229: ordered list of default include directories tried by
// compile_include when the source uses <foo.h> / "foo.h".
const char* default_include_dirs[] = {
    "./pc_includes",
    "../pc_includes",
    "/usr/include/peach-includes",
    "/usr/include",
};

static struct vector* compiler_extra_include_dirs;

void compiler_add_include_dir(const char* dir){
    if(!compiler_extra_include_dirs){
        compiler_extra_include_dirs = vector_create(sizeof(const char*));
    }
    vector_push(compiler_extra_include_dirs, &dir);
}

const char* compiler_include_dir_begin(struct compile_process* process){
    vector_set_peek_pointer(process->include_dirs, 0);
    const char* dir = vector_peek_ptr(process->include_dirs);
    return dir;
}

const char* compiler_include_dir_next(struct compile_process* process){
    const char* dir = vector_peek_ptr(process->include_dirs);
    return dir;
}

// The driver sets this to the pc_includes next to the compiler binary.
// Programs that call compile_file themselves (the chapter tests) keep
// the old list above.
static const char* compiler_own_include_dir;

void compiler_set_own_include_dir(const char* dir){
    compiler_own_include_dir = dir;
}

void compiler_setup_default_include_directories(struct vector* include_vec){
    if(compiler_own_include_dir){
        static const char* usr_include = "/usr/include";
        vector_push(include_vec, &compiler_own_include_dir);
        vector_push(include_vec, &usr_include);
        return;
    }
    size_t total = sizeof(default_include_dirs) / sizeof(const char*);
    for(int i = 0; i < (int)total; i++){
        vector_push(include_vec, &default_include_dirs[i]);
    }
}

struct compile_process* compile_process_create(const char* filename, const char* filename_out, int flags, struct compile_process* parent_process){
    struct compile_process* process = 0;
    FILE* file     = 0;
    FILE* out_file = 0;

    // "-" is standard input.
    bool from_stdin = S_EQ(filename, "-");
    file = from_stdin ? stdin : fopen(filename, "r");
    if(!file){
        goto out_err;
    }

    if(filename_out){
        out_file = fopen(filename_out, "w");
        if(!out_file){
            goto out_err;
        }
    }

    process = calloc(1, sizeof(struct compile_process));
    if(!process){
        goto out_err;
    }

    process->flags                = flags;
    // includes compile for the same machine as their parent;
    // a top-level process takes the current target (x86 unless the
    // driver chose another).
    process->target               = parent_process ? parent_process->target : target_current();
    process->cfile.fp             = file;
    process->cfile.abs_path       = filename;
    process->ofile                = out_file;
    process->generator            = codegenerator_new(process);
    // ch137: default resolver lives next to the codegen.
    process->resolver             = resolver_default_new_process(process);
    process->pos.line             = 1;
    process->pos.col              = 1;
    process->pos.filename         = filename;
    process->token_vec            = vector_create(sizeof(struct token));
    process->token_vec_original   = vector_create(sizeof(struct token));
    process->node_vec             = vector_create(sizeof(struct node*));
    process->node_tree_vec        = vector_create(sizeof(struct node*));

    // ch66: kick off the symbol resolver here so anyone creating a
    // compile_process gets a usable symbol table without parse() having
    // to do double duty.
    symresolver_initialize(process);
    symresolver_new_table(process);

    // ch200: nested compile_processes (e.g. for includes) inherit the
    // parent's preprocessor + include-dirs vector; top-level creates fresh.
    if(parent_process){
        process->preprocessor = parent_process->preprocessor;
        process->include_dirs = parent_process->include_dirs;
    }
    else{
        process->preprocessor = preprocessor_create(process);
        process->include_dirs = vector_create(sizeof(const char*));
        if(compiler_extra_include_dirs){
            vector_set_peek_pointer(compiler_extra_include_dirs, 0);
            const char* dir = vector_peek_ptr(compiler_extra_include_dirs);
            while(dir){
                vector_push(process->include_dirs, &dir);
                dir = vector_peek_ptr(compiler_extra_include_dirs);
            }
        }
        // ch229: load the default include directories. Not for
        // a samos target, whose headers must not be glibc's.
        if(!process->target->samos){
            compiler_setup_default_include_directories(process->include_dirs);
        }
    }

    // ch235: resolve the source path to absolute form so include
    // tracking can compare against canonical paths, and hand the
    // node vectors to the global node module so node_create works
    // for this process.
    char* path = malloc(PATH_MAX);
    if(from_stdin){
        strcpy(path, "<stdin>");
    } else {
        realpath(filename, path);
    }
    process->cfile.abs_path = path;
    // The path given may be a caller's stack buffer (compile_include).
    process->pos.filename   = path;
    node_set_vector(process->node_vec, process->node_tree_vec);

    return process;

out_err:
    if(out_file){
        fclose(out_file);
    }
    if(file && file != stdin){
        fclose(file);
    }
    return 0;
}

char compile_process_next_char(struct lex_process* lex_process){
    struct compile_process* compiler = lex_process->compiler;
    compiler->pos.col += 1;
    char c = getc(compiler->cfile.fp);
    if(c == '\n'){
        compiler->pos.line += 1;
        compiler->pos.col   = 1;
    }
    return c;
}

char compile_process_peek_char(struct lex_process* lex_process){
    struct compile_process* compiler = lex_process->compiler;
    char c = getc(compiler->cfile.fp);
    ungetc(c, compiler->cfile.fp);
    return c;
}

void compile_process_push_char(struct lex_process* lex_process, char c){
    struct compile_process* compiler = lex_process->compiler;
    ungetc(c, compiler->cfile.fp);
}
