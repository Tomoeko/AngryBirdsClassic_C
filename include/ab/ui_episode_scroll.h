#ifndef AB_UI_EPISODE_SCROLL_H
#define AB_UI_EPISODE_SCROLL_H

#include "ab/episode_progress.h"
#include "ab/ui_episode_button.h"
#include "ab/ui_scroll_frame.h"

#define AB_UI_EPISODE_SCROLL_CAPACITY 18

typedef struct {
    AbUiEpisodeButtonHost buttons;
    AbUiScrollHost scroll;
    const AbDataTable *episode_ids, *globals;
    const AbEpisodeProgress *progress;
    AbDataValue extras_changed, free_version_unlocked;
} AbUiEpisodeScrollHost;

typedef struct {
    AbUiScrollFrame scroll;
    AbUiWidgetMethods base_methods;
    AbUiEpisodeScrollHost host;
    AbUiEpisodeButton buttons[AB_UI_EPISODE_SCROLL_CAPACITY];
    AbUiFrameNode *frames[AB_UI_EPISODE_SCROLL_CAPACITY];
    AbUiDrawChild draw[AB_UI_EPISODE_SCROLL_CAPACITY];
    AbUiPointerChild pointer[AB_UI_EPISODE_SCROLL_CAPACITY];
    AbVec2 anchors[AB_UI_EPISODE_SCROLL_CAPACITY];
    uint8_t names[AB_UI_EPISODE_SCROLL_CAPACITY][64];
    AbEventListener listener;
    AbVec2 button_dimensions;
    size_t count;
    AbDataValue scroll_to_episode;
    bool autoscroll_on_entry;
    bool extras_subscribed, free_subscribed;
} AbUiEpisodeScroll;

/* Own the real catalog cards in source dictionary child order, forward/reverse
 * slots, source dimensions/layout/clipping/anchors and open-state updates.
 * Bind actual C card classes and the concrete ScrollFrame, including its three
 * draw phases, input, shared event manager and motion. All resource/state hosts
 * remain borrowed and live. No allocation, VM or frame resource work.
 *
 * This profile supports the APK's new layout with no browser extra pages and
 * no accelerometer. Reached unsupported profiles reject explicitly. The outer
 * EpisodeSelection root still owns decorations, navigation, saved selection
 * and service placements. Set scroll.current_anchor from its saved selection.
 * Detach listeners before releasing this stable owner, including failed init. */
bool ab_ui_episode_scroll_init(AbUiEpisodeScroll *owner, AbUiWidgetContext *context,
                               const AbUiScrollArguments *arguments,
                               AbUiEpisodeScrollHost host, AbError *error);
bool ab_ui_episode_scroll_detach(AbUiEpisodeScroll *owner, AbError *error);

#endif
