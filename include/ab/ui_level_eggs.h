#ifndef AB_UI_LEVEL_EGGS_H
#define AB_UI_LEVEL_EGGS_H

#include "ab/ui_level_selection.h"

typedef struct {
    AbSettingsStatistics *highscores;
    const AbEpisodeStatistics *statistics;
    void *context;
    bool (*assets)(void *context, bool loading, AbError *error);
    bool king_pig_enabled, release_build;
} AbUiLevelEggsHost;

typedef struct {
    AbUiLevelSelection *levels;
    AbUiWidgetMethods inherited;
    AbUiLevelEggsHost host;
    AbUiWidget king_pig;
    bool initialized;
} AbUiLevelEggs;

/* GoldenEggsLevelSelection308/752/1548/1711 over an unentered actual base.
 * Own source resource lifecycle, second-page unlock/anchor/dot rules and the
 * optional King Pig button with its seven-episode gold-score gate. The host
 * supplies actual build/feature inputs and LEVELSELECTION_GOLDEN_EGGS loading.
 * Pointer actions retain the enclosing root's source return values; no King
 * Pig gameplay or service response is implied. Borrow host/base until detach. */
bool ab_ui_level_eggs_attach(AbUiLevelEggs *owner, AbUiLevelSelection *levels,
                             AbUiLevelEggsHost host, AbError *error);

#endif
