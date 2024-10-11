section .data
section .text
global $set
; set function
$set:
push ebp
mov ebp, esp
push ebx
push dword [ebp+12]
push dword [ebp+8]
; INDIRECTION
pop ebx
push ebx
pop edx
pop eax
mov dword [edx], eax
push dword 0
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
push dword 0
pop eax
mov dword [ebp-4], eax
lea ebx, [$set]
push ebx
pop ebx
mov dword [function_call_1], ebx
sub esp, 12
push dword 12
lea ebx, [ebp-4]
push ebx
pop ebx
; PUSH ADDRESS &
push ebx
call [function_call_1]
add esp, 8
add esp, 12
push eax
pop eax
push eax
add esp, 4
push dword [ebp-4]
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
