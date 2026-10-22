#include "ab/ui_value_text.h"

#include "../tools/oracle/ui_value_text_cases.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "UI value text failed at %d: %s\n", __LINE__, #condition); \
            return false;                                                              \
        }                                                                              \
    } while (0)

static bool edges(void) {
    uint8_t storage[32];
    memset(storage, 0xa5, sizeof(storage));
    AbSpan output = {(const uint8_t *)"keep", 4};
    AbUiPointerValue value = {.kind = AB_UI_POINTER_NUMBER, .number_bits = 0x80000000};
    CHECK(ab_ui_value_text(value, storage, 2, &output) && output.size == 2 &&
          memcmp(output.data, "-0", 2) == 0 && storage[2] == 0xa5);
    output = (AbSpan){(const uint8_t *)"keep", 4};
    memset(storage, 0xa5, sizeof(storage));
    CHECK(!ab_ui_value_text(value, storage, 1, &output) && output.size == 4 &&
          storage[0] == 0xa5 && !ab_ui_value_text(value, NULL, 32, &output));
    value = (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN};
    CHECK(ab_ui_value_text(value, NULL, 0, &output) && output.size == 5 &&
          memcmp(output.data, "false", 5) == 0);
    value = (AbUiPointerValue){.kind = AB_UI_POINTER_TEXT,
                               .text = {(const uint8_t *)"A\0B", 3}};
    CHECK(ab_ui_value_text(value, NULL, 0, &output) && output.data == value.text.data &&
          output.size == 3);
    CHECK(ab_ui_native_value_text(value, NULL, 0, &output) &&
          output.data == value.text.data && output.size == 1);
    CHECK(ab_ui_native_value_text(
              (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN, .boolean = true}, NULL,
              0, &output) &&
          !output.size);
    value = (AbUiPointerValue){.kind = AB_UI_POINTER_OBJECT, .object = 1};
    output = (AbSpan){(const uint8_t *)"keep", 4};
    CHECK(!ab_ui_value_text(value, storage, sizeof(storage), &output) &&
          output.size == 4 && !memcmp(output.data, "keep", 4));
    value.kind = AB_UI_POINTER_FUNCTION;
    CHECK(!ab_ui_value_text(value, storage, sizeof(storage), &output));
    CHECK(ab_ui_native_value_text(value, NULL, 0, &output) && !output.size);
    value = (AbUiPointerValue){.kind = AB_UI_POINTER_TEXT, .text.size = 1};
    CHECK(!ab_ui_value_text(value, storage, sizeof(storage), &output) &&
          !ab_ui_value_text((AbUiPointerValue){0}, storage, sizeof(storage), NULL));
    const struct {
        uint32_t bits;
        const char *text;
    } numeric[] = {
        {0, "0"},
        {0x80000000, "-0"},
        {1, "1.40129846e-45"},
        {0x80000001, "-1.40129846e-45"},
        {0x007fffff, "1.17549421e-38"},
        {0x00800000, "1.17549435e-38"},
        {0x7f7fffff, "3.40282347e+38"},
        {0xff7fffff, "-3.40282347e+38"},
        {0x7f800000, "inf"},
        {0xff800000, "-inf"},
        {0x7fc12345, "nan"},
        {0xffc12345, "nan"},
        {0x7f812345, "nan"},
        {0xff812345, "nan"},
        {0x3f800000, "1"},
        {0xbf800000, "-1"},
        {0x3dcccccd, "0.100000001"},
        {0x4b800000, "16777216"},
    };
    for (size_t index = 0; index < sizeof(numeric) / sizeof(*numeric); ++index) {
        value = (AbUiPointerValue){.kind = AB_UI_POINTER_NUMBER,
                                   .number_bits = numeric[index].bits};
        size_t length = strlen(numeric[index].text);
        CHECK(ab_ui_value_text(value, storage, sizeof(storage), &output) &&
              output.size == length &&
              memcmp(output.data, numeric[index].text, length) == 0);
    }
    puts("UI plain value text, signed zero, borrowed embedded NUL, exact capacity, "
         "opaque rejection and preserved failure output checks pass.");
    return true;
}

