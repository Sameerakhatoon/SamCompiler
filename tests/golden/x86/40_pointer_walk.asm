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
lea eax, [esi+str_1 wrt ..gotoff]
push eax
pop eax
mov dword [ebp-4], eax
push dword [ebp-4]
pop eax
mov dword [ebp-8], eax
push dword 0
pop eax
mov dword [ebp-12], eax
.entry_point_2:
.while_start_4:
push dword [ebp-8]
; INDIRECTION
pop ebx
mov ebx, [ebx]
push ebx
pop eax
movsx eax, al
push eax
push dword 103
pop ecx
pop eax
cmp eax, ecx
setne al
movzx eax, al
push eax
pop eax
cmp eax, 0
je .while_end_5
push dword [ebp-8]
push dword 1
pop ecx
pop eax
add eax, ecx
push eax
pop eax
mov dword [ebp-8], eax
push dword [ebp-12]
push dword 1
pop ecx
pop eax
add eax, ecx
push eax
pop eax
mov dword [ebp-12], eax
jmp .while_start_4
.while_end_5:
.exit_point_3:
push dword [ebp-12]
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
str_1: db 'a', 'b', 'c', 'd', 'e', 'f', 'g', 0
section .note.GNU-stack noalloc noexec nowrite progbits
