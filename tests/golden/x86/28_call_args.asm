section .data
section .text
global f
; f function
f:
push ebp
mov ebp, esp
push dword [ebp+8]
push dword 100
pop ecx
pop eax
mov ecx, ecx
mul ecx
push eax
push dword [ebp+12]
push dword 10
pop ecx
pop eax
mov ecx, ecx
mul ecx
push eax
push dword [ebp+16]
pop ecx
pop eax
add eax, ecx
push eax
pop ecx
pop eax
add eax, ecx
push eax
push dword 300
pop ecx
pop eax
sub eax, ecx
push eax
pop eax
pop ebp
ret
pop ebp
ret
global main
; main function
main:
push ebp
mov ebp, esp
lea ebx, [f]
push ebx
pop ebx
mov dword [function_call_1], ebx
push dword 6
push dword 2
push dword 3
call [function_call_1]
add esp, 12
push eax
pop eax
push eax
pop eax
pop ebp
ret
pop ebp
ret
section .data
function_call_1: dd 0
section .rodata
