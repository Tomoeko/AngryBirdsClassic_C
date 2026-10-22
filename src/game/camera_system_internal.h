#ifndef AB_CAMERA_SYSTEM_INTERNAL_H
#define AB_CAMERA_SYSTEM_INTERNAL_H

#include "ab/camera_system.h"

typedef struct {
    AbVec2 position;
    float zoom, duration;
    bool validate;
    AbCameraMove move;
} CameraGoTo;

struct AbCameraSystem {
    AbCameraMover mover;
    AbCameraLimits limits;
    AbCameraSequenceManager sequence;
    AbCameraViewport viewport;
    bool viewport_present;
    AbVec2 pixel_size, screen_position;
    CameraGoTo automatic;
    bool automatic_present, follow_active;
    bool input_enabled;
    AbVec2 saved_toggle_position;
    bool saved_toggle_position_present;
    AbCameraSystemDone done;
    void *done_context;
    AbCameraFollow follow;
    AbCameraFollowTargets follow_curves;
    AbCameraFollowHost follow_host;
    AbCameraFollowHost *follow_environment;
    AbCameraSystemFollowResolver follow_resolver;
    void *follow_resolver_context;
};

bool ab_camera_system_follow_targets(AbCameraSystem *system, float seconds,
                                     bool user_input, AbCameraTargets *targets,
                                     bool *present, AbError *error);
bool ab_camera_system_move(AbCameraSystem *system, float seconds, AbError *error);

#endif
