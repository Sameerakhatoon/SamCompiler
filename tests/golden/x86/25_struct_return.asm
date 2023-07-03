section .data
section .text
global make
; make function
make:
push ebp
mov ebp, esp
sub esp, 16
push dword [ebp+12]
pop eax
mov dword [ebp-8], eax
push dword [ebp+16]
pop eax
mov dword [ebp-4], eax
lea ebx, [ebp-8]
push ebx
pop ebx
; STRUCTURE PUSH
push dword [ebx+4]
push dword [ebx+0]
; END STRUCTURE PUSH
mov edx, [ebp+8]
pop eax
mov [edx+0], eax
pop eax
mov [edx+4], eax
mov eax, [ebp+8]
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
sub esp, 16
lea ebx, [make]
push ebx
pop ebx
mov dword [function_call_1], ebx
; SUBTRACT ROOM FOR RETURNED STRUCTURE/UNION DATATYPE
sub esp, 8
push dword 7
push dword 2
lea ebx, [esp+8]
push ebx
call [function_call_1]
add esp, 12
mov ebx, eax
; STRUCTURE PUSH
push dword [ebx+4]
push dword [ebx+0]
; END STRUCTURE PUSH
; STRUCTURE PUSH
push dword [ebx+4]
push dword [ebx+0]
; END STRUCTURE PUSH
pop eax
mov [ebp-8+0], eax
pop eax
mov [ebp-8+4], eax
add esp, 16
push dword [ebp-8]
pop eax
push eax
push dword [ebp-4]
pop eax
push eax
pop ecx
pop eax
add eax, ecx
push eax
pop eax
add esp, 16
pop ebp
ret
add esp, 16
pop ebp
ret
section .data
function_call_1: dd 0
section .rodata
