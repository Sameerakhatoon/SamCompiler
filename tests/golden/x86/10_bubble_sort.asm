section .data
; cell values
values: times 24 db  0
section .text
global bubble_sort
; bubble_sort function
bubble_sort:
push ebp
mov ebp, esp
sub esp, 32
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
push dword [ebp-4]
pop ecx
pop eax
sub eax, ecx
push eax
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
lea ebx, [values]
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
lea ebx, [values]
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
lea ebx, [values]
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
lea ebx, [values]
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
lea ebx, [values]
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
lea ebx, [values]
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
add esp, 32
pop ebp
ret
add esp, 32
pop ebp
ret
global main
; main function
main:
push ebp
mov ebp, esp
push dword 5
lea ebx, [values]
push ebx
pop ebx
add ebx, 0
push ebx
pop edx
pop eax
mov dword [edx], eax
push dword 2
lea ebx, [values]
push ebx
pop ebx
add ebx, 4
push ebx
pop edx
pop eax
mov dword [edx], eax
push dword 8
lea ebx, [values]
push ebx
pop ebx
add ebx, 8
push ebx
pop edx
pop eax
mov dword [edx], eax
push dword 1
lea ebx, [values]
push ebx
pop ebx
add ebx, 12
push ebx
pop edx
pop eax
mov dword [edx], eax
push dword 7
lea ebx, [values]
push ebx
pop ebx
add ebx, 16
push ebx
pop edx
pop eax
mov dword [edx], eax
push dword 3
lea ebx, [values]
push ebx
pop ebx
add ebx, 20
push ebx
pop edx
pop eax
mov dword [edx], eax
lea ebx, [bubble_sort]
push ebx
pop ebx
mov dword [function_call_11], ebx
push dword 6
call [function_call_11]
add esp, 4
push eax
pop eax
push eax
add esp, 4
lea ebx, [values]
push ebx
pop ebx
add ebx, 0
push ebx
pop eax
mov eax, [eax]
push eax
pop eax
pop ebp
ret
pop ebp
ret
section .data
function_call_11: dd 0
section .rodata
