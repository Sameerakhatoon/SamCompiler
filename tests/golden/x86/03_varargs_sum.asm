section .data
section .text
global sum
; sum function
sum:
push ebp
mov ebp, esp
sub esp, 16
; NATIVE FUNCTION va_start
; va_start on variable num
lea ebx, [ebp+8]
push ebx
mov dword [ebp-4], ebx
; va_start end for variable num
push 0
add esp, 8
; NATIVE FUNCTION __builtin_va_arg
; native__builtin_va_arg start
lea ebx, [ebp-4]
push ebx
add dword [ebx], 4
mov dword eax, [ebx]
push dword [eax]
; native__builtin_va_arg end
pop eax
mov dword [ebp-8], eax
add esp, 4
; NATIVE FUNCTION __builtin_va_arg
; native__builtin_va_arg start
lea ebx, [ebp-4]
push ebx
add dword [ebx], 4
mov dword eax, [ebx]
push dword [eax]
; native__builtin_va_arg end
pop eax
mov dword [ebp-12], eax
add esp, 4
push dword 0
pop eax
add esp, 16
pop ebp
ret
add esp, 16
pop ebp
ret
global main
; main function
main:
push ebp
mov ebp, esp
lea ebx, [sum]
push ebx
pop ebx
mov dword [function_call_1], ebx
push dword 20
push dword 10
push dword 2
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
