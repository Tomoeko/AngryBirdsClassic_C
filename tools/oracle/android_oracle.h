#ifndef AB_ORACLE_ANDROID_PLATFORM_H
#define AB_ORACLE_ANDROID_PLATFORM_H

#include "android_io.h"
#include "reference_angles.h"

extern void *dlopen(const char *path, int mode);
extern void *dlsym(void *library, const char *symbol);
extern const char *dlerror(void);
extern float sinf(float angle);
extern float cosf(float angle);

static void *target_library;
static uint32_t math_values[4096][3];
static char text_buffer[32768];
static size_t text_size;

static void flush_text(void) {
    write_bytes(text_buffer, text_size);
    text_size = 0;
}

static uint32_t semihost(uint32_t operation, const void *parameters) {
    if (operation != 4 || !parameters) {
        system_exit(2);
    }
    const char *text = parameters;
    while (*text) {
        if (text_size == sizeof(text_buffer)) {
            flush_text();
        }
        text_buffer[text_size++] = *text++;
    }
    return 0;
}

static _Noreturn void finish(uint32_t status) {
    flush_text();
    system_exit((int)status);
}

static _Noreturn void fail(const char *message) {
    semihost(4, message);
    finish(1);
}

static _Noreturn void unresolved_import(void) {
    fail("Controlled target data reached an unaudited virtual callback\n");
}

static void initialize_target(void) {
    target_library = dlopen("/data/local/tmp/ab520/libAngryBirdsClassic.so", 2);
    if (!target_library) {
        const char *error = dlerror();
        fail(error ? error : "Cannot open the pinned Android reference library\n");
    }
    uint32_t controls = 0;
    __asm__ volatile("vmsr fpscr, %0" : : "r"(controls) : "memory");
    uint32_t state = UINT32_C(0x520a6a7e);
    for (size_t index = 0; index < 4096; ++index) {
        union {
            uint32_t bits;
            float number;
        } angle = {.bits = next_reference_angle(&state, index)},
          sine = {.number = sinf(angle.number)},
          cosine = {.number = cosf(angle.number)};
        math_values[index][0] = angle.bits;
        math_values[index][1] = sine.bits;
        math_values[index][2] = cosine.bits;
    }
}

static uintptr_t find_symbol(const char *name, unsigned kind) {
    if (kind != 1 && kind != 2) {
        fail("Unsupported native reference symbol kind\n");
    }
    void *symbol = dlsym(target_library, name);
    if (!symbol) {
        fail("Required target symbol is missing from the loaded Android library\n");
    }
    return (uintptr_t)symbol;
}

static uintptr_t find_function(const char *name) {
    return find_symbol(name, 2);
}

static void oracle_reference_rotation(size_t index, unsigned negative_zero,
                                      uint32_t *angle, uint32_t *sine,
                                      uint32_t *cosine) {
    (void)negative_zero;
    *angle = math_values[index][0];
    *sine = math_values[index][1];
    *cosine = math_values[index][2];
}

static void oracle_math_enable(unsigned enabled) {
    (void)enabled;
}

#endif
