#include "ab/slingshot_pose.h"

#include "ab/coordinates.h"
#include "ab/script_math.h"

#include <math.h>

bool ab_slingshot_start_pose(const AbSlingshotStartInput *input,
                             AbSlingshotStartPose *output, AbError *error) {
    if (!input || !output) {
        return ab_fail(error, "Missing slingshot start input or output");
    }
    if (!input->object_present) {
        return true;
    }
    float radius = input->radius_present ? input->radius : 0;
    if (!isfinite(input->object_position.x) || !isfinite(input->object_position.y) ||
        !isfinite(input->back_pivot_y) || !isfinite(input->back_height) ||
        !isfinite(input->physics_scale) || !isfinite(radius)) {
        return ab_fail(error, "Invalid slingshot start position or sprite bounds");
    }
    float extent = input->back_height - input->back_pivot_y;
    extent *= input->physics_scale;
    AbVec2 start = input->object_position;
    start.y -= extent;
    start.y += radius;
    start.y += 0.2f;
    AbSlingshotStartPose result = {.level_start = start,
                                   .base = input->object_position,
                                   .band = {start.x - 0.1f, start.y - 0.1f}};
    if (!isfinite(extent) || !isfinite(start.y) || !isfinite(result.band.x) ||
        !isfinite(result.band.y)) {
        return ab_fail(error, "Slingshot start position exceeds finite range");
    }
    *output = result;
    return true;
}

bool ab_slingshot_render_pose(const AbSlingshotPoseInput *input,
                              AbSlingshotPose *output, AbError *error) {
    if (!input || !output || !isfinite(input->band_angle) ||
        !isfinite(input->distance)) {
        return ab_fail(error, "Invalid slingshot pose input");
    }
    AbSlingshotPose result = {.angle = input->band_angle, .width = 25};
    if (!ab_coordinate_point(NULL, AB_SPACE_PHYSICS, AB_SPACE_WORLD,
                             input->band_physics, &result.stretch_position) ||
        !ab_coordinate_point(NULL, AB_SPACE_PHYSICS, AB_SPACE_WORLD,
                             input->start_physics, &result.position)) {
        return ab_fail(error, "Slingshot pose position exceeds finite range");
    }
    if (input->bird_present) {
        if (!isfinite(input->physics_to_world)) {
            return ab_fail(error, "Invalid slingshot bird conversion");
        }
        float radius = input->radius_present && !input->box ? input->bird_radius
                                                            : input->bird_width * 0.5f;
        float extent = (radius + 0.05f) * input->physics_to_world;
        float sine, cosine;
        if (!isfinite(radius) || !isfinite(extent) ||
            !ab_script_cosine(input->band_angle, &cosine) ||
            !ab_script_sine(input->band_angle, &sine)) {
            return ab_fail(error, "Unsupported slingshot bird adjustment");
        }
        result.stretch_position.x -= extent * cosine;
        result.stretch_position.y -= extent * sine;
    }
    if (input->distance > 0) {
        result.width = 50.0f / input->distance;
        if (result.width < 10) {
            result.width = 10;
        } else if (result.width > 25) {
            result.width = 25;
        }
    }
    if (!isfinite(result.stretch_position.x) || !isfinite(result.stretch_position.y)) {
        return ab_fail(error, "Slingshot band adjustment exceeds finite range");
    }
    *output = result;
    return true;
}
