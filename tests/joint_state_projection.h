#ifndef AB_TEST_JOINT_STATE_PROJECTION_H
#define AB_TEST_JOINT_STATE_PROJECTION_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

/* Constructors leave solver caches undefined. Preserve only constructor axes,
 * retained impulses and limit state when an island never initialized a joint. */
static void joint_state_mask_uninitialized(uint32_t kind, uint32_t words[37]) {
    for (size_t field = 0; field < 37; ++field) {
        bool retained = kind == 0   ? field == 17
                        : kind == 1 ? field >= 21 && field <= 23
                        : kind == 2 ? (field >= 22 && field <= 25) || field == 36
                                    : (field >= 26 && field <= 33) || field == 36;
        if (!retained) {
            words[field] = 0;
        }
    }
}

#endif
