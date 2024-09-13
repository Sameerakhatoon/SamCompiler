.data
// node nodes
.globl nodes
.balign 8
nodes: .zero 40
.text
.p2align 2
.globl sum_from
.type sum_from, %function
// sum_from function
sum_from:
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
add x0, x29, #16
ldrsw x0, [x0]
str x0, [sp, #-16]!
sub x0, x29, #8
mov x1, x0
ldr x0, [sp], #16
str w0, [x1]
.Lentry_point_1:
.Lwhile_start_3:
sub x0, x29, #8
ldrsw x0, [x0]
mov x1, #0
cmp x0, x1
b.lt .Lwhile_end_4
sub x0, x29, #4
ldrsw x0, [x0]
str x0, [sp, #-16]!
adrp x0, nodes
add x0, x0, :lo12:nodes
str x0, [sp, #-16]!
sub x0, x29, #8
ldrsw x0, [x0]
mov x12, #8
mul x0, x0, x12
ldr x1, [sp], #16
add x0, x0, x1
ldrsw x0, [x0]
mov x1, x0
ldr x0, [sp], #16
add w0, w0, w1
sxtw x0, w0
mov x1, x0
sub x0, x29, #4
mov x12, x0
mov x0, x1
mov x1, x12
str w0, [x1]
adrp x0, nodes
add x0, x0, :lo12:nodes
str x0, [sp, #-16]!
sub x0, x29, #8
ldrsw x0, [x0]
mov x12, #8
mul x0, x0, x12
ldr x1, [sp], #16
add x0, x0, x1
add x0, x0, #4
ldrsw x0, [x0]
mov x1, x0
sub x0, x29, #8
mov x12, x0
mov x0, x1
mov x1, x12
str w0, [x1]
b .Lwhile_start_3
.Lwhile_end_4:
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
.size sum_from, .-sum_from
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
mov x0, #10
str x0, [sp, #-16]!
adrp x0, nodes
add x0, x0, :lo12:nodes
str x0, [sp, #-16]!
mov x0, #0
mov x12, #8
mul x0, x0, x12
ldr x1, [sp], #16
add x0, x0, x1
mov x1, x0
ldr x0, [sp], #16
str w0, [x1]
mov x0, #2
str x0, [sp, #-16]!
adrp x0, nodes
add x0, x0, :lo12:nodes
str x0, [sp, #-16]!
mov x0, #0
mov x12, #8
mul x0, x0, x12
ldr x1, [sp], #16
add x0, x0, x1
add x0, x0, #4
mov x1, x0
ldr x0, [sp], #16
str w0, [x1]
mov x0, #20
str x0, [sp, #-16]!
adrp x0, nodes
add x0, x0, :lo12:nodes
str x0, [sp, #-16]!
mov x0, #2
mov x12, #8
mul x0, x0, x12
ldr x1, [sp], #16
add x0, x0, x1
mov x1, x0
ldr x0, [sp], #16
str w0, [x1]
mov x0, #4
str x0, [sp, #-16]!
adrp x0, nodes
add x0, x0, :lo12:nodes
str x0, [sp, #-16]!
mov x0, #2
mov x12, #8
mul x0, x0, x12
ldr x1, [sp], #16
add x0, x0, x1
add x0, x0, #4
mov x1, x0
ldr x0, [sp], #16
str w0, [x1]
mov x0, #30
str x0, [sp, #-16]!
adrp x0, nodes
add x0, x0, :lo12:nodes
str x0, [sp, #-16]!
mov x0, #4
mov x12, #8
mul x0, x0, x12
ldr x1, [sp], #16
add x0, x0, x1
mov x1, x0
ldr x0, [sp], #16
str w0, [x1]
mov x0, #1
neg w0, w0
sxtw x0, w0
str x0, [sp, #-16]!
adrp x0, nodes
add x0, x0, :lo12:nodes
str x0, [sp, #-16]!
mov x0, #4
mov x12, #8
mul x0, x0, x12
ldr x1, [sp], #16
add x0, x0, x1
add x0, x0, #4
mov x1, x0
ldr x0, [sp], #16
str w0, [x1]
mov x0, #999
str x0, [sp, #-16]!
adrp x0, nodes
add x0, x0, :lo12:nodes
str x0, [sp, #-16]!
mov x0, #1
mov x12, #8
mul x0, x0, x12
ldr x1, [sp], #16
add x0, x0, x1
mov x1, x0
ldr x0, [sp], #16
str w0, [x1]
mov x0, #1
neg w0, w0
sxtw x0, w0
str x0, [sp, #-16]!
adrp x0, nodes
add x0, x0, :lo12:nodes
str x0, [sp, #-16]!
mov x0, #1
mov x12, #8
mul x0, x0, x12
ldr x1, [sp], #16
add x0, x0, x1
add x0, x0, #4
mov x1, x0
ldr x0, [sp], #16
str w0, [x1]
mov x0, #777
str x0, [sp, #-16]!
adrp x0, nodes
add x0, x0, :lo12:nodes
str x0, [sp, #-16]!
mov x0, #3
mov x12, #8
mul x0, x0, x12
ldr x1, [sp], #16
add x0, x0, x1
mov x1, x0
ldr x0, [sp], #16
str w0, [x1]
mov x0, #1
neg w0, w0
sxtw x0, w0
str x0, [sp, #-16]!
adrp x0, nodes
add x0, x0, :lo12:nodes
str x0, [sp, #-16]!
mov x0, #3
mov x12, #8
mul x0, x0, x12
ldr x1, [sp], #16
add x0, x0, x1
add x0, x0, #4
mov x1, x0
ldr x0, [sp], #16
str w0, [x1]
mov x0, #0
str x0, [sp, #-16]!
ldr x0, [sp], #16
adrp x9, sum_from
add x9, x9, :lo12:sum_from
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
