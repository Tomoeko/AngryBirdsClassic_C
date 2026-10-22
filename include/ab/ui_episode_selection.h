#ifndef AB_UI_EPISODE_SELECTION_H
#define AB_UI_EPISODE_SELECTION_H

#include "ab/classic_loader.h"
#include "ab/episode_preferences.h"
#include "ab/ui_episode_scroll.h"

typedef struct {
    AbUiEpisodeScrollHost cards;
    AbEpisodePreferences *preferences;
    const AbDataTable *event_ids;
    /* Real inherited LevelSelectionRoot methods. Empty methods select Frame
     * for an explicitly isolated component owner, not a complete game menu. */
    AbUiWidgetMethods root;
    void *context;
    bool (*load_theme)(void *context, AbSpan theme, AbError *error);
    /* Actual settings setter/save and getAudioName("piglette") boundary. */
    bool (*open_gift)(void *context, AbUiPointerValue *sound, AbError *error);
} AbUiEpisodeSelectionHost;

typedef struct {
    AbUiWidget widget;
    AbUiEpisodeScroll cards;
    AbUiWidget left_decoration, right_decoration, back, left, right;
    AbUiFrameNode *frames[6];
    AbUiDrawChild draw[6];
    AbUiPointerChild pointer[6];
    AbUiEpisodeSelectionHost host;
    AbEventListener listener;
    AbClassicLoaderCall load;
    AbClassicLoaderCall *calls[2];
    AbClassicLoaderCalls loading_calls;
    AbDataTable loading_event, loading_items, completion, back_event, registration;
    AbDataEntry loading_fields[3], item, completion_fields[3], back_fields[3],
        registration_fields[3];
    AbDataValue selected_episode;
    uint8_t target[96];
    size_t current_selection;
    bool arrows, scroll_subscribed;
} AbUiEpisodeSelection;

/* Source EpisodeSelection controls around the actual 14-card scroller: child
 * order, decorations, back/arrows, selection restoration, layout and pointer
 * dispatch. Bind the source new-layout, no-browser/no-accelerometer profile.
 * Settings, resources, fonts, scaling and events stay borrowed. All storage is
 * stable and caller-owned; no allocation or resource I/O during a frame.
 *
 * EID_DO_LOADING carries a dense items table whose AB_DATA_OBJECT is the actual
 * AbClassicLoaderCall; the receiver binds it to the concrete C Loader. Completion
 * and registration events retain their source table shape. A request may not be
 * overwritten until its loading/registration consumer has finished. The owner
 * must enforce the Loader's shared skip-input gate. Service placements, inherited
 * root rendering, reward-video handling and platform profiles remain explicit
 * outer integrations, not supplied defaults. Detach before releasing storage. */
bool ab_ui_episode_selection_init(AbUiEpisodeSelection *owner,
                                  AbUiWidgetContext *context,
                                  AbUiEpisodeSelectionHost host, AbError *error);
bool ab_ui_episode_selection_update_selection(AbUiEpisodeSelection *owner, bool force,
                                              AbError *error);
bool ab_ui_episode_selection_detach(AbUiEpisodeSelection *owner, AbError *error);

#endif
