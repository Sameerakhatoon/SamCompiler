default rel
section .data
section .text
global $popcount
; popcount function
$popcount:
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
.entry_point_1:
.while_start_3:
lea rax, [rbp+16]
movsxd rax, dword [rax]
xor ecx, ecx
cmp rax, rcx
je .while_end_4
lea rax, [rbp-4]
movsxd rax, dword [rax]
push rax
lea rax, [rbp+16]
movsxd rax, dword [rax]
mov rcx, 1
and eax, ecx
movsxd rax, eax
mov rcx, rax
pop rax
add eax, ecx
movsxd rax, eax
mov rcx, rax
lea rax, [rbp-4]
xchg rax, rcx
mov dword [rcx], eax
lea rax, [rbp+16]
movsxd rax, dword [rax]
mov rcx, 1
sar eax, cl
movsxd rax, eax
mov rcx, rax
lea rax, [rbp+16]
xchg rax, rcx
mov dword [rcx], eax
jmp .while_start_3
.while_end_4:
.exit_point_2:
lea rax, [rbp-4]
movsxd rax, dword [rax]
mov rsp, rbp
pop rbp
ret 48
add rsp, 16
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
mov rax, 85
push rax
pop rdi
lea r11, [$popcount]
xor eax, eax
call r11
movsxd rax, eax
push rax
lea rax, [rbp-4]
mov rcx, rax
pop rax
mov dword [rcx], eax
mov rax, 7
push rax
pop rdi
lea r11, [$popcount]
xor eax, eax
call r11
movsxd rax, eax
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
