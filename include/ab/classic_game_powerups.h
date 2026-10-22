#ifndef AB_CLASSIC_GAME_POWERUPS_H
#define AB_CLASSIC_GAME_POWERUPS_H

#include "ab/classic_sling.h"
#include "ab/draw.h"

typedef struct {
    void *context;
    bool (*update)(void *context, float seconds, AbError *error);
    /* Resolved optional draw field. NULL denotes original nil, not false. */
    bool (*draw)(void *context, float zoom, AbDrawList *list, AbError *error);
} AbClassicPowerupUpdate;

typedef struct {
    AbClassicPowerupUpdate **items;
    size_t capacity;
} AbClassicPowerupUpdates;

typedef struct {
    AbClassicSling *sling;
    AbClassicPowerupUpdates *active, *passive;
    const AbDataValue *enabled, *cheats_enabled;
    const AbDataTable *released_keys;
    const float *physics_speed_factor;
    AbDataValue *skip_input;
    float *game_timer;
    void *context;
    /* Resolved actual manager activation, required only by reached cheat keys. */
    bool (*activate)(void *context, const char *name, AbError *error);
} AbClassicGamePowerupConfig;

typedef struct {
    AbClassicGamePowerupConfig config;
    bool initialized, updating, failed;
} AbClassicGamePowerups;

/* Concrete GameUpdate110400 instructions361..380 over PowerupManager update5418
 * and isTransitioning8459. Borrow canonical flags/timer, the default actual
 * sling and real active/passive dense numeric lists. Lists end at first NULL;
 * unused trailing slots are NULL. A callback may remove/shift/append within
 * capacity or replace a later list; keep captured arrays/records alive. Sparse
 * or mixed-key lists, replaced methods, foreign slings and dynamic classes are
 * unsupported. Empty must be the actual resolved list, not a missing service.
 * Construction/registration/reset/activation/purchase policy remain separate.
 * Initialize has no effects, allocation or default state assignments.
 */
bool ab_classic_game_powerups_initialize(AbClassicGamePowerupConfig config,
                                         AbClassicGamePowerups *output, AbError *error);
/* Active transition plus positive speed sets skipInput before manager update.
 * Cheat1 queries actual current-index bird; other selected cheat keys require
 * activation directly. Nonempty active list cancels drag before its callbacks;
 * passive callbacks follow. Then gameTimer advances even if powerups disabled.
 * Only reached numbers need be finite. No frame allocation/implicit clock.
 * Reentry rejects. Late failure retains prior effects and stops further calls.
 */
bool ab_classic_game_powerups_update(AbClassicGamePowerups *powerups, float seconds,
                                     AbError *error);
/* Actual isTransitioning8459 over this same live active list. Read-only queries
 * are permitted within update callbacks; missing/sparse lists and stopped
 * owners reject. Direct AbUiEagleServices powerup_transitioning callback. */
bool ab_classic_game_powerups_transitioning(void *context, AbDataValue *output,
                                            AbError *error);
/* Actual PowerupManager.draw6202: passive list first, then active, in dense
 * numeric pairs order. Each nonnil draw field is a bound callable; nil skips.
 * No self argument is passed to the original function, only captured zoom.
 * Borrow the same real instance lists as update. Missing optional methods are
 * not synthesized. Prior callbacks/commands survive failure; stop the owner. */
bool ab_classic_game_powerups_draw(AbClassicGamePowerups *powerups, float zoom,
                                   AbDrawList *list, AbError *error);

#endif
