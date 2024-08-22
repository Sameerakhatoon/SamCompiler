section .data
; scanner sc
sc: times 16 db  0
section .text
global is_space
; is_space function
is_space:
push ebp
mov ebp, esp
push ebx
push dword [ebp+8]
push dword 32
pop ecx
pop eax
cmp eax, ecx
sete al
movzx eax, al
push eax
pop eax
mov ebx, [ebp-4]
mov esp, ebp
pop ebp
ret
pop ebx
pop ebp
ret
global scan_step
; scan_step function
scan_step:
push ebp
mov ebp, esp
sub esp, 16
push ebx
mov ebx, [sc]
push ebx
pop ebx
push dword [sc+4]
pop eax
push eax
pop eax
add ebx, eax
push ebx
pop eax
mov eax, [eax]
movsx eax, al
push eax
pop eax
mov dword [ebp-4], eax
push dword [ebp-4]
push dword 0
pop ecx
pop eax
cmp eax, ecx
sete al
movzx eax, al
push eax
pop eax
cmp eax, 0
je .if_2
push dword 0
pop eax
mov ebx, [ebp-20]
mov esp, ebp
pop ebp
ret
jmp .if_end_1
.if_2:
.if_end_1:
.entry_point_3:
.switch_stmt_5:
lea ebx, [is_space]
push ebx
pop ebx
mov dword [function_call_6], ebx
push dword [ebp-4]
call [function_call_6]
add esp, 4
push eax
pop eax
push eax
pop eax
cmp eax, 1
je .switch_stmt_5_case_1
cmp eax, 0
je .switch_stmt_5_case_0
jmp .exit_point_4
.switch_stmt_5_case_1:
; CASE 1
push dword 0
pop eax
mov dword [sc+12], eax
jmp .exit_point_4
.switch_stmt_5_case_0:
; CASE 0
push dword [sc+12]
pop eax
push eax
push dword 0
pop ecx
pop eax
cmp eax, ecx
sete al
movzx eax, al
push eax
pop eax
cmp eax, 0
je .if_8
push dword [sc+8]
pop eax
push eax
push dword 1
pop ecx
pop eax
add eax, ecx
push eax
pop eax
mov dword [sc+8], eax
push dword 1
pop eax
mov dword [sc+12], eax
jmp .if_end_7
.if_8:
.if_end_7:
jmp .exit_point_4
.switch_stmt_5_end:
.exit_point_4:
push dword [sc+4]
pop eax
push eax
push dword 1
pop ecx
pop eax
add eax, ecx
push eax
pop eax
mov dword [sc+4], eax
push dword 0
pop eax
mov ebx, [ebp-20]
mov esp, ebp
pop ebp
ret
pop ebx
add esp, 16
pop ebp
ret
global main
; main function
main:
push ebp
mov ebp, esp
push ebx
mov eax, str_9
push eax
pop eax
mov dword [sc], eax
push dword 0
pop eax
mov dword [sc+4], eax
push dword 0
pop eax
mov dword [sc+8], eax
push dword 0
pop eax
mov dword [sc+12], eax
.entry_point_10:
.while_start_12:
mov ebx, [sc]
push ebx
pop ebx
push dword [sc+4]
pop eax
push eax
pop eax
add ebx, eax
push ebx
pop eax
mov eax, [eax]
movsx eax, al
push eax
push dword 0
pop ecx
pop eax
cmp eax, ecx
setne al
movzx eax, al
push eax
pop eax
cmp eax, 0
je .while_end_13
lea ebx, [scan_step]
push ebx
pop ebx
mov dword [function_call_14], ebx
sub esp, 4
call [function_call_14]
add esp, 4
push eax
pop eax
push eax
add esp, 4
jmp .while_start_12
.while_end_13:
.exit_point_11:
push dword [sc+8]
pop eax
push eax
pop eax
mov ebx, [ebp-4]
mov esp, ebp
pop ebp
ret
pop ebx
pop ebp
ret
section .data
function_call_6: dd 0
function_call_14: dd 0
section .rodata
str_9: db 'o', 'n', 'e', ' ', 't', 'w', 'o', ' ', 't', 'h', 'r', 'e', 'e', ' ', 'f', 'o', 'u', 'r', 0
