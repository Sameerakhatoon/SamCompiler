.data
.text
.p2align 2
.globl add
.type add, %function
// add function
add:
sub sp, sp, #64
stp x29, x30, [sp, #-16]!
mov x29, sp
stp x0, x1, [x29, #16]
stp x2, x3, [x29, #32]
stp x4, x5, [x29, #48]
stp x6, x7, [x29, #64]
add x0, x29, #16
ldrsw x0, [x0]
str x0, [sp, #-16]!
add x0, x29, #24
ldrsw x0, [x0]
mov x1, x0
ldr x0, [sp], #16
add w0, w0, w1
sxtw x0, w0
mov sp, x29
ldp x29, x30, [sp], #16
add sp, sp, #64
ret
ldp x29, x30, [sp], #16
add sp, sp, #64
ret
.size add, .-add
.globl twice
.type twice, %function
// twice function
twice:
sub sp, sp, #64
stp x29, x30, [sp, #-16]!
mov x29, sp
stp x0, x1, [x29, #16]
stp x2, x3, [x29, #32]
stp x4, x5, [x29, #48]
stp x6, x7, [x29, #64]
add x0, x29, #16
ldrsw x0, [x0]
mov x1, #2
mul w0, w0, w1
sxtw x0, w0
mov sp, x29
ldp x29, x30, [sp], #16
add sp, sp, #64
ret
ldp x29, x30, [sp], #16
add sp, sp, #64
ret
.size twice, .-twice
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
mov x0, #4
str x0, [sp, #-16]!
mov x0, #3
str x0, [sp, #-16]!
ldr x0, [sp], #16
ldr x1, [sp], #16
adrp x9, add
add x9, x9, :lo12:add
blr x9
sxtw x0, w0
str x0, [sp, #-16]!
mov x0, #5
str x0, [sp, #-16]!
ldr x0, [sp], #16
adrp x9, twice
add x9, x9, :lo12:twice
blr x9
sxtw x0, w0
str x0, [sp, #-16]!
ldr x0, [sp], #16
ldr x1, [sp], #16
adrp x9, add
add x9, x9, :lo12:add
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
