#ifndef AB_ORACLE_UTF8_ENCODE_H
#define AB_ORACLE_UTF8_ENCODE_H

static void probe_utf8_encode(void) {
    typedef uint32_t (*Encode)(const void *, void *, const void *, uint32_t *,
                               uint32_t);
    Encode encode = (Encode)find_function("_ZNK4lang12UTFConverter6encodeEPvS1_Pji");
    void (*convert)(char **, const void *) =
        (void (*)(char **, const void *))find_function(
            "_ZN4lang6string12toUTF8stringERKNS_17basic_string_viewIDiSt11char_"
            "traitsIDiEEE");
    void (*release)(void *) = (void (*)(void *))find_function("_ZdlPv");
    uint32_t empty_code = 0;
    const uint32_t *empty_view[] = {&empty_code, &empty_code};
    char *empty = NULL;
    convert(&empty, empty_view);
    if (!empty) {
        fail("Native UTF8 empty-string initialization failed\n");
    }
    uint32_t converter[] = {0, 0, 2};
    static const uint32_t edges[] = {
        0,        1,        0x7f,       0x80,       0x7ff,      0x800,      0xd7ff,
        0xd800,   0xdfff,   0xe000,     0xffff,     0x10000,    0x10ffff,   0x110000,
        0x1fffff, 0x200000, 0x7fffffff, 0x80000000, 0x8000007f, 0x80000080, 0xffffffff};
    uint32_t random = 0x520e8c01;
    semihost(4, "AB_ANDROID_UTF8_ENCODE_V1 65536\n");
    for (uint32_t test = 0; test < 65536; ++test) {
        uint32_t input[] = {test % 6, random_word(&random)};
        if (test < sizeof(edges) / sizeof(*edges) * 6) {
            input[1] = edges[test / 6];
        } else if (test % 3 == 0) {
            input[1] = edges[random_word(&random) % (sizeof(edges) / sizeof(*edges))];
        }
        uint32_t bytes[] = {0xa5a5a5a5, 0x5a5a5a5a};
        uint32_t written = 0x12345678;
        uint32_t okay =
            encode(converter, bytes, (uint8_t *)bytes + input[0], &written, input[1]);
        uint32_t output[] = {okay, written, bytes[0], bytes[1]};
        print_words(input, 2);
        print_words(output, 4);
        if (test % 16 == 0) {
            uint32_t codes[8], record[9];
            uint32_t count = (test / 16) % 9;
            record[0] = count;
            for (uint32_t index = 0; index < count; ++index) {
                codes[index] =
                    index % 3
                        ? edges[random_word(&random) % (sizeof(edges) / sizeof(*edges))]
                        : random_word(&random);
                record[index + 1] = codes[index];
            }
            print_words(record, (size_t)count + 1);
            const uint32_t *view[] = {codes, codes + count};
            char *encoded = NULL;
            convert(&encoded, view);
            if (!encoded) {
                fail("Native UTF8 conversion returned no string\n");
            }
            uint32_t length = ((const uint32_t *)encoded)[-3];
            if (length > 32 || ((const uint32_t *)encoded)[-2] > 128) {
                fail("Native UTF8 conversion exceeds bounded output\n");
            }
            uint32_t packed[9] = {length};
            for (size_t index = 0; index < length; ++index) {
                packed[1 + index / 4] |= (uint32_t)(uint8_t)encoded[index]
                                         << ((index % 4) * 8);
            }
            print_words(packed, 1 + (length + 3) / 4);
            if (encoded != empty) {
                release(encoded - 12);
            }
        }
    }
    semihost(4, "AB UTF8 encode reference complete\n");
}

#endif
