#include "ab/game_rectangle.h"

#include <math.h>

static bool integer(float value, int32_t *output) {
    if (!isfinite(value) || value < -0x1p31f || value >= 0x1p31f) {
        return false;
    }
    *output = (int32_t)value;
    return true;
}

bool ab_game_rectangle_plan(const AbGameRectangle *rectangle,
                            AbGameRectanglePlan *output, AbError *error) {
    if (!rectangle || !output || !isfinite(rectangle->left) ||
        !isfinite(rectangle->top) || !isfinite(rectangle->right) ||
        !isfinite(rectangle->bottom)) {
        return ab_fail(error, "Invalid game rectangle arguments");
    }
    int32_t color[4];
    for (size_t index = 0; index < 4; ++index) {
        if (!isfinite(rectangle->color[index]) ||
            !integer(rectangle->color[index] * 255, color + index)) {
            return ab_fail(error, "Game rectangle color exceeds integer domain");
        }
    }
    AbGameRectanglePlan plan = {.argb = (uint32_t)color[2] | ((uint32_t)color[1] << 8) |
                                        ((uint32_t)color[0] << 16) |
                                        ((uint32_t)color[3] << 24),
                                .reset_state = !rectangle->respect_state};
    if (!integer(rectangle->left, &plan.left) || !integer(rectangle->top, &plan.top) ||
        !integer(rectangle->right - rectangle->left, &plan.width) ||
        !integer(rectangle->bottom - rectangle->top, &plan.height)) {
        return ab_fail(error, "Game rectangle position/extent exceeds integer domain");
    }
    *output = plan;
    return true;
}
