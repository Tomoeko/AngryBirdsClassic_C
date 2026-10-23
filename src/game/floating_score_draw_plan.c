#include "ab/floating_score_draw.h"

#include <math.h>

bool ab_floating_score_draw_plan(AbVec2 position, float xs, AbCoordinateCamera camera,
                                 float ui_scale, AbFloatingScoreDrawPlan *output,
                                 AbError *error) {
    if (!output || !isfinite(position.x) || !isfinite(position.y) || !isfinite(xs) ||
        !isfinite(ui_scale) || !isfinite(camera.top_left.x) ||
        !isfinite(camera.top_left.y) || !isfinite(camera.zoom.x))
        return ab_fail(error, "Nonfinite floating-score draw geometry");
    AbFloatingScoreDrawPlan plan = {0};
    plan.screen.x = (position.x * 20.0f - camera.top_left.x) * camera.zoom.x;
    plan.screen.y = (position.y * 20.0f - camera.top_left.y) * camera.zoom.x;
    plan.scale = xs * ui_scale;
    if (!isfinite(plan.screen.x) || !isfinite(plan.screen.y) || !isfinite(plan.scale))
        return ab_fail(error, "Floating-score screen geometry overflow");
    if (plan.scale != 0) {
        plan.text_position =
            (AbVec2){plan.screen.x / plan.scale, plan.screen.y / plan.scale};
        if (!isfinite(plan.text_position.x) || !isfinite(plan.text_position.y))
            return ab_fail(error, "Floating-score scaled coordinates overflow");
        plan.sprite_position =
            (AbVec2){floorf(plan.text_position.x), floorf(plan.text_position.y)};
        plan.visible = true;
    }
    *output = plan;
    return true;
}
