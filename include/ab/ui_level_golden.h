#ifndef AB_UI_LEVEL_GOLDEN_H
#define AB_UI_LEVEL_GOLDEN_H

#include "ab/settings_golden_eggs.h"
#include "ab/ui_level_button.h"

#define AB_UI_LEVEL_GOLDEN_LIMIT 64

typedef struct {
    AbUiWidget *scroll;
    AbSettingsStatistics *highscores;
    AbUiWidget halos[AB_UI_LEVEL_GOLDEN_LIMIT];
    const AbDataTable *levels[AB_UI_LEVEL_GOLDEN_LIMIT];
    char names[AB_UI_LEVEL_GOLDEN_LIMIT][32];
    size_t count;
} AbUiLevelGolden;

/* LSGoldenEggsLayout11169/11489/12280/12400/12786/13050/13670. The scroll owns
 * this bounded storage. Create each page's halo children before its actual
 * base ImageButtons, each with one carved-star child. Borrow annotated metadata
 * and the live highscore owner; original nil-only progress queries can create
 * its rows. Preserve unlock/played/completed visibility and source placement,
 * scaling and halo rotation. No resources, persistence, VM or service responses.
 * GoldenEggsLevelSelection's assets, anchors and King Pig control are separate. */
bool ab_ui_level_golden_page(AbUiLevelGolden *owner, AbUiWidget *scroll,
                             const AbDataTable *page, AbSettingsStatistics *highscores,
                             AbError *error);
bool ab_ui_level_golden_button(AbUiLevelButton *button, AbUiWidgetContext *context,
                               const AbDataTable *level, AbUiLevelButtonHost host,
                               AbError *error);
bool ab_ui_level_golden_refresh(AbUiLevelGolden *owner, AbUiLevelButton *buttons,
                                size_t count, AbError *error);
bool ab_ui_level_golden_layout(AbUiLevelGolden *owner, AbUiLevelButton *buttons,
                               size_t count, bool arrows, AbVec2 margin,
                               AbError *error);
bool ab_ui_level_golden_update(AbUiLevelGolden *owner, float seconds, AbError *error);

#endif
