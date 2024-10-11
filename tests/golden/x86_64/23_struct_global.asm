default rel
section .data
; pt g
global $g
$g: dq 0
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
mov rax, 6
push rax
lea rax, [$g]
mov rcx, rax
pop rax
mov dword [rcx], eax
mov rax, 7
push rax
lea rax, [$g]
add rax, 4
mov rcx, rax
pop rax
mov dword [rcx], eax
lea rax, [$g]
movsxd rax, dword [rax]
push rax
lea rax, [$g]
add rax, 4
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
section .data
section .rodata
section .note.GNU-stack noalloc noexec nowrite progbits
