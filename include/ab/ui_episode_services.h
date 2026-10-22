#ifndef AB_UI_EPISODE_SERVICES_H
#define AB_UI_EPISODE_SERVICES_H

#include "ab/episode_progress.h"
#include "ab/episode_statistics.h"
#include "ab/ui_episode_button.h"

typedef struct {
    const AbEpisodeStatistics *statistics;
    const AbEpisodeProgress *progress;
} AbUiEpisodeServices;

/* Bind the concrete C queries to the card's reached source calls. Borrow live
 * service state and its tables for the entire widget lifetime. The owner still
 * supplies fonts, Scaling, RNG and the resolved platform profile. */
AbUiEpisodeButtonHost ab_ui_episode_services_host(AbUiEpisodeServices *services);

#endif
