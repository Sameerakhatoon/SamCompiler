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
xor eax, eax
push rax
lea rax, [rbp-4]
mov rcx, rax
pop rax
mov dword [rcx], eax
mov rax, 1
push rax
lea rax, [rbp-8]
mov rcx, rax
pop rax
mov dword [rcx], eax
.entry_point_1:
.do_while_start_3:
.entry_point_4:
.switch_stmt_6:
lea rax, [rbp-8]
movsxd rax, dword [rax]
cmp eax, 1
je .switch_stmt_6_case_1
cmp eax, 2
je .switch_stmt_6_case_2
cmp eax, 3
je .switch_stmt_6_case_3
cmp eax, 4
je .switch_stmt_6_case_4
cmp eax, 5
je .switch_stmt_6_case_5
cmp eax, 6
je .switch_stmt_6_case_6
jmp .exit_point_5
.switch_stmt_6_case_1:
; CASE 1
lea rax, [rbp-4]
movsxd rax, dword [rax]
mov rcx, 1
add eax, ecx
movsxd rax, eax
mov rcx, rax
lea rax, [rbp-4]
xchg rax, rcx
mov dword [rcx], eax
jmp .exit_point_5
.switch_stmt_6_case_2:
; CASE 2
lea rax, [rbp-4]
movsxd rax, dword [rax]
mov rcx, 4
add eax, ecx
movsxd rax, eax
mov rcx, rax
lea rax, [rbp-4]
xchg rax, rcx
mov dword [rcx], eax
jmp .exit_point_5
.switch_stmt_6_case_3:
; CASE 3
lea rax, [rbp-4]
movsxd rax, dword [rax]
mov rcx, 9
add eax, ecx
movsxd rax, eax
mov rcx, rax
lea rax, [rbp-4]
xchg rax, rcx
mov dword [rcx], eax
jmp .exit_point_5
.switch_stmt_6_case_4:
; CASE 4
lea rax, [rbp-4]
movsxd rax, dword [rax]
mov rcx, 4
add eax, ecx
movsxd rax, eax
mov rcx, rax
lea rax, [rbp-4]
xchg rax, rcx
mov dword [rcx], eax
jmp .exit_point_5
.switch_stmt_6_case_5:
; CASE 5
lea rax, [rbp-4]
movsxd rax, dword [rax]
mov rcx, 5
add eax, ecx
movsxd rax, eax
mov rcx, rax
lea rax, [rbp-4]
xchg rax, rcx
mov dword [rcx], eax
jmp .exit_point_5
.switch_stmt_6_case_6:
; CASE 6
lea rax, [rbp-4]
movsxd rax, dword [rax]
mov rcx, 6
add eax, ecx
movsxd rax, eax
mov rcx, rax
lea rax, [rbp-4]
xchg rax, rcx
mov dword [rcx], eax
jmp .exit_point_5
.switch_stmt_6_end:
.exit_point_5:
lea rax, [rbp-8]
movsxd rax, dword [rax]
mov rcx, 1
add eax, ecx
movsxd rax, eax
mov rcx, rax
lea rax, [rbp-8]
xchg rax, rcx
mov dword [rcx], eax
lea rax, [rbp-8]
movsxd rax, dword [rax]
mov rcx, 6
cmp rax, rcx
setle al
movzx eax, al
cmp rax, 0
jne .do_while_start_3
.exit_point_2:
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
