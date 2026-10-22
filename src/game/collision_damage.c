#include "ab/collision_damage.h"

#include <math.h>

static bool valid_body(const AbLegacyCollisionBody *body) {
    return isfinite(body->velocity.x) && isfinite(body->velocity.y) &&
           isfinite(body->mass) && body->mass >= 0 &&
           (!body->has_strength || isfinite(body->strength)) &&
           (!body->has_defence || isfinite(body->defence));
}

static float apply_damage(const AbLegacyCollisionBody *body, float force,
                          bool ignore_ground, float *remaining, bool *dead) {
    if (ignore_ground || body->ignore_all_damage || !body->has_strength) {
        return 0;
    }
    float defence = body->has_defence ? body->defence : 0;
    if (!(defence < force)) {
        return 0;
    }
    float available = force - defence;
    float damage = body->strength;
    if (available < damage) {
        damage = available;
    }
    *remaining = body->strength - damage;
    *dead = *remaining <= 0;
    return damage;
}

bool ab_legacy_block_damage(const AbLegacyCollisionBody bodies[2],
                            AbLegacyBlockDamage *output) {
    if (!bodies || !output || !valid_body(bodies) || !valid_body(bodies + 1)) {
        return false;
    }
    AbVec2 first = {bodies[0].velocity.x * bodies[0].mass,
                    bodies[0].velocity.y * bodies[0].mass};
    AbVec2 second = {bodies[1].velocity.x * bodies[1].mass,
                     bodies[1].velocity.y * bodies[1].mass};
    AbVec2 difference = {first.x - second.x, first.y - second.y};
    float squared = difference.x * difference.x + difference.y * difference.y;
    float force = sqrtf(squared) * 0x1.99999ap-4f;
    if (!isfinite(force)) {
        return false;
    }
    AbLegacyBlockDamage result = {
        .force = force,
        .remaining_strength = {bodies[0].has_strength ? bodies[0].strength : 0,
                               bodies[1].has_strength ? bodies[1].strength : 0},
    };
    bool ignore_ground =
        (bodies[0].ignore_ground_damage && bodies[1].ground_or_static) ||
        (bodies[1].ignore_ground_damage && bodies[0].ground_or_static);
    float total = 0;
    for (size_t index = 0; index < 2; ++index) {
        if (!bodies[1 - index].no_collision_damage) {
            float damage = apply_damage(bodies + index, force, ignore_ground,
                                        result.remaining_strength + index,
                                        result.mark_dead + index);
            total = total + damage;
        }
    }
    result.callback_damage = floorf(total);
    if (!isfinite(result.remaining_strength[0]) ||
        !isfinite(result.remaining_strength[1]) || !isfinite(result.callback_damage)) {
        return false;
    }
    *output = result;
    return true;
}
