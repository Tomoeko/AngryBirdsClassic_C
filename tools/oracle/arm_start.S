.syntax unified
.arm
.section .text.start, "ax"
.global _start
.type _start, %function
_start:
    ldr sp, =0x47fff000
    ldr r0, =oracle_vectors
    mcr p15, 0, r0, c12, c0, 0
    isb
    mrc p15, 0, r0, c1, c0, 2
    orr r0, r0, #0x00f00000
    mcr p15, 0, r0, c1, c0, 2
    isb
    mov r0, #0x40000000
    vmsr fpexc, r0
    mov r0, #0
    vmsr fpscr, r0
    bl oracle_main
1:
    b 1b
.size _start, . - _start

.section .text.vectors, "ax"
.balign 32
oracle_vectors:
    b _start
    b oracle_exception
    b oracle_exception
    b oracle_exception
    b oracle_exception
    b oracle_exception
    b oracle_exception
    b oracle_exception
oracle_exception:
    ldr sp, =0x47ffe000
    b oracle_abort
