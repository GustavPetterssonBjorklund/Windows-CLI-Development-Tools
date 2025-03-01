; FILE: main.asm
; Author: Gustav Pettersson
; Email: gustav.pettersson2@outlook.com
; DATE: 2025-03-02

BITS 64
default rel
extern GetStdHandle
extern WriteFile
extern ExitProcess
global _start
section .data
msg db "Hello, World!", 0
msg_len equ $-msg
section .bss
StdHandle resq 1
BytesWritten resq 1
section .text
_start:
sub rsp, 40 ; Reserve space for the parameters
; Get standard output handle
mov rcx, -11
call GetStdHandle
mov qword [StdHandle], rax
; Write message to standard output
mov rcx, qword [StdHandle]
lea rdx, [rel msg]
mov r8d, msg_len
lea r9, [rel BytesWritten]
mov qword [rsp+32], 0
call WriteFile
; Exit the process
mov rcx, 0
call ExitProcess
