.data
.text
.p2align 2
.globl factorial
.type factorial, %function
// factorial function
factorial:
sub sp, sp, #64
stp x29, x30, [sp, #-16]!
mov x29, sp
stp x0, x1, [x29, #16]
stp x2, x3, [x29, #32]
stp x4, x5, [x29, #48]
stp x6, x7, [x29, #64]
add x0, x29, #16
ldrsw x0, [x0]
mov x1, #1
cmp x0, x1
b.gt .Lif_2
mov x0, #1
mov sp, x29
ldp x29, x30, [sp], #16
add sp, sp, #64
ret
b .Lif_end_1
.Lif_2:
.Lif_end_1:
add x0, x29, #16
ldrsw x0, [x0]
str x0, [sp, #-16]!
add x0, x29, #16
ldrsw x0, [x0]
mov x1, #1
sub w0, w0, w1
sxtw x0, w0
str x0, [sp, #-16]!
ldr x0, [sp], #16
adrp x9, factorial
add x9, x9, :lo12:factorial
blr x9
sxtw x0, w0
mov x1, x0
ldr x0, [sp], #16
mul w0, w0, w1
sxtw x0, w0
mov sp, x29
ldp x29, x30, [sp], #16
add sp, sp, #64
ret
ldp x29, x30, [sp], #16
add sp, sp, #64
ret
.size factorial, .-factorial
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
mov x0, #5
str x0, [sp, #-16]!
ldr x0, [sp], #16
adrp x9, factorial
add x9, x9, :lo12:factorial
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
