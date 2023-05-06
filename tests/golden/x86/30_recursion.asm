section .data
section .text
global fib
; fib function
fib:
push ebp
mov ebp, esp
push dword [ebp+8]
push dword 2
pop ecx
pop eax
cmp eax, ecx
setl al
movzx eax, al
push eax
pop eax
cmp eax, 0
je .if_2
push dword [ebp+8]
pop eax
pop ebp
ret
jmp .if_end_1
.if_2:
.if_end_1:
lea ebx, [fib]
push ebx
pop ebx
mov dword [function_call_3], ebx
push dword [ebp+8]
push dword 1
pop ecx
pop eax
sub eax, ecx
push eax
call [function_call_3]
add esp, 4
push eax
pop eax
push eax
lea ebx, [fib]
push ebx
pop ebx
mov dword [function_call_4], ebx
push dword [ebp+8]
push dword 2
pop ecx
pop eax
sub eax, ecx
push eax
call [function_call_4]
add esp, 4
push eax
pop eax
push eax
pop ecx
pop eax
add eax, ecx
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
lea ebx, [fib]
push ebx
pop ebx
mov dword [function_call_5], ebx
push dword 10
call [function_call_5]
add esp, 4
push eax
pop eax
push eax
pop eax
pop ebp
ret
pop ebp
ret
section .data
function_call_3: dd 0
function_call_4: dd 0
function_call_5: dd 0
section .rodata
