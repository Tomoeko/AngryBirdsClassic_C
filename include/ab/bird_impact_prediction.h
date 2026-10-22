#ifndef AB_BIRD_IMPACT_PREDICTION_H
#define AB_BIRD_IMPACT_PREDICTION_H

#include "ab/lua_data.h"
#include "ab/physics_geometry.h"

typedef struct {
    AbVec2 position, velocity;
    float gravity, steps;
    bool steps_present;
} AbBirdImpactInput;

typedef struct {
    bool hit;
    AbDataValue name;
    AbVec2 point;
} AbBirdImpactRayHit;

typedef struct {
    void *context;
    size_t maximum_raycasts;
    bool (*radius)(void *context, float *output, AbError *error);
    bool (*raycast)(void *context, AbVec2 first, AbVec2 second,
                    AbBirdImpactRayHit *output, AbError *error);
    bool (*print)(void *context, size_t raycasts, AbError *error);
} AbBirdImpactHost;

typedef struct {
    AbVec2 point;
    AbDataValue name;
    float time;
    size_t raycasts;
    bool point_present;
} AbBirdImpactPrediction;

/* PhysicsQueries.predictBirdImpactPosition, prototype 199 and its three local
 * helpers. Capture initial position and optional steps (default ten). Velocity
 * and world gravity remain live across radius/raycast callbacks. Sample from
 * time zero at 1/steps, with three rays per iteration and a lower interval of
 * 1/(steps*8). The first truthy hit of those rays replaces the retained hit and
 * halves the interval without advancing time. Misses advance time; 200
 * consecutive misses stop. Print the total query count before returning.
 *
 * Preserve the original helper's unused radius-offset parameter: still query
 * radius twice, but all rays use a normalized perpendicular unit offset. Zero
 * velocity can produce native NaNs; do not introduce a normalization fallback.
 * Numeric coordinates and radius results, ordinary original Vector2 helpers,
 * and Boolean native ray hits only. Name/table bytes remain borrowed. No world
 * query implementation, allocation, VM, coercion, metatables or camera updates.
 *
 * Input storage/host bindings stay borrowed and may change at callbacks; initial
 * position/steps stay captured. Do not reenter or free storage. The explicit
 * query budget is at most 2^24; pathological non-progressing inputs fail at that
 * boundary instead of claiming native completion. Missing services fail when
 * reached. Earlier host mutations survive failure; output commits on success.
 * Binary32 result parity and not floating-point status flags is the contract.
 */
bool ab_bird_impact_predict(AbBirdImpactInput *input, AbBirdImpactHost *host,
                            AbBirdImpactPrediction *output, AbError *error);

#endif
