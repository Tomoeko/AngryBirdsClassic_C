#ifndef AB_UI_LEVEL_SELECTION_H
#define AB_UI_LEVEL_SELECTION_H

#include "ab/episode_preferences.h"
#include "ab/episode_progress.h"
#include "ab/ui_level_scroll.h"

#define AB_UI_LEVEL_SELECTION_CHILDREN (AB_UI_LEVEL_SCROLL_PAGES * 2 + 6)

typedef struct {
    AbUiLevelScrollHost levels;
    AbEpisodePreferences *preferences;
    const AbEpisodeProgress *episode_progress;
    AbDataValue exit_link;
    const float *script_time;
    bool interpolate_background, trial_version, free_version;
    /* Empty inherited methods select Frame for an isolated component view.
     * Complete MainMenuRoot/LevelSelectionRoot services remain a host binding. */
    AbUiWidgetMethods root;
    void *context;
    bool (*query_enter_level_selection)(void *context, AbError *error);
} AbUiLevelSelectionHost;

typedef struct {
    AbUiWidget widget, left_decoration, right_decoration, back, left, right;
    AbUiWidget dots[AB_UI_LEVEL_SCROLL_PAGES];
    AbUiTextWidget page_numbers[AB_UI_LEVEL_SCROLL_PAGES];
    AbUiLevelScroll levels;
    AbUiFrameNode *frames[AB_UI_LEVEL_SELECTION_CHILDREN];
    AbUiDrawChild draw[AB_UI_LEVEL_SELECTION_CHILDREN];
    AbUiPointerChild pointer[AB_UI_LEVEL_SELECTION_CHILDREN];
    AbUiLevelSelectionHost host;
    AbEventListener listener;
    AbDataValue episode, exit_link;
    const AbDataTable *metadata;
    size_t current_selection;
    float background[4], next_page_timestamp;
    char name[80], dot_names[AB_UI_LEVEL_SCROLL_PAGES][24],
        page_names[AB_UI_LEVEL_SCROLL_PAGES][24];
    AbDataTable back_event, launch_event, registration;
    AbDataEntry back_fields[3], launch_fields[4], registration_fields[3];
    bool scroll_to_next_page, scroll_subscribed, eagle_subscribed;
} AbUiLevelSelection;

/* Concrete LevelSelection over the prepared basic grid profile. Own source
 * child order, decorations, page dots/text, Back/arrows, saved/visited selection,
 * background-color interpolation, deferred page scrolling and canonical launch
 * events. Borrow resources/preferences/services for this owner's lifetime.
 * Source inherited root behavior and the age/gender placement are explicit host
 * integrations; omitted bindings describe an isolated component, not bootstrap
 * or successful SDK execution. Bonus/special/locked layouts and reward-video
 * dialogs are outside this component profile and reject when reached.
 *
 * Launch events retain this owner's tables and the actual button-meta identity.
 * Consumers finish/copy them before another launch or owner release. No frame
 * allocation, resource loading or VM. Detach before releasing caller storage. */
bool ab_ui_level_selection_init(AbUiLevelSelection *owner, AbUiWidgetContext *context,
                                AbDataValue episode, AbUiLevelSelectionHost host,
                                AbError *error);
bool ab_ui_level_selection_update_selection(AbUiLevelSelection *owner, bool force,
                                            AbError *error);
bool ab_ui_level_selection_detach(AbUiLevelSelection *owner, AbError *error);
/* Common gamelogic drawLevelSelectionBackground94236, called by the inherited
 * LevelSelectionRoot before MainMenuRoot.draw for its high-quality abc profile.
 * Preserve the two mirrored ceil/floor halves and seven-argument state resets.
 * The free-version early return touches no render state. Resources are borrowed. */
bool ab_ui_level_selection_background(AbUiWidgetContext *context, AbVec2 screen,
                                      bool free_version, AbError *error);

#endif
