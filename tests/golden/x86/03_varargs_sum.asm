extern _GLOBAL_OFFSET_TABLE_
section .data
section .text
global $sum
; sum function
$sum:
push ebp
mov ebp, esp
sub esp, 16
push ebx
push esi
push edi
call .pic_base
.pic_base:
pop esi
add esi, _GLOBAL_OFFSET_TABLE_+$$-.pic_base wrt ..gotpc
; NATIVE FUNCTION va_start
; va_start on variable num
lea ebx, [ebp+8]
push ebx
add ebx, 4
mov dword [ebp-4], ebx
; va_start end for variable num
push 0
add esp, 8
; NATIVE FUNCTION __builtin_va_arg
; native__builtin_va_arg start
lea ebx, [ebp-4]
push ebx
pop ebx
mov dword eax, [ebx]
add dword [ebx], 4
push eax
; native__builtin_va_arg end
; CAST
; INDIRECTION
pop ebx
mov ebx, [ebx]
push ebx
pop eax
push eax
pop eax
mov dword [ebp-8], eax
; NATIVE FUNCTION __builtin_va_arg
; native__builtin_va_arg start
lea ebx, [ebp-4]
push ebx
pop ebx
mov dword eax, [ebx]
add dword [ebx], 4
push eax
; native__builtin_va_arg end
; CAST
; INDIRECTION
pop ebx
mov ebx, [ebx]
push ebx
pop eax
push eax
pop eax
mov dword [ebp-12], eax
push dword 0
pop eax
mov ebx, [ebp-20]
mov esi, [ebp-24]
mov edi, [ebp-28]
mov esp, ebp
pop ebp
ret
pop edi
pop esi
pop ebx
add esp, 16
pop ebp
ret
global $main
; main function
$main:
push ebp
mov ebp, esp
push ebx
push esi
push edi
call .pic_base
.pic_base:
pop esi
add esi, _GLOBAL_OFFSET_TABLE_+$$-.pic_base wrt ..gotpc
lea ebx, [esi+$sum wrt ..gotoff]
push ebx
pop ebx
mov dword [esi+function_call_1 wrt ..gotoff], ebx
push dword 20
push dword 10
push dword 2
call [esi+function_call_1 wrt ..gotoff]
add esp, 12
push eax
pop eax
push eax
pop eax
mov ebx, [ebp-4]
mov esi, [ebp-8]
mov edi, [ebp-12]
mov esp, ebp
pop ebp
ret
pop edi
pop esi
pop ebx
pop ebp
ret
section .data
function_call_1: dd 0
section .rodata
section .note.GNU-stack noalloc noexec nowrite progbits
