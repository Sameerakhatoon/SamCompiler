.data
.text
.p2align 2
.globl sum
.type sum, %function
// sum function
sum:
sub sp, sp, #64
stp x29, x30, [sp, #-16]!
mov x29, sp
stp x0, x1, [x29, #16]
stp x2, x3, [x29, #32]
stp x4, x5, [x29, #48]
stp x6, x7, [x29, #64]
sub sp, sp, #96
sub sp, sp, #64
sub x11, x29, #160
str d0, [x11, #0]
str d1, [x11, #8]
str d2, [x11, #16]
str d3, [x11, #24]
str d4, [x11, #32]
str d5, [x11, #40]
str d6, [x11, #48]
str d7, [x11, #56]
add x0, x29, #24
str x0, [sp, #-16]!
sub x0, x29, #40
mov x1, x0
ldr x0, [sp], #16
str x0, [x1]
add x0, x29, #80
str x0, [sp, #-16]!
sub x0, x29, #40
add x0, x0, #8
mov x1, x0
ldr x0, [sp], #16
str x0, [x1]
add x0, x29, #80
str x0, [sp, #-16]!
sub x0, x29, #40
add x0, x0, #16
mov x1, x0
ldr x0, [sp], #16
str x0, [x1]
sub x0, x29, #160
str x0, [sp, #-16]!
sub x0, x29, #40
add x0, x0, #24
mov x1, x0
ldr x0, [sp], #16
str x0, [x1]
sub x0, x29, #96
str x0, [sp, #-16]!
sub x0, x29, #40
add x0, x0, #32
mov x1, x0
ldr x0, [sp], #16
str x0, [x1]
sub x0, x29, #40
ldr x0, [x0]
add x0, x0, #8
str x0, [sp, #-16]!
sub x0, x29, #40
add x0, x0, #8
ldr x0, [x0]
mov x1, x0
ldr x0, [sp], #16
cmp x0, x1
cset w0, hi
cbnz x0, .Lexp_3
sub x0, x29, #40
ldr x0, [x0]
add x0, x0, #8
str x0, [sp, #-16]!
sub x0, x29, #40
mov x1, x0
ldr x0, [sp], #16
str x0, [x1]
sub x0, x29, #40
ldr x0, [x0]
sub x0, x0, #8
b .Lexp_4
.Lexp_3:
sub x0, x29, #40
add x0, x0, #8
ldr x0, [x0]
str x0, [sp, #-16]!
sub x0, x29, #40
mov x1, x0
ldr x0, [sp], #16
str x0, [x1]
sub x0, x29, #40
add x0, x0, #16
ldr x0, [x0]
add x0, x0, #8
str x0, [sp, #-16]!
sub x0, x29, #40
add x0, x0, #16
mov x1, x0
ldr x0, [sp], #16
str x0, [x1]
sub x0, x29, #40
add x0, x0, #16
ldr x0, [x0]
sub x0, x0, #8
.Lexp_4:
ldrsw x0, [x0]
str x0, [sp, #-16]!
sub x0, x29, #44
mov x1, x0
ldr x0, [sp], #16
str w0, [x1]
sub x0, x29, #40
ldr x0, [x0]
add x0, x0, #8
str x0, [sp, #-16]!
sub x0, x29, #40
add x0, x0, #8
ldr x0, [x0]
mov x1, x0
ldr x0, [sp], #16
cmp x0, x1
cset w0, hi
cbnz x0, .Lexp_7
sub x0, x29, #40
ldr x0, [x0]
add x0, x0, #8
str x0, [sp, #-16]!
sub x0, x29, #40
mov x1, x0
ldr x0, [sp], #16
str x0, [x1]
sub x0, x29, #40
ldr x0, [x0]
sub x0, x0, #8
b .Lexp_8
.Lexp_7:
sub x0, x29, #40
add x0, x0, #8
ldr x0, [x0]
str x0, [sp, #-16]!
sub x0, x29, #40
mov x1, x0
ldr x0, [sp], #16
str x0, [x1]
sub x0, x29, #40
add x0, x0, #16
ldr x0, [x0]
add x0, x0, #8
str x0, [sp, #-16]!
sub x0, x29, #40
add x0, x0, #16
mov x1, x0
ldr x0, [sp], #16
str x0, [x1]
sub x0, x29, #40
add x0, x0, #16
ldr x0, [x0]
sub x0, x0, #8
.Lexp_8:
ldrsw x0, [x0]
str x0, [sp, #-16]!
sub x0, x29, #48
mov x1, x0
ldr x0, [sp], #16
str w0, [x1]
sub x0, x29, #44
ldrsw x0, [x0]
sub x0, x29, #48
ldrsw x0, [x0]
mov x0, #0
mov sp, x29
ldp x29, x30, [sp], #16
add sp, sp, #64
ret
add sp, sp, #64
add sp, sp, #96
ldp x29, x30, [sp], #16
add sp, sp, #64
ret
.size sum, .-sum
.globl main
.type main, %function
// main function
main:
sub sp, sp, #64
stp x29, x30, [sp, #-16]!
mov x29, sp
stp x0, x1, [x29, #16]
stp x2, x3, [x29, #32]
stp x4, x5, [x29, #48]
stp x6, x7, [x29, #64]
mov x0, #20
str x0, [sp, #-16]!
mov x0, #10
str x0, [sp, #-16]!
mov x0, #2
str x0, [sp, #-16]!
ldr x0, [sp], #16
ldr x1, [sp], #16
ldr x2, [sp], #16
adrp x9, sum
add x9, x9, :lo12:sum
blr x9
sxtw x0, w0
mov sp, x29
ldp x29, x30, [sp], #16
add sp, sp, #64
ret
ldp x29, x30, [sp], #16
add sp, sp, #64
ret
.size main, .-main
.data
.section .rodata
.section .note.GNU-stack,"",@progbits
