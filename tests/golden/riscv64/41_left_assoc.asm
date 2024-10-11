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
addi sp, sp, -48
li a0, 100
addi sp, sp, -16
sd a0, 0(sp)
addi a0, s0, -4
mv a1, a0
ld a0, 0(sp)
addi sp, sp, 16
sw a0, 0(a1)
li a0, 30
addi sp, sp, -16
sd a0, 0(sp)
addi a0, s0, -8
mv a1, a0
ld a0, 0(sp)
addi sp, sp, 16
sw a0, 0(a1)
li a0, 20
addi sp, sp, -16
sd a0, 0(sp)
addi a0, s0, -12
mv a1, a0
ld a0, 0(sp)
addi sp, sp, 16
sw a0, 0(a1)
addi a0, s0, -4
lw a0, 0(a0)
addi sp, sp, -16
sd a0, 0(sp)
addi a0, s0, -8
lw a0, 0(a0)
mv a1, a0
ld a0, 0(sp)
addi sp, sp, 16
subw a0, a0, a1
sext.w a0, a0
addi sp, sp, -16
sd a0, 0(sp)
addi a0, s0, -12
lw a0, 0(a0)
mv a1, a0
ld a0, 0(sp)
addi sp, sp, 16
subw a0, a0, a1
sext.w a0, a0
addi sp, sp, -16
sd a0, 0(sp)
addi a0, s0, -16
mv a1, a0
ld a0, 0(sp)
addi sp, sp, 16
sw a0, 0(a1)
li a0, 64
addi sp, sp, -16
sd a0, 0(sp)
addi a0, s0, -20
mv a1, a0
ld a0, 0(sp)
addi sp, sp, 16
sw a0, 0(a1)
addi a0, s0, -20
lw a0, 0(a0)
li a1, 4
divw a0, a0, a1
sext.w a0, a0
li a1, 2
divw a0, a0, a1
sext.w a0, a0
addi sp, sp, -16
sd a0, 0(sp)
addi a0, s0, -24
mv a1, a0
ld a0, 0(sp)
addi sp, sp, 16
sw a0, 0(a1)
addi a0, s0, -4
lw a0, 0(a0)
addi sp, sp, -16
sd a0, 0(sp)
addi a0, s0, -8
lw a0, 0(a0)
mv a1, a0
ld a0, 0(sp)
addi sp, sp, 16
remw a0, a0, a1
sext.w a0, a0
li a1, 7
remw a0, a0, a1
sext.w a0, a0
addi sp, sp, -16
sd a0, 0(sp)
addi a0, s0, -28
mv a1, a0
ld a0, 0(sp)
addi sp, sp, 16
sw a0, 0(a1)
li a0, 1
addi sp, sp, -16
sd a0, 0(sp)
addi a0, s0, -32
mv a1, a0
ld a0, 0(sp)
addi sp, sp, 16
sw a0, 0(a1)
addi a0, s0, -32
lw a0, 0(a0)
li a1, 2
sllw a0, a0, a1
sext.w a0, a0
li a1, 3
sllw a0, a0, a1
sext.w a0, a0
addi sp, sp, -16
sd a0, 0(sp)
addi a0, s0, -36
mv a1, a0
ld a0, 0(sp)
addi sp, sp, 16
sw a0, 0(a1)
li a0, 256
addi sp, sp, -16
sd a0, 0(sp)
addi a0, s0, -40
mv a1, a0
ld a0, 0(sp)
addi sp, sp, 16
sw a0, 0(a1)
addi a0, s0, -40
lw a0, 0(a0)
li a1, 2
sraw a0, a0, a1
sext.w a0, a0
li a1, 3
sraw a0, a0, a1
sext.w a0, a0
addi sp, sp, -16
sd a0, 0(sp)
addi a0, s0, -44
mv a1, a0
ld a0, 0(sp)
addi sp, sp, 16
sw a0, 0(a1)
addi a0, s0, -16
lw a0, 0(a0)
addi sp, sp, -16
sd a0, 0(sp)
addi a0, s0, -24
lw a0, 0(a0)
mv a1, a0
ld a0, 0(sp)
addi sp, sp, 16
addw a0, a0, a1
sext.w a0, a0
addi sp, sp, -16
sd a0, 0(sp)
addi a0, s0, -28
lw a0, 0(a0)
mv a1, a0
ld a0, 0(sp)
addi sp, sp, 16
addw a0, a0, a1
sext.w a0, a0
addi sp, sp, -16
sd a0, 0(sp)
addi a0, s0, -36
lw a0, 0(a0)
mv a1, a0
ld a0, 0(sp)
addi sp, sp, 16
addw a0, a0, a1
sext.w a0, a0
addi sp, sp, -16
sd a0, 0(sp)
addi a0, s0, -44
lw a0, 0(a0)
mv a1, a0
ld a0, 0(sp)
addi sp, sp, 16
addw a0, a0, a1
sext.w a0, a0
li a1, 10
addw a0, a0, a1
sext.w a0, a0
mv sp, s0
ld ra, 8(sp)
ld s0, 0(sp)
addi sp, sp, 16
addi sp, sp, 64
ret
addi sp, sp, 48
ld ra, 8(sp)
ld s0, 0(sp)
addi sp, sp, 16
addi sp, sp, 64
ret
.size main, .-main
.data
.section .rodata
.section .note.GNU-stack,"",@progbits
