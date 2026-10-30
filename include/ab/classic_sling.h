#ifndef AB_CLASSIC_SLING_H
#define AB_CLASSIC_SLING_H

#include "ab/audio_bank.h"
#include "ab/classic_shots.h"
#include "ab/game_sling_mouse.h"
#include "ab/shot_services.h"
#include "ab/slingshot.h"
#include "ab/slingshot_control.h"
#include "ab/slingshot_pose.h"

typedef struct AbClassicSling AbClassicSling;

typedef enum {
    AB_SLING_HAT_HOLD,
    AB_SLING_HAT_TRANSFER,
    AB_SLING_HAT_ACTIVATE,
} AbClassicSlingHatPhase;

typedef struct {
    void *context;
    bool (*call)(void *context, size_t bird, AbClassicSlingHatPhase phase,
                 float transfer_time, AbError *error);
} AbClassicSlingHatHost;

typedef struct {
    AbClassicLevel *level;
    AbClassicShots *shots;
    AbClassicBehaviors *behaviors;
    AbShotServices *services;
    AbEventManager *events;
    AbAudioBank *audio;
    AbCameraSystem *camera;
    AbSlingshotStartPose start;
    float physics_scale, default_force;
    float current_bird_index;
    bool fill_in_next_bird, allow_reset_to_bird_camera;
    void *context;
    /* Original cameraSystem distance queries, in their original order. Called
     * only at transfer completion. Do not change sling/queue state here. */
    bool (*camera_distances)(void *context, float *castle_to_bird,
                             float *distance_to_bird, AbError *error);
    /* Direct res.playAudio(getAudioName(g_sound_next_bird), gain, false),
     * distinct from the script playAudio wrapper. A missing service rejects. */
    bool (*reload_sound)(void *context, float gain, AbError *error);
    size_t storage_bytes;
    bool disabled_for_current_level;
    AbClassicSlingHatHost hats;
} AbClassicSlingConfig;

typedef struct {
    AbSlingBandState band;
    AbSlingMouseState mouse;
    AbSlingTransferInput transfer;
    float current_bird_index;
    float cooldown_trigger_time;
    size_t transfer_object;
    bool fill_in_next_bird, allow_reset_to_bird_camera, launch_cue;
    bool disabled_for_current_level;
    bool king, has_scope;
} AbClassicSlingView;

/* Own ordinary default-sling input/queue state and stable event/vector tables
 * for the fixed initial level. Borrow every configured owner/context until
 * close; drain event observers before close. Open has no world/audio effects.
 * Controller/bootstrap values are explicit inputs. The shared shot state owns
 * currentBirdName, readiness, firing and reload timers; do not keep another copy.
 * Initial queue names/start numbers and controllable properties are read from
 * the live owned level in birds-table order. No dynamic insertion/reuse, delta
 * gestures or custom reload functions. Hats require their real borrowed host.
 * KingSling selection changes power
 * and the live sprite profile; scope trajectory/effect scheduling is external. Reached
 * unsupported work rejects, retaining preceding effects. No frame allocation.
 * Keep calls serialized and stop the session after an error. */
bool ab_classic_sling_open(AbClassicSlingConfig config, AbClassicSling **output,
                           AbError *error);
void ab_classic_sling_close(AbClassicSling *sling);
size_t ab_classic_sling_bytes(const AbClassicSling *sling);
AbClassicSlingView ab_classic_sling_view(const AbClassicSling *sling);
/* Borrow the actual shared shot owner; NULL for a missing sling. */
AbClassicShots *ab_classic_sling_shots(const AbClassicSling *sling);
/* SlingshotSystem.getSlingshotPower23570 through the selected ordinary sling. */
bool ab_classic_sling_power(AbClassicSling *sling, float *output, AbError *error);
/* Select the recovered KingSling class; retain system-level band/queue state.
 * The enclosing activation supplies its source audio, particles and camera. */
bool ab_classic_sling_set_king(AbClassicSling *sling, bool scope, AbError *error);
bool ab_classic_sling_set_scope(AbClassicSling *sling, bool enabled, AbError *error);
/* getLaunchVelocity24118 over the live definition, physical mass and band.
 * A missing current bird returns zero. The caller gates a band at rest. */
bool ab_classic_sling_launch_velocity(AbClassicSling *sling, AbVec2 *output,
                                      AbError *error);
/* playScopeSound6753: direct laser_sight playback once per rearmed stretch.
 * Successful launch and resetBand rearm the original private sound flag. */
bool ab_classic_sling_play_scope_sound(AbClassicSling *sling, AbError *error);

/* Bind as the owned shots' service through a stable session context. This
 * prepares the actual BIRD_SHOT vector identities, then forwards to services.
 * The launch-position table follows subsequent in-place band resets; a new
 * stretch replaces that identity. Impulse tables remain stable until close. */
bool ab_classic_sling_shot(AbClassicSling *sling, AbShotCall *call, AbError *error);
void ab_classic_sling_set_launch_cue(AbClassicSling *sling);

