section .data
section .text
extern printf
global main
; main function
main:
push ebp
mov ebp, esp
lea ebx, [printf]
push ebx
pop ebx
mov dword [function_call_1], ebx
mov eax, str_2
push eax
push dword 42
mov eax, str_3
push eax
call [function_call_1]
add esp, 12
push eax
pop eax
push eax
add esp, 4
push dword 0
pop eax
pop ebp
ret
pop ebp
ret
section .data
function_call_1: dd 0
section .rodata
str_2: db 'h', 'i', 0
str_3: db 'v', '=', '%', 'd', ' ', 's', '=', '%', 's', 10, 0
