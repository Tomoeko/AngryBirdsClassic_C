#ifndef AB_EPISODE_PREFERENCES_H
#define AB_EPISODE_PREFERENCES_H

#include "ab/settings_statistics.h"

#define AB_EPISODE_PREFERENCES_CAPACITY 32

typedef struct {
    AbDataValue episode;
    AbSpan episode_key, pack_key;
    uint8_t bytes[128];
} AbEpisodePreferenceKey;

typedef struct {
    AbSettingsStatistics *storage;
    const AbDataTable *episodes;
    AbEpisodePreferenceKey keys[AB_EPISODE_PREFERENCES_CAPACITY];
    size_t count;
    bool initialized;
} AbEpisodePreferences;

/* Concrete SettingsWrapper episode/page mutations over the actual owned local
 * settings graph. Bind a fresh stable owner and the real catalog once. Prefix
 * keys stay owned here until storage closes; catalog strings remain borrowed.
 * The caller opens storage with enough tables/entries, restores saved values
 * explicitly, and keeps all owners alive. No allocation after initialization,
 * file I/O, save scheduling, events, automatic unlocks or VM.
 *
 * Episode arguments are catalog keys. Selected episode retains raw scalar
 * values; reached page/version arithmetic uses the target's numeric records.
 * Visited pages use dense scalar arrays. Source nil-only versus false-or-empty
 * creation, locked/restoration gates, duplicate visits and monotone version
 * marks are preserved. Capacity failures retain preceding storage effects. */
bool ab_episode_preferences_init(AbEpisodePreferences *preferences,
                                 AbSettingsStatistics *storage,
                                 const AbDataTable *episodes, AbError *error);
bool ab_episode_preferences_selected(const AbEpisodePreferences *preferences,
                                     AbDataValue *output, AbError *error);
bool ab_episode_preferences_select(AbEpisodePreferences *preferences,
                                   AbDataValue episode, AbError *error);
bool ab_episode_preferences_current_page(AbEpisodePreferences *preferences,
                                         AbDataValue episode, AbDataValue *output,
                                         AbError *error);
bool ab_episode_preferences_set_page(AbEpisodePreferences *preferences,
                                     AbDataValue episode, AbDataValue page,
                                     AbError *error);
bool ab_episode_preferences_visit_page(AbEpisodePreferences *preferences,
                                       AbDataValue episode, AbDataValue page,
                                       AbError *error);
bool ab_episode_preferences_see_change(AbEpisodePreferences *preferences,
                                       AbDataValue episode, AbError *error);

#endif
