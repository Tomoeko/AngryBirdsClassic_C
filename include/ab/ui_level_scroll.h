#ifndef AB_UI_LEVEL_SCROLL_H
#define AB_UI_LEVEL_SCROLL_H

#include "ab/event_timer.h"
#include "ab/level_progress.h"
#include "ab/ui_level_button.h"
#include "ab/ui_level_facebook.h"
#include "ab/ui_level_golden.h"
#include "ab/ui_scroll_frame.h"

/* Implementation storage bounds, checked against the prepared catalog; these
 * are not claims about original engine limits. */
#define AB_UI_LEVEL_SCROLL_BUTTONS 64
#define AB_UI_LEVEL_SCROLL_PAGES 8
#define AB_UI_LEVEL_SCROLL_CHILDREN                                                    \
    (AB_UI_LEVEL_SCROLL_BUTTONS * 2 + AB_UI_LEVEL_SCROLL_PAGES)

typedef struct {
    AbUiLevelButtonHost buttons;
    const AbLevelProgress *progress;
    AbUiScrollHost scroll;
    const AbDataTable *event_ids;
    AbVec2 margin;
    float initial_time;
    bool arrows;
    void *context;
    bool (*layout_root)(void *context, AbError *error);
    bool (*age_filter)(void *context, bool *output, AbError *error);
    AbSettingsStatistics *golden_eggs;
} AbUiLevelScrollHost;

typedef struct {
    AbUiWidget *parent, *image;
    AbUiTextWidget *text;
    AbVec2 position;
} AbUiLevelVisible;

typedef struct {
    AbUiScrollFrame scroll;
    AbUiWidgetMethods base_methods;
    AbUiLevelScrollHost host;
    AbUiLevelButton buttons[AB_UI_LEVEL_SCROLL_BUTTONS];
    AbUiLevelFacebook facebook[AB_UI_LEVEL_SCROLL_PAGES];
    AbUiLevelGolden golden;
    AbUiFrameNode *frames[AB_UI_LEVEL_SCROLL_CHILDREN];
    AbUiDrawChild draw[AB_UI_LEVEL_SCROLL_CHILDREN];
    AbUiPointerChild pointer[AB_UI_LEVEL_SCROLL_CHILDREN];
    AbVec2 anchors[AB_UI_LEVEL_SCROLL_PAGES];
    AbUiLevelVisible visible_images[AB_UI_LEVEL_SCROLL_BUTTONS * 5];
    AbUiLevelVisible visible_text[AB_UI_LEVEL_SCROLL_BUTTONS];
    size_t image_count, text_count, count, page_count, current_selection;
    const AbDataTable *pages;
    AbDataValue episode;
    AbEventTimer timer;
    AbEventListener listener;
    bool sync_subscribed, purchase_subscribed;
} AbUiLevelScroll;

/* Concrete LevelScrollFrame over real basic grid buttons and ScrollFrame2.
 * Preserve source child order, live stars/unlock refresh, sync subscriptions,
 * timer/update ordering, layout and visible-image/text cache projection. This
 * component supports prepared grid, Facebook and Golden Eggs pages without
 * placeholder/locked-page signs or accelerometer control. Other source layouts
 * reject explicitly. Facebook levels remain locally open, with live age-filtered
 * creation, visibility, art and row layout. Golden Eggs retain source unlock,
 * played/completed state, ordered halos/star children and rotation. Enclosing
 * root service actions and specialized screen factories remain host bindings.
 *
 * The outer LevelSelection assigns current_selection and the source per-page
 * anchors before layout. The default selection here is its ordinary first-page
 * component input. Resources, settings, event IDs and host state stay borrowed.
 * Caller owns fresh stable storage; detach before releasing it, including after
 * failure. No allocation, resource loading, VM or invented service responses. */
bool ab_ui_level_scroll_init(AbUiLevelScroll *owner, AbUiWidgetContext *context,
                             AbDataValue episode, AbUiLevelScrollHost host,
                             AbError *error);
bool ab_ui_level_scroll_refresh(AbUiLevelScroll *owner, AbError *error);
bool ab_ui_level_scroll_visible(AbUiLevelScroll *owner, AbError *error);
bool ab_ui_level_scroll_detach(AbUiLevelScroll *owner, AbError *error);

#endif
