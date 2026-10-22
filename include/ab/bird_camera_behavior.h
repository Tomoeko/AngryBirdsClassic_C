#ifndef AB_BIRD_CAMERA_BEHAVIOR_H
#define AB_BIRD_CAMERA_BEHAVIOR_H

#include "ab/base.h"

typedef struct {
    void *context;
    bool (*start)(void *context, const char *resource_name, AbError *error);
    bool (*cancel)(void *context, AbError *error);
} AbBirdCameraBehaviorHost;

/* BirdCameraFollowBehavior and MuseumBirdCameraFollowBehavior.onLaunch share
 * this ordinary service sequence. The host loads/starts the exact named camera
 * resource; behavior registration, game-object ownership and launch scheduling
 * are separate. No allocation or VM. Bindings remain borrowed during the call.
 * Services may change later bindings; preceding effects survive failure. */
bool ab_bird_camera_on_launch(AbBirdCameraBehaviorHost *host, AbError *error);

/* MuseumBluesCameraFollowBehavior.init cancels before starting the shot-follow
 * sequence. Cancellation may replace the start service/context before use. */
bool ab_bird_camera_blues_initialize(AbBirdCameraBehaviorHost *host, AbError *error);

#endif
