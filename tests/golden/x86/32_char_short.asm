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
push dword 2
pop eax
neg eax
push eax
pop eax
mov byte [ebp-1], al
push dword 5
pop eax
mov word [ebp-4], ax
mov eax, [ebp-1]
movsx eax, al
push eax
mov eax, [ebp-4]
movsx eax, ax
push eax
pop ecx
pop eax
add eax, ecx
push eax
pop eax
mov dword [ebp-8], eax
push dword [ebp-8]
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
