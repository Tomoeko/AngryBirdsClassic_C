#include "ab/bird_camera_behavior.h"

bool ab_bird_camera_on_launch(AbBirdCameraBehaviorHost *host, AbError *error) {
    if (!host || !host->start) {
        return ab_fail(error, "Missing bird camera sequence-start service");
    }
    return host->start(host->context, "FollowShotBirdCameraSequence", error);
}

bool ab_bird_camera_blues_initialize(AbBirdCameraBehaviorHost *host, AbError *error) {
    if (!host || !host->cancel) {
        return ab_fail(error, "Missing Blues camera sequence-cancel service");
    }
    return host->cancel(host->context, error) && ab_bird_camera_on_launch(host, error);
}
