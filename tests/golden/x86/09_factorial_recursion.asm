section .data
section .text
global $factorial
; factorial function
$factorial:
push ebp
mov ebp, esp
push ebx
push dword [ebp+8]
push dword 1
pop ecx
pop eax
cmp eax, ecx
setle al
movzx eax, al
push eax
pop eax
cmp eax, 0
je .if_2
push dword 1
pop eax
mov ebx, [ebp-4]
mov esp, ebp
pop ebp
ret
jmp .if_end_1
.if_2:
.if_end_1:
push dword [ebp+8]
lea ebx, [$factorial]
push ebx
pop ebx
mov dword [function_call_3], ebx
sub esp, 12
push dword [ebp+8]
push dword 1
pop ecx
pop eax
sub eax, ecx
push eax
call [function_call_3]
add esp, 4
add esp, 12
push eax
pop eax
push eax
pop ecx
pop eax
mov ecx, ecx
imul ecx
push eax
pop eax
mov ebx, [ebp-4]
mov esp, ebp
pop ebp
ret
pop ebx
pop ebp
ret
global $main
; main function
$main:
push ebp
mov ebp, esp
push ebx
lea ebx, [$factorial]
push ebx
pop ebx
mov dword [function_call_4], ebx
push dword 5
call [function_call_4]
add esp, 4
push eax
pop eax
push eax
pop eax
mov ebx, [ebp-4]
mov esp, ebp
pop ebp
ret
pop ebx
pop ebp
ret
section .data
function_call_3: dd 0
function_call_4: dd 0
section .rodata
section .note.GNU-stack noalloc noexec nowrite progbits
