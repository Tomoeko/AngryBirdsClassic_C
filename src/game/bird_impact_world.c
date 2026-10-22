#include "ab/bird_impact_world.h"

typedef struct {
    AbBirdImpactWorld *world;
    AbBirdImpactWorldHost *host;
} ImpactWorld;

static bool radius(void *context, float *output, AbError *error) {
    AbBirdImpactWorldHost *host = ((ImpactWorld *)context)->host;
    return host->radius ? host->radius(host->context, output, error)
                        : ab_fail(error, "Missing world prediction radius service");
}

static bool print(void *context, size_t raycasts, AbError *error) {
    AbBirdImpactWorldHost *host = ((ImpactWorld *)context)->host;
    return host->print ? host->print(host->context, raycasts, error)
                       : ab_fail(error, "Missing world prediction print service");
}

static bool raycast(void *context, AbVec2 first, AbVec2 second,
                    AbBirdImpactRayHit *output, AbError *error) {
    ImpactWorld *binding = context;
    const AbBirdImpactWorld *world = binding->world;
    AbLevelRaycast result = {0};
    if (!ab_level_bodies_raycast(world->world, world->bodies, world->removed, first,
                                 second, &result, error)) {
        return false;
    }
    AbBirdImpactWorldHost *host = binding->host;
    if (host->observe && !host->observe(host->context, first, second, &result, error)) {
        return false;
    }
    *output = (AbBirdImpactRayHit){result.hit, result.name, result.point};
    return true;
}

bool ab_bird_impact_predict_world(AbBirdImpactInput *input, AbBirdImpactWorld *world,
                                  AbBirdImpactWorldHost *host,
                                  AbBirdImpactPrediction *output, AbError *error) {
    if (!world || !host) {
        return ab_fail(error, "Missing world prediction binding or services");
    }
    ImpactWorld binding = {world, host};
    AbBirdImpactHost callbacks = {&binding, host->maximum_raycasts, radius, raycast,
                                  print};
    return ab_bird_impact_predict(input, &callbacks, output, error);
}
