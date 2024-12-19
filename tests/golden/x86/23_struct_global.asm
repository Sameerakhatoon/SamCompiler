extern _GLOBAL_OFFSET_TABLE_
section .data
; pt g
global $g
$g: dq 0
section .text
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
push dword 6
pop eax
mov dword [esi+$g wrt ..gotoff], eax
push dword 7
pop eax
mov dword [esi+$g+4 wrt ..gotoff], eax
push dword [esi+$g wrt ..gotoff]
pop eax
push eax
push dword [esi+$g+4 wrt ..gotoff]
pop eax
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
section .data
section .rodata
section .note.GNU-stack noalloc noexec nowrite progbits
