default rel
section .data
section .text
global $classify
; classify function
$classify:
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
.switch_stmt_3:
lea rax, [rbp+16]
movsxd rax, dword [rax]
cmp eax, 1
je .switch_stmt_3_case_1
cmp eax, 2
je .switch_stmt_3_case_2
jmp .switch_stmt_3_case_default
.switch_stmt_3_case_1:
; CASE 1
mov rax, 10
mov rcx, rax
lea rax, [rbp-4]
xchg rax, rcx
mov dword [rcx], eax
jmp .exit_point_2
.switch_stmt_3_case_2:
; CASE 2
mov rax, 20
mov rcx, rax
lea rax, [rbp-4]
xchg rax, rcx
mov dword [rcx], eax
jmp .exit_point_2
; DEFAULT CASE
.switch_stmt_3_case_default:
mov rax, 3
mov rcx, rax
lea rax, [rbp-4]
xchg rax, rcx
mov dword [rcx], eax
jmp .exit_point_2
.switch_stmt_3_end:
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
mov rax, 2
push rax
pop rdi
lea r11, [$classify]
xor eax, eax
call r11
movsxd rax, eax
push rax
lea rax, [rbp-4]
mov rcx, rax
pop rax
mov dword [rcx], eax
mov rax, 1
push rax
pop rdi
lea r11, [$classify]
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
