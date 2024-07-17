# ch100 - adjusting the function stack size

`parse_body` now bubbles the accumulated `variable_size` up to the
enclosing function's `func.stack_size` when the body sits inside a
function body (`HISTORY_FLAG_INSIDE_FUNCTION_BODY`). Replaces the
`#warning "Don't forget to adjust the function stack size"` placeholder.

Test: `tests/56-func-stack-size.sh` parses
`int main() { int a; int b; }` and asserts `func.stack_size == 8`.
