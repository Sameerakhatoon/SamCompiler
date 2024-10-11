default rel
section .data
; int g
global $g
$g: times 16 db  0
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
mov rax, 4
push rax
lea rax, [$g]
push rax
mov rax, 1
imul rax, rax, 4
pop rcx
add rax, rcx
mov rcx, rax
pop rax
mov dword [rcx], eax
mov rax, 5
push rax
lea rax, [$g]
push rax
mov rax, 3
imul rax, rax, 4
pop rcx
add rax, rcx
mov rcx, rax
pop rax
mov dword [rcx], eax
lea rax, [$g]
push rax
mov rax, 1
imul rax, rax, 4
pop rcx
add rax, rcx
movsxd rax, dword [rax]
push rax
lea rax, [rbp-4]
mov rcx, rax
pop rax
mov dword [rcx], eax
lea rax, [$g]
push rax
mov rax, 3
imul rax, rax, 4
pop rcx
add rax, rcx
movsxd rax, dword [rax]
push rax
lea rax, [rbp-8]
mov rcx, rax
pop rax
mov dword [rcx], eax
lea rax, [rbp-4]
movsxd rax, dword [rax]
push rax
lea rax, [rbp-8]
movsxd rax, dword [rax]
mov rcx, rax
pop rax
add eax, ecx
movsxd rax, eax
mov rsp, rbp
pop rbp
ret 48
add rsp, 16
pop rbp
ret 48
section .data
section .rodata
section .note.GNU-stack noalloc noexec nowrite progbits
