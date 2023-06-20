# Target guide: riscv64

Every command below was run in WSL Ubuntu 24.04 from
a SamCompiler checkout; the output is pasted as it came out,
trimmed only where marked `...`.

## 1. What this target is

| | |
|---|---|
| `-target=` | `riscv64` |
| Data model | LP64: int 4, long 8, pointer 8 |
| ABI | RISC-V LP64 psABI (lp64d): arguments in a0-a7, result in a0 |
| Assembler | GNU as (`riscv64-linux-gnu-as`, default `-march=rv64gc`) |
| Linker | `riscv64-linux-gnu-gcc -static` |
| Runs | under qemu-user (`qemu-riscv64`) on the x86-64 host |
| Backend | `codegen_riscv64.c` |

SamCompiler is a native x86-64 program acting as a cross compiler;
the cross binutils/gcc package assembles and links.

## 2. Prerequisites

```
sudo apt-get install -y gcc-riscv64-linux-gnu qemu-user qemu-user-binfmt gdb-multiarch file
```

Check them:

```
$ riscv64-linux-gnu-gcc --version | head -1
riscv64-linux-gnu-gcc (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0
$ riscv64-linux-gnu-as --version | head -1
GNU assembler (GNU Binutils for Ubuntu) 2.42
$ qemu-riscv64 --version | head -1
qemu-riscv64 version 8.2.2 (Debian 1:8.2.2+ds-0ubuntu1.18)
$ dpkg -l gcc-riscv64-linux-gnu binutils-riscv64-linux-gnu libc6-dev-riscv64-cross | grep '^ii' | awk '{print $2, $3}'
binutils-riscv64-linux-gnu 2.42-4ubuntu2.10
gcc-riscv64-linux-gnu 4:13.2.0-7ubuntu1
libc6-dev-riscv64-cross 2.39-0ubuntu8cross1
```

gdb-multiarch 15.1 and qemu-user 8.2.2 are the same packages the
aarch64 guide uses. The sysroot is `/usr/riscv64-linux-gnu`.

## 3. Build the compiler

```
$ ./build.sh
```

## 4. Compile a C file for this target

`/tmp/g/add.c`:

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
$ ./main -target=riscv64 /tmp/g/add.c /tmp/g/add_rv
...
everything compiled fine
riscv64-linux-gnu-as /tmp/g/add_rv -o /tmp/g/add_rv.o && riscv64-linux-gnu-gcc -static /tmp/g/add_rv.o -o /tmp/g/add_rv
run with: qemu-riscv64 /tmp/g/add_rv
```

Stop after the assembler:

```
$ ./main -target=riscv64 /tmp/g/add.c /tmp/g/add_rv.s object | tail -2
everything compiled fine
riscv64-linux-gnu-as /tmp/g/add_rv.s -o /tmp/g/add_rv.s.o
```

## 5. Read the output

```
.text
.p2align 2
.globl add
.type add, @function
add:
addi sp, sp, -64      # home area for a0-a7
addi sp, sp, -16      # frame record
sd ra, 8(sp)          # return address
sd s0, 0(sp)          # caller's frame pointer
mv s0, sp
sd a0, 16(s0)         # spill the argument registers
sd a1, 24(s0)
...
sd a7, 72(s0)
lw a0, 16(s0)         # a (lw sign-extends)
addi sp, sp, -16      # push: make a 16-byte slot
sd a0, 0(sp)          #       and store into it
lw a0, 24(s0)         # b
addi sp, sp, -16
sd a0, 0(sp)
ld a1, 0(sp)          # pop the right operand
addi sp, sp, 16
ld a0, 0(sp)          # pop the left operand
addi sp, sp, 16
addw a0, a0, a1       # 32-bit add, result sign-extended
addi sp, sp, -16
sd a0, 0(sp)
ld a0, 0(sp)          # result in a0
addi sp, sp, 16
ld ra, 8(sp)
ld s0, 0(sp)
addi sp, sp, 16
addi sp, sp, 64       # drop the home area
ret                   # jalr x0, 0(ra)
```

and the call in `main`:

```
lla t0, add           # pc-relative address of add
addi sp, sp, -16
sd t0, 0(sp)          # callee kept under the arguments
...
li a0, 3
addi sp, sp, -16
sd a0, 0(sp)
li a0, 2
addi sp, sp, -16
sd a0, 0(sp)
ld a0, 0(sp)          # first argument
addi sp, sp, 16
ld a1, 0(sp)          # second argument
addi sp, sp, 16
ld t0, 0(sp)          # callee
addi sp, sp, 16
jalr t0               # call, return address in ra
sext.w a0, a0
```

The encodings, from objdump:

```
$ riscv64-linux-gnu-objdump -d add_rv.o | sed -n '/<add>:/,/^$/p' | head -12
0000000000000000 <add>:
   0:	fc010113          	addi	sp,sp,-64
   4:	ff010113          	addi	sp,sp,-16
   8:	00113423          	sd	ra,8(sp)
   c:	00813023          	sd	s0,0(sp)
  10:	00010413          	mv	s0,sp
  14:	00a43823          	sd	a0,16(s0)
  18:	00b43c23          	sd	a1,24(s0)
  1c:	02c43023          	sd	a2,32(s0)
  20:	02d43423          	sd	a3,40(s0)
  24:	02e43823          	sd	a4,48(s0)
  28:	02f43c23          	sd	a5,56(s0)
