.data
.text
.p2align 2
.globl sum
.type sum, @function
# sum function
sum:
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
addi sp, sp, -96
addi a0, s0, 24
addi sp, sp, -16
sd a0, 0(sp)
addi a0, s0, -40
mv a1, a0
ld a0, 0(sp)
addi sp, sp, 16
sd a0, 0(a1)
addi a0, s0, 80
addi sp, sp, -16
sd a0, 0(sp)
addi a0, s0, -40
addi a0, a0, 8
mv a1, a0
ld a0, 0(sp)
addi sp, sp, 16
sd a0, 0(a1)
addi a0, s0, 80
addi sp, sp, -16
sd a0, 0(sp)
addi a0, s0, -40
addi a0, a0, 16
mv a1, a0
ld a0, 0(sp)
addi sp, sp, 16
sd a0, 0(a1)
addi a0, s0, -96
addi sp, sp, -16
sd a0, 0(sp)
addi a0, s0, -40
addi a0, a0, 24
mv a1, a0
ld a0, 0(sp)
addi sp, sp, 16
sd a0, 0(a1)
addi a0, s0, -32
addi sp, sp, -16
sd a0, 0(sp)
addi a0, s0, -40
addi a0, a0, 32
mv a1, a0
ld a0, 0(sp)
addi sp, sp, 16
sd a0, 0(a1)
addi a0, s0, -40
ld a0, 0(a0)
addi a0, a0, 8
addi sp, sp, -16
sd a0, 0(sp)
addi a0, s0, -40
mv a1, a0
ld a0, 0(sp)
addi sp, sp, 16
sd a0, 0(a1)
addi a0, s0, -40
ld a0, 0(a0)
addi a0, a0, -8
lw a0, 0(a0)
addi sp, sp, -16
sd a0, 0(sp)
addi a0, s0, -44
mv a1, a0
ld a0, 0(sp)
addi sp, sp, 16
sw a0, 0(a1)
addi a0, s0, -40
ld a0, 0(a0)
addi a0, a0, 8
addi sp, sp, -16
sd a0, 0(sp)
addi a0, s0, -40
mv a1, a0
ld a0, 0(sp)
addi sp, sp, 16
sd a0, 0(a1)
addi a0, s0, -40
ld a0, 0(a0)
addi a0, a0, -8
lw a0, 0(a0)
addi sp, sp, -16
sd a0, 0(sp)
addi a0, s0, -48
mv a1, a0
ld a0, 0(sp)
addi sp, sp, 16
sw a0, 0(a1)
addi a0, s0, -44
lw a0, 0(a0)
addi a0, s0, -48
lw a0, 0(a0)
li a0, 0
mv sp, s0
ld ra, 8(sp)
ld s0, 0(sp)
addi sp, sp, 16
addi sp, sp, 64
ret
addi sp, sp, 96
ld ra, 8(sp)
ld s0, 0(sp)
addi sp, sp, 16
addi sp, sp, 64
ret
.size sum, .-sum
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
li a0, 20
addi sp, sp, -16
sd a0, 0(sp)
li a0, 10
addi sp, sp, -16
sd a0, 0(sp)
li a0, 2
addi sp, sp, -16
sd a0, 0(sp)
ld a0, 0(sp)
addi sp, sp, 16
ld a1, 0(sp)
addi sp, sp, 16
ld a2, 0(sp)
addi sp, sp, 16
lla t0, sum
jalr t0
sext.w a0, a0
mv sp, s0
ld ra, 8(sp)
ld s0, 0(sp)
addi sp, sp, 16
addi sp, sp, 64
ret
ld ra, 8(sp)
ld s0, 0(sp)
addi sp, sp, 16
addi sp, sp, 64
ret
.size main, .-main
.data
.section .rodata
.section .note.GNU-stack,"",@progbits
