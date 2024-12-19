extern _GLOBAL_OFFSET_TABLE_
section .data
section .text
global $main
; main function
$main:
push ebp
mov ebp, esp
sub esp, 32
push ebx
push esi
push edi
call .pic_base
.pic_base:
pop esi
add esi, _GLOBAL_OFFSET_TABLE_+$$-.pic_base wrt ..gotpc
push dword 1
pop eax
mov dword [ebp-12], eax
push dword 2
pop eax
mov dword [ebp-8], eax
push dword 3
pop eax
mov dword [ebp-4], eax
push dword [ebp-12]
pop eax
push eax
pop eax
mov dword [ebp-16], eax
push dword [ebp-8]
pop eax
push eax
pop eax
mov dword [ebp-20], eax
push dword [ebp-4]
pop eax
push eax
pop eax
mov dword [ebp-24], eax
push dword [ebp-16]
push dword [ebp-20]
pop ecx
pop eax
add eax, ecx
push eax
push dword [ebp-24]
pop ecx
pop eax
add eax, ecx
push eax
pop eax
mov ebx, [ebp-36]
mov esi, [ebp-40]
mov edi, [ebp-44]
mov esp, ebp
pop ebp
ret
pop edi
pop esi
pop ebx
add esp, 32
pop ebp
ret
section .data
section .rodata
section .note.GNU-stack noalloc noexec nowrite progbits
