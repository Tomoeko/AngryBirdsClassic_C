#ifndef AB_UI_LEVEL_BUTTON_H
#define AB_UI_LEVEL_BUTTON_H

#include "ab/episode_statistics.h"
#include "ab/ui_scaling.h"
#include "ab/ui_text_widget.h"

typedef struct AbUiLevelButton AbUiLevelButton;

typedef struct {
    const AbDataTable *episodes;
    const AbEpisodeStatistics *statistics;
    AbUiScaling *scaling;
    AbUiFontResources *fonts;
    void *context;
    bool (*eagle_used_in)(void *context, AbDataValue free_power,
                          const AbDataTable **output, AbError *error);
    bool (*on_close)(void *context, AbUiLevelButton *button, AbError *error);
} AbUiLevelButtonHost;

struct AbUiLevelButton {
    AbUiWidget widget, stars, eagle, feather;
    AbUiTextWidget number;
    AbUiFrameNode *frames[5];
    AbUiDrawChild draw[5];
    AbUiPointerChild pointer[5];
    AbUiLevelButtonHost host;
    const AbDataTable *metadata, *episode_metadata, *page_metadata;
    AbDataValue episode;
    AbDataTable meta;
    AbDataEntry meta_fields[5];
    AbUiPointerValue has_video_button;
    /* Optional actual subclass child; add it to widget before assigning. */
    AbUiWidget *stars_empty;
    size_t page, index;
    char name[48];
    uint8_t number_text[32], star_image[48];
};

/* createLevelButton + createBasicLevelButton: actual ImageButton and four
 * ordered Text/Image children, source images, Scaling, name and event metadata.
 * Borrow an annotated catalog, fonts, live scores and reached settings services.
 * No placeholder/bonus/special-layout behavior is implied by this basic class.
 * Caller owns fresh stable storage and all borrowed lifetimes. No allocation,
 * resource loading or VM. A reached failure stops the shared widget context. */
bool ab_ui_level_button_init(AbUiLevelButton *button, AbUiWidgetContext *context,
                             const AbDataTable *level, AbUiLevelButtonHost host,
                             AbError *error);
bool ab_ui_level_button_open(AbUiLevelButton *button, AbError *error);
bool ab_ui_level_button_close(AbUiLevelButton *button, AbError *error);
/* Source LSGridLayout positions one existing level child, using the current
 * logical screen and the page's real rows/cols/index. Out-of-grid children are
 * left untouched, as in the source's bounded row/column traversal. */
bool ab_ui_level_button_grid_layout(AbUiLevelButton *button, bool arrows, AbVec2 margin,
                                    AbError *error);

#endif
