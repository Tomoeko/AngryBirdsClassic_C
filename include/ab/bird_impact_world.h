#ifndef AB_BIRD_IMPACT_WORLD_H
#define AB_BIRD_IMPACT_WORLD_H

#include "ab/bird_impact_prediction.h"
#include "ab/level_raycast.h"

typedef struct {
    const AbPhysicsWorld *world;
    const AbLevelBodies *bodies;
    const bool *removed;
} AbBirdImpactWorld;

typedef struct {
    void *context;
    size_t maximum_raycasts;
    bool (*radius)(void *context, float *output, AbError *error);
    bool (*print)(void *context, size_t raycasts, AbError *error);
    /* Optional diagnostic observation after the real named world query. Miss
     * fields other than hit remain absent. May reject the enclosing prediction,
     * but must not change inputs, bindings, world state or the borrowed result. */
    bool (*observe)(void *context, AbVec2 first, AbVec2 second,
                    const AbLevelRaycast *result, AbError *error);
} AbBirdImpactWorldHost;

/* Compose the recovered predictor with actual world/tree/shape queries and live
 * canonical object names. No synthetic hit provider, allocation or VM. Source
 * storage remains borrowed and stable until return. Input velocity/gravity may
 * change at radius callbacks as in the scalar predictor; position/steps are
 * captured on entry, along with the query budget. Radius and print are required
 * only when reached. Sources
 * may be rebound between callbacks, but not during a query or observation.
 * Prior callback effects survive failure; output commits only on success.
 * All endpoint words, including native NaNs at zero velocity, reach the query.
 */
bool ab_bird_impact_predict_world(AbBirdImpactInput *input, AbBirdImpactWorld *world,
                                  AbBirdImpactWorldHost *host,
                                  AbBirdImpactPrediction *output, AbError *error);

#endif
