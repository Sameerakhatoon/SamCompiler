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
lea rax, [str_1]
push rax
lea rax, [rbp-8]
mov rcx, rax
pop rax
mov qword [rcx], rax
lea rax, [rbp-8]
mov rax, qword [rax]
push rax
lea rax, [rbp-16]
mov rcx, rax
pop rax
mov qword [rcx], rax
xor eax, eax
push rax
lea rax, [rbp-20]
mov rcx, rax
pop rax
mov dword [rcx], eax
.entry_point_2:
.while_start_4:
lea rax, [rbp-16]
mov rax, qword [rax]
movsx rax, byte [rax]
mov rcx, 103
cmp rax, rcx
je .while_end_5
lea rax, [rbp-16]
mov rax, qword [rax]
mov rcx, 1
add rax, rcx
mov rcx, rax
lea rax, [rbp-16]
xchg rax, rcx
mov qword [rcx], rax
lea rax, [rbp-20]
movsxd rax, dword [rax]
mov rcx, 1
add eax, ecx
movsxd rax, eax
mov rcx, rax
lea rax, [rbp-20]
xchg rax, rcx
mov dword [rcx], eax
jmp .while_start_4
.while_end_5:
.exit_point_3:
lea rax, [rbp-20]
movsxd rax, dword [rax]
mov rsp, rbp
pop rbp
ret 48
add rsp, 32
pop rbp
ret 48
section .data
section .rodata
str_1: db 'a', 'b', 'c', 'd', 'e', 'f', 'g', 0
section .note.GNU-stack noalloc noexec nowrite progbits
