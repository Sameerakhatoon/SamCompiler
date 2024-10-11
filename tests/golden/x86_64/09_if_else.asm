default rel
section .data
section .text
global $pick
; pick function
$pick:
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
lea rax, [rbp+16]
movsxd rax, dword [rax]
mov rcx, 1
cmp rax, rcx
jne .if_2
mov rax, 10
mov rsp, rbp
pop rbp
ret 48
jmp .if_end_1
.if_2:
lea rax, [rbp+16]
movsxd rax, dword [rax]
mov rcx, 2
cmp rax, rcx
jne .if_3
mov rax, 2
mov rsp, rbp
pop rbp
ret 48
jmp .if_end_1
.if_3:
mov rax, 30
mov rsp, rbp
pop rbp
ret 48
.if_end_1:
xor eax, eax
mov rsp, rbp
pop rbp
ret 48
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
mov rax, 2
push rax
pop rdi
lea r11, [$pick]
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
