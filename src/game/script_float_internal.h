#ifndef AB_SCRIPT_FLOAT_INTERNAL_H
#define AB_SCRIPT_FLOAT_INTERNAL_H

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

typedef enum {
    AB_SCRIPT_FLOAT_ADD,
    AB_SCRIPT_FLOAT_MULTIPLY,
    AB_SCRIPT_FLOAT_DIVIDE,
    AB_SCRIPT_FLOAT_SUBTRACT,
} AbScriptFloatOperation;

static float script_float_word(uint32_t word) {
    float value;
    memcpy(&value, &word, sizeof(value));
    return value;
}

/* Original ARM binary32 arithmetic: signaling operands take priority before
 * quiet operands, then the first operand wins. Quiet the selected payload;
 * newly invalid operations yield positive default NaN. Status flags are outside
 * this numeric contract, as in the measured camera-drag arithmetic boundary. */
static float script_float_binary(float first, float second,
                                 AbScriptFloatOperation operation) {
    uint32_t a, b;
    memcpy(&a, &first, sizeof(a));
    memcpy(&b, &second, sizeof(b));
    bool a_nan = (a & UINT32_C(0x7fffffff)) > UINT32_C(0x7f800000);
    bool b_nan = (b & UINT32_C(0x7fffffff)) > UINT32_C(0x7f800000);
    uint32_t selected = 0;
    if (a_nan && !(a & UINT32_C(0x00400000))) {
        selected = a;
    } else if (b_nan && !(b & UINT32_C(0x00400000))) {
        selected = b;
    } else if (a_nan) {
        selected = a;
    } else if (b_nan) {
        selected = b;
    }
    if (selected) {
        return script_float_word(selected | UINT32_C(0x00400000));
    }
    float value = operation == AB_SCRIPT_FLOAT_ADD        ? first + second
                  : operation == AB_SCRIPT_FLOAT_MULTIPLY ? first * second
                  : operation == AB_SCRIPT_FLOAT_DIVIDE   ? first / second
                                                          : first - second;
    return isnan(value) ? script_float_word(UINT32_C(0x7fc00000)) : value;
}

#endif
