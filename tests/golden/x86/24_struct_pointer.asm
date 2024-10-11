section .data
section .text
global $sum
; sum function
$sum:
push ebp
mov ebp, esp
push ebx
lea ebx, [ebp+8]
push ebx
pop ebx
mov ebx, [ebx]
add ebx, 0
push ebx
pop eax
mov eax, [eax]
push eax
lea ebx, [ebp+8]
push ebx
pop ebx
mov ebx, [ebx]
add ebx, 4
push ebx
pop eax
mov eax, [eax]
push eax
pop ecx
pop eax
add eax, ecx
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
sub esp, 16
push ebx
push dword 5
pop eax
mov dword [ebp-8], eax
push dword 10
pop eax
mov dword [ebp-4], eax
lea ebx, [$sum]
push ebx
pop ebx
mov dword [function_call_1], ebx
lea ebx, [ebp-8]
push ebx
pop ebx
; PUSH ADDRESS &
push ebx
call [function_call_1]
add esp, 4
push eax
pop eax
push eax
pop eax
mov ebx, [ebp-20]
mov esp, ebp
pop ebp
ret
pop ebx
add esp, 16
pop ebp
ret
section .data
function_call_1: dd 0
section .rodata
section .note.GNU-stack noalloc noexec nowrite progbits
