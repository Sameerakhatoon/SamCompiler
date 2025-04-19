#include "compiler.h"
#include "helpers/vector.h"
#include <assert.h>

// ch238: va_start(list, final_argument_in_function). Emits the asm
// that stores the address just past `final_argument_in_function`
// (the first variadic argument) into `list`.
void native_va_start(struct generator* generator, struct native_function* func, struct vector* arguments)
{
    struct compile_process* compiler = generator->compiler;
    if (vector_count(arguments) != 2)
    {
        compiler_error(compiler, "va_start expects two arguments %i was provided", vector_count(arguments));
    }

    struct node* list_arg  = vector_peek_ptr(arguments);
    struct node* stack_arg = vector_peek_ptr(arguments);
    if (stack_arg->type != NODE_TYPE_IDENTIFIER)
    {
        compiler_error(compiler, "Expecting a valid stack argument for va_start");
    }
    generator->asm_push("; va_start on variable %s", stack_arg->sval);
    vector_set_peek_pointer(arguments, 0);
    generator->gen_exp(generator, stack_arg, EXPRESSION_GET_ADDRESS);

    struct resolver_result* result = resolver_follow(compiler->resolver, list_arg);
    assert(resolver_result_ok(result));
    struct resolver_entity* list_arg_entity = resolver_result_entity_root(result);
    struct generator_entity_address address_out;
    generator->entity_address(generator, list_arg_entity, &address_out);
    struct resolver_result* last = resolver_follow(compiler->resolver, stack_arg);
    assert(resolver_result_ok(last));
    int last_size = datatype_size(&resolver_result_entity_root(last)->dtype);
    // the store itself is target-specific.
    generator->va_start_store(generator, address_out.address, last_size);
    generator->asm_push("; va_start end for variable %s", stack_arg->sval);

    struct datatype void_datatype;
    datatype_set_void(&void_datatype);
    generator->ret(&void_datatype, "0");
}

// ch239: __builtin_va_arg(list, size) - return the address the list
// holds and step it past an argument of `size` bytes. The user-facing
// va_arg macro in pc_includes/stdarg.h plugs the sizeof() in for
// `size` and reads the type at the address.
void native__builtin_va_arg(struct generator* generator, struct native_function* func, struct vector* arguments)
{
    struct compile_process* compiler = generator->compiler;
    if (vector_count(arguments) != 2 && vector_count(arguments) != 3)
    {
        compiler_error(compiler, "va_arg expects two arguments, %i provided", vector_count(arguments));
    }
    // A21: the va_arg macro passes `(type*)0` as well, for its type. A
    // double is 8 bytes of the stack (4-aligned in cdecl), which the
    // address and the step of `size` cover. A float passed to `...` was
    // promoted to double, so there is no float to read.
    if (vector_count(arguments) == 3)
    {
        struct node* type_argument = *(struct node**)vector_at(arguments, 2);
        if (type_argument->type == NODE_TYPE_CAST)
        {
            struct datatype* type = datatype_pointer_reduce(&type_argument->cast.dtype, 1);
            if (datatype_is_floating(type) && type->size != DATA_SIZE_DDWORD)
            {
                compiler_error(compiler, "va_arg of a float: a float passed to `...` is promoted to double, read a double\n");
            }
        }
    }

    generator->asm_push("; native__builtin_va_arg start");
    vector_set_peek_pointer(arguments, 0);
    struct node* list_arg = vector_peek_ptr(arguments);
    while (list_arg->type == NODE_TYPE_EXPRESSION_PARENTHESES)
    {
        list_arg = list_arg->parenthesis.exp;
    }
    // `va_arg(*ap, type)` with a `va_list* ap`: the list's address is
    // the pointer's value. The address of an indirection loaded through
    // it once more and stepped the argument instead of the list.
    if (list_arg->type == NODE_TYPE_UNARY && S_EQ(list_arg->unary.op, "*"))
    {
        generator->gen_exp(generator, list_arg->unary.operand, 0);
    }
    else
    {
        generator->gen_exp(generator, list_arg, EXPRESSION_GET_ADDRESS);
    }
    struct node* size_argument = vector_peek_ptr(arguments);
    if (size_argument->type != NODE_TYPE_NUMBER)
    {
        compiler_error(compiler, "native__builtin_va_arg expects a second argument to be numeric size of variable argument. Use the macro va_arg for automation");
    }

    // stepping the list and loading the argument is
    // target-specific (slot size, registers).
    generator->va_arg_fetch(generator, size_argument->llnum);
    generator->asm_push("; native__builtin_va_arg end");
}

// ch239: va_end. No state to tear down here; just emit a void return.
void native_va_end(struct generator* generator, struct native_function* func, struct vector* arguments)
{
    struct datatype void_datatype;
    datatype_set_void(&void_datatype);
    generator->ret(&void_datatype, "0");
}

void preprocessor_stdarg_internal_include(struct preprocessor* preprocessor, struct preprocessor_included_file* file)
{
    native_create_function(preprocessor->compiler, "va_start",
        &(struct native_function_callbacks){ .call = native_va_start });

    native_create_function(preprocessor->compiler, "__builtin_va_arg",
        &(struct native_function_callbacks){ .call = native__builtin_va_arg });

    native_create_function(preprocessor->compiler, "va_end",
        &(struct native_function_callbacks){ .call = native_va_end });

    // va_copy on x86_64, where the va_list is a pointer to
    // state of its own (exp64_stdarg). The x86 backend never sees it.
    native_create_function(preprocessor->compiler, "__builtin_va_copy",
        &(struct native_function_callbacks){ .call = native_va_end });
}
