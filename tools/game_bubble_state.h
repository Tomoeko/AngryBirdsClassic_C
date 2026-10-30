#ifndef AB_GAME_BUBBLE_STATE_H
#define AB_GAME_BUBBLE_STATE_H

#include "ab/bird_bubbles.h"

typedef struct PreviewBubbles PreviewBubbles;

typedef struct {
    PreviewBubbles *owner;
    size_t object, promotions_used;
    AbObjectUpdates owned_updates;
    AbObjectUpdateList promotions[4];
    const AbObjectUpdateFunction *functions[4][8];
    AbObjectUpdateFunction update;
} PreviewBubbleObject;

struct PreviewBubbles {
    PreviewGame *game;
    PreviewBubbleObject *objects;
    size_t capacity;
    float charge_duration;
    AbLuaAssetSpriteBinding sprites[3];
    AbExtraDrawFunction draw;
};

#endif
