#include "ab/slingshot_draw.h"

#include <math.h>

const char *ab_slingshot_sprite_name(AbSlingshotSprite sprite) {
    switch (sprite) {
        case AB_SLING_SPRITE_BACK:
            return "SLING_SHOT_01_BACK";
        case AB_SLING_SPRITE_HOLDER:
            return "SLING_HOLDER";
        case AB_SLING_SPRITE_FRONT:
            return "SLING_SHOT_01_FRONT";
        case AB_SLING_SPRITE_FRONT_SCOPE:
            return "SLINGSHOT_POWERUP_BASIC";
    }
    return NULL;
}

static bool finite_vector(AbVec2 value) {
    return isfinite(value.x) && isfinite(value.y);
}

bool ab_slingshot_sprite_quad(const AbSpriteRegion *region, AbVec2 position,
                              AbSlingshotDrawState state, uint32_t viewport_width,
                              uint32_t viewport_height, const float uv[8],
                              AbDrawQuad *output, bool *visible, AbError *error) {
    if (!region || !finite_vector(position)) {
        return ab_fail(error, "Invalid sling sprite region or position");
    }
    float pivot_x = (float)(int32_t)((float)region->pivot_x + 0.5f);
    float pivot_y = (float)(int32_t)((float)region->pivot_y + 0.5f);
    AbImageRectangle rectangle = {position.x - pivot_x, position.y - pivot_y,
                                  (float)region->width, (float)region->height};
    AbSpriteRenderState render = {.angle_radians = state.angle,
                                  .translation_x = state.translation.x,
                                  .translation_y = state.translation.y,
                                  .scale_x = state.scale.x,
                                  .scale_y = state.scale.y,
                                  .rotation_origin_x = state.pivot.x,
                                  .rotation_origin_y = state.pivot.y};
    return ab_image_rectangle_quad(rectangle, render, viewport_width, viewport_height,
                                   uv, output, visible, error);
}

static void append_state(AbSlingshotDrawPlan *plan, AbSlingshotDrawState state) {
    plan->commands[plan->count++] =
        (AbSlingshotDrawCommand){.kind = AB_SLING_COMMAND_STATE, .state = state};
}

static void append_sprite(AbSlingshotDrawPlan *plan, AbSlingshotSprite sprite,
                          AbVec2 position) {
    plan->commands[plan->count++] = (AbSlingshotDrawCommand){
        .kind = AB_SLING_COMMAND_SPRITE, .sprite = {sprite, position}};
}

bool ab_slingshot_draw_plan(const AbSlingshotDrawInput *input, AbSlingshotDrawPart part,
                            AbSlingshotDrawPlan *output, AbError *error) {
    if (!input || !output || (unsigned)part > AB_SLING_DRAW_FRONT) {
        return ab_fail(error, "Invalid slingshot draw input or part");
    }
    AbSlingshotDrawPlan plan = {0};
    if (input->disabled) {
        *output = plan;
        return true;
    }
    if (!finite_vector(input->position) || !finite_vector(input->camera_top_left) ||
        !isfinite(input->zoom)) {
        return ab_fail(error, "Invalid slingshot camera or position");
    }
    if (part != AB_SLING_DRAW_FRONT &&
        (!finite_vector(input->stretch_position) || !isfinite(input->band_width))) {
        return ab_fail(error, "Invalid slingshot band position or width");
    }
    if (part == AB_SLING_DRAW_HOLDER && input->dragged &&
        (!isfinite(input->band_angle) || !finite_vector(input->holder_pivot))) {
        return ab_fail(error, "Invalid dragged sling holder angle or pivot");
    }
    AbSlingshotDrawState state = {
        .translation = {-input->camera_top_left.x, -input->camera_top_left.y},
        .scale = {input->zoom, input->zoom}};
    append_state(&plan, state);
    if (part == AB_SLING_DRAW_FRONT) {
        append_sprite(&plan,
                      input->has_scope ? AB_SLING_SPRITE_FRONT_SCOPE
                                       : AB_SLING_SPRITE_FRONT,
                      input->position);
    } else {
        AbVec2 band_start = input->position;
        if (part == AB_SLING_DRAW_BACK) {
            append_sprite(&plan, AB_SLING_SPRITE_BACK, input->position);
            band_start.x += 20.0f;
        } else {
            if (!input->dragged) {
                append_sprite(&plan, AB_SLING_SPRITE_HOLDER, input->stretch_position);
            }
            band_start.x -= 21.0f;
            band_start.y -= 3.0f;
        }
        if (!finite_vector(band_start)) {
            return ab_fail(error, "Slingshot band anchor exceeds finite range");
        }
        plan.commands[plan.count++] = (AbSlingshotDrawCommand){
            .kind = AB_SLING_COMMAND_BAND,
            .band = {band_start, input->stretch_position, input->band_width}};
        if (part == AB_SLING_DRAW_BACK) {
            plan.commands[plan.count++] =
                (AbSlingshotDrawCommand){.kind = AB_SLING_COMMAND_EXTRAS};
        } else if (input->dragged) {
            AbSlingshotDrawState rotated = state;
            rotated.angle = input->band_angle;
            rotated.pivot = input->holder_pivot;
            append_state(&plan, rotated);
            append_sprite(&plan, AB_SLING_SPRITE_HOLDER, input->stretch_position);
            append_state(&plan, state);
        }
    }
    *output = plan;
    return true;
}
