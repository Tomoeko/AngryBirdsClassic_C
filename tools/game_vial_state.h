#ifndef AB_GAME_VIAL_STATE_H
#define AB_GAME_VIAL_STATE_H

#include "ab/delayed_callbacks.h"

typedef struct PreviewVials PreviewVials;
typedef struct PreviewVialObject PreviewVialObject;

typedef enum {
    GAME_VIAL_ORANGE,
    GAME_VIAL_BLUE,
    GAME_VIAL_PINK,
    GAME_VIAL_EFFECTS
} PreviewVialKind;

typedef struct {
    PreviewVialObject *target;
    uint8_t name[288];
    size_t name_size;
    float time, duration, speed;
    PreviewVialKind kind;
    bool started;
} PreviewVialVisual;

typedef struct {
    PreviewVialObject *target;
    AbDataValue definition, vial_definition;
    AbDelayedFunction transition, inflate;
    float clone_width;
} PreviewVialRequest;

struct PreviewVialObject {
    PreviewVials *owner;
    size_t object, requests_used, promotion_count;
    PreviewVialRequest *requests;
    AbDataEntry *queue_entries;
    AbDataTable queue, puffer, puffed;
    AbDataEntry puffer_fields[10], puffed_fields[5];
    AbObjectUpdates owned_updates;
    AbObjectUpdateList promotions[4];
    const AbObjectUpdateFunction *functions[4][8];
    AbObjectUpdateFunction update, spin;
    AbObjectUpdateFunction floating;
    AbDataTable captured, remove_functions;
    AbDataEntry *captured_entries;
    size_t *captured_indices;
    AbDataEntry remove_entry;
    AbUiPointerValue removal_items[2];
    AbBlockRemovalFunctions removal_list;
    AbDataTable spin_center;
    AbDataEntry spin_fields[2];
    AbDataValue definition;
    AbVec2 particles_position;
    AbDelayedFunction particles;
    float glow_time, glow_rotation;
    bool glow_animate;
    PreviewVialVisual visuals[GAME_VIAL_EFFECTS];
};

struct PreviewVials {
    PreviewGame *game;
    PreviewVialObject *objects;
    PreviewVialRequest *requests;
    AbDataEntry *queue_entries;
    AbDataTable queue_snapshot;
    AbDataEntry *snapshot_entries;
    AbStringMap *vials, *effects, *goal_copy, *jointed, *capture_workspace;
    AbLuaAssetSpriteBinding sprites[GAME_VIAL_EFFECTS];
    AbExtraDrawFunction draw;
    size_t capacity, request_capacity;
    size_t captured_bytes;
};

#endif
