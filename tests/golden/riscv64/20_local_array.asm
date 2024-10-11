.data
.text
.p2align 2
.globl main
.type main, @function
# main function
main:
addi sp, sp, -64
addi sp, sp, -16
sd ra, 8(sp)
sd s0, 0(sp)
mv s0, sp
sd a0, 16(s0)
sd a1, 24(s0)
sd a2, 32(s0)
sd a3, 40(s0)
sd a4, 48(s0)
sd a5, 56(s0)
sd a6, 64(s0)
sd a7, 72(s0)
addi sp, sp, -32
li a0, 1
addi sp, sp, -16
sd a0, 0(sp)
addi a0, s0, -12
addi sp, sp, -16
sd a0, 0(sp)
li a0, 0
li t3, 4
mul a0, a0, t3
ld a1, 0(sp)
addi sp, sp, 16
add a0, a0, a1
mv a1, a0
ld a0, 0(sp)
addi sp, sp, 16
sw a0, 0(a1)
li a0, 2
addi sp, sp, -16
sd a0, 0(sp)
addi a0, s0, -12
addi sp, sp, -16
sd a0, 0(sp)
li a0, 1
li t3, 4
mul a0, a0, t3
ld a1, 0(sp)
addi sp, sp, 16
add a0, a0, a1
mv a1, a0
ld a0, 0(sp)
addi sp, sp, 16
sw a0, 0(a1)
li a0, 3
addi sp, sp, -16
sd a0, 0(sp)
addi a0, s0, -12
addi sp, sp, -16
sd a0, 0(sp)
li a0, 2
li t3, 4
mul a0, a0, t3
ld a1, 0(sp)
addi sp, sp, 16
add a0, a0, a1
mv a1, a0
ld a0, 0(sp)
addi sp, sp, 16
sw a0, 0(a1)
addi a0, s0, -12
addi sp, sp, -16
sd a0, 0(sp)
li a0, 0
li t3, 4
mul a0, a0, t3
ld a1, 0(sp)
addi sp, sp, 16
add a0, a0, a1
lw a0, 0(a0)
addi sp, sp, -16
sd a0, 0(sp)
addi a0, s0, -16
mv a1, a0
ld a0, 0(sp)
addi sp, sp, 16
sw a0, 0(a1)
addi a0, s0, -12
addi sp, sp, -16
sd a0, 0(sp)
li a0, 1
li t3, 4
mul a0, a0, t3
ld a1, 0(sp)
addi sp, sp, 16
add a0, a0, a1
lw a0, 0(a0)
addi sp, sp, -16
sd a0, 0(sp)
addi a0, s0, -20
mv a1, a0
ld a0, 0(sp)
addi sp, sp, 16
sw a0, 0(a1)
addi a0, s0, -12
addi sp, sp, -16
sd a0, 0(sp)
li a0, 2
li t3, 4
mul a0, a0, t3
ld a1, 0(sp)
addi sp, sp, 16
add a0, a0, a1
lw a0, 0(a0)
addi sp, sp, -16
sd a0, 0(sp)
addi a0, s0, -24
mv a1, a0
ld a0, 0(sp)
addi sp, sp, 16
sw a0, 0(a1)
addi a0, s0, -16
lw a0, 0(a0)
addi sp, sp, -16
sd a0, 0(sp)
addi a0, s0, -20
lw a0, 0(a0)
mv a1, a0
ld a0, 0(sp)
addi sp, sp, 16
addw a0, a0, a1
sext.w a0, a0
addi sp, sp, -16
sd a0, 0(sp)
addi a0, s0, -24
lw a0, 0(a0)
mv a1, a0
ld a0, 0(sp)
addi sp, sp, 16
addw a0, a0, a1
sext.w a0, a0
mv sp, s0
ld ra, 8(sp)
ld s0, 0(sp)
addi sp, sp, 16
addi sp, sp, 64
ret
addi sp, sp, 32
ld ra, 8(sp)
ld s0, 0(sp)
addi sp, sp, 16
addi sp, sp, 64
ret
.size main, .-main
.data
.section .rodata
.section .note.GNU-stack,"",@progbits
