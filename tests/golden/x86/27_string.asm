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
mov ebx, [ebp-4]
push ebx
pop ebx
push dword 1
pop eax
add ebx, eax
push ebx
pop eax
mov eax, [eax]
movsx eax, al
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
str_1: db 'h', 'i', 0
section .note.GNU-stack noalloc noexec nowrite progbits
