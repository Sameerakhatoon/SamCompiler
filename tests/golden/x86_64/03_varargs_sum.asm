default rel
section .data
section .text
global $sum
; sum function
$sum:
pop r11
push r9
push r8
push rcx
push rdx
push rsi
push rdi
push r11
push rbp
mov rbp, rsp
sub rsp, 96
sub rsp, 64
movq [rbp-160], xmm0
movq [rbp-152], xmm1
movq [rbp-144], xmm2
movq [rbp-136], xmm3
movq [rbp-128], xmm4
movq [rbp-120], xmm5
movq [rbp-112], xmm6
movq [rbp-104], xmm7
lea rax, [rbp+24]
push rax
lea rax, [rbp-40]
mov rcx, rax
pop rax
mov qword [rcx], rax
lea rax, [rbp+64]
push rax
lea rax, [rbp-40]
add rax, 8
mov rcx, rax
pop rax
mov qword [rcx], rax
lea rax, [rbp+64]
push rax
lea rax, [rbp-40]
add rax, 16
mov rcx, rax
pop rax
mov qword [rcx], rax
lea rax, [rbp-160]
push rax
lea rax, [rbp-40]
add rax, 24
mov rcx, rax
pop rax
mov qword [rcx], rax
lea rax, [rbp-96]
push rax
lea rax, [rbp-40]
add rax, 32
mov rcx, rax
pop rax
mov qword [rcx], rax
lea rax, [rbp-40]
mov rax, qword [rax]
add rax, 8
push rax
lea rax, [rbp-40]
add rax, 8
mov rax, qword [rax]
mov rcx, rax
pop rax
cmp rax, rcx
seta al
movzx eax, al
cmp rax, 0
jne .exp_3
lea rax, [rbp-40]
mov rax, qword [rax]
add rax, 8
push rax
lea rax, [rbp-40]
mov rcx, rax
pop rax
mov qword [rcx], rax
lea rax, [rbp-40]
mov rax, qword [rax]
add rax, -8
jmp .exp_4
.exp_3:
lea rax, [rbp-40]
add rax, 16
mov rax, qword [rax]
add rax, 8
push rax
lea rax, [rbp-40]
add rax, 16
mov rcx, rax
pop rax
mov qword [rcx], rax
lea rax, [rbp-40]
add rax, 16
mov rax, qword [rax]
add rax, -8
.exp_4:
movsxd rax, dword [rax]
push rax
lea rax, [rbp-44]
mov rcx, rax
pop rax
mov dword [rcx], eax
lea rax, [rbp-40]
mov rax, qword [rax]
add rax, 8
push rax
lea rax, [rbp-40]
add rax, 8
mov rax, qword [rax]
mov rcx, rax
pop rax
cmp rax, rcx
seta al
movzx eax, al
cmp rax, 0
jne .exp_7
lea rax, [rbp-40]
mov rax, qword [rax]
add rax, 8
push rax
lea rax, [rbp-40]
mov rcx, rax
pop rax
mov qword [rcx], rax
lea rax, [rbp-40]
mov rax, qword [rax]
add rax, -8
jmp .exp_8
.exp_7:
lea rax, [rbp-40]
add rax, 16
mov rax, qword [rax]
add rax, 8
push rax
lea rax, [rbp-40]
add rax, 16
mov rcx, rax
pop rax
mov qword [rcx], rax
lea rax, [rbp-40]
add rax, 16
mov rax, qword [rax]
add rax, -8
.exp_8:
movsxd rax, dword [rax]
push rax
lea rax, [rbp-48]
mov rcx, rax
pop rax
mov dword [rcx], eax
lea rax, [rbp-44]
movsxd rax, dword [rax]
lea rax, [rbp-48]
movsxd rax, dword [rax]
xor eax, eax
mov rsp, rbp
pop rbp
ret 48
add rsp, 64
add rsp, 96
pop rbp
ret 48
global $main
; main function
$main:
pop r11
push r9
push r8
push rcx
push rdx
push rsi
push rdi
push r11
push rbp
mov rbp, rsp
mov rax, 20
push rax
mov rax, 10
push rax
mov rax, 2
push rax
pop rdi
pop rsi
pop rdx
lea r11, [$sum]
xor eax, eax
call r11
movsxd rax, eax
mov rsp, rbp
pop rbp
ret 48
pop rbp
ret 48
section .data
section .rodata
section .note.GNU-stack noalloc noexec nowrite progbits
