# ch229 - implementing includes - part 1

Scaffolds the `#include` machinery. The directive itself
isn't wired yet (lands in part 2); this chapter lays down the
include-directory iteration, the file-existence helper, and the
`compile_include` driver that lex+preprocesses an included file
without running parse / codegen.

What landed:
- `compiler.h`:
  - Decls for `compiler_include_dir_begin/next`,
    `compiler_setup_default_include_directories`,
    `compile_include`, `file_exists`.
- `cprocess.c`:
  - `default_include_dirs[]` static array containing
    `./pc_includes`, `../pc_includes`,
    `/usr/include/peach-includes`, `/usr/include`.
  - `compiler_include_dir_begin(process)`:
    vector_set_peek_pointer(0); vector_peek_ptr().
  - `compiler_include_dir_next(process)`:
    vector_peek_ptr() (no reset).
  - `compiler_setup_default_include_directories(include_vec)`:
    pushes each default_include_dirs[i] into include_vec.
  - top-level `compile_process_create` (no parent) now calls
    `compiler_setup_default_include_directories(process->include_dirs)`
    instead of the prior `// setup default include dirs...` stub.
- `helper.c`:
  - `file_exists(filename)`: fopen("r") + fclose.
- `compiler.c`:
  - `compile_include_for_include_dir(include_dir, filename,
    parent)`: builds `<dir>/<file>`, falls back to the bare
    filename if joined path is missing; creates a child
    compile_process, runs lex + preprocessor_run; returns the
    process (parse + codegen stay on the parent).
  - `compile_include(filename, parent)`: walks parent's
    include_dirs via the begin/next iterators, returns the
    first successful sub-process or NULL.

Test: `tests/159-include-dirs.sh` confirms a fresh
compile_process has 4 include dirs registered, file_exists
returns 1 for `/dev/null` and 0 for a bogus path, and
compiler_include_dir_begin returns a non-NULL pointer.
