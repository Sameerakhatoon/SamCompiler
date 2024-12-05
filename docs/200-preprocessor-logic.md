# ch200 - beginning the preprocessor logic

Book labels this "Beginning the preprocessor logic" with no
lecture number between 199 and 201; we slot it in as ch200.

Wires the preprocessor into the compile pipeline. Adds the
preprocessor struct hierarchy, plumbs it through
compile_process_create, and runs a preprocessor pass between
lex and parse.

What landed:
- `compiler.h`:
  - Includes `<linux/limits.h>` for `PATH_MAX`.
  - New forward decls: `struct preprocessor`,
    `struct preprocessor_definition`,
    `struct preprocessor_function_argument`,
    `struct preprocessor_included_file`, and
    `PREPROCESSOR_STATIC_INCLUDE_HANDLER_POST_CREATION` typedef.
  - `enum PREPROCESSOR_DEFINITION_STANDARD / _MACRO_FUNCTION /
    _NATIVE_CALLBACK / _TYPEDEF`.
  - `struct preprocessor_function_argument { struct vector*
    tokens; }`, `struct preprocessor_function_arguments
    { struct vector* arguments; }`.
  - Native-callback typedefs `..._NATIVE_CALL_EVALUATE`,
    `..._NATIVE_CALL_VALUE`.
  - `struct preprocessor_definition`: tagged union over
    standard / typedef / native callback variants.
  - `struct preprocessor_included_file { char filename[PATH_MAX]; }`.
  - `struct preprocessor` with definitions / exp_vector /
    expressionable / compiler / includes vectors.
  - Decls for `preprocessor_create`, `preprocessor_run`.
  - `struct compile_process` gains
    `token_vec_original`, `include_dirs`, `preprocessor`.
  - `compile_process_create` signature gains
    `struct compile_process* parent_process` so nested compiles
    (for includes) can inherit the parent's preprocessor +
    include dirs.
- `cprocess.c`:
  - compile_process_create now accepts parent_process and
    initializes token_vec + token_vec_original up front.
    Nested-process branch reuses the parent's preprocessor +
    include_dirs; top-level branch creates fresh ones.
- `compiler.c`:
  - compile_file passes `NULL` for parent_process and now
    routes lexer output through `process->token_vec_original`
    via `lex_process_tokens(lex_process)`, then calls
    `preprocessor_run` before `parse`.
- `preprocessor/preprocessor.c`:
  - `preprocessor_execute_warning` / `preprocessor_execute_error`
    wrap compiler_warning / compiler_error with the `#warning` /
    `#error` prefix.
  - `preprocessor_add_included_file` / `preprocessor_create_static_include`.
  - `preprocessor_is_keyword` recognizes `defined`.
  - `preprocessor_build_value_vector_for_integer` returns a
    single-token NUMBER vector for an integer literal.
  - `preprocessor_token_vec_push_keyword_and_identifier` pushes
    a keyword + identifier token pair onto a vector.
  - `preprocessor_node_create` clones a preprocessor_node onto
    the heap (memcpy from a stack copy).
  - `preprocessor_definition_argument_exists` scans the
    standard argument vector for a name match.
  - `preprocessor_function_argument_at` /
    `preprocessor_token_push_to_function_arguments` /
    `preprocessor_function_argument_push_to_vec`.
  - `preprocessor_initialize` zeros the preprocessor and
    creates its definitions + includes vectors (with a TODO
    warning for default definitions).
  - `preprocessor_create` allocates a preprocessor, initializes,
    and links it to the compile_process.
  - `preprocessor_next_token` peek-increments the original
    token vector.
  - `preprocessor_handle_token` switch with only a default
    case that copies through to compiler->token_vec (deviation
    below).
  - `preprocessor_run` walks the original token vector and
    hands each token to handle_token.

Deviation from upstream: the upstream
`preprocessor_handle_token` switch has no cases at all - it
just falls off. That means token_vec stays empty and the
parser sees nothing, breaking the rest of the pipeline. We add
a `default:` arm that copies the token through to
`compiler->token_vec`. Later chapters introduce explicit cases
that handle `#define`, `#include`, expansion, etc.; until then
this keeps every existing end-to-end test green.

Test: `tests/130-preprocessor-pipeline.sh` confirms a trivial
`int main(){ return 0; }` source still reaches codegen via the
new pipeline and that `compile_process_create` wires the
preprocessor + include_dirs + token vectors with the expected
shape (empty definitions / includes vectors).
