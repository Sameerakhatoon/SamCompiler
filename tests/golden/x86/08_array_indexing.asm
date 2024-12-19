extern _GLOBAL_OFFSET_TABLE_
section .data
; cell values
global $values
$values: times 20 db  0
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
push dword 0
pop eax
mov dword [ebp-4], eax
push dword 0
pop eax
mov dword [ebp-4], eax
jmp .for_loop1
.entry_point_3:
push dword [ebp-4]
push dword 1
pop ecx
pop eax
add eax, ecx
push eax
pop eax
mov dword [ebp-4], eax
.for_loop1:
push dword [ebp-4]
push dword 5
pop ecx
pop eax
cmp eax, ecx
setl al
movzx eax, al
push eax
pop eax
cmp eax, 0
je .for_loop_end2
push dword 5
lea ebx, [esi+$values wrt ..gotoff]
push ebx
pop ebx
push dword [ebp-4]
pop eax
imul eax, 4
add ebx, eax
push ebx
pop ebx
add ebx, 0
push ebx
pop edx
pop eax
mov dword [edx], eax
push dword [ebp-4]
push dword 1
pop ecx
pop eax
add eax, ecx
push eax
pop eax
mov dword [ebp-4], eax
jmp .for_loop1
.for_loop_end2:
.exit_point_4:
push dword 0
pop eax
mov dword [ebp-8], eax
push dword 0
pop eax
mov dword [ebp-4], eax
jmp .for_loop5
.entry_point_7:
push dword [ebp-4]
push dword 1
pop ecx
pop eax
add eax, ecx
push eax
pop eax
mov dword [ebp-4], eax
.for_loop5:
push dword [ebp-4]
push dword 5
pop ecx
pop eax
cmp eax, ecx
setl al
movzx eax, al
push eax
pop eax
cmp eax, 0
je .for_loop_end6
push dword [ebp-8]
lea ebx, [esi+$values wrt ..gotoff]
push ebx
pop ebx
push dword [ebp-4]
pop eax
imul eax, 4
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
mov dword [ebp-8], eax
push dword [ebp-4]
push dword 1
pop ecx
pop eax
add eax, ecx
push eax
pop eax
mov dword [ebp-4], eax
jmp .for_loop5
.for_loop_end6:
.exit_point_8:
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
