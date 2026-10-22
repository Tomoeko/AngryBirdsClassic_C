#include "ab/collision_damage.h"
#include "ab/game_collision.h"

#include <math.h>

static bool is_bird(uint32_t collider) {
    return collider == AB_COLLIDER_BIRD || collider == AB_COLLIDER_EAGLE;
}

bool ab_museum_collision_kind(uint32_t first_collider, uint32_t second_collider,
                              AbMuseumCollisionKind *output) {
    if (!output || first_collider >= AB_GAME_COLLIDER_TYPES ||
        second_collider >= AB_GAME_COLLIDER_TYPES) {
        return false;
    }
    bool first = is_bird(first_collider), second = is_bird(second_collider);
    *output = first && second ? AB_MUSEUM_BIRD_PAIR_COLLISION
              : first         ? AB_MUSEUM_FIRST_BIRD_COLLISION
              : second        ? AB_MUSEUM_SECOND_BIRD_COLLISION
                              : AB_MUSEUM_BLOCK_COLLISION;
    return true;
}

bool ab_bird_pair_force(const AbLegacyCollisionBody bodies[2], float *output) {
    if (!bodies || !output) {
        return false;
    }
    float squared[2];
    for (size_t index = 0; index < 2; ++index) {
        const AbLegacyCollisionBody *body = bodies + index;
        if (!isfinite(body->velocity.x) || !isfinite(body->velocity.y) ||
            !isfinite(body->mass) || body->mass < 0) {
            return false;
        }
        squared[index] =
            body->velocity.x * body->velocity.x + body->velocity.y * body->velocity.y;
        if (!isfinite(squared[index])) {
            return false;
        }
    }
    const AbLegacyCollisionBody *body = bodies + (squared[1] < squared[0] ? 0 : 1);
    AbVec2 momentum = {body->velocity.x * body->mass, body->velocity.y * body->mass};
    float magnitude = momentum.x * momentum.x + momentum.y * momentum.y;
    if (!isfinite(magnitude)) {
        return false;
    }
    *output = sqrtf(magnitude) / 10.0f;
    return true;
}

bool ab_museum_callback_contact(AbMuseumCollisionKind kind, AbVec2 normal, AbVec2 point,
                                AbMuseumCallbackContact *output) {
    if (!output || (unsigned)kind > AB_MUSEUM_BIRD_PAIR_COLLISION ||
        !isfinite(normal.x) || !isfinite(normal.y) || !isfinite(point.x) ||
        !isfinite(point.y)) {
        return false;
    }
    bool single = kind == AB_MUSEUM_FIRST_BIRD_COLLISION ||
                  kind == AB_MUSEUM_SECOND_BIRD_COLLISION;
    *output = (AbMuseumCallbackContact){.normal = single ? point : normal,
                                        .point = single ? (AbVec2){0} : point,
                                        .point_present = !single};
    return true;
}
