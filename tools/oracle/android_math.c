/* Android reference tool only. Run against the reference platform's C library;
 * no Android or original-game implementation is linked into the host runtime.
 */
#include <stddef.h>
#include <stdint.h>

#include "android_io.h"
#include "reference_angles.h"
#if defined(AB_MATH_COMPONENTS)
#include "reference_components.h"
#endif

extern float sinf(float angle);
extern float cosf(float angle);
extern float atan2f(float vertical, float horizontal);
extern double atan2(double vertical, double horizontal);

static void print_words(const uint32_t *words, size_t count) {
    static const char digits[] = "0123456789abcdef";
    char line[64];
    if (!count || count > 7) {
        system_exit(2);
    }
    for (size_t word = 0; word < count; ++word) {
        for (unsigned digit = 0; digit < 8; ++digit) {
            line[word * 9 + digit] = digits[(words[word] >> (28 - digit * 4)) & 15];
        }
        line[word * 9 + 8] = word + 1 < count ? ' ' : '\n';
    }
    write_bytes(line, count * 9);
}

void android_math_main(void) {
#if defined(AB_MATH_SCRIPT_ANGLE)
    static const char header[] = "AB_ANDROID_SCRIPT_ANGLE_V1 65536\n";
    const size_t case_count = 65536;
#elif defined(AB_MATH_COMPONENTS)
    static const char header[] = "AB_ANDROID_ANGLE_V1 65536\n";
    const size_t case_count = 65536;
#elif defined(AB_MATH_EXTENDED)
    static const char header[] = "AB_ANDROID_MATH_EXTENDED_V1 65536\n";
    const size_t case_count = 65536;
#else
    static const char header[] = "AB_ANDROID_MATH_V1 4096\n";
    const size_t case_count = 4096;
#endif
    write_bytes(header, sizeof(header) - 1);
    uint32_t controls;
    __asm__ volatile("vmrs %0, fpscr" : "=r"(controls));
    print_words(&controls, 1);
    uint32_t state = UINT32_C(0x520a6a7e);
    for (size_t index = 0; index < case_count; ++index) {
#if defined(AB_MATH_COMPONENTS)
        uint32_t components[2];
        next_reference_components(&state, index, components);
        union {
            uint32_t bits;
            float number;
        } vertical = {.bits = components[0]}, horizontal = {.bits = components[1]};
        if (index >= 176 && index % 4 == 0) {
            union {
                uint32_t bits;
                float number;
            } angle = {.bits = next_extended_angle(&state, index)};
            vertical.number = sinf(angle.number);
            horizontal.number = cosf(angle.number);
        }
        union {
            uint32_t bits;
            float number;
        } result = {
#if defined(AB_MATH_SCRIPT_ANGLE)
            .number = (float)atan2((double)vertical.number, (double)horizontal.number)
#else
            .number = atan2f(vertical.number, horizontal.number)
#endif
        };
        const uint32_t words[] = {vertical.bits, horizontal.bits, result.bits};
#else
        union {
            uint32_t bits;
            float number;
        } angle;
#if defined(AB_MATH_EXTENDED)
        angle.bits = next_extended_angle(&state, index);
#else
        angle.bits = next_reference_angle(&state, index);
#endif
        union {
            uint32_t bits;
            float number;
        } sine = {.number = sinf(angle.number)},
          cosine = {.number = cosf(angle.number)};
        const uint32_t words[] = {angle.bits, sine.bits, cosine.bits};
#endif
        print_words(words, 3);
    }
#if defined(AB_MATH_COMPONENTS)
    static const char complete[] = "AB angle reference complete\n";
    write_bytes(complete, sizeof(complete) - 1);
#endif
    system_exit(0);
}
