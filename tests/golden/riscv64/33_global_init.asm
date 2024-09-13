.data
# int g
.globl g
.balign 4
g: .word 20
# char k
.globl k
.balign 1
k: .byte 100
# char msg
.globl msg
.balign 8
msg: .dword str_1
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
lla a0, g
lw a0, 0(a0)
addi sp, sp, -16
sd a0, 0(sp)
lla a0, k
lbu a0, 0(a0)
mv a1, a0
ld a0, 0(sp)
addi sp, sp, 16
sext.w a1, a1
addw a0, a0, a1
sext.w a0, a0
addi sp, sp, -16
sd a0, 0(sp)
lla a0, msg
ld a0, 0(a0)
addi sp, sp, -16
sd a0, 0(sp)
li a0, 1
ld a1, 0(sp)
addi sp, sp, 16
add a0, a0, a1
lbu a0, 0(a0)
mv a1, a0
ld a0, 0(sp)
addi sp, sp, 16
sext.w a1, a1
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
str_1: .asciz "x"
