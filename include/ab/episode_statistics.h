#ifndef AB_EPISODE_STATISTICS_H
#define AB_EPISODE_STATISTICS_H

#include "ab/lua_data.h"

typedef struct {
    const AbDataTable *episodes, *episode_ids, *highscores, *star_table;
} AbEpisodeStatistics;

/* Recovered Statistics queries over the actual catalog, live highscore graph
 * and starLimits.lua records. Tables remain borrowed and stable during a query.
 * Arrays are dense and bounded; used score arithmetic is finite binary32.
 * Includes the source eggDefenderStarCountLogic override, without a script VM,
 * callbacks, allocation, invented scores or persistence effects. */
bool ab_episode_statistics_level_stars(const AbEpisodeStatistics *statistics,
                                       AbDataValue episode, size_t page,
                                       const AbDataTable *level, float *output,
                                       AbError *error);
bool ab_episode_statistics_stars(const AbEpisodeStatistics *statistics,
                                 AbDataValue episode, bool golden_three, float *earned,
                                 float *available, AbError *error);
bool ab_episode_statistics_score(const AbEpisodeStatistics *statistics,
                                 AbDataValue episode, float *output, AbError *error);
bool ab_episode_statistics_eagle_score(const AbEpisodeStatistics *statistics,
                                       AbSpan name, float *output, AbError *error);
bool ab_episode_statistics_feathers(const AbEpisodeStatistics *statistics,
                                    AbDataValue episode, float *earned,
                                    float *available, AbError *error);
/* Stop at the first source episode_end marker. Completion requires every level
 * through that marker; an episode with no marker is not complete. */
bool ab_episode_statistics_complete(const AbEpisodeStatistics *statistics,
                                    AbDataValue episode, bool *output, AbError *error);

/* Main-menu rewards use level_utils.isEpisodeCompleted: visit every page and
 * level, including those after episode_end. An empty episode is complete. */
bool ab_episode_statistics_all_complete(const AbEpisodeStatistics *statistics,
                                        const AbDataTable *episode, bool *output,
                                        AbError *error);
/* Sum selection episodes followed by g_additionalFeatherEpisodes. */
bool ab_episode_statistics_all_feathers(const AbEpisodeStatistics *statistics,
                                        const AbDataTable *additional_episodes,
                                        float *earned, float *available,
                                        AbError *error);

#endif
