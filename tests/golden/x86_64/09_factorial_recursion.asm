default rel
section .data
section .text
global $factorial
; factorial function
$factorial:
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
jg .if_2
mov rax, 1
mov rsp, rbp
pop rbp
ret 48
jmp .if_end_1
.if_2:
.if_end_1:
lea rax, [rbp+16]
movsxd rax, dword [rax]
push rax
lea rax, [rbp+16]
movsxd rax, dword [rax]
mov rcx, 1
sub eax, ecx
movsxd rax, eax
push rax
pop rdi
sub rsp, 8
lea r11, [$factorial]
xor eax, eax
call r11
add rsp, 8
movsxd rax, eax
mov rcx, rax
pop rax
imul eax, ecx
movsxd rax, eax
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
mov rax, 5
push rax
pop rdi
lea r11, [$factorial]
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
