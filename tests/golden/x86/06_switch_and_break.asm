extern _GLOBAL_OFFSET_TABLE_
section .data
section .text
global $main
; main function
$main:
push ebp
mov ebp, esp
sub esp, 16
push ebx
push esi
push edi
call .pic_base
.pic_base:
pop esi
add esi, _GLOBAL_OFFSET_TABLE_+$$-.pic_base wrt ..gotpc
push dword 0
pop eax
mov dword [ebp-4], eax
push dword 1
pop eax
mov dword [ebp-8], eax
.entry_point_1:
.do_while_start_3:
.entry_point_4:
.switch_stmt_6:
push dword [ebp-8]
pop eax
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
push dword [ebp-4]
push dword 1
pop ecx
pop eax
add eax, ecx
push eax
pop eax
mov dword [ebp-4], eax
jmp .exit_point_5
.switch_stmt_6_case_2:
; CASE 2
push dword [ebp-4]
push dword 4
pop ecx
pop eax
add eax, ecx
push eax
pop eax
mov dword [ebp-4], eax
jmp .exit_point_5
.switch_stmt_6_case_3:
; CASE 3
push dword [ebp-4]
push dword 9
pop ecx
pop eax
add eax, ecx
push eax
pop eax
mov dword [ebp-4], eax
jmp .exit_point_5
.switch_stmt_6_case_4:
; CASE 4
push dword [ebp-4]
push dword 4
pop ecx
pop eax
add eax, ecx
push eax
pop eax
mov dword [ebp-4], eax
jmp .exit_point_5
.switch_stmt_6_case_5:
; CASE 5
push dword [ebp-4]
push dword 5
pop ecx
pop eax
add eax, ecx
push eax
pop eax
mov dword [ebp-4], eax
jmp .exit_point_5
.switch_stmt_6_case_6:
; CASE 6
push dword [ebp-4]
push dword 6
pop ecx
pop eax
add eax, ecx
push eax
pop eax
mov dword [ebp-4], eax
jmp .exit_point_5
.switch_stmt_6_end:
.exit_point_5:
push dword [ebp-8]
push dword 1
pop ecx
pop eax
add eax, ecx
push eax
pop eax
mov dword [ebp-8], eax
push dword [ebp-8]
push dword 6
pop ecx
pop eax
cmp eax, ecx
setle al
movzx eax, al
push eax
pop eax
cmp eax, 0
jne .do_while_start_3
.exit_point_2:
push dword [ebp-4]
pop eax
mov ebx, [ebp-20]
mov esi, [ebp-24]
mov edi, [ebp-28]
mov esp, ebp
pop ebp
ret
pop edi
pop esi
pop ebx
add esp, 16
pop ebp
ret
section .data
section .rodata
section .note.GNU-stack noalloc noexec nowrite progbits
