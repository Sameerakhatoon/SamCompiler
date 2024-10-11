default rel
section .data
section .text
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
sub rsp, 32
mov rax, 1
push rax
lea rax, [rbp-4]
mov rcx, rax
pop rax
mov dword [rcx], eax
xor eax, eax
push rax
lea rax, [rbp-8]
mov rcx, rax
pop rax
mov dword [rcx], eax
lea rax, [rbp-4]
movsxd rax, dword [rax]
cmp rax, 0
je .exp_1
lea rax, [rbp-8]
movsxd rax, dword [rax]
cmp rax, 0
je .exp_1
mov rax, 1
jmp .exp_2
.exp_1:
xor eax, eax
.exp_2:
push rax
lea rax, [rbp-12]
mov rcx, rax
pop rax
mov dword [rcx], eax
lea rax, [rbp-4]
movsxd rax, dword [rax]
cmp rax, 0
jne .exp_3
lea rax, [rbp-8]
movsxd rax, dword [rax]
cmp rax, 0
jne .exp_3
xor eax, eax
jmp .exp_4
.exp_3:
mov rax, 1
.exp_4:
push rax
lea rax, [rbp-16]
mov rcx, rax
pop rax
mov dword [rcx], eax
lea rax, [rbp-8]
movsxd rax, dword [rax]
cmp rax, 0
sete al
movzx eax, al
push rax
lea rax, [rbp-20]
mov rcx, rax
pop rax
mov dword [rcx], eax
lea rax, [rbp-12]
movsxd rax, dword [rax]
push rax
lea rax, [rbp-16]
movsxd rax, dword [rax]
mov rcx, 2
imul eax, ecx
movsxd rax, eax
mov rcx, rax
pop rax
add eax, ecx
movsxd rax, eax
push rax
lea rax, [rbp-20]
movsxd rax, dword [rax]
mov rcx, 4
imul eax, ecx
movsxd rax, eax
mov rcx, rax
pop rax
add eax, ecx
movsxd rax, eax
mov rsp, rbp
pop rbp
ret 48
add rsp, 32
pop rbp
ret 48
section .data
section .rodata
section .note.GNU-stack noalloc noexec nowrite progbits
