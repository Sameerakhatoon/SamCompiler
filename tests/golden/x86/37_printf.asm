extern _GLOBAL_OFFSET_TABLE_
section .data
section .text
extern $printf
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
mov edi, [esi+$printf wrt ..got]
lea ebx, [edi]
push ebx
pop ebx
mov dword [esi+function_call_1 wrt ..gotoff], ebx
lea eax, [esi+str_2 wrt ..gotoff]
push eax
push dword 42
lea eax, [esi+str_3 wrt ..gotoff]
push eax
call [esi+function_call_1 wrt ..gotoff]
add esp, 12
push eax
pop eax
push eax
add esp, 4
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
section .data
function_call_1: dd 0
section .rodata
str_2: db 'h', 'i', 0
str_3: db 'v', '=', '%', 'd', ' ', 's', '=', '%', 's', 10, 0
section .note.GNU-stack noalloc noexec nowrite progbits
