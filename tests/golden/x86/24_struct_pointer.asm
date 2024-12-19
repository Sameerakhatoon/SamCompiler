extern _GLOBAL_OFFSET_TABLE_
section .data
section .text
global $sum
; sum function
$sum:
push ebp
mov ebp, esp
push ebx
push esi
push edi
call .pic_base
.pic_base:
pop esi
add esi, _GLOBAL_OFFSET_TABLE_+$$-.pic_base wrt ..gotpc
lea ebx, [ebp+8]
push ebx
pop ebx
mov ebx, [ebx]
add ebx, 0
push ebx
pop eax
mov eax, [eax]
push eax
lea ebx, [ebp+8]
push ebx
pop ebx
mov ebx, [ebx]
add ebx, 4
push ebx
pop eax
mov eax, [eax]
push eax
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
push dword 5
pop eax
mov dword [ebp-8], eax
push dword 10
pop eax
mov dword [ebp-4], eax
lea ebx, [esi+$sum wrt ..gotoff]
push ebx
pop ebx
mov dword [esi+function_call_1 wrt ..gotoff], ebx
sub esp, 8
lea ebx, [ebp-8]
push ebx
pop ebx
; PUSH ADDRESS &
push ebx
call [esi+function_call_1 wrt ..gotoff]
add esp, 4
add esp, 8
push eax
pop eax
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
function_call_1: dd 0
section .rodata
section .note.GNU-stack noalloc noexec nowrite progbits
