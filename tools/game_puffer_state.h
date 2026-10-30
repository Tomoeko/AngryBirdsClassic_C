#ifndef AB_GAME_PUFFER_STATE_H
#define AB_GAME_PUFFER_STATE_H

typedef struct PreviewPuffers PreviewPuffers;

typedef enum {
    GAME_PUFFER_DELAY,
    GAME_PUFFER_INFLATED,
    GAME_PUFFER_DEFLATING,
    GAME_PUFFER_CALLBACKS
} PreviewPufferPhase;

typedef struct {
    PreviewPuffers *owner;
    size_t object, promotion_count;
    AbObjectUpdates owned_updates;
    AbObjectUpdateList promotions[4];
    const AbObjectUpdateFunction *functions[4][8];
    AbObjectUpdateFunction callbacks[GAME_PUFFER_CALLBACKS];
} PreviewPufferObject;

struct PreviewPuffers {
    PreviewGame *game;
    PreviewPufferObject *objects;
    size_t capacity;
};

#endif
