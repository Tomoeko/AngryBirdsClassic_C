#ifndef AB_OBJECT_ANIMATION_CASES_H
#define AB_OBJECT_ANIMATION_CASES_H

#include <stddef.h>
#include <stdint.h>

enum { AB_OBJECT_ANIMATION_CASES = 256, AB_OBJECT_ANIMATION_STEPS = 8 };

typedef struct {
    float duration, timer, frame, seconds[AB_OBJECT_ANIMATION_STEPS];
    unsigned duration_kind, timer_present, frames, false_frame;
    unsigned looping, finished, disabled, active;
} AbObjectAnimationCase;

static AbObjectAnimationCase ab_object_animation_case(size_t index) {
    static const float durations[] = {0, 0.125f, 0.5f, -0.25f};
    static const float seconds[] = {0, 0.03125f, 0.5f, 2, -0.125f, 0.125f, 4, 0};
    AbObjectAnimationCase value = {.duration = durations[index % 4],
                                   .timer = (float)((int)(index % 9) - 4) * 0.125f,
                                   .frame = (float)(1 + index % 7),
                                   .duration_kind = (unsigned)(index / 4) % 3,
                                   .timer_present = (unsigned)(index / 3) % 2,
                                   .frames = (unsigned)(index / 7) % 8,
                                   .false_frame = (unsigned)(index / 5) % 9,
                                   .looping = (unsigned)(index / 11) % 2,
                                   .finished = index % 23 == 0,
                                   .disabled = index % 19 == 0,
                                   .active = index % 17 != 0};
    for (size_t step = 0; step < AB_OBJECT_ANIMATION_STEPS; ++step)
        value.seconds[step] = seconds[(step + index) % AB_OBJECT_ANIMATION_STEPS];
    return value;
}

#endif
