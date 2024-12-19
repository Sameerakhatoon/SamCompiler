extern _GLOBAL_OFFSET_TABLE_
section .data
; cell values
global $values
$values: times 24 db  0
section .text
global $bubble_sort
; bubble_sort function
$bubble_sort:
push ebp
mov ebp, esp
sub esp, 32
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
mov dword [ebp-8], eax
push dword 0
pop eax
mov dword [ebp-12], eax
push dword 0
pop eax
mov dword [ebp-16], eax
push dword 0
pop eax
mov dword [ebp-20], eax
push dword 0
pop eax
mov dword [ebp-24], eax
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
push dword [ebp+8]
push dword 1
pop ecx
pop eax
sub eax, ecx
push eax
pop ecx
pop eax
cmp eax, ecx
setl al
movzx eax, al
push eax
pop eax
cmp eax, 0
je .for_loop_end2
push dword 0
pop eax
mov dword [ebp-8], eax
jmp .for_loop5
.entry_point_7:
push dword [ebp-8]
push dword 1
pop ecx
pop eax
add eax, ecx
push eax
pop eax
mov dword [ebp-8], eax
.for_loop5:
push dword [ebp-8]
push dword [ebp+8]
push dword 1
pop ecx
pop eax
sub eax, ecx
push eax
push dword [ebp-4]
pop ecx
pop eax
sub eax, ecx
push eax
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
push dword 1
pop ecx
pop eax
add eax, ecx
push eax
pop eax
mov dword [ebp-12], eax
lea ebx, [esi+$values wrt ..gotoff]
push ebx
pop ebx
push dword [ebp-8]
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
pop eax
mov dword [ebp-20], eax
lea ebx, [esi+$values wrt ..gotoff]
push ebx
pop ebx
push dword [ebp-12]
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
pop eax
mov dword [ebp-24], eax
push dword [ebp-20]
push dword [ebp-24]
pop ecx
pop eax
cmp eax, ecx
setg al
movzx eax, al
push eax
pop eax
cmp eax, 0
je .if_10
lea ebx, [esi+$values wrt ..gotoff]
push ebx
pop ebx
push dword [ebp-8]
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
pop eax
mov dword [ebp-16], eax
lea ebx, [esi+$values wrt ..gotoff]
push ebx
pop ebx
push dword [ebp-12]
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
lea ebx, [esi+$values wrt ..gotoff]
push ebx
pop ebx
push dword [ebp-8]
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
push dword [ebp-16]
lea ebx, [esi+$values wrt ..gotoff]
push ebx
pop ebx
push dword [ebp-12]
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
jmp .if_end_9
.if_10:
.if_end_9:
push dword [ebp-8]
push dword 1
pop ecx
pop eax
add eax, ecx
push eax
pop eax
mov dword [ebp-8], eax
jmp .for_loop5
.for_loop_end6:
.exit_point_8:
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
mov ebx, [ebp-36]
mov esi, [ebp-40]
mov edi, [ebp-44]
mov esp, ebp
pop ebp
ret
pop edi
pop esi
pop ebx
add esp, 32
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
push dword 5
lea ebx, [esi+$values wrt ..gotoff]
push ebx
pop ebx
add ebx, 0
push ebx
pop edx
pop eax
mov dword [edx], eax
push dword 2
lea ebx, [esi+$values wrt ..gotoff]
push ebx
pop ebx
add ebx, 4
push ebx
pop edx
pop eax
mov dword [edx], eax
push dword 8
lea ebx, [esi+$values wrt ..gotoff]
push ebx
pop ebx
add ebx, 8
push ebx
pop edx
pop eax
mov dword [edx], eax
push dword 1
lea ebx, [esi+$values wrt ..gotoff]
push ebx
pop ebx
add ebx, 12
push ebx
pop edx
pop eax
mov dword [edx], eax
push dword 7
lea ebx, [esi+$values wrt ..gotoff]
push ebx
pop ebx
add ebx, 16
push ebx
pop edx
pop eax
mov dword [edx], eax
push dword 3
lea ebx, [esi+$values wrt ..gotoff]
push ebx
pop ebx
add ebx, 20
push ebx
pop edx
pop eax
mov dword [edx], eax
lea ebx, [esi+$bubble_sort wrt ..gotoff]
push ebx
pop ebx
mov dword [esi+function_call_11 wrt ..gotoff], ebx
sub esp, 8
push dword 6
call [esi+function_call_11 wrt ..gotoff]
add esp, 4
add esp, 8
push eax
pop eax
push eax
add esp, 4
lea ebx, [esi+$values wrt ..gotoff]
push ebx
pop ebx
add ebx, 0
push ebx
pop eax
mov eax, [eax]
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
function_call_11: dd 0
section .rodata
section .note.GNU-stack noalloc noexec nowrite progbits
