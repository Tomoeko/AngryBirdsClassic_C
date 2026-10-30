#ifndef AB_GAME_SHOCKWAVE_STATE_H
#define AB_GAME_SHOCKWAVE_STATE_H

#include "ab/electric_shockwave.h"
#include "ab/game_sprite.h"
#include "ab/ui_value_text.h"

typedef struct PreviewShockwaves PreviewShockwaves;

typedef struct {
    AbDataTable table;
    AbDataEntry fields[6];
} PreviewElectrocute;

typedef struct {
    PreviewShockwaves *owner;
    size_t object;
    AbBirdShockwave wave;
    AbElectricShockwave electric;
    AbVec2 origin;
    float visual_scale, bird_scale, bird_angle;
    char draw_name[256];
    AbExtraDrawFunction draw;
    AbDataTable event;
    AbDataTable *electro;
    AbDataEntry event_id;
    AbObjectUpdates owned_updates;
    AbObjectUpdateList promotions[4];
    const AbObjectUpdateFunction *functions[4][8];
    size_t promotion_count;
    AbObjectUpdateFunction invulnerability, electrocute, delayed;
    AbObjectUpdateFunction electricity_update;
    AbExtraDrawFunction electricity_draw;
    AbLuaAssetSpriteBinding electricity_sprites[2][16];
    AbDataValue electricity_sets[2];
    size_t electricity_counts[2];
    char electricity_draw_name[256];
    AbDataTable zero_offset;
    AbDataEntry zero_fields[2];
    bool used, electro_present;
} PreviewShockObject;

struct PreviewShockwaves {
    PreviewGame *game;
    PreviewShockObject *objects;
    PreviewElectrocute *electros;
    size_t capacity;
    AbStringMap *waves, *electrocuted, *dynamic;
    AbLuaAssetSpriteBinding sprites[AB_ELECTRIC_SPRITES];
    AbExtraDrawFunction pig_draw;
    AbClassicGameUpdateFunction update;
    AbDataTable overlay;
    AbDataEntry overlay_fields[4];
};

#endif
