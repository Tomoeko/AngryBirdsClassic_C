#ifndef AB_ORACLE_UTF8_H
#define AB_ORACLE_UTF8_H

#if defined(AB_ORACLE_UTF8_ENCODE_ONLY)
#include "utf8_encode_probe.h"
#endif

static void probe_utf8(void) {
#if defined(AB_ORACLE_UTF8_ENCODE_ONLY)
    probe_utf8_encode();
    return;
#endif
    typedef uint32_t (*Decode)(const void *, const void *, const void *, uint32_t *,
                               uint32_t *);
    typedef void (*Convert)(uint32_t **, const void *);
    typedef void (*Delete)(void *);
    Decode decode = (Decode)find_function("_ZNK4lang12UTFConverter6decodeEPKvS2_PjPi");
    Convert convert =
        (Convert)find_function("_ZN4lang6string13toUTF32stringERKNS_17basic_string_"
                               "viewIcSt11char_traitsIcEEE");
    Delete release = (Delete)find_function("_ZdlPv");
    uint32_t *empty =
        (uint32_t *)find_symbol(
            "_ZNSbIDiSt11char_traitsIDiESaIDiEE4_Rep20_S_empty_rep_storageE", 1) +
        3;
    uint32_t converter[] = {0, 0, 2};
    uint32_t random = UINT32_C(0x5208f032);
    semihost(4, "AB_ANDROID_UTF8_V1 65536\n");
    for (uint32_t item = 0; item < 65536; ++item) {
        uint32_t input[] = {item % 9, random_word(&random), random_word(&random)};
        if (item < 256) {
            input[1] = (input[1] & UINT32_C(0xffffff00)) | item;
        }
        const uint8_t *begin = (const uint8_t *)(input + 1);
        const uint8_t *end = begin + input[0];
        uint32_t output[] = {0, UINT32_C(0x5a5a5a5a), UINT32_C(0x12345678)};
        output[0] = decode(converter, begin, end, output + 1, output + 2);
        print_words(input, 3);
        print_words(output, 3);
        if (item % 16 == 0) {
            const void *view[] = {begin, end};
            uint32_t *text = NULL;
            convert(&text, view);
            if (!text || text[-3] > 8 || text[-2] > 64 || text[-1] != 0) {
                fail("Native UTF-32 string exceeds bounded output or ownership\n");
            }
            uint32_t record[9];
            record[0] = text[-3];
            for (uint32_t code = 0; code < record[0]; ++code) {
                record[1 + code] = text[code];
            }
            print_words(record, (size_t)record[0] + 1);
            if (text != empty) {
                release(text - 3);
            }
        }
    }
    semihost(4, "AB UTF8 reference complete\n");
}

#endif
