#ifndef AB_UI_MIGHTY_EAGLE_BUTTON_H
#define AB_UI_MIGHTY_EAGLE_BUTTON_H

#include "ab/session_timer.h"
#include "ab/ui_scaling.h"
#include "ab/ui_text_clipping.h"

typedef enum {
    AB_UI_EAGLE_AVAILABLE,
    AB_UI_EAGLE_LEVEL_ENDING,
    AB_UI_EAGLE_ENABLED,
    AB_UI_EAGLE_SUBSYSTEM_ENABLED,
    AB_UI_EAGLE_SHOW_ELEMENTS,
    AB_UI_EAGLE_LEVEL_DISABLED,
    AB_UI_EAGLE_CHILDLOCK,
    AB_UI_EAGLE_SETTINGS_ENABLED,
    AB_UI_EAGLE_FREE_ENABLED,
    AB_UI_EAGLE_QUERY_COUNT,
} AbUiEagleQuery;

typedef struct {
    AbUiScaling *scaling;
    AbUiFontResources *fonts;
    AbEventManager *events;
    const AbDataTable *event_ids, *globals;
    AbUiPointerValue basic_font_getter;
    AbSessionClock clock;
    AbUiTextClipStorage timer_storage;
    void *context;
    /* Bind actual queries against canonical session/settings state. Retain both
     * original enabled-call boundaries; subsystemsapi delegates to the common
     * getter in this target. globals is the same live graph used by queries. */
    bool (*query)(void *context, AbUiEagleQuery query, AbUiPointerValue *output,
                  AbError *error);
    bool (*episode_power)(void *context, const AbDataTable **output, AbError *error);
    bool (*used_time)(void *context, AbUiPointerValue free_enabled, AbDataValue *output,
                      AbError *error);
} AbUiMightyEagleHost;

typedef enum {
    AB_UI_EAGLE_STILL,
    AB_UI_EAGLE_BOUNCE,
    AB_UI_EAGLE_DISAPPEAR,
} AbUiMightyEagleState;

typedef struct {
    AbUiWidget widget;
    AbUiTextWidget timer;
    AbUiTextClipping clipping;
    AbUiMightyEagleHost host;
    AbEventListener listener;
    AbUiFrameNode *frames[1];
    AbUiDrawChild draw[1];
    AbUiPointerChild pointer[1];
    AbUiPointerValue result, from, timer_hancor, want_visible;
    AbUiPointerValue info_timer, scale_timer;
    AbUiMightyEagleState state;
    char timer_text[64];
    bool subscriptions[8];
    bool initialized;
} AbUiMightyEagleButton;

/* Concrete MightyEagleButton with real ImageButton/Text/Scaling/EventManager
 * composition. Fresh caller-owned stable storage, retained clipping pools and
 * live canonical bindings are required. Timer text spans last until their next
 * assignment; clipped line bytes retain their own generations. Queries execute
 * only when reached, preserving source short-circuit and failure prefixes.
 * Ordinary numeric dates/fonts and builtin class methods only. No allocation,
 * VM, replacement service responses or resource loading.
 *
 * onExit preserves the original omission of LEVEL_COMPLETED unsubscription.
 * detach removes any remaining lifetime subscriptions without source effects. */
bool ab_ui_mighty_eagle_button_init(AbUiMightyEagleButton *button,
                                    AbUiWidgetContext *context,
                                    const AbUiImageButtonArguments *arguments,
                                    AbUiPointerValue from, AbUiMightyEagleHost host,
                                    AbError *error);
bool ab_ui_mighty_eagle_button_visible(AbUiMightyEagleButton *button,
                                       AbUiPointerValue visible, AbError *error);
bool ab_ui_mighty_eagle_button_event(AbUiMightyEagleButton *button, AbDataTable *event,
                                     AbError *error);
bool ab_ui_mighty_eagle_button_detach(AbUiMightyEagleButton *button, AbError *error);

#endif
