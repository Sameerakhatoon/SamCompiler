extern _GLOBAL_OFFSET_TABLE_
section .data
section .text
global $simple_hash
; simple_hash function
$simple_hash:
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
mov ebx, [ebp+8]
push ebx
pop ebx
push dword [ebp-8]
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
je .while_end_4
push dword [ebp-4]
mov ebx, [ebp+8]
push ebx
pop ebx
push dword [ebp-8]
pop eax
add ebx, eax
push ebx
pop eax
mov eax, [eax]
movsx eax, al
push eax
pop ecx
pop eax
add eax, ecx
push eax
pop eax
mov dword [ebp-4], eax
push dword [ebp-8]
push dword 1
pop ecx
pop eax
add eax, ecx
push eax
pop eax
mov dword [ebp-8], eax
jmp .while_start_3
.while_end_4:
.exit_point_2:
.entry_point_5:
.while_start_7:
push dword [ebp-4]
push dword 17
pop ecx
pop eax
cmp eax, ecx
setge al
movzx eax, al
push eax
pop eax
cmp eax, 0
je .while_end_8
push dword [ebp-4]
push dword 17
pop ecx
pop eax
sub eax, ecx
push eax
pop eax
mov dword [ebp-4], eax
jmp .while_start_7
.while_end_8:
.exit_point_6:
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
global $main
; main function
$main:
push ebp
mov ebp, esp
push ebx
push esi
push edi
call .pic_base
.pic_base:
pop esi
add esi, _GLOBAL_OFFSET_TABLE_+$$-.pic_base wrt ..gotpc
lea ebx, [esi+$simple_hash wrt ..gotoff]
push ebx
pop ebx
mov dword [esi+function_call_9 wrt ..gotoff], ebx
sub esp, 8
lea eax, [esi+str_10 wrt ..gotoff]
push eax
call [esi+function_call_9 wrt ..gotoff]
add esp, 4
add esp, 8
push eax
pop eax
push eax
pop eax
mov ebx, [ebp-4]
mov esi, [ebp-8]
mov edi, [ebp-12]
mov esp, ebp
pop ebp
ret
pop edi
pop esi
pop ebx
pop ebp
ret
section .data
function_call_9: dd 0
section .rodata
str_10: db 'a', 'g', 0
section .note.GNU-stack noalloc noexec nowrite progbits
