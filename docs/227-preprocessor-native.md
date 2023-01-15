# ch227 - creating native definitions - part 1

Adds the native-callback machinery so preprocessor definitions
can hand off evaluation to C code instead of a token vector.
Lands the `__LINE__` macro as the first native built-in.

What landed:
- `Makefile`: `./build/native.o` joins OBJECTS; build rule for
  `./preprocessor/native.c -> ./build/native.o`.
- `compiler.h`: decls for
  `preprocessor_create_definitions`,
  `preprocessor_previous_token`,
  `preprocessor_build_value_vector_for_integer`,
  `preprocessor_definition_create_native`.
- `preprocessor/native.c` (new):
  - `preprocessor_line_macro_evaluate(definition, arguments)`:
    errors if `arguments != NULL` (`__LINE__` is a non-function
    macro); returns the previous token's `pos.line`.
  - `preprocessor_line_macro_value(definition, arguments)`:
    same arg check, returns `preprocessor_build_value_vector_
    for_integer(previous_token->pos.line)`.
  - `preprocessor_create_definitions(preprocessor)`: registers
    `__LINE__` via `preprocessor_definition_create_native`.
- `preprocessor/preprocessor.c`:
  - `preprocessor_initialize` now calls
    `preprocessor_create_definitions(preprocessor)` instead of
    the prior `#warning` stub.
  - `preprocessor_definition_create_native(name, evaluate,
    value, preprocessor)`: callocs a definition with type
    NATIVE_CALLBACK, stamps the callbacks, pushes onto
    preprocessor->definitions.

Test: `tests/157-preprocessor-native-line.sh` creates a fresh
compile_process, walks its preprocessor->definitions, and
confirms one NATIVE_CALLBACK definition named `__LINE__` is
registered.
