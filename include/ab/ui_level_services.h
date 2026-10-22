#ifndef AB_UI_LEVEL_SERVICES_H
#define AB_UI_LEVEL_SERVICES_H

#include "ab/ui_level_button.h"

typedef struct {
    const AbEpisodeStatistics *statistics;
    const AbDataTable *local_settings;
    AbDataTable empty;
} AbUiLevelServices;

/* Bind the actual live statistics and settingsWrapper.getEagleUsedIn read.
 * Missing/false saved collections use this owner's immutable empty view;
 * the UI only iterates it and does not expose a script table identity.
 * Borrow services/settings for every derived button lifetime. The caller
 * supplies Scaling/fonts and any actual subclass on-close method. */
AbUiLevelButtonHost ab_ui_level_services_host(AbUiLevelServices *services);

#endif