static bool words(FILE *file, uint32_t *output, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        CHECK(fscanf(file, "%" SCNx32, output + index) == 1 &&
              fgetc(file) == (index + 1 == count ? '\n' : ' '));
    }
    return true;
}

static bool compare_case(FILE *file, bool native, size_t *numbers, size_t *bytes) {
    uint32_t input[11];
    CHECK(words(file, input, 11) && input[0] < (native ? 6u : 4u) && input[2] <= 32);
    AbUiPointerValue value = {.kind = (AbUiPointerKind)input[0]};
    if (value.kind == AB_UI_POINTER_BOOLEAN) {
        CHECK(input[1] < 2);
        value.boolean = input[1] != 0;
    } else if (value.kind == AB_UI_POINTER_NUMBER) {
        value.number_bits = input[1];
    } else if (value.kind == AB_UI_POINTER_TEXT) {
        CHECK(input[1] < 8);
        value.text = (AbSpan){(const uint8_t *)ui_value_text_cases[input[1]],
                              ui_value_text_lengths[input[1]]};
    } else if (value.kind == AB_UI_POINTER_OBJECT) {
        value.object = 1;
    } else if (value.kind == AB_UI_POINTER_FUNCTION) {
        value.function = 1;
    }
    uint8_t storage[32];
    memset(storage, 0xa5, sizeof(storage));
    AbSpan output;
    bool (*convert)(AbUiPointerValue, uint8_t *, size_t, AbSpan *) =
        native ? ab_ui_native_value_text : ab_ui_value_text;
    CHECK(convert(value, storage, sizeof(storage), &output) && output.size == input[2]);
    for (size_t index = 0; index < 32; ++index) {
        uint32_t byte = (input[3 + index / 4] >> ((index % 4) * 8)) & 255;
        if (index < output.size) {
            CHECK(output.data[index] == byte);
        } else {
            CHECK(byte == 0);
        }
        if (value.kind != AB_UI_POINTER_NUMBER || index >= output.size) {
            CHECK(storage[index] == 0xa5);
        }
    }
    if (value.kind == AB_UI_POINTER_NUMBER) {
        CHECK(output.size > 0 && output.data == storage);
        memset(storage, 0xa5, sizeof(storage));
        AbSpan preserved = {(const uint8_t *)"keep", 4};
        CHECK(!convert(value, storage, output.size - 1, &preserved) &&
              preserved.size == 4 && memcmp(preserved.data, "keep", 4) == 0);
        for (size_t index = 0; index < sizeof(storage); ++index) {
            CHECK(storage[index] == 0xa5);
        }
        ++*numbers;
    }
    *bytes += output.size;
    return true;
}

static bool compare_native(FILE *file, bool native) {
    char header[80];
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, native ? "AB_ANDROID_UI_NATIVE_VALUE_TEXT_V1 65536\n"
                                : "AB_ANDROID_UI_VALUE_TEXT_V1 65536\n") == 0);
    size_t numbers = 0, bytes = 0;
    for (size_t test = 0; test < 65536; ++test) {
        if (!compare_case(file, native, &numbers, &bytes)) {
            fprintf(stderr, "Original value text conversion case %zu differs\n", test);
            return false;
        }
    }
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, native ? "AB UI native value text reference complete\n"
                                : "AB UI value text reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    printf("65536 original %s conversions (%zu numbers), all %zu bytes "
           "and numeric capacity/preserved-output checks match.\n",
           native ? "native string" : "plain tostring", numbers, bytes);
    return true;
}

int main(int argc, char **argv) {
    if (!edges()) {
        return 1;
    }
    if (argc == 1) {
        return 0;
    }
    bool native = argc == 3 && !strcmp(argv[2], "native");
    if (argc != 2 && !native) {
        return 2;
    }
    FILE *file = fopen(argv[1], "rb");
    if (!file) {
        return 1;
    }
    bool okay = compare_native(file, native);
    if (fclose(file) != 0) {
        okay = false;
    }
    return okay ? 0 : 1;
}
