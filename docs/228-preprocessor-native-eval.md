# ch228 - creating native definitions - part 2

Closes the loop on native preprocessor definitions. After
ch227 the registration path existed but the evaluation paths
still returned the prior `#warning + NULL / -1` stubs; this
chapter wires the NATIVE_CALLBACK branches to actually invoke
the registered callbacks.

What landed in `preprocessor/preprocessor.c`:
- `preprocessor_definition_value_for_native(definition,
  arguments)`: calls `definition->native.value(definition,
  arguments)`.
- `preprocessor_definition_evaluated_value_for_native(definition,
  arguments)`: calls `definition->native.evaluate(definition,
  arguments)`.
- `preprocessor_definition_value_with_arguments`'s
  NATIVE_CALLBACK branch now dispatches to
  `value_for_native` instead of returning NULL.
- `preprocessor_definition_evaluated_value`'s NATIVE_CALLBACK
  branch now dispatches to `evaluated_value_for_native`
  instead of returning -1.

Test: `tests/158-preprocessor-native-line-eval.sh` writes
`int x = __LINE__;` to a temp file, runs lex + preprocessor,
and confirms the resulting token_vec contains a NUMBER token
with value 1 (the line `__LINE__` appeared on) and no
surviving `__LINE__` identifier.
