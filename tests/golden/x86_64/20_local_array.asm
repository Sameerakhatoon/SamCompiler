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
lea rax, [rbp-12]
push rax
xor eax, eax
imul rax, rax, 4
pop rcx
add rax, rcx
mov rcx, rax
pop rax
mov dword [rcx], eax
mov rax, 2
push rax
lea rax, [rbp-12]
push rax
mov rax, 1
imul rax, rax, 4
pop rcx
add rax, rcx
mov rcx, rax
pop rax
mov dword [rcx], eax
mov rax, 3
push rax
lea rax, [rbp-12]
push rax
mov rax, 2
imul rax, rax, 4
pop rcx
add rax, rcx
mov rcx, rax
pop rax
mov dword [rcx], eax
lea rax, [rbp-12]
push rax
xor eax, eax
imul rax, rax, 4
pop rcx
add rax, rcx
movsxd rax, dword [rax]
push rax
lea rax, [rbp-16]
mov rcx, rax
pop rax
mov dword [rcx], eax
lea rax, [rbp-12]
push rax
mov rax, 1
imul rax, rax, 4
pop rcx
add rax, rcx
movsxd rax, dword [rax]
push rax
lea rax, [rbp-20]
mov rcx, rax
pop rax
mov dword [rcx], eax
lea rax, [rbp-12]
push rax
mov rax, 2
imul rax, rax, 4
pop rcx
add rax, rcx
movsxd rax, dword [rax]
push rax
lea rax, [rbp-24]
mov rcx, rax
pop rax
mov dword [rcx], eax
lea rax, [rbp-16]
movsxd rax, dword [rax]
push rax
lea rax, [rbp-20]
movsxd rax, dword [rax]
mov rcx, rax
pop rax
add eax, ecx
movsxd rax, eax
push rax
lea rax, [rbp-24]
movsxd rax, dword [rax]
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
