#ifndef AB_UI_GAME_HUD_CONTROLS_H
#define AB_UI_GAME_HUD_CONTROLS_H

#include "ab/classic_level_flow.h"
#include "ab/classic_level_request.h"
#include "ab/classic_sling.h"
#include "ab/ui_animated_image.h"
#include "ab/ui_hud_buttons.h"
#include "ab/ui_hud_powerups.h"
#include "ab/ui_mighty_eagle_button.h"
#include "ab/ui_pause_page.h"

typedef struct AbUiGameHudControls AbUiGameHudControls;

typedef struct {
    AbDataTable table;
    AbDataEntry id;
} AbUiGameHudNotice;

typedef enum {
    AB_UI_GAME_HUD_SHOW_PAUSE,
    AB_UI_GAME_HUD_INSTANT_SHOW_PAUSE,
    AB_UI_GAME_HUD_INSTANT_HIDE_PAUSE,
    AB_UI_GAME_HUD_RETURN_TO_GAME,
} AbUiGameHudPauseAction;

typedef enum {
    AB_UI_HUD_PREMIUM,
    AB_UI_HUD_POWERUP_VISIBLE,
    AB_UI_HUD_OFFER_ON_START,
    AB_UI_HUD_CHILDLOCK,
    AB_UI_HUD_TRIAL,
    AB_UI_HUD_RMF_PURCHASED,
    AB_UI_HUD_RMF_VISIBLE,
    AB_UI_HUD_SHOCKWAVE_VISIBLE,
} AbUiGameHudQuery;

typedef struct {
    const AbDataTable *globals;
    const AbClassicLevelRequest *level;
    AbUiHudPowerupsHost powerups;
    AbUiMightyEagleHost eagle;
    AbUiPausePageHost pause;
    AbClassicLevelFlow *flow;
    AbClassicSling *sling;
    AbUiGameHudNotice *notices;
    size_t notice_capacity;
    AbDataValue *tutorials_requested;
    /* Root GameHud.lua's captured register0, selected at class load. For the
     * Android model it is raw g_isAccelerometerControl. It is unrelated to the
     * separately defined endGameAtPPBack function. */
    AbUiPointerValue restart_control;
    float default_profile;
    void *context;
    bool (*query)(void *context, AbUiGameHudControls *controls, AbUiGameHudQuery query,
                  AbUiPointerValue *output, AbError *error);
    /* A reached SaleBanner must be the actual owned class, not an empty Frame.
     * Its owner remains alive until this graph and all callbacks are released. */
    bool (*sale_banner)(void *context, AbUiWidget **output, AbError *error);
} AbUiGameHudControlsHost;

enum { AB_UI_GAME_HUD_CHILDREN = 12, AB_UI_GAME_HUD_BUTTONS = 7 };

struct AbUiGameHudControls {
    AbUiWidget widget, ads_off, highscore_indicator, rmf;
    AbUiHudPauseButton restart, pause;
    AbUiHudSkipButton skip;
    AbUiHudPowerups powerups;
    AbUiMightyEagleButton eagle;
    AbUiPausePage pause_page;
    AbUiHudPowerup blackbird;
    AbUiAnimatedImage electricity;
    AbDataEntry electricity_fields[6], rmf_fields[2];
    AbDataTable electricity_frames, rmf_meta;
    AbUiFrameNode *frames[AB_UI_GAME_HUD_CHILDREN], *blackbird_frames[3];
    AbUiDrawChild draw[AB_UI_GAME_HUD_CHILDREN], blackbird_draw[3];
    AbUiPointerChild pointer[AB_UI_GAME_HUD_CHILDREN], blackbird_pointer[3];
    AbUiWidget *hud_buttons[AB_UI_GAME_HUD_BUTTONS], *banner;
    size_t hud_button_count;
    size_t notice_count;
    AbUiGameHudControlsHost host;
    AbClassicLevelFlowListener tutorials;
    AbUiPointerValue text_group, text_id, show_skip_button, hide_powerups;
    float frame;
    bool can_show_tutorials, allow_pause;
    bool restart_present, ads_present, powerups_present, eagle_present;
    bool blackbird_present, rmf_present, highscore_present;
    bool initialized, layout_active, pause_active;
};

/* GameHud.init4036's ordinary Android-model widget construction. The shared
 * GLES2/Metal/native-window backend is independent of that pinned APK model.
 * Other device models and overridden classes are outside this constructor.
 * Build the actual Frame, ordered HUD list, conditional controls, real slider,
 * Eagle, PausePage, black-bird counter/animation and tutorial-flow request.
 * Borrow stable actual class/global/manager/resource/service owners and pools.
 * Missing reached services reject; no SDK responses or gameplay defaults are
 * supplied. No allocation, resource I/O or VM. Failures stop the shared context.
 *
 * This is the constructor-owned graph, not the enclosing GameHud lifecycle,
 * draw, input, events or application factory. Its root remains the real Frame
 * initialized by the source's first phase. Do not promote it to a game scene
 * until the concrete enclosing GameHud methods have been installed. */
bool ab_ui_game_hud_controls_init(AbUiGameHudControls *controls,
                                  AbUiWidgetContext *context,
                                  const AbUiFrameFields *arguments,
                                  AbUiPointerValue returning_to_game,
                                  AbUiGameHudControlsHost host, AbError *error);
/* Actual ordinary GameHud.layout22112 around this same graph, live Scaling,
 * screen/margin, resource pivots, Eagle/registry queries and real child layouts.
 * No resource reload or allocation. The enclosing class calls this at its
 * layout boundary; it does not install other missing GameHud methods. */
bool ab_ui_game_hud_controls_layout(AbUiGameHudControls *controls, AbError *error);

/* Source showPauseMenu29572, instantlyShow31142, instantlyHide30249 and
 * returnToGame35001. These change this graph and call the actual PausePage.
 * Return queues a fresh BACK_TO_GAME_CLICKED identity before cancelling the
 * actual sling drag. The common event listener owns game-mode resumption.
 * Borrow notice storage until queued consumers finish. No allocation or empty
 * service substitutions; a reached failure retains its prefix and stops UI. */
bool ab_ui_game_hud_controls_pause(AbUiGameHudControls *controls,
                                   AbUiGameHudPauseAction action, AbError *error);

#endif
