.data
.text
.p2align 2
.globl simple_hash
.type simple_hash, %function
// simple_hash function
simple_hash:
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
mov x0, #0
str x0, [sp, #-16]!
sub x0, x29, #8
mov x1, x0
ldr x0, [sp], #16
str w0, [x1]
.Lentry_point_1:
.Lwhile_start_3:
add x0, x29, #16
ldr x0, [x0]
str x0, [sp, #-16]!
sub x0, x29, #8
ldrsw x0, [x0]
ldr x1, [sp], #16
add x0, x0, x1
ldrb w0, [x0]
mov x1, #0
sxtw x0, w0
cmp x0, x1
b.eq .Lwhile_end_4
sub x0, x29, #4
ldrsw x0, [x0]
str x0, [sp, #-16]!
add x0, x29, #16
ldr x0, [x0]
str x0, [sp, #-16]!
sub x0, x29, #8
ldrsw x0, [x0]
ldr x1, [sp], #16
add x0, x0, x1
ldrb w0, [x0]
mov x1, x0
ldr x0, [sp], #16
sxtw x1, w1
add w0, w0, w1
sxtw x0, w0
mov x1, x0
sub x0, x29, #4
mov x12, x0
mov x0, x1
mov x1, x12
str w0, [x1]
sub x0, x29, #8
ldrsw x0, [x0]
mov x1, #1
add w0, w0, w1
sxtw x0, w0
mov x1, x0
sub x0, x29, #8
mov x12, x0
mov x0, x1
mov x1, x12
str w0, [x1]
b .Lwhile_start_3
.Lwhile_end_4:
.Lexit_point_2:
.Lentry_point_5:
.Lwhile_start_7:
sub x0, x29, #4
ldrsw x0, [x0]
mov x1, #17
cmp x0, x1
b.lt .Lwhile_end_8
sub x0, x29, #4
ldrsw x0, [x0]
mov x1, #17
sub w0, w0, w1
sxtw x0, w0
mov x1, x0
sub x0, x29, #4
mov x12, x0
mov x0, x1
mov x1, x12
str w0, [x1]
b .Lwhile_start_7
.Lwhile_end_8:
.Lexit_point_6:
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
.size simple_hash, .-simple_hash
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
adrp x0, str_9
add x0, x0, :lo12:str_9
str x0, [sp, #-16]!
ldr x0, [sp], #16
adrp x9, simple_hash
add x9, x9, :lo12:simple_hash
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
str_9: .asciz "ag"
