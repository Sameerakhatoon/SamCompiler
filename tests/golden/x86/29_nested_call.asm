extern _GLOBAL_OFFSET_TABLE_
section .data
section .text
global $add
; add function
$add:
push ebp
mov ebp, esp
push ebx
push esi
push edi
call .pic_base
.pic_base:
pop esi
add esi, _GLOBAL_OFFSET_TABLE_+$$-.pic_base wrt ..gotpc
push dword [ebp+8]
push dword [ebp+12]
pop ecx
pop eax
add eax, ecx
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
global $twice
; twice function
$twice:
push ebp
mov ebp, esp
push ebx
push esi
push edi
call .pic_base
.pic_base:
pop esi
add esi, _GLOBAL_OFFSET_TABLE_+$$-.pic_base wrt ..gotpc
push dword [ebp+8]
push dword 2
pop ecx
pop eax
mov ecx, ecx
imul ecx
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
lea ebx, [esi+$add wrt ..gotoff]
push ebx
pop ebx
mov dword [esi+function_call_1 wrt ..gotoff], ebx
sub esp, 4
lea ebx, [esi+$add wrt ..gotoff]
push ebx
pop ebx
mov dword [esi+function_call_2 wrt ..gotoff], ebx
push dword 4
push dword 3
call [esi+function_call_2 wrt ..gotoff]
add esp, 8
push eax
pop eax
push eax
lea ebx, [esi+$twice wrt ..gotoff]
push ebx
pop ebx
mov dword [esi+function_call_3 wrt ..gotoff], ebx
push dword 5
call [esi+function_call_3 wrt ..gotoff]
add esp, 4
push eax
pop eax
push eax
call [esi+function_call_1 wrt ..gotoff]
add esp, 8
add esp, 4
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
function_call_1: dd 0
function_call_2: dd 0
function_call_3: dd 0
section .rodata
section .note.GNU-stack noalloc noexec nowrite progbits