```

## 6. Assemble and link by hand

```
$ cd /tmp/g
$ riscv64-linux-gnu-as add_rv.s -o add_rv.o
$ riscv64-linux-gnu-gcc -static add_rv.o -o add_rv_manual
$ qemu-riscv64 ./add_rv_manual; echo "exit=$?"
exit=5
$ file /tmp/g/add_rv
/tmp/g/add_rv: ELF 64-bit LSB executable, UCB RISC-V, RVC, double-float ABI, version 1 (SYSV), statically linked, BuildID[sha1]=33f0850363d5f462b182c62e3edba085f8846933, for GNU/Linux 4.15.0, not stripped
```

(`RVC` is set because the static libc uses compressed instructions;
the code SamCompiler emits is plain 32-bit encodings.)

A dynamic link needs the sysroot at run time:

```
$ riscv64-linux-gnu-gcc add_rv.o -o add_rv_dyn
$ qemu-riscv64 ./add_rv_dyn; echo "exit=$?"
qemu-riscv64: Could not open '/lib/ld-linux-riscv64-lp64d.so.1': No such file or directory
exit=255
$ qemu-riscv64 -L /usr/riscv64-linux-gnu ./add_rv_dyn; echo "exit=$?"
exit=5
```

## 7. Run it

```
$ qemu-riscv64 /tmp/g/add_rv; echo "exit=$?"
exit=5
```

With `qemu-user-binfmt` the binary also runs as `/tmp/g/add_rv`; the
tests call `qemu-riscv64` explicitly.

## 8. Run the samples

`samples/run_all.sh ARCH=riscv64` arrives with the samples runner. Until then
`tests/221-riscv64-run.sh` runs every sample and golden input. Run
by hand, all 54 pass:

```
01_return_const exit=42 expected=42 OK
...
25_struct_return exit=9 expected=9 OK
...
37_printf exit=0 expected=0 OK out=[v=42 s=hi]
...
14_token_scanner exit=4 expected=4 OK
passed: 54 failed: 0
```

## 9. Run the tests

```
$ bash tests/220-riscv64-codegen.sh
220-riscv64-codegen ... ok
$ bash tests/221-riscv64-run.sh
221-riscv64-run ... ok
$ bash tests/176-x86-golden.sh
176-x86-golden ... ok
```

## 10. Debug

```
$ cat gdbrv.cmd
target remote :1234
break add
continue
stepi 6
x/i $pc
info registers a0 a1 s0 ra sp
x/4gx $s0
x/dw $s0+16
x/dw $s0+24
kill
$ qemu-riscv64 -g 1234 ./add_rv_manual &
$ gdb-multiarch -q -batch -x gdbrv.cmd ./add_rv_manual
...
Breakpoint 1, 0x00000000000105f8 in add ()
0x0000000000010610 in add ()
=> 0x10610 <add+116>:	ld	s0,0(sp)
a0             0x5	5
a1             0x3	3
s0             0x2aaaab2ab0c0      0x2aaaab2ab0c0
ra             0x106bc	0x106bc <main+136>
sp             0x2aaaab2ab0c0	0x2aaaab2ab0c0
0x2aaaab2ab0c0:	0x00002aaaab2ab110	0x00000000000106bc
0x2aaaab2ab0d0:	0x0000000000000002	0x0000000000000003
0x2aaaab2ab0d0:	2
0x2aaaab2ab0d8:	3
```

gdb put the breakpoint after the prologue, so six steps land in the
epilogue with the sum already in a0. `0(s0)` is the caller's s0,
`8(s0)` the saved ra (also live in `ra`, pointing back into `main`),
`16(s0)`/`24(s0)` the home slots for a0 = 2 and a1 = 3.

## 11. Troubleshooting

| Symptom | Cause | Fix |
|---|---|---|
| `qemu-riscv64: Could not open '/lib/ld-linux-riscv64-lp64d.so.1'` | dynamically linked binary | link `-static` (the driver does) or `qemu-riscv64 -L /usr/riscv64-linux-gnu` |
| `warning: BFD: warning: system-supplied DSO at ... corrupt string table index` in gdb | qemu's vDSO | harmless |
| `riscv64: calls with more than 8 register arguments are not supported` (7 with a struct return) | v1 limit | pass a struct pointer |
| `file` says `RVC` although the asm has no compressed instructions | the static libc is built with C | expected |
| hand-written asm: `Error: illegal operands `ld a0,4000(s0)'` | load/store offsets are 12 bits signed (-2048..2047) | build the address first (`li` + `add`, as `codegen_mem_operand` does) |
