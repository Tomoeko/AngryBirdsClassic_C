#ifndef AB_TEST_CLASSIC_SLING_DRAW_H
#define AB_TEST_CLASSIC_SLING_DRAW_H

#include "ab/classic_sling_draw.h"

/* Real shared shot/physical owners; prove that the pose reads the canonical
 * radius rather than frozen constructor/body dimensions, then restore it. */
static bool lifecycle_sling_draw(ShotTest *shot, AbClassicShots *shots,
                                 AbClassicSlingDraw *draw, size_t object,
                                 AbError *error) {
    AbShotState *state = ab_classic_shots_state(shots);
    AbShotBird *bird = ab_classic_shots_bird(shots, object);
    CHECK(state && bird);
    const char *saved_name = state->current_bird_name;
    state->current_bird_name = bird->name;
    AbDataTable *physical = &shot->level.properties->objects[object].physical;
    AbDataValue radius = field(physical, "radius");
    CHECK(radius.kind == AB_DATA_NUMBER &&
          ab_classic_sling_draw_calculate(draw, error));
    AbSlingshotPose before, changed, restored;
    CHECK(ab_classic_sling_draw_pose(draw, &before));
    float number = 0;
    memcpy(&number, &radius.bits, sizeof(number));
    AbDataValue larger = {.kind = AB_DATA_NUMBER, .bits = bits(number + 1)};
    CHECK(ab_level_property_set(shot->level.properties, object, span("radius"), larger,
                                error));
    CHECK(ab_classic_sling_draw_calculate(draw, error));
    CHECK(ab_classic_sling_draw_pose(draw, &changed));
    CHECK(before.stretch_position.x != changed.stretch_position.x ||
          before.stretch_position.y != changed.stretch_position.y);
    CHECK(before.position.x == changed.position.x &&
          before.position.y == changed.position.y);
    CHECK(ab_level_property_set(shot->level.properties, object, span("radius"), radius,
                                error));
    CHECK(ab_classic_sling_draw_calculate(draw, error));
    CHECK(ab_classic_sling_draw_pose(draw, &restored));
    CHECK(!memcmp(&before, &restored, sizeof(before)));
    state->current_bird_name = saved_name;
    CHECK(ab_classic_sling_draw_calculate(draw, error));
    return true;
}

#endif
