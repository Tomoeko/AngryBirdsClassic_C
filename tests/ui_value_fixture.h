#ifndef AB_TEST_UI_VALUE_FIXTURE_H
#define AB_TEST_UI_VALUE_FIXTURE_H

#include "ab/ui_pointer.h"

#include <string.h>

/* Bounded native fixture codec: one identity-only function token, 32-bit
 * logical object IDs and a caller-owned text dictionary. No script execution. */
static bool ab_test_ui_value_decode(const uint32_t input[2], const char *const *texts,
                                    size_t text_count, AbUiPointerValue *output) {
    if (input[0] > AB_UI_POINTER_FUNCTION) {
        return false;
    }
    AbUiPointerValue value = {.kind = (AbUiPointerKind)input[0]};
    switch (value.kind) {
        case AB_UI_POINTER_NONE:
        case AB_UI_POINTER_FUNCTION:
            if (input[1]) {
                return false;
            }
            break;
        case AB_UI_POINTER_BOOLEAN:
            if (input[1] > 1) {
                return false;
            }
            value.boolean = input[1] != 0;
            break;
        case AB_UI_POINTER_NUMBER:
            value.number_bits = input[1];
            break;
        case AB_UI_POINTER_TEXT:
            if (input[1] >= text_count) {
                return false;
            }
            value.text =
                (AbSpan){(const uint8_t *)texts[input[1]], strlen(texts[input[1]])};
            break;
        case AB_UI_POINTER_OBJECT:
            value.object = input[1];
            break;
    }
    *output = value;
    return true;
}

static bool ab_test_ui_value_encode(AbUiPointerValue value, const char *const *texts,
                                    size_t text_count, uint32_t output[2]) {
    if (!ab_ui_pointer_value_valid(&value)) {
        return false;
    }
    output[0] = (uint32_t)value.kind;
    output[1] = 0;
    switch (value.kind) {
        case AB_UI_POINTER_NONE:
            break;
        case AB_UI_POINTER_FUNCTION:
            return value.function == 0;
        case AB_UI_POINTER_BOOLEAN:
            output[1] = value.boolean;
            break;
        case AB_UI_POINTER_NUMBER:
            output[1] = value.number_bits;
            break;
        case AB_UI_POINTER_OBJECT:
            if (value.object > UINT32_MAX) {
                return false;
            }
            output[1] = (uint32_t)value.object;
            break;
        case AB_UI_POINTER_TEXT:
            for (size_t index = 0; index < text_count; ++index) {
                size_t size = strlen(texts[index]);
                if (size == value.text.size &&
                    (!size || memcmp(texts[index], value.text.data, size) == 0)) {
                    output[1] = (uint32_t)index;
                    return true;
                }
            }
            return false;
    }
    return true;
}

#endif
