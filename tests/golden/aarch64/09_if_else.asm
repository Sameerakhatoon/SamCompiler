.data
.text
.p2align 2
.globl pick
.type pick, %function
// pick function
pick:
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
b.ne .Lif_2
mov x0, #10
mov sp, x29
ldp x29, x30, [sp], #16
add sp, sp, #64
ret
b .Lif_end_1
.Lif_2:
add x0, x29, #16
ldrsw x0, [x0]
mov x1, #2
cmp x0, x1
b.ne .Lif_3
mov x0, #2
mov sp, x29
ldp x29, x30, [sp], #16
add sp, sp, #64
ret
b .Lif_end_1
.Lif_3:
mov x0, #30
mov sp, x29
ldp x29, x30, [sp], #16
add sp, sp, #64
ret
.Lif_end_1:
mov x0, #0
mov sp, x29
ldp x29, x30, [sp], #16
add sp, sp, #64
ret
ldp x29, x30, [sp], #16
add sp, sp, #64
ret
.size pick, .-pick
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
mov x0, #2
str x0, [sp, #-16]!
ldr x0, [sp], #16
adrp x9, pick
add x9, x9, :lo12:pick
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
