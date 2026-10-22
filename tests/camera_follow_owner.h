#ifndef AB_TEST_CAMERA_FOLLOW_OWNER_H
#define AB_TEST_CAMERA_FOLLOW_OWNER_H

#include "ab/camera_system.h"
#include <stdlib.h>

typedef struct {
    size_t predictions, transforms, sequences;
    float prediction_y;
} OwnerFollowTest;

static bool owner_follow_service(void *context, AbCameraFollowCall *call,
                                 AbError *error) {
    OwnerFollowTest *test = context;
    if (call->operation == AB_CAMERA_FOLLOW_PREDICT) {
        ++test->predictions;
        call->prediction = (AbBirdImpactPrediction){
            .point = {100, test->prediction_y}, .time = 1, .point_present = true};
        return true;
    }
    if (call->operation == AB_CAMERA_FOLLOW_TRANSFORM) {
        ++test->transforms;
        call->vector = call->point;
        return true;
    }
    if (call->operation == AB_CAMERA_FOLLOW_SEQUENCE) {
        ++test->sequences;
        return true;
    }
    return ab_fail(error, "Owned follow delegated an internal camera operation");
}

static bool owner_follow_before_remove(AbCameraSystem *system, AbCameraFollowBird *bird,
                                       AbCameraFollowHost *environment,
                                       OwnerFollowTest *test) {
    CHECK(ab_camera_system_follow_bird(system, bird, NULL, NULL, environment, NULL));
    CHECK(ab_camera_system_target_bird(system) == bird);
    AbCameraSystemView view = ab_camera_system_view(system);
    CHECK(view.follow && view.follow->allow_cancel && view.automatic_present);
    AbCameraSystemInput input = {0};
    CHECK(ab_camera_system_update(system, 0.1f, &input, NULL));
    view = ab_camera_system_view(system);
    CHECK(test->predictions == 1 && test->transforms == 2 && view.follow_curves &&
          view.follow_curves->custom_zoom && view.follow_curves->custom_y &&
          view.screen_position.x == view.mover->current_position.x &&
          view.screen_position.y == view.mover->current_position.y);
    AbCameraFollowBird unrelated = {0};
    CHECK(ab_camera_system_remove_bird(system, &unrelated) &&
          ab_camera_system_target_bird(system) == bird);
    CHECK(ab_camera_system_remove_bird(system, bird) &&
          !ab_camera_system_target_bird(system));
    view = ab_camera_system_view(system);
    CHECK(!view.follow && !view.automatic_present && view.follow_curves);
    return true;
}

static bool owner_follow_after_remove(AbCameraSystem *system, OwnerFollowTest *test) {
    AbCameraSystemInput input = {0};
    CHECK(ab_camera_system_update(system, 0.1f, &input, NULL));
    CHECK(test->predictions == 1 && test->transforms == 2 && !test->sequences);
    AbCameraFollowBird next = {
        .position = {0, -400}, .launch_velocity = {10, -10}, .launch_present = true};
    bool allow_cancel = false;
    CHECK(ab_camera_system_follow_bird(system, &next, &allow_cancel, NULL, NULL, NULL));
    AbCameraSystemView view = ab_camera_system_view(system);
    CHECK(view.follow && !view.follow->allow_cancel);
    AbVec2 target = view.mover->target_position;
    input.user_input = true;
    input.targets_present = true;
    input.targets.values = (AbCameraValues){.position = {100000, 100000}, .present = 3};
    CHECK(ab_camera_system_update(system, 0.1f, &input, NULL));
    view = ab_camera_system_view(system);
    CHECK(!view.automatic_present && !view.follow &&
          view.mover->target_position.x == target.x &&
          view.mover->target_position.y == target.y && test->predictions == 1);
    AbCameraValues position = {.position = {20, 30}, .present = 3};
    CHECK(ab_camera_system_goto_position(system, &position, NULL, NULL));
    CHECK(ab_camera_system_view(system).automatic_present &&
          !ab_camera_system_target_bird(system));
    CHECK(ab_camera_system_remove_bird(system, NULL) &&
          !ab_camera_system_view(system).automatic_present);
    return true;
}

static bool owner_follow_controls(void) {
    const AbCameraSnap snaps[] = {{{0, 0}, 2}, {{500, 0}, 1}};
    AbCameraSystem *system = NULL;
    AbCameraFollowBird *bird = malloc(sizeof(*bird));
    AbCameraFollowHost *environment = malloc(sizeof(*environment));
    OwnerFollowTest test = {.prediction_y = -300};
    bool okay = bird && environment;
    if (okay) {
        *bird = (AbCameraFollowBird){.position = {0, -400},
                                     .launch_velocity = {10, -10},
                                     .launch_present = true};
        *environment = (AbCameraFollowHost){&test, 20, owner_follow_service};
        okay = ab_camera_system_open(snaps, 2, (AbVec2){320, 180}, 1000, 50, 65536,
                                     &system, NULL) &&
               owner_follow_before_remove(system, bird, environment, &test);
    }
    free(bird);
    free(environment);
    if (okay) {
        okay = owner_follow_after_remove(system, &test);
    }
    ab_camera_system_close(system);
    return okay;
}

static bool owner_follow_absent_fit_zoom(void) {
    const AbCameraSnap snaps[] = {{{0, 0}, 2}, {{500, 0}, 1}};
    AbCameraSystem *system = NULL;
    OwnerFollowTest test = {.prediction_y = -50};
    AbCameraFollowBird bird = {
        .position = {0, -400}, .launch_velocity = {10, -10}, .launch_present = true};
    AbCameraFollowHost environment = {&test, 20, owner_follow_service};
    AbCameraSystemInput input = {0};
    bool okay =
        ab_camera_system_open(snaps, 2, (AbVec2){320, 180}, 1000, 50, 65536, &system,
                              NULL) &&
        ab_camera_system_follow_bird(system, &bird, NULL, NULL, &environment, NULL) &&
        !ab_camera_system_update(system, 0.1f, &input, NULL);
    if (okay) {
        AbCameraSystemView view = ab_camera_system_view(system);
        okay = view.follow && view.follow->hit_present &&
               view.follow->original_zoom_present &&
               view.follow->time_to_peak_present && !view.follow_curves &&
               view.mover->current_position.x == 0 && view.mover->current_zoom == 1;
    }
    ab_camera_system_close(system);
    CHECK(okay);
    return true;
}

#endif
