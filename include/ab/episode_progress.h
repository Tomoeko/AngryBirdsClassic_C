#ifndef AB_EPISODE_PROGRESS_H
#define AB_EPISODE_PROGRESS_H

#include "ab/lua_data.h"

typedef struct {
    const AbDataTable *episodes, *local_settings, *globals, *purchases;
} AbEpisodeProgress;

/* Read live episode metadata, localSettings, purchase and source feature state.
 * These are borrowed real tables; callers restore saves and initialize source
 * globals before use. No generated unlocks, SDK replies, persistence or VM.
 * Settings keys use the target's episode-prefix convention. Numeric identifiers
 * are the catalog's positive integral IDs; string IDs remain byte-exact. Used
 * page arrays are dense. Outputs commit on success, including source nil. */
bool ab_episode_progress_has_new_levels(const AbEpisodeProgress *progress,
                                        AbDataValue episode, bool *output,
                                        AbError *error);
bool ab_episode_progress_new_page(const AbEpisodeProgress *progress,
                                  AbDataValue episode, AbDataValue *output,
                                  AbError *error);
bool ab_episode_progress_page_visited(const AbEpisodeProgress *progress,
                                      AbDataValue episode, AbDataValue page,
                                      bool *output, AbError *error);
bool ab_episode_progress_change_seen(const AbEpisodeProgress *progress,
                                     AbDataValue episode, bool *output, AbError *error);
bool ab_episode_progress_show_eagle(const AbEpisodeProgress *progress, bool *output,
                                    AbError *error);
/* Shared isFreeEagleEnabled query over the same live status/episode globals. */
bool ab_episode_progress_free_eagle(const AbEpisodeProgress *progress, bool *output,
                                    AbError *error);
/* Original trial gate: episode 1 and G always open. Registration uses actual
 * flags; reached gamerService/marketplace methods require further integration. */
bool ab_episode_progress_open(const AbEpisodeProgress *progress, AbDataValue episode,
                              bool *output, AbError *error);

#endif
