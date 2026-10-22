#ifndef AB_ORACLE_ANDROID_IO_H
#define AB_ORACLE_ANDROID_IO_H

#include <stddef.h>
#include <stdint.h>

static int system_call(int number, int first, const void *second, size_t third) {
    register int result __asm__("r0") = first;
    register const void *argument __asm__("r1") = second;
    register size_t length __asm__("r2") = third;
    register int operation __asm__("r7") = number;
    __asm__ volatile("svc #0"
                     : "+r"(result)
                     : "r"(argument), "r"(length), "r"(operation)
                     : "memory", "cc");
    return result;
}

static _Noreturn void system_exit(int status) {
    (void)system_call(1, status, NULL, 0);
    for (;;) {
    }
}

static void write_bytes(const char *bytes, size_t size) {
    while (size) {
        int written = system_call(4, 1, bytes, size);
        if (written <= 0 || (size_t)written > size) {
            system_exit(1);
        }
        bytes += written;
        size -= (size_t)written;
    }
}

#endif
