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
mov rax, 300
push rax
lea rax, [rbp-4]
mov rcx, rax
pop rax
mov dword [rcx], eax
lea rax, [rbp-4]
movsxd rax, dword [rax]
movsx rax, al
push rax
lea rax, [rbp-5]
mov rcx, rax
pop rax
mov byte [rcx], al
lea rax, [rbp-5]
movsx rax, byte [rax]
mov rsp, rbp
pop rbp
ret 48
add rsp, 16
pop rbp
ret 48
section .data
section .rodata
section .note.GNU-stack noalloc noexec nowrite progbits
