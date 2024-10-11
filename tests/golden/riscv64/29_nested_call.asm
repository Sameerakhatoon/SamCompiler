.data
.text
.p2align 2
.globl add
.type add, @function
# add function
add:
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
addi a0, s0, 16
lw a0, 0(a0)
addi sp, sp, -16
sd a0, 0(sp)
addi a0, s0, 24
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
ld ra, 8(sp)
ld s0, 0(sp)
addi sp, sp, 16
addi sp, sp, 64
ret
.size add, .-add
.globl twice
.type twice, @function
# twice function
twice:
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
addi a0, s0, 16
lw a0, 0(a0)
li a1, 2
mulw a0, a0, a1
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
.size twice, .-twice
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
li a0, 4
addi sp, sp, -16
sd a0, 0(sp)
li a0, 3
addi sp, sp, -16
sd a0, 0(sp)
ld a0, 0(sp)
addi sp, sp, 16
ld a1, 0(sp)
addi sp, sp, 16
lla t0, add
jalr t0
sext.w a0, a0
addi sp, sp, -16
sd a0, 0(sp)
li a0, 5
addi sp, sp, -16
sd a0, 0(sp)
ld a0, 0(sp)
addi sp, sp, 16
lla t0, twice
jalr t0
sext.w a0, a0
addi sp, sp, -16
sd a0, 0(sp)
ld a0, 0(sp)
addi sp, sp, 16
ld a1, 0(sp)
addi sp, sp, 16
lla t0, add
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
