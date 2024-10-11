.data
.text
.p2align 2
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
sub sp, sp, #16
mov x0, #0
str x0, [sp, #-16]!
sub x0, x29, #4
mov x1, x0
ldr x0, [sp], #16
str w0, [x1]
.Lentry_point_1:
.Ldo_while_start_3:
sub x0, x29, #4
ldrsw x0, [x0]
mov x1, #1
add w0, w0, w1
sxtw x0, w0
mov x1, x0
sub x0, x29, #4
mov x12, x0
mov x0, x1
mov x1, x12
str w0, [x1]
sub x0, x29, #4
ldrsw x0, [x0]
mov x1, #4
cmp x0, x1
cset w0, lt
cbnz x0, .Ldo_while_start_3
.Lexit_point_2:
sub x0, x29, #4
ldrsw x0, [x0]
mov sp, x29
ldp x29, x30, [sp], #16
add sp, sp, #64
ret
add sp, sp, #16
ldp x29, x30, [sp], #16
add sp, sp, #64
ret
.size main, .-main
.data
.section .rodata
.section .note.GNU-stack,"",@progbits
