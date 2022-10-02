# ch144 - modifying parser_datatype_init_type_and_size

After the per-expected-type init switch, `parser_datatype_init_type_and_size`
now stamps `DATATYPE_FLAG_IS_POINTER` and the actual `pointer_depth`
onto the datatype whenever `pointer_depth > 0`. Previously the depth
was tracked by the caller but never written through to the parsed
datatype's flags.

Test: `tests/91-pointer-depth-flag.sh` parses `int** p;` and confirms
`p->var.type.flags & DATATYPE_FLAG_IS_POINTER` and
`var.type.pointer_depth == 2`.
