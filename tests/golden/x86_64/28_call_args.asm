default rel
section .data
section .text
global $f
; f function
$f:
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
mov rcx, 100
imul eax, ecx
movsxd rax, eax
push rax
lea rax, [rbp+24]
movsxd rax, dword [rax]
mov rcx, 10
imul eax, ecx
movsxd rax, eax
mov rcx, rax
pop rax
add eax, ecx
movsxd rax, eax
push rax
lea rax, [rbp+32]
movsxd rax, dword [rax]
mov rcx, rax
pop rax
add eax, ecx
movsxd rax, eax
mov rcx, 300
sub eax, ecx
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
mov rax, 6
push rax
mov rax, 2
push rax
mov rax, 3
push rax
pop rdi
pop rsi
pop rdx
lea r11, [$f]
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
