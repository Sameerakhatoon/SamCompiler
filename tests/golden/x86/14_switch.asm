section .data
section .text
global classify
; classify function
classify:
push ebp
mov ebp, esp
sub esp, 16
push dword 0
pop eax
mov dword [ebp-4], eax
.entry_point_1:
.switch_stmt_3:
push dword [ebp+8]
pop eax
cmp eax, 1
je .switch_stmt_3_case_1
cmp eax, 2
je .switch_stmt_3_case_2
jmp .exit_point_2
.switch_stmt_3_case_1:
; CASE 1
push dword 10
pop eax
mov dword [ebp-4], eax
jmp .exit_point_2
.switch_stmt_3_case_2:
; CASE 2
push dword 20
pop eax
mov dword [ebp-4], eax
jmp .exit_point_2
; DEFAULT CASE
.switch_stmt_3_case_default:
push dword 3
pop eax
mov dword [ebp-4], eax
jmp .exit_point_2
.switch_stmt_3_end:
.exit_point_2:
push dword [ebp-4]
pop eax
add esp, 16
pop ebp
ret
add esp, 16
pop ebp
ret
global main
; main function
main:
push ebp
mov ebp, esp
sub esp, 16
lea ebx, [classify]
push ebx
pop ebx
mov dword [function_call_4], ebx
push dword 2
call [function_call_4]
add esp, 4
push eax
pop eax
push eax
pop eax
mov dword [ebp-4], eax
lea ebx, [classify]
push ebx
pop ebx
mov dword [function_call_5], ebx
push dword 1
call [function_call_5]
add esp, 4
push eax
pop eax
push eax
pop eax
mov dword [ebp-8], eax
push dword [ebp-4]
push dword [ebp-8]
pop ecx
pop eax
add eax, ecx
push eax
pop eax
add esp, 16
pop ebp
ret
add esp, 16
pop ebp
ret
section .data
function_call_4: dd 0
function_call_5: dd 0
section .rodata
