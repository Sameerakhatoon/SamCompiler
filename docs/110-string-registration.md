# ch110 - building the string registration system

`.rodata` gets a real emitter, fed by a deduplicating string table on
the code generator.

What landed in `compiler.h`:
- `struct string_table_element { const char* str; char label[50]; }`.
- `code_generator.string_table` (vector of element pointers).

What landed in `codegen.c`:
- `codegenerator_new`: also allocates `string_table`.
- `asm_push_no_nl`: like `asm_push` but no trailing newline, so the
  string emitter can build one line piecewise.
- `codegen_get_label_for_string(str)`: linear scan of the table for an
  existing entry; returns its label or NULL.
- `codegen_register_string(str)`: returns the existing label if any;
  otherwise allocates a new element, stamps the label `str_<N>`, and
  pushes onto the table.
- `codegen_write_string_char_escaped(c)`: `\n` -> `10`, `\t` -> `9`,
  emitted as raw decimal. Returns false for unhandled chars.
- `codegen_write_string(element)`: emits `<label>: db 'c', 'c', ... 0`
  using the escape helper, then closes the line with `0` and a
  newline.
- `codegen_write_strings`: walks the string table and calls
  `codegen_write_string` per element. Replaces the ch105 `#warning`
  placeholder.
- `codegen()`: smoke-test by registering "Hello world!!" three times
  (must dedupe) and "Abc\\n" once, then emits `.rodata`. The ch108
  entry/exit smoke at end-of-codegen is gone (its job was wiring; the
  string smoke now exercises the rest).

Tests:
- `tests/62-string-table.sh` checks "Hello world!!" appears exactly
  once and that "Abc\\n" becomes `'A', 'b', 'c', 10, 0`.
- `tests/61-label-system.sh` was reduced to confirming the generator
  + entry/exit vectors are allocated, since the always-on entry/exit
  emitter is gone.
