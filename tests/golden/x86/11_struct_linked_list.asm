extern _GLOBAL_OFFSET_TABLE_
section .data
; node nodes
global $nodes
$nodes: times 40 db  0
section .text
global $sum_from
; sum_from function
$sum_from:
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
push dword 0
pop eax
mov dword [ebp-4], eax
push dword [ebp+8]
pop eax
mov dword [ebp-8], eax
.entry_point_1:
.while_start_3:
push dword [ebp-8]
push dword 0
pop ecx
pop eax
cmp eax, ecx
setge al
movzx eax, al
push eax
pop eax
cmp eax, 0
je .while_end_4
push dword [ebp-4]
lea ebx, [esi+$nodes wrt ..gotoff]
push ebx
pop ebx
push dword [ebp-8]
pop eax
imul eax, 8
add ebx, eax
push ebx
pop ebx
add ebx, 0
push ebx
pop eax
mov eax, [eax]
push eax
pop ecx
pop eax
add eax, ecx
push eax
pop eax
mov dword [ebp-4], eax
lea ebx, [esi+$nodes wrt ..gotoff]
push ebx
pop ebx
push dword [ebp-8]
pop eax
imul eax, 8
add ebx, eax
push ebx
pop ebx
add ebx, 4
push ebx
pop eax
mov eax, [eax]
push eax
pop eax
mov dword [ebp-8], eax
jmp .while_start_3
.while_end_4:
.exit_point_2:
push dword [ebp-4]
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
push dword 10
lea ebx, [esi+$nodes wrt ..gotoff]
push ebx
pop ebx
add ebx, 0
push ebx
pop edx
pop eax
mov dword [edx], eax
push dword 2
lea ebx, [esi+$nodes wrt ..gotoff]
push ebx
pop ebx
add ebx, 4
push ebx
pop edx
pop eax
mov dword [edx], eax
push dword 20
lea ebx, [esi+$nodes wrt ..gotoff]
push ebx
pop ebx
add ebx, 16
push ebx
pop edx
pop eax
mov dword [edx], eax
push dword 4
lea ebx, [esi+$nodes wrt ..gotoff]
push ebx
pop ebx
add ebx, 20
push ebx
pop edx
pop eax
mov dword [edx], eax
push dword 30
lea ebx, [esi+$nodes wrt ..gotoff]
push ebx
pop ebx
add ebx, 32
push ebx
pop edx
pop eax
mov dword [edx], eax
push dword 1
pop eax
neg eax
push eax
lea ebx, [esi+$nodes wrt ..gotoff]
push ebx
pop ebx
add ebx, 36
push ebx
pop edx
pop eax
mov dword [edx], eax
push dword 999
lea ebx, [esi+$nodes wrt ..gotoff]
push ebx
pop ebx
add ebx, 8
push ebx
pop edx
pop eax
mov dword [edx], eax
push dword 1
pop eax
neg eax
push eax
lea ebx, [esi+$nodes wrt ..gotoff]
push ebx
pop ebx
add ebx, 12
push ebx
pop edx
pop eax
mov dword [edx], eax
push dword 777
lea ebx, [esi+$nodes wrt ..gotoff]
push ebx
pop ebx
add ebx, 24
push ebx
pop edx
pop eax
mov dword [edx], eax
push dword 1
pop eax
neg eax
push eax
lea ebx, [esi+$nodes wrt ..gotoff]
push ebx
pop ebx
add ebx, 28
push ebx
pop edx
pop eax
mov dword [edx], eax
lea ebx, [esi+$sum_from wrt ..gotoff]
push ebx
pop ebx
mov dword [esi+function_call_5 wrt ..gotoff], ebx
sub esp, 8
push dword 0
call [esi+function_call_5 wrt ..gotoff]
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
function_call_5: dd 0
section .rodata
section .note.GNU-stack noalloc noexec nowrite progbits
