# ch238 - implementing VALIST part 1

Replaces the ch236/ch237 `test` stub with a real `va_start`
native function and adds `pc_includes/stdarg.h`. After this,
`#include <stdarg.h>` exposes a `va_list` typedef and a working
`va_start(list, last_fixed_arg)`.

What landed:
- `pc_includes/stdarg.h` (new):
  - Include guards + `#include <stdarg-internal.h>` to pull the
    static include in (registers the native va_start).
  - `typedef int __builtin_va_list;` then `typedef
    __builtin_va_list va_list;`.
  - Commented placeholder for `va_arg` (lands in part 2).
- `preprocessor/static-includes/stdarg.c`:
  - Drops the `test` stub.
  - `native_va_start(generator, func, arguments)`: validates
    arg count == 2; pulls the list (NODE_TYPE_IDENTIFIER) +
    stack-arg names; emits a `; va_start on variable <stack>`
    tag; calls `generator->gen_exp(stack_arg,
    EXPRESSION_GET_ADDRESS)` so the address of the last fixed
    argument lands in ebx; runs `resolver_follow` on the list
    identifier; grabs its address via
    `generator->entity_address`; emits `mov dword
    [<list_addr>], ebx` so `list` holds that pointer; closes
    with a void return.
  - `preprocessor_stdarg_internal_include` now registers
    `va_start` instead of `test`.
- `codegen.c`:
  - `codegen_gen_exp` switches from `codegen_history_down(
    remembered.history, flags)` to `codegen_history_begin(flags)`.
    The remembered.history slot is always NULL at native
    callback entry; history_begin synthesizes a fresh top-level
    history.

Test: `tests/167-native-function-dispatch.sh` (refreshed for
ch238) compiles `int sum(int num, ...){ va_list list;
va_start(list, num); ... }` + `int main(){ return sum(3, ...); }`
and confirms the emitted asm contains both the `; NATIVE
FUNCTION va_start` dispatch tag and the va_start callback's
`; va_start on variable num` line.
