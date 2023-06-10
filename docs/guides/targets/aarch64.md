# Target guide: aarch64

Every command below was run in WSL Ubuntu 24.04 from
a SamCompiler checkout; the output is pasted as it came out,
trimmed only where marked `...`.

## 1. What this target is

| | |
|---|---|
| `-target=` | `aarch64` |
| Data model | LP64: int 4, long 8, pointer 8 |
| ABI | AAPCS64 (Linux): arguments in x0-x7, result in x0, struct result address in x8 |
| Assembler | GNU as (`aarch64-linux-gnu-as`) |
| Linker | `aarch64-linux-gnu-gcc -static` |
| Runs | under qemu-user (`qemu-aarch64`) on the x86-64 host |
| Backend | `codegen_aarch64.c` |

SamCompiler is still a native x86-64 program; for this target it
acts as a cross compiler, and the cross binutils/gcc package does
the assembling and linking.

## 2. Prerequisites

```
sudo apt-get install -y gcc-aarch64-linux-gnu qemu-user qemu-user-binfmt gdb-multiarch file
```

Check them:

```
$ aarch64-linux-gnu-gcc --version | head -1
aarch64-linux-gnu-gcc (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0
$ aarch64-linux-gnu-as --version | head -1
GNU assembler (GNU Binutils for Ubuntu) 2.42
$ qemu-aarch64 --version | head -1
qemu-aarch64 version 8.2.2 (Debian 1:8.2.2+ds-0ubuntu1.18)
$ gdb-multiarch --version | head -1
GNU gdb (Ubuntu 15.1-1ubuntu1~24.04.1) 15.1
$ dpkg -l gcc-aarch64-linux-gnu binutils-aarch64-linux-gnu qemu-user qemu-user-binfmt gdb-multiarch libc6-dev-arm64-cross | grep '^ii' | awk '{print $2, $3}'
binutils-aarch64-linux-gnu 2.42-4ubuntu2.10
gcc-aarch64-linux-gnu 4:13.2.0-7ubuntu1
gdb-multiarch 15.1-1ubuntu1~24.04.1
libc6-dev-arm64-cross 2.39-0ubuntu8cross1
qemu-user 1:8.2.2+ds-0ubuntu1.18
qemu-user-binfmt 1:8.2.2+ds-0ubuntu1.18
```

`libc6-dev-arm64-cross` (pulled in by the gcc package) is the
aarch64 sysroot, in `/usr/aarch64-linux-gnu`.

## 3. Build the compiler

```
$ ./build.sh
```

The same native build for every target.

## 4. Compile a C file for this target

`/tmp/g/add.c` (the same file as in the x86 guides):

```c
int add(int a, int b)
{
    return a + b;
}

int main()
{
    return add(2, 3);
}
```

```
$ ./main -target=aarch64 /tmp/g/add.c /tmp/g/add_a64
...
everything compiled fine
aarch64-linux-gnu-as /tmp/g/add_a64 -o /tmp/g/add_a64.o && aarch64-linux-gnu-gcc -static /tmp/g/add_a64.o -o /tmp/g/add_a64
run with: qemu-aarch64 /tmp/g/add_a64
```

Stop after the assembler with `object`; the GNU as source is the
file you name, the object is `<name>.o`:

```
$ ./main -target=aarch64 /tmp/g/add.c /tmp/g/add_a64.s object | tail -2
everything compiled fine
aarch64-linux-gnu-as /tmp/g/add_a64.s -o /tmp/g/add_a64.s.o
```

## 5. Read the output

```
.text
.p2align 2                  // instructions are 4 bytes, 4-byte aligned
.globl add
.type add, %function
add:
sub sp, sp, #64             // home area for x0-x7
stp x29, x30, [sp, #-16]!   // save fp and the return address (lr)
mov x29, sp
stp x0, x1, [x29, #16]      // spill the argument registers
stp x2, x3, [x29, #32]
stp x4, x5, [x29, #48]
stp x6, x7, [x29, #64]
ldursw x0, [x29, #16]       // a, sign-extended to 64 bits
str x0, [sp, #-16]!         // push: one 16-byte slot
ldursw x0, [x29, #24]       // b
str x0, [sp, #-16]!
ldr x1, [sp], #16           // pop the right operand
ldr x0, [sp], #16           // pop the left operand
add w0, w0, w1              // int add on w registers
sxtw x0, w0                 // back to the 64-bit form
str x0, [sp, #-16]!
ldr x0, [sp], #16           // result in x0
ldp x29, x30, [sp], #16
add sp, sp, #64             // drop the home area
ret                         // branch to x30
```

and the call in `main`:

```
adrp x9, add                // page of add
add x9, x9, :lo12:add       // plus the low 12 bits
str x9, [sp, #-16]!         // callee kept under the arguments
mov x0, #3
str x0, [sp, #-16]!
mov x0, #2
str x0, [sp, #-16]!
ldr x0, [sp], #16           // first argument
ldr x1, [sp], #16           // second argument
ldr x9, [sp], #16           // callee
blr x9                      // call, return address in x30
sxtw x0, w0
```

## 6. Assemble and link by hand

```
$ cd /tmp/g
$ aarch64-linux-gnu-as add_a64.s -o add_a64.o
$ aarch64-linux-gnu-gcc -static add_a64.o -o add_a64_manual
$ qemu-aarch64 ./add_a64_manual; echo "exit=$?"
exit=5
```

The driver links statically so qemu-user needs nothing else:

