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
addi a0, s0, -4
mv a1, a0
ld a0, 0(sp)
addi sp, sp, 16
sw a0, 0(a1)
li a0, 0
addi sp, sp, -16
sd a0, 0(sp)
addi a0, s0, -8
mv a1, a0
ld a0, 0(sp)
addi sp, sp, 16
sw a0, 0(a1)
addi a0, s0, -4
lw a0, 0(a0)
beqz a0, .Lexp_1
addi a0, s0, -8
lw a0, 0(a0)
beqz a0, .Lexp_1
li a0, 1
j .Lexp_2
.Lexp_1:
li a0, 0
.Lexp_2:
addi sp, sp, -16
sd a0, 0(sp)
addi a0, s0, -12
mv a1, a0
ld a0, 0(sp)
addi sp, sp, 16
sw a0, 0(a1)
addi a0, s0, -4
lw a0, 0(a0)
bnez a0, .Lexp_3
addi a0, s0, -8
lw a0, 0(a0)
bnez a0, .Lexp_3
li a0, 0
j .Lexp_4
.Lexp_3:
li a0, 1
.Lexp_4:
addi sp, sp, -16
sd a0, 0(sp)
addi a0, s0, -16
mv a1, a0
ld a0, 0(sp)
addi sp, sp, 16
sw a0, 0(a1)
addi a0, s0, -8
lw a0, 0(a0)
seqz a0, a0
addi sp, sp, -16
sd a0, 0(sp)
addi a0, s0, -20
mv a1, a0
ld a0, 0(sp)
addi sp, sp, 16
sw a0, 0(a1)
addi a0, s0, -12
lw a0, 0(a0)
addi sp, sp, -16
sd a0, 0(sp)
addi a0, s0, -16
lw a0, 0(a0)
li a1, 2
mulw a0, a0, a1
sext.w a0, a0
mv a1, a0
ld a0, 0(sp)
addi sp, sp, 16
addw a0, a0, a1
sext.w a0, a0
addi sp, sp, -16
sd a0, 0(sp)
addi a0, s0, -20
lw a0, 0(a0)
li a1, 4
mulw a0, a0, a1
sext.w a0, a0
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
