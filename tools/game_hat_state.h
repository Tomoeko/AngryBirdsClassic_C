#ifndef AB_GAME_HAT_STATE_H
#define AB_GAME_HAT_STATE_H

#include "ab/bird_hat.h"

typedef struct {
    AbBirdHat source;
    size_t bird, object;
    AbDataTable joint;
    AbDataEntry joint_fields[12];
    char selected[64];
} PreviewHat;

typedef struct {
    PreviewHat *records;
    size_t count, capacity, creating;
    AbDataValue counter;
} PreviewHats;

#endif
