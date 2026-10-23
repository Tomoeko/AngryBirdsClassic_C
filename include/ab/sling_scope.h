#ifndef AB_SLING_SCOPE_H
#define AB_SLING_SCOPE_H

#include "ab/slingshot_draw.h"

typedef struct {
    float timer, angle;
    AbVec2 velocity;
} AbSlingScopeState;

typedef struct {
    AbVec2 velocity, position;
    float timer, vertical_force, mass;
    float gravity_y, maximum_translation, physics_to_world;
    float length, alpha_factor;
    bool body_present, alpha_decay;
    AbSlingshotDrawState render;
} AbSlingScopeInput;

typedef struct {
    AbVec2 position;
    AbSlingshotDrawState render;
    float alpha;
} AbSlingScopeDot;

enum { AB_SLING_SCOPE_DOTS = 64 };
typedef struct {
    AbSlingScopeDot dots[AB_SLING_SCOPE_DOTS];
    size_t count;
    AbSlingshotDrawState final_render;
    float final_alpha;
} AbSlingScopePlan;

/* SlingshotSystem.updateSlingScope26068 numeric phase. The caller supplies the
 * source dragged/non-rest velocity, or (1,0) otherwise, and resolves the actual
 * accessory image/pivot query. Preserve float angle wrapping and smoothing. */
bool ab_sling_scope_update(AbSlingScopeState *state, AbVec2 velocity, float seconds,
                           AbError *error);

/* Native GameLua.drawSlingScope geometry/alpha plan. Capture its current render
 * state, real world gravity, object mass and native translation limit. Preserve
 * speed clamping, fixed-step ballistic samples, interpolation, phase spacing,
 * division before rendering and final native state writes. Sprite lookup,
 * pivot anchoring, image submission, scope sound and GUI gates are caller work.
 * Finite used values, positive native limits/mass and length in [0,12] are
 * supported. No frame allocation or implicit physics step. Failure preserves
 * output; native full-frame/GPU ordering is a separate comparison boundary. */
bool ab_sling_scope_plan(const AbSlingScopeInput *input, AbSlingScopePlan *output,
                         AbError *error);

#endif
