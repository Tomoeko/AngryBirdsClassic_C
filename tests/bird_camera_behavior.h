#ifndef AB_TEST_BIRD_CAMERA_BEHAVIOR_H
#define AB_TEST_BIRD_CAMERA_BEHAVIOR_H

#include "ab/bird_camera_behavior.h"

typedef struct BirdCameraControl BirdCameraControl;
struct BirdCameraControl {
    AbBirdCameraBehaviorHost *host;
    BirdCameraControl *replacement;
    size_t starts, cancels;
    bool fail;
};

static bool bird_camera_start(void *context, const char *name, AbError *error) {
    BirdCameraControl *test = context;
    ++test->starts;
    CHECK(!strcmp(name, "FollowShotBirdCameraSequence"));
    return !test->fail || ab_fail(error, "Reject camera start");
}

static bool bird_camera_cancel(void *context, AbError *error) {
    BirdCameraControl *test = context;
    ++test->cancels;
    if (test->fail)
        return ab_fail(error, "Reject camera cancellation");
    if (test->replacement)
        test->host->context = test->replacement;
    return true;
}

static bool bird_camera_behavior_checks(void) {
    BirdCameraControl first = {0}, second = {0};
    AbBirdCameraBehaviorHost host = {&first, bird_camera_start, bird_camera_cancel};
    first.host = &host;
    first.replacement = &second;
    CHECK(ab_bird_camera_blues_initialize(&host, NULL) && first.cancels == 1 &&
          !first.starts && second.starts == 1);
    CHECK(ab_bird_camera_on_launch(&host, NULL) && second.starts == 2);
    host.context = &first;
    first.fail = true;
    CHECK(!ab_bird_camera_blues_initialize(&host, NULL) && first.cancels == 2 &&
          !first.starts && second.starts == 2);
    host.start = NULL;
    CHECK(!ab_bird_camera_on_launch(&host, NULL) &&
          !ab_bird_camera_on_launch(NULL, NULL));
    host.cancel = NULL;
    CHECK(!ab_bird_camera_blues_initialize(&host, NULL) &&
          !ab_bird_camera_blues_initialize(NULL, NULL));
    return true;
}

#endif
