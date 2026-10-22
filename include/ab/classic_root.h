#ifndef AB_CLASSIC_ROOT_H
#define AB_CLASSIC_ROOT_H

#include "ab/classic_game_globals.h"
#include "ab/classic_audio_ramp.h"
#include "ab/classic_anger.h"
#include "ab/classic_root_queries.h"
#include "ab/url_requests.h"
#include "ab/event_manager.h"
#include "ab/native_events.h"
#include "ab/process_manager.h"
#include "ab/physics_geometry.h"

typedef struct AbClassicRoot AbClassicRoot;

typedef enum {
    AB_ROOT_SAFE_TO_QUIT, /* Owned menu-state query executes directly. */
    AB_ROOT_GAMER_UPDATE,
    AB_ROOT_ANGER_UPDATE, /* Owned AngerLevel executes directly. */
    AB_ROOT_VIEWPORT,
    AB_ROOT_CLIP,
    AB_ROOT_DEBUG_UPDATE,
    AB_ROOT_OPEN_URL,
    AB_ROOT_FULLSCREEN,
    AB_ROOT_URL_TIMERS, /* Owned URL requests execute directly. */
    AB_ROOT_KEY_RELEASED,
    AB_ROOT_CHANGE_AUDIO,
    AB_ROOT_AUDIO_RAMP, /* Owned ramp executes directly; callback slot unused. */
    AB_ROOT_FREE_EAGLE, /* Owned metadata query executes directly. */
    AB_ROOT_EAGLE_USED_TIME,
    AB_ROOT_MENU_UPDATE,
    AB_ROOT_ALLOW_INPUT,
    AB_ROOT_AD_UPDATE,
    AB_ROOT_CURRENT_MODE,
    AB_ROOT_MENU_DRAW,
    AB_ROOT_UNLOCKING,
    AB_ROOT_NOTIFICATION_LAYOUT,
    AB_ROOT_MENU_LAYOUT,
    AB_ROOT_DEBUG_DRAW,
    AB_ROOT_RENDER_STATE,
    AB_ROOT_SHOP_UPDATE,
    AB_ROOT_SERVICE_COUNT,
} AbClassicRootService;

typedef struct {
    void *context;
    bool (*call)(void *context, const AbDataValue *arguments, size_t count,
                 AbDataValue *result, AbError *error);
} AbClassicRootCall;

typedef struct {
    AbClassicRootCall **items;
    size_t count, capacity;
} AbClassicRootCalls;

typedef struct {
    AbDataValue safe_to_quit, dt, global_time, playtime, last_video_ad_time;
    AbDataValue input_delay, double_click, double_click_timer, double_click_state;
    AbDataValue eagle_info_timer;
    AbDataValue show_background, game_was_resumed, restore_fullscreen;
    AbDataValue updated_width, updated_height, old_width, old_height;
    float width, height;
    AbVec2 clip_offset, touch_offset;
    AbDataTable *delayed_url;
} AbClassicRootState;

typedef struct {
    AbDataValue gamer_present, gamer_dev_mode, restricted_aspect;
    AbDataValue release_build, cheats_enabled, show_editor, block_audio_change;
    AbDataValue settings_converted, has_iaps, ios, is_pc, cursor_enabled;
    AbSpan device_model;
    /* These source objects are absent or valid resolved instances, never raw
     * false/noncallable records. Call bindings remain live during each frame. */
    bool menu_present, ads_present, settings_present, achievement_present;
    bool notifications_present, shop_present;
} AbClassicRootFlags;

typedef enum {
    AB_ROOT_EVENT_TICK,
    AB_ROOT_PROCESS_UPDATE,
    AB_ROOT_NATIVE_ANIMATION,
    AB_ROOT_AUDIO_RAMP_UPDATE,
    AB_ROOT_URL_TIMERS_UPDATE,
    AB_ROOT_ANGER_TIMERS_UPDATE,
    AB_ROOT_SAFE_QUIT_QUERY,
    AB_ROOT_FREE_EAGLE_QUERY,
} AbClassicRootOwnedPhase;

typedef struct {
    AbClassicRootState *state;
    AbClassicRootFlags *flags;
    AbClassicGameGlobals *globals;
    AbDataValue *skip_input;
    const AbDataTable *held_keys, *pressed_keys, *released_keys;
    const AbDataValue *fullscreen_event, *resolution_event;
    AbEventManager *events;
    AbProcessManager *processes;
    AbNativeAnimation *native_animation;
    AbClassicAudioRamp *audio_ramp;
    AbUrlRequests *url_requests;
    AbClassicAnger *anger;
    /* Same actual global menuManager used by remaining menu service bindings;
     * NULL represents its source absence and must match flags.menu_present. */
    AbUiMenuManager *menus;
    const AbClassicEagleQuery *eagle_query;
    AbClassicRootCalls *subsystems, *updatable, *tail_updates;
    AbClassicRootCall *services[AB_ROOT_SERVICE_COUNT];
    size_t notifications, storage_bytes;
    void *observer_context;
    /* Optional observation immediately before the real owned phase. No
     * replacement is selected when absent; real managers always execute. */
    bool (*observe)(void *context, AbClassicRootOwnedPhase phase, AbError *error);
} AbClassicRootConfig;

/* Ordinary root update57000. Own distinct event payloads once; borrow canonical
 * clocks, raw input, flags, ordered prepared function tables and required live
 * service bindings. All source control flow in this profile is C. No script
 * interpreter, resource factory, replacement service or default bootstrap.
 *
 * Supported: finite numeric clocks/dimensions, nil/false clock initialization,
 * ordinary platform flags, visible/menu/ad/provider calls, deferred URL/resume,
 * input-delay and double-click transitions, fixed prepared iteration order.
 * Reached restricted-aspect setup, nonnil eagle cooldown, PC synchronization
 * and software cursor require their full source owner and reject explicitly.
 * Prepared pair-order arrays must represent actual original table iteration;
 * arbitrary hash reordering and raw clip-table identity remain outside scope.
 * Bootstrap, concrete subsystem/menu/service factories and native outer frame
 * scheduling remain caller work. No playable or all-level claim is implied.
 */
bool ab_classic_root_open(AbClassicRootConfig config, AbClassicRoot **output,
                          AbError *error);
void ab_classic_root_close(AbClassicRoot *root);
size_t ab_classic_root_bytes(const AbClassicRoot *root);
/* No frame allocation. Original second argument is passed to reached debug
 * update only. Preserve earlier changes after a late failure, then stop owner.
 * Reentry rejects without disturbing the running invocation. */
bool ab_classic_root_update(AbClassicRoot *root, float seconds,
                            AbDataValue second_argument, AbError *error);

#endif
