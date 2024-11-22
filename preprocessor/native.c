#include "compiler.h"
#include "helpers/vector.h"
#include <stdlib.h>
#include <string.h>

int preprocessor_line_macro_evaluate(struct preprocessor_definition* definition, struct preprocessor_function_arguments* arguments)
{
    struct preprocessor*    preprocessor = definition->preprocessor;
    struct compile_process* compiler     = preprocessor->compiler;

    if (arguments)
    {
        compiler_error(compiler, "__LINE__ macro expects no arguments");
    }

    struct token* previous_token = preprocessor_previous_token(compiler);
    return previous_token->pos.line;
}

struct vector* preprocessor_line_macro_value(struct preprocessor_definition* definition, struct preprocessor_function_arguments* arguments)
{
    struct preprocessor*    preprocessor = definition->preprocessor;
    struct compile_process* compiler     = preprocessor->compiler;

    if (arguments)
    {
        compiler_error(compiler, "__LINE__ macro expects no arguments");
    }
    struct token* previous_token = preprocessor_previous_token(compiler);
    return preprocessor_build_value_vector_for_integer(previous_token->pos.line);
}

// __FILE__ is the path of the file that holds the token, as a string
// literal; in #if it is not a number, so it evaluates to 0.
int preprocessor_file_macro_evaluate(struct preprocessor_definition* definition, struct preprocessor_function_arguments* arguments)
{
    if (arguments)
    {
        compiler_error(definition->preprocessor->compiler, "__FILE__ macro expects no arguments");
    }
    return 0;
}

struct vector* preprocessor_file_macro_value(struct preprocessor_definition* definition, struct preprocessor_function_arguments* arguments)
{
    struct compile_process* compiler = definition->preprocessor->compiler;
    if (arguments)
    {
        compiler_error(compiler, "__FILE__ macro expects no arguments");
    }
    struct token* previous_token = preprocessor_previous_token(compiler);
    struct vector* token_vec = vector_create(sizeof(struct token));
    struct token t1 = {};
    t1.type = TOKEN_TYPE_STRING;
    t1.sval = previous_token->pos.filename ? previous_token->pos.filename : "";
    vector_push(token_vec, &t1);
    return token_vec;
}

// the target's predefined macros (__i386__, __x86_64__,
// __aarch64__, __riscv ...). This runs from preprocessor_create
// before preprocessor->compiler is set, and only for the top-level
// process, whose target is the current one.
static void preprocessor_create_target_definitions(struct preprocessor* preprocessor)
{
    const struct target_macro* macro = target_current()->macros;
    while (macro && macro->name)
    {
        preprocessor_definition_create(macro->name, preprocessor_build_value_vector_for_integer(macro->value), NULL, preprocessor);
        macro++;
    }
}

// GNU spellings that system headers and code written for gcc use. None
// of them changes what this compiler generates, so they expand to
// nothing: __attribute__((...)) with whatever it holds, __extension__,
// and the reserved spellings of restrict and inline.
static void preprocessor_create_gnu_definitions(struct preprocessor* preprocessor)
{
    static const char* empty[] = { "__extension__", "__restrict", "__restrict__", "__inline", "__inline__", 0 };
    for (int i = 0; empty[i]; i++)
    {
        preprocessor_definition_create(empty[i], vector_create(sizeof(struct token)), NULL, preprocessor);
    }
    // Variadic, so the commas in __attribute__((format(printf, 1, 2)))
    // are fine.
    struct vector* arguments = vector_create(sizeof(const char*));
    const char* argument = "__VA_ARGS__";
    vector_push(arguments, &argument);
    struct preprocessor_definition* attribute = preprocessor_definition_create("__attribute__", vector_create(sizeof(struct token)), arguments, preprocessor);
    attribute->standard.variadic = true;
}

void preprocessor_create_definitions(struct preprocessor* preprocessor)
{
    preprocessor_create_gnu_definitions(preprocessor);
    preprocessor_definition_create_native("__LINE__", preprocessor_line_macro_evaluate, preprocessor_line_macro_value, preprocessor);
    preprocessor_definition_create_native("__FILE__", preprocessor_file_macro_evaluate, preprocessor_file_macro_value, preprocessor);
    // C11 6.10.8.1. __STDC_VERSION__ says C99: the language this
    // compiler aims at.
    preprocessor_definition_create("__STDC__", preprocessor_build_value_vector_for_integer(1), NULL, preprocessor);
    preprocessor_definition_create("__STDC_VERSION__", preprocessor_build_value_vector_for_integer(199901), NULL, preprocessor);
    preprocessor_definition_create("__STDC_HOSTED__", preprocessor_build_value_vector_for_integer(1), NULL, preprocessor);
    preprocessor_create_target_definitions(preprocessor);
}

// ch236: register a C-implemented native function as a symbol so the
// resolver / codegen can dispatch through the v-table.
struct symbol* native_create_function(struct compile_process* compiler, const char* name,
                                      struct native_function_callbacks* callbacks)
{
    struct native_function* func = calloc(1, sizeof(struct native_function));
    memcpy(&func->callbacks, callbacks, sizeof(func->callbacks));
    func->name = name;
    return symresolver_register_symbol(compiler, name, SYMBOL_TYPE_NATIVE_FUNCTION, func);
}

// ch237: look up a previously-registered native function by name.
struct native_function* native_function_get(struct compile_process* compiler, const char* name)
{
    struct symbol* sym = symresolver_get_symbol_for_native_function(compiler, name);
    if (!sym)
    {
        return NULL;
    }
    return sym->data;
}
