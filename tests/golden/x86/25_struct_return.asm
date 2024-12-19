extern _GLOBAL_OFFSET_TABLE_
section .data
section .text
global $make
; make function
$make:
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
push dword [ebp+12]
pop eax
mov dword [ebp-8], eax
push dword [ebp+16]
pop eax
mov dword [ebp-4], eax
lea ebx, [ebp-8]
push ebx
pop ebx
; STRUCTURE PUSH
push dword [ebx+4]
push dword [ebx+0]
; END STRUCTURE PUSH
mov edx, [ebp+8]
pop eax
mov [edx+0], eax
pop eax
mov [edx+4], eax
mov eax, [ebp+8]
mov ebx, [ebp-20]
mov esi, [ebp-24]
mov edi, [ebp-28]
mov esp, ebp
pop ebp
ret 4
pop edi
pop esi
pop ebx
add esp, 16
pop ebp
ret 4
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
lea ebx, [esi+$make wrt ..gotoff]
push ebx
pop ebx
mov dword [esi+function_call_1 wrt ..gotoff], ebx
; SUBTRACT ROOM FOR RETURNED STRUCTURE/UNION DATATYPE
sub esp, 8
sub esp, 8
push dword 7
push dword 2
lea ebx, [esp+16]
push ebx
call [esi+function_call_1 wrt ..gotoff]
add esp, 8
add esp, 8
pop eax
mov [ebp-8+0], eax
pop eax
mov [ebp-8+4], eax
push dword [ebp-8]
pop eax
push eax
push dword [ebp-4]
pop eax
push eax
pop ecx
pop eax
add eax, ecx
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