bool ab_classic_sling_select(AbClassicSling *sling, AbError *error);
bool ab_classic_sling_stretch(AbClassicSling *sling, AbVec2 displacement,
                              AbError *error);
bool ab_classic_sling_release(AbClassicSling *sling, AbError *error);
/* The actual SlingshotSystem.cancelBirdDrag boundary, including the shared
 * band's in-place reset and real laser-audio query. No implicit launch. */
bool ab_classic_sling_cancel_drag(AbClassicSling *sling, AbError *error);
/* State portion of ordinary levelLoadReset5411, using the real laser-audio query after
 * clearing the start vectors and band. Clear drag/selection/queue flags only
 * after that query succeeds. Current bird/flying globals are the enclosing
 * Museum loader's earlier writes; this method does not replace them. Preserve
 * the retained allowResetToBirdCamera capture, as the source does. */
bool ab_classic_sling_level_reset(AbClassicSling *sling, AbError *error);
/* setPositionByStartObject5035 over the canonical physical table. SIZE_MAX
 * models nil and leaves all vectors unchanged. Pivot/height are the actual
 * selected SLING_SHOT_01_BACK resource replies. This does not transfer, move,
 * select or fire the bird. Finite source geometry and ordinary numeric fields;
 * no resource I/O/allocation. */
bool ab_classic_sling_position_start(AbClassicSling *sling, size_t object,
                                     float back_pivot_y, float back_height,
                                     AbError *error);
AbSlingshotStartPose ab_classic_sling_start_pose(const AbClassicSling *sling);
/* getNextBird over the current fixed initial birds registry. SIZE_MAX denotes
 * nil; read live controllable/startNumber and skip removed members. Pure query. */
bool ab_classic_sling_next_bird(AbClassicSling *sling, float index, size_t *object,
                                AbError *error);
/* launchMightyBait88284's explicit selection clear, then its post-construction
 * currentBirdIndex, nextBirdTimer and fillInNextBird writes. The caller performs
 * source removals/construction/events between these two separate boundaries. */
bool ab_classic_sling_clear_selection(AbClassicSling *sling, AbError *error);
bool ab_classic_sling_reload_after(AbClassicSling *sling, float index, float delay,
                                   AbError *error);
/* Actual named abilities may change the shared allowResetToBirdCamera flag. */
bool ab_classic_sling_allow_camera_reset(AbClassicSling *sling, bool allowed,
                                         AbError *error);
/* Bind the source hat service once after construction, before any hat-bearing
 * frame. Hold/transfer/activation calls remain at their original native body
 * and script boundaries. The host/registry outlive the sling; no effects here. */
bool ab_classic_sling_bind_hats(AbClassicSling *sling, AbClassicSlingHatHost host,
                                AbError *error);
/* Source disableSlingshotForCurrentLevel3559 ignores arguments and sets its
 * private flag true; isSlingshotDisabledForCurrentLevel3611 returns that flag.
 * Level reset/bootstrap supplies the false initial value in config. */
bool ab_classic_sling_disable_for_level(AbClassicSling *sling, AbError *error);
/* GameUpdate255..258 writes currentBirdIndex=0 then fillInNextBird=true after
 * level-start audio/menu stop. Preserve all other live transfer/shot state. */
bool ab_classic_sling_start_queue(AbClassicSling *sling, AbError *error);
/* Source isSlingshotVisible23300: logical camera left < world base x. */
bool ab_classic_sling_is_visible(AbClassicSling *sling, bool *visible, AbError *error);
/* GameUpdate1421..1428, after object scripts and before GFX/draw. */
bool ab_classic_sling_finish_input(AbClassicSling *sling, AbVec2 cursor,
                                   AbError *error);
/* cursor_physics/zoom/screen dimensions are resolved by the real camera/input
 * owner. Replace bird/rest/readiness/range/disabled replies with live owned values, run
 * the recovered mouse plan and execute its selection/stretch/release effects.
 * Public query functions are fixed and non-mutating in this domain. */
bool ab_classic_sling_mouse(AbClassicSling *sling, AbSlingMouseInput input,
                            AbError *error);
/* SlingshotSystem.gameUpdate: hold current bird, advance default reload, then
 * update the default sling (scope drawing is initially disabled). Actual body,
 * history, character and physical-table writes happen at their boundaries.
 * This does not step physics or schedule the enclosing game frame. */
bool ab_classic_sling_update(AbClassicSling *sling, float seconds, AbError *error);
/* Separate GameUpdate spring phase after gameUpdate, gated by live birdFired.
 * Preserve the original <=0.05 substeps and fixed spring-speed increment.
 * maximum_steps bounds work without changing the supplied time. An exhausted
 * budget retains prior substeps and fails. Retained launch-position aliases
 * observe these in-place band changes. No audio or allocation. */
bool ab_classic_sling_wobble(AbClassicSling *sling, float seconds, size_t maximum_steps,
                             AbError *error);

#endif
