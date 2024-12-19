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
push dword 0
pop eax
mov dword [ebp-8], eax
.entry_point_1:
.while_start_3:
push dword [ebp-4]
push dword 100
pop ecx
pop eax
cmp eax, ecx
setl al
movzx eax, al
push eax
pop eax
cmp eax, 0
je .while_end_4
push dword [ebp-4]
push dword 1
pop ecx
pop eax
add eax, ecx
push eax
pop eax
mov dword [ebp-4], eax
push dword [ebp-4]
push dword 2
pop ecx
pop eax
cmp eax, ecx
sete al
movzx eax, al
push eax
pop eax
cmp eax, 0
je .if_6
jmp .entry_point_1
jmp .if_end_5
.if_6:
.if_end_5:
push dword [ebp-4]
push dword 4
pop ecx
pop eax
cmp eax, ecx
setg al
movzx eax, al
push eax
pop eax
cmp eax, 0
je .if_8
jmp .exit_point_2
jmp .if_end_7
.if_8:
.if_end_7:
push dword [ebp-8]
push dword [ebp-4]
pop ecx
pop eax
add eax, ecx
push eax
pop eax
mov dword [ebp-8], eax
jmp .while_start_3
.while_end_4:
.exit_point_2:
push dword [ebp-8]
push dword 1
pop ecx
pop eax
add eax, ecx
push eax
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
