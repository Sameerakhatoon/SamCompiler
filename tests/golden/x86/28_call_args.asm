extern _GLOBAL_OFFSET_TABLE_
section .data
section .text
global $f
; f function
$f:
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
push dword 100
pop ecx
pop eax
mov ecx, ecx
imul ecx
push eax
push dword [ebp+12]
push dword 10
pop ecx
pop eax
mov ecx, ecx
imul ecx
push eax
pop ecx
pop eax
add eax, ecx
push eax
push dword [ebp+16]
pop ecx
pop eax
add eax, ecx
push eax
push dword 300
pop ecx
pop eax
sub eax, ecx
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
lea ebx, [esi+$f wrt ..gotoff]
push ebx
pop ebx
mov dword [esi+function_call_1 wrt ..gotoff], ebx
push dword 6
push dword 2
push dword 3
call [esi+function_call_1 wrt ..gotoff]
add esp, 12
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
section .rodata
section .note.GNU-stack noalloc noexec nowrite progbits
