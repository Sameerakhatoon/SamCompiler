default rel
section .data
section .text
global $add
; add function
$add:
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
push rax
lea rax, [rbp+24]
movsxd rax, dword [rax]
mov rcx, rax
pop rax
add eax, ecx
movsxd rax, eax
mov rsp, rbp
pop rbp
ret 48
pop rbp
ret 48
global $twice
; twice function
$twice:
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
mov rcx, 2
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
mov rax, 4
push rax
mov rax, 3
push rax
pop rdi
pop rsi
lea r11, [$add]
xor eax, eax
call r11
movsxd rax, eax
push rax
mov rax, 5
push rax
pop rdi
sub rsp, 8
lea r11, [$twice]
xor eax, eax
call r11
add rsp, 8
movsxd rax, eax
push rax
pop rdi
pop rsi
lea r11, [$add]
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
