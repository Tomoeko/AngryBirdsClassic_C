#ifndef AB_TEST_OBJECT_REMOVAL_NOTIFY_CASES_H
#define AB_TEST_OBJECT_REMOVAL_NOTIFY_CASES_H

#include <stdint.h>

/* Catalog/blocks/raw kinds; onRemoved key/binding; level key/entry/bindings;
 * named/level mutations, definition-key kind, marker and handlerB binding. */
static void object_removal_notify_case(uint32_t sample, uint32_t output[15]) {
    output[0] = sample;
    for (uint32_t field = 1; field < 15; ++field) {
        output[field] = ((sample * (field * 2u + 1u)) / (field + 1u)) % 7u;
    }
    output[7] = (sample / 17u) % 4u;
    output[10] = (sample * 73u + sample / 7u) & 255u;
    output[11] = (sample * 37u + sample / 11u) & 63u;
    output[12] = (sample / 13u) % 5u;
    output[13] %= 6u;
    if (sample < 1024u) {
        output[1] = output[2] = output[3] = 6;
        output[4] = sample % 7u;
        output[5] = (sample / 7u) % 7u;
        output[6] = (sample / 49u) % 7u;
        output[12] = 0;
    }
    if (sample < 16u) {
        static const uint32_t mutations[] = {0,   128, 2, 13,  32,  16,  64,  165,
                                             144, 12,  9, 129, 132, 141, 224, 255};
        output[4] = 4;
        output[5] = output[8] = 5;
        output[6] = 4;
        output[7] = 1;
        output[9] = 6;
        output[10] = mutations[sample];
        output[11] = sample == 8 ? 63u : 0u;
    }
}

#endif
