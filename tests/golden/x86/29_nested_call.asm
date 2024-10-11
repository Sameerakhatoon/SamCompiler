section .data
section .text
global $add
; add function
$add:
push ebp
mov ebp, esp
push ebx
push dword [ebp+8]
push dword [ebp+12]
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
global $twice
; twice function
$twice:
push ebp
mov ebp, esp
push ebx
push dword [ebp+8]
push dword 2
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
lea ebx, [$add]
push ebx
pop ebx
mov dword [function_call_1], ebx
sub esp, 12
lea ebx, [$add]
push ebx
pop ebx
mov dword [function_call_2], ebx
push dword 4
push dword 3
call [function_call_2]
add esp, 8
push eax
pop eax
push eax
lea ebx, [$twice]
push ebx
pop ebx
mov dword [function_call_3], ebx
push dword 5
call [function_call_3]
add esp, 4
push eax
pop eax
push eax
call [function_call_1]
add esp, 8
add esp, 12
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
function_call_1: dd 0
function_call_2: dd 0
function_call_3: dd 0
section .rodata
section .note.GNU-stack noalloc noexec nowrite progbits
