#ifndef AB_CAMERA_FOLLOW_WORLD_H
#define AB_CAMERA_FOLLOW_WORLD_H

#include "ab/bird_impact_world.h"
#include "ab/camera_follow.h"

typedef struct {
    AbBirdImpactWorld *world;
    AbBirdImpactWorldHost *prediction;
    const float *gravity;
    void *context;
    bool (*start_sequence)(void *context, const char *resource_name, AbError *error);
} AbCameraFollowWorldConfig;

typedef struct {
    AbCameraFollowHost host;
    AbCameraFollowWorldConfig config;
} AbCameraFollowWorld;

/* Provide the owned camera's three game services: original impact prediction
 * over real named world queries, physics-to-world coordinates, and the exact
 * terminal sequence identifier. Own no resources. After initialization, borrow
 * &binding->host for ab_camera_system_follow_bird or its sequence resolver.
 * Binding/configuration pointers remain stable and alive through each update;
 * do not copy an initialized binding, whose host refers to its own address.
 * World fields and ordinary bird/gravity/service state may change at radius
 * callbacks. Preserve captured prediction position and live launch/gravity reads.
 * Service failures retain preceding effects. Complete launch/object ownership
 * and scheduling remain caller responsibilities. No VM or allocation.
 * Initialization failure preserves binding; config itself is copied. */
bool ab_camera_follow_world_initialize(AbCameraFollowWorld *binding,
                                       const AbCameraFollowWorldConfig *config,
                                       AbError *error);

#endif
