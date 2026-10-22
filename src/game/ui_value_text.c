#include "ab/ui_value_text.h"

#include <stdio.h>
#include <string.h>

static int format_number(uint32_t bits, char bytes[32]) {
    uint32_t magnitude = bits & UINT32_C(0x7fffffff);
    if (magnitude > UINT32_C(0x7f800000)) {
        memcpy(bytes, "nan", 4);
        return 3;
    }
    if (magnitude == UINT32_C(0x7f800000)) {
        if (bits & UINT32_C(0x80000000)) {
            memcpy(bytes, "-inf", 5);
            return 4;
        }
        memcpy(bytes, "inf", 4);
        return 3;
    }
    float number;
    memcpy(&number, &bits, sizeof(number));
    return snprintf(bytes, 32, "%.9g", (double)number);
}

bool ab_ui_value_text(AbUiPointerValue value, uint8_t *storage, size_t capacity,
                      AbSpan *output) {
    if (!output || !ab_ui_pointer_value_valid(&value)) {
        return false;
    }
    AbSpan result;
    if (value.kind == AB_UI_POINTER_TEXT) {
        result = value.text;
    } else if (value.kind == AB_UI_POINTER_NONE) {
        result = (AbSpan){(const uint8_t *)"nil", 3};
    } else if (value.kind == AB_UI_POINTER_BOOLEAN) {
        result = value.boolean ? (AbSpan){(const uint8_t *)"true", 4}
                               : (AbSpan){(const uint8_t *)"false", 5};
    } else if (value.kind == AB_UI_POINTER_NUMBER) {
        char bytes[32];
        int length = format_number(value.number_bits, bytes);
        if (length <= 0 || (size_t)length >= sizeof(bytes) ||
            (size_t)length > capacity || !storage) {
            return false;
        }
        memcpy(storage, bytes, (size_t)length);
        result = (AbSpan){storage, (size_t)length};
    } else {
        return false;
    }
    *output = result;
    return true;
}

bool ab_ui_native_value_text(AbUiPointerValue value, uint8_t *storage, size_t capacity,
                             AbSpan *output) {
    if (!output || !ab_ui_pointer_value_valid(&value))
        return false;
    AbSpan result = {(const uint8_t *)"", 0};
    if (value.kind == AB_UI_POINTER_TEXT) {
        result = value.text;
        const uint8_t *end = result.size ? memchr(result.data, 0, result.size) : NULL;
        if (end)
            result.size = (size_t)(end - result.data);
    } else if (value.kind == AB_UI_POINTER_NUMBER &&
               !ab_ui_value_text(value, storage, capacity, &result)) {
        return false;
    }
    *output = result;
    return true;
}
