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
sub rsp, 16
mov rax, 5
push rax
lea rax, [rbp-4]
mov rcx, rax
pop rax
mov dword [rcx], eax
lea rax, [rbp-4]
movsxd rax, dword [rax]
mov rcx, 3
cmp rax, rcx
setg al
movzx eax, al
cmp rax, 0
je .exp_1
mov rax, 8
jmp .exp_2
.exp_1:
mov rax, 1
.exp_2:
push rax
lea rax, [rbp-8]
mov rcx, rax
pop rax
mov dword [rcx], eax
lea rax, [rbp-8]
movsxd rax, dword [rax]
mov rsp, rbp
pop rbp
ret 48
add rsp, 16
pop rbp
ret 48
section .data
section .rodata
section .note.GNU-stack noalloc noexec nowrite progbits
