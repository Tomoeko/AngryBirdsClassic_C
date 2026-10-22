#ifndef AB_TEST_JOINT_NATIVE_H
#define AB_TEST_JOINT_NATIVE_H

#include "ab/physics_joint_constraint.h"
#include "step_body_native.h"

#include "joint_constraint_native.h"

static bool compare_words(const uint32_t *actual, const uint32_t *expected,
                          size_t count, uint32_t sample, const char *kind,
                          size_t index) {
    for (size_t field = 0; field < count; ++field) {
        if (actual[field] != expected[field]) {
            fprintf(stderr,
                    "Joint island %" PRIu32 " %s %zu field %zu: %08" PRIx32
                    " != %08" PRIx32 "\n",
                    sample, kind, index, field, actual[field], expected[field]);
            return false;
        }
    }
    return true;
}

#endif
