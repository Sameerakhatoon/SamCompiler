extern _GLOBAL_OFFSET_TABLE_
section .data
section .text
global $pick
; pick function
$pick:
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
push dword 1
pop ecx
pop eax
cmp eax, ecx
sete al
movzx eax, al
push eax
pop eax
cmp eax, 0
je .if_2
push dword 10
pop eax
mov ebx, [ebp-4]
mov esi, [ebp-8]
mov edi, [ebp-12]
mov esp, ebp
pop ebp
ret
jmp .if_end_1
.if_2:
push dword [ebp+8]
push dword 2
pop ecx
pop eax
cmp eax, ecx
sete al
movzx eax, al
push eax
pop eax
cmp eax, 0
je .if_3
push dword 2
pop eax
mov ebx, [ebp-4]
mov esi, [ebp-8]
mov edi, [ebp-12]
mov esp, ebp
pop ebp
ret
jmp .if_end_1
.if_3:
push dword 30
pop eax
mov ebx, [ebp-4]
mov esi, [ebp-8]
mov edi, [ebp-12]
mov esp, ebp
pop ebp
ret
.if_end_1:
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
push ebx
push esi
push edi
call .pic_base
.pic_base:
pop esi
add esi, _GLOBAL_OFFSET_TABLE_+$$-.pic_base wrt ..gotpc
lea ebx, [esi+$pick wrt ..gotoff]
push ebx
pop ebx
mov dword [esi+function_call_4 wrt ..gotoff], ebx
sub esp, 8
push dword 2
call [esi+function_call_4 wrt ..gotoff]
add esp, 4
add esp, 8
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
function_call_4: dd 0
section .rodata
section .note.GNU-stack noalloc noexec nowrite progbits
