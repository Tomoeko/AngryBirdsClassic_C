#ifndef AB_CLASSIC_LEVEL_PREPARE_H
#define AB_CLASSIC_LEVEL_PREPARE_H

#include "ab/classic_level_request.h"
#include "ab/classic_loader.h"
#include "ab/episode_preferences.h"
#include "ab/episode_statistics.h"
#include "ab/giftbox_session.h"

typedef struct AbClassicLevelPrepare AbClassicLevelPrepare;

typedef struct {
    AbDataValue current_level_number, current_level_number_in_theme;
    AbDataValue current_theme_number, current_world_number, current_page_number;
    AbDataValue level_folder, level_name, number_of_attempts, current_episode;
    const AbDataTable *level_started_table;
    AbDataValue current_level_started_count;
} AbClassicLevelPrepareState;

typedef struct {
    const AbDataTable *episodes, *event_ids;
    const AbEpisodeStatistics *statistics;
    AbSettingsStatistics *settings;
    AbEpisodePreferences *preferences;
    AbEventManager *events;
    AbClassicLevelPrepareState *state;
    bool *editing, *physics_enabled;
    const AbDataValue *giftbox_enabled;
    AbGiftBoxSession *giftbox;
    AbClassicLoaderCall *start_level;
    size_t requests, storage_bytes;
    void *context;
    bool (*golden_egg_played)(void *context, AbDataValue name, AbError *error);
    bool (*release_assets)(void *context, const AbDataTable *groups, AbError *error);
    bool (*before_load)(void *context, AbDataValue method, AbError *error);
    /* Original loadLevelInternal argument, e.g. /levels/pack1/Level1. This is
     * reread from live globals after before_load, without prefix or extension. */
    bool (*load_level_internal)(void *context, AbDataValue path, AbError *error);
    /* Optional diagnostic observation; no replacement for source game work. */
    bool (*log_load_time)(void *context, AbDataValue minimum, AbError *error);
} AbClassicLevelPrepareConfig;

/* Source prepareLevel28689, retained analytics helper28090 and Loader closure
 * 30379, for canonical page-list requests accepted by classic_level_request.
 * Bind actual settings/preferences/statistics, flags, the local GiftBox session
 * sharing these settings, and concrete reached services. Missing giftbox/egg/
 * resource/load work fails at its source boundary.
 * Open chooses no defaults and has no source effects. Own bounded distinct event,
 * callable and concatenation records once; ordinary calls allocate nothing.
 * Source constructor fields retain nil omission and typed false/zero values.
 *
 * Callback/source failures retain the completed mutation/event prefix and stop
 * this owner. Nested prepare/load dispatch rejects. Keep config/state/catalog,
 * callbacks and input metadata stable through their retained Loader calls.
 * Drain events/loaders before close; state.level_folder and retained notices
 * expire at close. This does not provide the selected ModeSystem/Museum factory,
 * startLevel services, bonus launch binding, global save scheduler or gameplay.
 */
bool ab_classic_level_prepare_open(AbClassicLevelPrepareConfig config,
                                   AbClassicLevelPrepare **output, AbError *error);
void ab_classic_level_prepare_close(AbClassicLevelPrepare *prepare);
bool ab_classic_level_prepare(AbClassicLevelPrepare *prepare,
                              const AbDataTable *metadata, AbDataValue from,
                              AbDataValue minimum_display_time, AbError *error);

/* Adapt only an actual retained EID_DO_LOADING table from this owner into the
 * already implemented C Loader request, preserving callable identities and raw
 * minimum time. No event dispatch, owner mutation or implicit defaults. */
bool ab_classic_level_prepare_loader(const AbClassicLevelPrepare *prepare,
                                     const AbDataTable *event,
                                     AbClassicLoaderRequest *output, AbError *error);

#endif
