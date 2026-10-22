#include "ab/slingshot.h"
#include "ab/script_math.h"

#include <math.h>
#include <stdint.h>

bool ab_slingshot_launch_velocity(const AbSlingLaunchInput *input, AbVec2 *output) {
    if (!input || !output) {
        return false;
    }
    if (!input->bird_present) {
        *output = (AbVec2){0};
        return true;
    }
    if (!isfinite(input->rest.x) || !isfinite(input->rest.y) ||
        !isfinite(input->rubber_band.x) || !isfinite(input->rubber_band.y) ||
        !isfinite(input->stretch) || !isfinite(input->launch_force) ||
        !isfinite(input->physics_scale) || !isfinite(input->bird_mass) ||
        input->bird_mass <= 0 || !isfinite(input->slingshot_power)) {
        return false;
    }
    AbVec2 delta = {input->rubber_band.x - input->rest.x,
                    input->rubber_band.y - input->rest.y};
    float squared = delta.x * delta.x + delta.y * delta.y;
    float distance = sqrtf(squared);
    float force = input->launch_force * input->physics_scale;
    force = force * input->bird_mass;
    force = force * input->slingshot_power;
    float inverse_mass = 1.0f / input->bird_mass;
    if (!isfinite(squared) || distance <= 0 || !isfinite(force) ||
        !isfinite(inverse_mass)) {
        return false;
    }
    AbVec2 velocity = {delta.x / distance, delta.y / distance};
    velocity.x = velocity.x * force;
    velocity.x = velocity.x * input->stretch;
    velocity.y = velocity.y * force;
    velocity.y = velocity.y * input->stretch;
    velocity.x = velocity.x * inverse_mass;
    velocity.y = velocity.y * inverse_mass;
    if (!isfinite(velocity.x) || !isfinite(velocity.y)) {
        return false;
    }
    *output = velocity;
    return true;
}

bool ab_slingshot_transfer_frame(const AbSlingTransferInput *input,
                                 AbSlingTransferFrame *output) {
    if (!input || !output || !isfinite(input->start.x) || !isfinite(input->start.y) ||
        !isfinite(input->target.x) || !isfinite(input->target.y) ||
        !isfinite(input->height) || !isfinite(input->phase_angle) ||
        !isfinite(input->time) || !isfinite(input->rotation) ||
        !isfinite(input->seconds)) {
        return false;
    }
    float time = input->time + input->seconds;
    float phase = input->phase_angle * time;
    float sine;
    if (!isfinite(time) || !ab_script_sine(phase, &sine)) {
        return false;
    }
    float first = input->start.x * (1.0f - time);
    float second = input->target.x * time;
    AbSlingTransferFrame result = {
        .position = {first + second, input->start.y + sine * input->height},
        .rotation = input->rotation + 0x1.921fb6p+2f * input->seconds,
        .next_time = time < 1 ? time : 0,
        .complete = 1 <= time,
    };
    if (!isfinite(result.position.x) || !isfinite(result.position.y) ||
        !isfinite(result.rotation)) {
        return false;
    }
    *output = result;
    return true;
}

bool ab_slingshot_transfer_begin(const AbSlingTransferStart *input,
                                 AbSlingTransferInput *output) {
    if (!input || !output || !isfinite(input->position.x) ||
        !isfinite(input->position.y) || !isfinite(input->target.x) ||
        !isfinite(input->target.y) || !isfinite(input->rotation) ||
        (input->jumping && !isfinite(input->saved_y))) {
        return false;
    }
    AbVec2 start = {input->position.x,
                    input->jumping ? input->saved_y : input->position.y};
    float height = (input->target.y - start.y) * 0x1.547ae2p+0f;
    if (!isfinite(height)) {
        return false;
    }
    *output = (AbSlingTransferInput){.start = start,
                                     .target = input->target,
                                     .height = height,
                                     .phase_angle = 0.75f * 0x1.921fb6p+1f,
                                     .rotation = input->rotation};
    return true;
}
