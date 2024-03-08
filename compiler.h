#ifndef SAMCOMPILER_H
#define SAMCOMPILER_H

#include <stdio.h>

// Result codes returned from compile_file.
enum {
    COMPILER_FILE_COMPILED_OK,
    COMPILER_FAILED_WITH_ERRORS,
};

typedef struct compile_process compile_process_t;
typedef struct compile_process_input_file compile_process_input_file_t;

struct compile_process_input_file {
    FILE*       fp;
    const char* abs_path;
};

struct compile_process {
    // Flags controlling how this file should be compiled.
    int flags;

    struct compile_process_input_file cfile;

    FILE* ofile;
};

int                     compile_file(const char* filename, const char* out_filename, int flags);
struct compile_process* compile_process_create(const char* filename, const char* filename_out, int flags);

#endif
