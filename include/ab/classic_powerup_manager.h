#ifndef AB_CLASSIC_POWERUP_MANAGER_H
#define AB_CLASSIC_POWERUP_MANAGER_H

#include "ab/classic_game_powerups.h"
#include "ab/event_manager.h"
#include "ab/powerup_catalog.h"
#include "ab/powerup_uses.h"
#include "ab/settings_inventory.h"

enum { AB_CLASSIC_POWERUPS = 6, AB_CLASSIC_POWERUP_NOTICES = 128 };

typedef struct {
    AbDataTable table;
    AbDataEntry fields[6];
} AbClassicPowerupNotice;

typedef struct {
    const AbPowerupCatalog *catalog;
    AbSettingsStatistics *settings;
    AbEventManager *events;
    AbPowerupUses *uses;
    AbClassicSling *sling;
    const AbDataValue *flying, *specialty_available, *level_name;
    bool infinite;
    void *context;
    bool (*activate)(void *context, size_t powerup, bool *result, AbError *error);
    AbClassicPowerupUpdate *instances[AB_CLASSIC_POWERUPS];
    bool (*reset[AB_CLASSIC_POWERUPS])(void *context, AbError *error);
    AbClassicPowerupUpdates *active, *passive;
} AbClassicPowerupManagerConfig;

typedef struct {
    AbClassicPowerupManagerConfig config;
    AbClassicPowerupUpdate *active[AB_CLASSIC_POWERUPS + 1];
    AbClassicPowerupUpdate *passive[AB_CLASSIC_POWERUPS + 1];
    bool infinite[AB_CLASSIC_POWERUPS], enabled[AB_CLASSIC_POWERUPS];
    AbClassicPowerupNotice notices[AB_CLASSIC_POWERUP_NOTICES];
    size_t notice_count;
    bool initialized, calling, failed;
} AbClassicPowerupManager;

/* Ordinary Android registration state, source activation gates, inventory
 * consumption, use records and retained events. Borrow catalog/settings and
 * canonical game fields through the session. Each reached effect must have a
 * real initialized instance and activation callback. The caller supplies its
 * concrete effects; this owner supplies no effect or purchase substitutes.
 * The explicit infinite option uses the source makeInfinite state and leaves
 * saved balances unchanged. No frame allocation, resource I/O or VM. */
bool ab_classic_powerup_manager_initialize(AbClassicPowerupManagerConfig config,
                                           AbClassicPowerupManager *output,
                                           AbError *error);
bool ab_classic_powerup_manager_can_activate(AbClassicPowerupManager *manager,
                                             size_t powerup, bool *output,
                                             AbError *error);
bool ab_classic_powerup_manager_activate(AbClassicPowerupManager *manager,
                                         size_t powerup, AbDataValue from, bool *output,
                                         AbError *error);
bool ab_classic_powerup_manager_deactivate(AbClassicPowerupManager *manager,
                                           size_t powerup, bool passive,
                                           AbError *error);
/* isPowerupInUse: membership in either source update list. */
bool ab_classic_powerup_manager_in_use(AbClassicPowerupManager *manager, size_t powerup,
                                       bool *output, AbError *error);

#endif
