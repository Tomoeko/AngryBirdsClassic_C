#ifndef AB_UI_PAUSE_PAGE_H
#define AB_UI_PAUSE_PAGE_H

#include "ab/audio_bank.h"
#include "ab/classic_audio_ramp.h"
#include "ab/event_timer.h"
#include "ab/ui_scaling.h"
#include "ab/ui_text_clipping.h"
#include "ab/ui_toggle_button.h"

typedef struct {
    bool shop_demo, mattel_active, channel_enabled, channel_available;
    bool powerups_enabled, purchase_elements, hide_powerup_button, trial;
    bool childlock_on, use_scaling, no_audio_on_pause;
    float width_ratio;
    AbUiPointerValue show_checkpoint;
    AbSpan device_model;
    AbUiPointerValue menu_font;
} AbUiPausePageEnvironment;

typedef struct {
    AbDataTable table;
    AbDataEntry id;
} AbUiPauseNotice;

typedef struct {
    AbUiScaling *scaling;
    AbUiFontResources *fonts;
    AbEventManager *events;
    const AbDataTable *event_ids;
    const AbVec2 *screen, *margin;
    const AbUiPausePageEnvironment *environment;
    const AbDataValue *time, *tutorial_active, *current_music;
    AbDataValue *skip_input;
    AbAudioMixer *mixer;
    AbAudioBank *audio;
    AbClassicAudioRampState *ramp;
    uint32_t solid_texture;
    AbUiPauseNotice *notices;
    size_t notice_capacity;
    AbEventTimerEntry *timers;
    size_t timer_capacity;
    AbUiTextClipStorage label_storage;
    void *context;
    bool (*audio_enabled)(void *context, bool *output, AbError *error);
    bool (*level_display)(void *context, AbUiPointerValue *output, AbError *error);
    /* Actual commongamelogic.showPauseMenu and setScalingEnabled services. */
    bool (*show_pause_menu)(void *context, AbError *error);
    bool (*scaling_enabled)(void *context, bool enabled, AbError *error);
    /* NULL models the absent EggDefenderSingleInstanceSound object. */
    bool (*egg_audio)(void *context, bool muted, AbError *error);
} AbUiPausePageHost;

typedef enum {
    AB_UI_PAUSE_RESUME,
    AB_UI_PAUSE_CHECKPOINT,
    AB_UI_PAUSE_RESTART,
    AB_UI_PAUSE_MENU,
    AB_UI_PAUSE_SHOP,
    AB_UI_PAUSE_TOONS,
    AB_UI_PAUSE_TUTORIALS,
    AB_UI_PAUSE_CONTROL_COUNT,
} AbUiPauseControl;

typedef struct {
    AbUiWidget widget, dummy, middle, controls[AB_UI_PAUSE_CONTROL_COUNT];
    AbUiToggleButton sounds;
    AbDataTable sound_images;
    AbDataEntry sound_image_fields[2];
    AbUiTextWidget number;
    AbUiTextClipping number_clipping;
    AbEventTimer timer;
    AbEventListener listener;
    AbUiPausePageHost host;
    AbUiFrameNode *frames[1], *dummy_frames[5], *middle_frames[5];
    AbUiDrawChild draw[1], dummy_draw[5], middle_draw[5];
    AbUiPointerChild pointer[1], dummy_pointer[5], middle_pointer[5];
    float extend_state, extend_speed, available_height;
    size_t notice_count;
    bool shop_present, toons_present, block_input, registered[5];
    bool initialized, busy;
} AbUiPausePage;

/* Concrete ordinary PausePage/MiddleButtonsFrame graph. Own fresh stable caller
 * storage; borrow actual decoded UI resources/fonts, live resolved environment,
 * settings/display services, timer/notice pools, audio/ramp and shared globals.
 * Environment booleans are resolved source values, not SDK responses. Source
 * shop/demo/Mattel/channel branches select actual controls. No missing service
 * succeeds as empty work. The enclosing HUD owns its own visibility/actions.
 *
 * Source entry/exit/layout/update/draw/pointer dispatch is installed on widget.
 * Timer, line histories and distinct notice identities remain bounded and retained
 * until all event consumers finish. Detach before releasing borrowed storage;
 * it is lifetime cleanup, not source onExit. Reached failure stops the widget
 * context with completed effects retained. Recursive public calls, custom child
 * classes, colored/nonstring clipped labels and foreign graph mutations are
 * unsupported. No allocation, VM, frame I/O or implicit gameplay defaults. */
bool ab_ui_pause_page_init(AbUiPausePage *page, AbUiWidgetContext *context,
                           AbUiPausePageHost host, AbError *error);
bool ab_ui_pause_page_show(AbUiPausePage *page, AbError *error);
bool ab_ui_pause_page_hide(AbUiPausePage *page, AbError *error);
bool ab_ui_pause_page_audio_state(AbUiPausePage *page, AbError *error);
bool ab_ui_pause_page_total_width(AbUiPausePage *page, float *output, AbError *error);
bool ab_ui_pause_page_detach(AbUiPausePage *page, AbError *error);

#endif
