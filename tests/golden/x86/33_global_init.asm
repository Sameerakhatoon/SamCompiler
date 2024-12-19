extern _GLOBAL_OFFSET_TABLE_
section .data
; int g
global $g
$g: dd 20
; char k
global $k
$k: db 100
; char msg
global $msg
$msg: dd str_1
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
push dword [esi+$g wrt ..gotoff]
mov eax, [esi+$k wrt ..gotoff]
movsx eax, al
push eax
pop ecx
pop eax
add eax, ecx
push eax
mov ebx, [esi+$msg wrt ..gotoff]
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
str_1: db 'x', 0
section .note.GNU-stack noalloc noexec nowrite progbits
