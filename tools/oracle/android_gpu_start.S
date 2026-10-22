.syntax unified
.arm
.section .text.start, "ax", %progbits
.global _start
.type _start, %function
_start:
    mov r0, sp
    bl theme_gpu_main
    mov r0, #1
    mov r7, #1
    svc #0
.size _start, . - _start
.section .note.GNU-stack, "", %progbits
