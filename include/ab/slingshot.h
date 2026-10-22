#ifndef AB_SLINGSHOT_H
#define AB_SLINGSHOT_H

#include "ab/physics_geometry.h"

#include <stdint.h>

typedef struct {
    AbVec2 rest;
    AbVec2 rubber_band;
    float stretch;
    float launch_force;
    float physics_scale;
    float bird_mass;
    float slingshot_power;
    bool bird_present;
} AbSlingLaunchInput;

/* Museum SlingshotSystem.getLaunchVelocity numeric result. Resolve definition
 * force/default selection and sling properties before calling. Preserve each
 * binary32 multiply, divide and inverse-mass multiply; cancelling bird mass
 * algebraically changes target rounding. Missing bird returns positive zero.
 * Otherwise inputs/intermediates must be finite, mass positive and the two
 * positions distinct. Reject unsupported numeric ranges without changing output.
 * Input/touch, drag clamping, force selection and actual launch remain separate.
 */
bool ab_slingshot_launch_velocity(const AbSlingLaunchInput *input, AbVec2 *output);

/* Actual SlingshotSystem.launchBird impulse arithmetic. Default direction is
 * rest minus band; an explicit vector overrides that direction and its length.
 * Negate force before scale/mass/power multiplication, then apply normalized
 * direction, force and stretch in target order. No inverse-mass multiply occurs.
 * A bird and nonzero measured vector length are required. All used values and
 * results must be finite, mass positive. Failure preserves output. Statistics,
 * position/velocity resets, event hooks and applying the impulse are separate.
 */
bool ab_slingshot_launch_impulse(const AbSlingLaunchInput *input,
                                 const AbVec2 *vector_to_rest, AbVec2 *output);

/* Release launches only when measured distance is strictly greater than the
 * configured threshold; equality cancels. Comparisons retain IEEE behavior.
 * Backwards statistics use the chosen vector-to-rest's X component, and are
 * suppressed by literal Lua true for eagle bait or any truthy special mode.
 * The caller resolves those two distinct Lua predicates. Every launch still
 * increments total shots, after its optional backwards increment and before
 * definition/force lookup. These predicates do not execute callbacks.
 */
bool ab_slingshot_release_ready(float distance, float threshold);
bool ab_slingshot_counts_backwards(float vector_x, bool eagle_bait_is_true,
                                   bool special_mode_truthy);

typedef struct {
    AbVec2 start;
    AbVec2 target;
    float height;
    float phase_angle;
    float time;
    float rotation;
    float seconds;
} AbSlingTransferInput;

typedef struct {
    AbVec2 position;
    float rotation;
    float next_time;
    bool complete;
} AbSlingTransferFrame;

typedef struct {
    AbVec2 position;
    AbVec2 target;
    float saved_y;
    float rotation;
    bool jumping;
} AbSlingTransferStart;

/* Default transfer setup after level reset. jumping means literal Lua true;
 * it selects saved_y, otherwise select current position.y. The caller restores
 * current position.y to selected start.y, clears the jumping flag, retains its
 * saved-Y field, and sets jumpTimer to 10000.
 * Preserve phase = binary32 0.75 * pi and height = (target.y - start.y) * 1.33.
 * Return initial time/seconds zero. Caller owns name/queue/script state and
 * reload-delay gating. Finite required inputs/results; failure preserves output.
 */
bool ab_slingshot_transfer_begin(const AbSlingTransferStart *input,
                                 AbSlingTransferInput *output);

/* One active defaultAnimateBirdToSlingShot arithmetic pass after initialization.
 * Emit this position then rotation before completion work. Time is not clamped
 * before arc evaluation; completed updates return next_time zero. Completion
 * then sets target minus binary32 0.1 in both axes, makes the body dynamic and
 * sets rotation zero; callbacks, hats, camera/audio and queue state are separate.
 * Finite inputs/intermediates and |phase_angle * (time + seconds)| <= 1024 apply.
 * Lua's binary64 sine differs from the physics sinf approximation; the authored
 * arc evaluator preserves the measured float results without platform sine.
 * Input is unchanged; unsupported arithmetic preserves output.
 */
bool ab_slingshot_transfer_frame(const AbSlingTransferInput *input,
                                 AbSlingTransferFrame *output);

typedef struct {
    float timer;
    float current_bird_index;
    bool bird_fired;
    bool fill_in_next_bird;
    bool allow_reset_to_bird_camera;
} AbSlingReloadState;

typedef struct {
    AbSlingReloadState next;
    float queried_bird_index;
    bool query_next_bird;
    bool clear_launch_cue;
} AbSlingReloadGate;

/* Prefix of defaultAnimateBirdToSlingShot. Subtract seconds only from a
 * strictly positive timer; query current index + binary32 one only when that
 * subtraction reaches <= zero. A non-nil next-bird result and truthy birdFired
 * clear birdFired, set fillInNextBird and clear the retained launch cue. A nil
 * result disables camera reset. Boolean state is resolved by the caller;
 * next_bird_present distinguishes nil from every other Lua value, even false.
 * No next-bird query is executed here: query_next_bird describes that boundary,
 * and the caller supplies its result. Existing transfer/queue state is separate.
 * Finite numeric inputs/results required; failure preserves output. */
bool ab_slingshot_reload_gate(const AbSlingReloadState *state, float seconds,
                              bool next_bird_present, AbSlingReloadGate *output);

typedef enum {
    AB_SLING_DRAG_QUERY_STRETCH = 1,
    AB_SLING_DRAG_PLAY_STRETCH,
    AB_SLING_DRAG_PLAY_SELECTION
} AbSlingDragEvent;

typedef struct {
    float distance;
    float factor;
} AbSlingStretch;

/* Recompute length from rounded band minus rest, then divide by maximum length
 * and clamp to [0, 1]. A band produced by radial saturation can still measure
 * just below maximum. Finite positions/intermediates and positive finite maximum
 * required; failure preserves output.
 */
bool ab_slingshot_measure_stretch(AbVec2 rest, AbVec2 band, float maximum_length,
                                  AbSlingStretch *output);

typedef struct {
    AbVec2 rest;
    AbVec2 displacement;
    float maximum_length;
    float previous_distance;
    float wobble;
    bool selection_played;
    bool stretch_audio_playing;
} AbSlingDragInput;

typedef struct {
    AbVec2 rubber_band;
    float angle;
    float distance;
    float stretch_factor;
    float wobble;
    bool selection_played;
    AbSlingDragEvent events[3];
    uint32_t event_count;
} AbSlingDragFrame;

/* Default Slingshot/SlingshotSystem stretch pass from physics displacement.
 * Preserve radial multiplication/division order and angle-dependent vertical
 * stretchability. The angle uses Lua's promoted atan2, or positive zero for a
 * zero measured length. Recompute final distance from the rounded band point;
 * the system stores this as its previous-distance state for the next call.
 * stretch_factor is that distance divided by maximum_length, clamped to [0, 1].
 * Events describe ordered audio query/play boundaries; caller supplies the
 * query result, resolves the selection sound and executes audio policy.
 * All inputs/intermediates/results must be finite and maximum_length positive.
 * Failure preserves output. Pointer mapping, touch selection and release are
 * separate from this numeric/state pass.
 */
bool ab_slingshot_drag(const AbSlingDragInput *input, AbSlingDragFrame *output);

#endif
