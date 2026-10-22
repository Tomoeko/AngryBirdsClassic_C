#ifndef AB_UI_EAGLE_SERVICES_H
#define AB_UI_EAGLE_SERVICES_H

#include "ab/classic_level_request.h"
#include "ab/episode_progress.h"
#include "ab/ui_mighty_eagle_button.h"

typedef struct {
    AbEpisodeProgress progress;
    const AbDataTable *settings;
    /* Actual getLevelById result for the current canonical name. NULL models
     * an actual absent result. A mismatched retained request rejects. */
    const AbClassicLevelRequest *level;
    AbSessionClock clock;
    void *context;
    bool (*powerup_transitioning)(void *context, AbDataValue *output, AbError *error);
    bool (*gamer_trial)(void *context, AbDataValue *output, AbError *error);
    bool (*marketplace_trial)(void *context, AbDataValue *output, AbError *error);
} AbUiEagleServices;

/* Actual ordinary common-game/settings Eagle queries. All tables are borrowed
 * canonical live state, including nils; no flags, unlocks, dates, SDK responses
 * or high scores are synthesized. Used table shapes are checked when reached.
 * Bind external real manager/trial methods only for present source objects;
 * reached missing methods fail. The original script time-of-day difference
 * applies; year/month and calendar normalization are unused. Dense used-in
 * arrays/string lengths and ordinary builtin methods
 * only. No allocation, persistence, VM, gameplay effects or retained outputs.
 * These callbacks directly fit AbUiMightyEagleHost. */
bool ab_ui_eagle_services_query(void *context, AbUiEagleQuery query,
                                AbUiPointerValue *output, AbError *error);
bool ab_ui_eagle_services_power(void *context, const AbDataTable **output,
                                AbError *error);
bool ab_ui_eagle_services_used(void *context, AbUiPointerValue free_enabled,
                               AbDataValue *output, AbError *error);
/* The same actual common-game trial query used by GameHud construction. */
bool ab_ui_eagle_services_trial(AbUiEagleServices *services, AbDataValue *output,
                                AbError *error);
/* Bind these real methods, clock and the very same canonical global graph into
 * a prepared host. Resource owners and clipping pools remain caller supplied.
 * The services record must stay at its stable address through the class lifetime. */
bool ab_ui_eagle_services_bind(AbUiEagleServices *services, AbUiMightyEagleHost *host,
                               AbError *error);

#endif
