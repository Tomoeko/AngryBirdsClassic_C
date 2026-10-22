#ifndef AB_TEST_CLASSIC_CAMERA_CASES_H
#define AB_TEST_CLASSIC_CAMERA_CASES_H

#include "world_raycast_cases.h"
#include "camera_follow_system_cases.h"

enum { CLASSIC_CAMERA_BODIES = 3 };

/* Explicit cache publications after the launch handler and before each camera
 * update. Preserve the first two absent-launch frames in the selected case.
 * These are comparison inputs; the physics world remains frozen. */
static inline void classic_camera_frame_values(const uint32_t config[24], uint32_t step,
                                               uint32_t role, uint32_t output[5]) {
    uint32_t first = role ? 19u : 9u;
    output[0] = world_ray_word(owner_follow_number(config[first]) +
                               (float)(step % 7u) * 0.125f);
    output[1] = world_ray_word(owner_follow_number(config[first + 1u]) -
                               (float)(step % 5u) * 0.0625f);
    output[2] = world_ray_word(owner_follow_number(config[first + 2u]) +
                               (float)(step / 13u % 3u) * 0.125f);
    output[3] = world_ray_word(owner_follow_number(config[first + 3u]) -
                               (float)(step / 17u % 2u) * 0.25f);
    output[4] = role != 0 || !(config[18] & 2u) || step >= 2;
}

/* Logical roles bird1, bird2, ground. Actual constructor traversal selects the
 * first two roles; ground is appended last. This frozen world is independent
 * of the explicit cached camera poses/launch values in each scene. */
static void classic_camera_body(uint32_t role, uint32_t output[13]) {
    for (uint32_t index = 0; index < WORLD_RAY_BODY_WORDS; ++index)
        output[index] = 0;
    output[0] = role + 1u;
    output[1] = role < 2 ? 2u : 0u;
    output[2] = world_ray_word(role == 0 ? 1.0f : role == 1 ? 5.0f : 0.0f);
    output[3] = world_ray_word(role == 0 ? 2.0f : role == 1 ? 3.0f : 10.0f);
    output[5] = 1;
    output[6] = role == 2 ? 2u : 0u;
    output[7] = world_ray_word(role == 2 ? 0.1f : 0.5f);
    output[8] = world_ray_word(role == 2 ? 1000.0f : 0.0f);
    output[9] = world_ray_word(role == 2 ? 5.0f : 0.0f);
    output[11] = 1;
    output[12] = 65535;
}

#endif
