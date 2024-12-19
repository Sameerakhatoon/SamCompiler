extern _GLOBAL_OFFSET_TABLE_
section .data
section .text
global $popcount
; popcount function
$popcount:
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
.entry_point_1:
.while_start_3:
push dword [ebp+8]
push dword 0
pop ecx
pop eax
cmp eax, ecx
setne al
movzx eax, al
push eax
pop eax
cmp eax, 0
je .while_end_4
push dword [ebp-4]
push dword [ebp+8]
push dword 1
pop ecx
pop eax
and eax, ecx
push eax
pop ecx
pop eax
add eax, ecx
push eax
pop eax
mov dword [ebp-4], eax
push dword [ebp+8]
push dword 1
pop ecx
pop eax
sar eax, cl
push eax
pop eax
mov dword [ebp+8], eax
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
sub esp, 16
push ebx
push esi
push edi
call .pic_base
.pic_base:
pop esi
add esi, _GLOBAL_OFFSET_TABLE_+$$-.pic_base wrt ..gotpc
lea ebx, [esi+$popcount wrt ..gotoff]
push ebx
pop ebx
mov dword [esi+function_call_5 wrt ..gotoff], ebx
sub esp, 8
push dword 85
call [esi+function_call_5 wrt ..gotoff]
add esp, 4
add esp, 8
push eax
pop eax
push eax
pop eax
mov dword [ebp-4], eax
lea ebx, [esi+$popcount wrt ..gotoff]
push ebx
pop ebx
mov dword [esi+function_call_6 wrt ..gotoff], ebx
sub esp, 8
push dword 7
call [esi+function_call_6 wrt ..gotoff]
add esp, 4
add esp, 8
push eax
pop eax
push eax
pop eax
mov dword [ebp-8], eax
push dword [ebp-4]
push dword [ebp-8]
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
function_call_5: dd 0
function_call_6: dd 0
section .rodata
section .note.GNU-stack noalloc noexec nowrite progbits
