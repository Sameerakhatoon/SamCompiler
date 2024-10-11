default rel
section .data
section .text
global $set
; set function
$set:
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
lea rax, [rbp+24]
movsxd rax, dword [rax]
push rax
lea rax, [rbp+16]
mov rax, qword [rax]
mov rcx, rax
pop rax
mov dword [rcx], eax
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
sub rsp, 16
xor eax, eax
push rax
lea rax, [rbp-4]
mov rcx, rax
pop rax
mov dword [rcx], eax
mov rax, 12
push rax
lea rax, [rbp-4]
push rax
pop rdi
pop rsi
lea r11, [$set]
xor eax, eax
call r11
movsxd rax, eax
lea rax, [rbp-4]
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
