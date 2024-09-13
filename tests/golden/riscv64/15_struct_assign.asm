.data
# pt a
.globl a
.balign 8
a: .dword 0
# pt b
.globl b
.balign 8
b: .dword 0
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
li a0, 8
addi sp, sp, -16
sd a0, 0(sp)
lla a0, b
mv a1, a0
ld a0, 0(sp)
addi sp, sp, 16
sw a0, 0(a1)
li a0, 11
addi sp, sp, -16
sd a0, 0(sp)
lla a0, b
addi a0, a0, 4
mv a1, a0
ld a0, 0(sp)
addi sp, sp, 16
sw a0, 0(a1)
lla a0, b
mv a1, a0
lla a0, a
mv t2, a1
mv t3, a0
ld t1, 0(t2)
sd t1, 0(t3)
addi t2, t2, 8
addi t3, t3, 8
lla a0, a
lw a0, 0(a0)
addi sp, sp, -16
sd a0, 0(sp)
lla a0, a
addi a0, a0, 4
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
.size main, .-main
.data
.section .rodata
