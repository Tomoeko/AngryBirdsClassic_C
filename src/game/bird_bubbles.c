#include "ab/bird_bubbles.h"

#include <math.h>

bool ab_bird_bubble_candidate(AbBirdBubbleCandidate input, bool *inside,
                              AbError *error) {
    if (!inside)
        return ab_fail(error, "Missing bubble candidate output");
    if (input.same || input.excluded_material ||
        (input.controllable && (!input.shot || input.puffer || input.flying))) {
        *inside = false;
        return true;
    }
    if (!isfinite(input.relative.x) || !isfinite(input.relative.y) ||
        !isfinite(input.radius))
        return ab_fail(error, "Invalid bubble candidate geometry");
    float squared =
        input.relative.x * input.relative.x + input.relative.y * input.relative.y;
    float distance = (float)sqrt((double)squared);
    *inside = distance < input.radius;
    return true;
}

bool ab_bird_bubble_timer(float timer, float seconds, float speed, float *next,
                          bool *cleanup, AbError *error) {
    if (!next || !cleanup || !isfinite(timer) || !isfinite(seconds) ||
        !isfinite(speed) || seconds < 0 || speed < 0)
        return ab_fail(error, "Invalid bubble timer input");
    float result = timer > 0 ? timer - seconds * speed : timer;
    if (!isfinite(result))
        return ab_fail(error, "Bubble timer exceeds finite range");
    *next = result;
    *cleanup = !(timer > 0);
    return true;
}

bool ab_bird_bubble_force(AbVec2 velocity, bool held_a, float lateral, float mass,
                          float gravity, AbVec2 *force, bool *apply, AbError *error) {
    if (!force || !apply || !isfinite(velocity.y) || (held_a && !isfinite(velocity.x)))
        return ab_fail(error, "Invalid bubble velocity");
    bool active = held_a ? (float)sqrt((double)(velocity.x * velocity.x +
                                                velocity.y * velocity.y)) < 10
                         : velocity.y > -10;
    if (!active) {
        *apply = false;
        return true;
    }
    AbVec2 result = {lateral * mass, (-gravity * mass) * 1.8f};
    if (!isfinite(lateral) || !isfinite(mass) || !isfinite(gravity) ||
        !isfinite(result.x) || !isfinite(result.y))
        return ab_fail(error, "Invalid bubble force");
    *force = result;
    *apply = true;
    return true;
}
