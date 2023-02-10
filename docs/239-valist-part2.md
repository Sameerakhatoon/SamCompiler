# ch239 - implementing VALIST part 2

Closes the stdarg loop. After this, `va_list list; va_start(list,
num); va_arg(list, int); va_end(list);` all work end-to-end.

What landed:
- `pc_includes/stdarg.h`: uncomments the `va_arg` macro -
  `#define va_arg(list, type) __builtin_va_arg(list,
  sizeof(type))`.
- `preprocessor/static-includes/stdarg.c`:
  - `native__builtin_va_arg(generator, func, arguments)`:
    asserts arg count == 2; emits `; native__builtin_va_arg
    start`; calls `generator->gen_exp(list_arg,
    EXPRESSION_GET_ADDRESS)` so ebx holds the address of the
    list; checks the second arg is a NUMBER (sizeof() compiles
    to a literal at parse time per ch232); emits `add dword
    [ebx], <size>` to bump the stashed pointer past the read,
    then `mov dword eax, [ebx]`; returns the dereferenced
    value at eax as a void pointer.
  - `native_va_end(generator, func, arguments)`: void no-op
    (no state to tear down in this representation).
  - `preprocessor_stdarg_internal_include` registers all three
    natives: `va_start`, `__builtin_va_arg`, `va_end`.

Test: `tests/168-valist-complete.sh` compiles `int sum(int num,
...){ va_list list; va_start(list, num); for(i=0; i<num; i+=1)
result += va_arg(list, int); va_end(list); return result; }`
+ `int main(){ return sum(3, 20, 30, 40); }` and confirms the
emitted asm contains the va_start tag plus both native__builtin
_va_arg start / end tags - i.e. all three native callbacks
fired.