```
$ file /tmp/g/add_a64
/tmp/g/add_a64: ELF 64-bit LSB executable, ARM aarch64, version 1 (GNU/Linux), statically linked, BuildID[sha1]=c872bb3bcb2e8dcb8e3fb774cc444f6dcb8f80cb, for GNU/Linux 3.7.0, not stripped
```

A dynamic link also works, but qemu then has to find the aarch64
dynamic loader and libc. Point it at the cross sysroot with `-L`:

```
$ aarch64-linux-gnu-gcc add_a64.o -o add_a64_dyn
$ qemu-aarch64 ./add_a64_dyn; echo "exit=$?"
qemu-aarch64: Could not open '/lib/ld-linux-aarch64.so.1': No such file or directory
exit=255
$ qemu-aarch64 -L /usr/aarch64-linux-gnu ./add_a64_dyn; echo "exit=$?"
exit=5
$ file add_a64_dyn
add_a64_dyn: ELF 64-bit LSB pie executable, ARM aarch64, version 1 (SYSV), dynamically linked, interpreter /lib/ld-linux-aarch64.so.1, BuildID[sha1]=c4ead4dc509817f5ca29f4cf68759e40ea05aa71, for GNU/Linux 3.7.0, not stripped
```

(Unlike x86_64, the aarch64 asm builds addresses with
`adrp`/`add :lo12:`, which is PC-relative, so the default PIE link
works.)

## 7. Run it

```
$ qemu-aarch64 /tmp/g/add_a64; echo "exit=$?"
exit=5
```

With `qemu-user-binfmt` installed the kernel hands aarch64 ELF files
to qemu by itself, so `/tmp/g/add_a64` runs directly too:

```
$ /tmp/g/add_a64; echo "binfmt exit=$?"
binfmt exit=5
```

The tests call `qemu-aarch64` explicitly so they do not depend on
binfmt.

## 8. Run the samples

`samples/run_all.sh ARCH=aarch64` arrives with the samples runner. Until then
`tests/211-aarch64-run.sh` builds and runs every sample and every
golden input under qemu-aarch64. Run by hand, all 54 pass:

```
01_return_const exit=42 expected=42 OK
...
25_struct_return exit=9 expected=9 OK
...
37_printf exit=0 expected=0 OK out=[v=42 s=hi]
...
10_bubble_sort exit=1 expected=1 OK
...
14_token_scanner exit=4 expected=4 OK
passed: 54 failed: 0
```

(Sample 10 failed was found; see Troubleshooting.)

## 9. Run the tests

```
$ bash tests/210-aarch64-codegen.sh
210-aarch64-codegen ... ok
$ bash tests/211-aarch64-run.sh
211-aarch64-run ... ok
$ bash tests/176-x86-golden.sh
176-x86-golden ... ok
```

211 prints `skip (no aarch64-linux-gnu-as)` (or the missing tool)
and exits 0 on a machine without the cross toolchain or qemu.

## 10. Debug

qemu-user has a gdb stub: `-g 1234` starts the program stopped and
waits for a debugger on port 1234. gdb-multiarch understands the
aarch64 binary:

```
$ cat gdbarm.cmd
target remote :1234
break add
continue
stepi 7
x/i $pc
info registers x0 x1 x29 x30 sp
x/4gx $x29
x/dw $x29+16
x/dw $x29+24
kill
$ qemu-aarch64 -g 1234 ./add_a64_manual &
$ gdb-multiarch -q -batch -x gdbarm.cmd ./add_a64_manual
...
Breakpoint 1, 0x00000000004007cc in add ()
0x00000000004007e8 in add ()
=> 0x4007e8 <add+56>:	sxtw	x0, w0
x0             0x5                 5
x1             0x3                 3
x29            0x4000008000a0      70368752566432
x30            0x40085c            4196444
sp             0x4000008000a0      0x4000008000a0
0x4000008000a0:	0x00004000008000f0	0x000000000040085c
0x4000008000b0:	0x0000000000000002	0x0000000000000003
0x4000008000b0:	2
0x4000008000b8:	3
```

`x0` already holds the sum, `[x29]` is the caller's x29, `[x29+8]`
is x30 (the return address, also live in the x30 register), and
`[x29+16]`/`[x29+24]` are the home-area slots holding the two
arguments. gdb also prints `warning: BFD: ... corrupt string table
index` for qemu's vDSO; it is harmless.

Interactively: `layout asm`, `stepi`, `info registers`, `x/8gx $sp`
(each expression slot is 16 bytes, the value in the low 8).

## 11. Troubleshooting

| Symptom | Cause | Fix |
|---|---|---|
| `qemu-aarch64: Could not open '/lib/ld-linux-aarch64.so.1'` | dynamically linked binary, qemu looks for the loader on the host | link `-static` (what the driver does) or run `qemu-aarch64 -L /usr/aarch64-linux-gnu` |
| `warning: BFD: warning: system-supplied DSO at 0x400000810000 has a corrupt string table index` in gdb | qemu's vDSO | harmless |
| sample 10 crashed in `__run_exit_handlers` (exit 124 from `timeout`, qemu reports signal 11) | `n - 1 - i` parsed as `n - (1 - i)`, the loop wrote past `values[]` and hit libc's exit handler list | sample rewritten as `(n - 1) - i`; the parser bug remains |
| `aarch64: calls with more than 8 register arguments are not supported` (7 with a struct return) | v1 limit | pass a struct pointer |
| `Target aarch64 has no code generator yet` | a `./main` built before the aarch64 backend | `./build.sh` |
| a `;` comment in hand-written GNU as for aarch64 splits the line | `;` separates statements on AArch64 | comment with `//` (the backend rewrites the natives' `;` comments) |
