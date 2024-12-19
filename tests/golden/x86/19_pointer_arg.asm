extern _GLOBAL_OFFSET_TABLE_
section .data
section .text
global $set
; set function
$set:
push ebp
mov ebp, esp
push ebx
push esi
push edi
call .pic_base
.pic_base:
pop esi
add esi, _GLOBAL_OFFSET_TABLE_+$$-.pic_base wrt ..gotpc
push dword [ebp+12]
push dword [ebp+8]
; INDIRECTION
pop ebx
push ebx
pop edx
pop eax
mov dword [edx], eax
push dword 0
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
lea ebx, [esi+$set wrt ..gotoff]
push ebx
pop ebx
mov dword [esi+function_call_1 wrt ..gotoff], ebx
sub esp, 4
push dword 12
lea ebx, [ebp-4]
push ebx
pop ebx
; PUSH ADDRESS &
push ebx
call [esi+function_call_1 wrt ..gotoff]
add esp, 8
add esp, 4
push eax
pop eax
push eax
add esp, 4
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
function_call_1: dd 0
section .rodata
section .note.GNU-stack noalloc noexec nowrite progbits
