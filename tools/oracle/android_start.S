.syntax unified
.arm
.section .text.start, "ax", %progbits
.global _start
.type _start, %function
#ifndef AB_ANDROID_ENTRY
#define AB_ANDROID_ENTRY android_math_main
#endif
_start:
    bl AB_ANDROID_ENTRY
    mov r0, #1
    mov r7, #1
    svc #0
.size _start, . - _start
.section .note.GNU-stack, "", %progbits
