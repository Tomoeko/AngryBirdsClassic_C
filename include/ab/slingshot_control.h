#ifndef AB_SLINGSHOT_CONTROL_H
#define AB_SLINGSHOT_CONTROL_H

#include "ab/bird_shot.h"
#include "ab/coordinates.h"

/* Original delta-based sling displacement accumulator. Disabled updates ignore
 * numeric/camera inputs and preserve delta; enabled updates subtract the two
 * screen-distance conversions times sensitivity, then clamp vector length to
 * shoot_range + binary32 3.2. Only camera.zoom.x participates in conversion.
 * The original enabled method returns its retained vector; the disabled method
 * returns nil. This value API leaves identity/return tagging to the caller.
 * Required numbers/intermediates must be finite, zoom.x nonzero and shoot_range
 * nonnegative. Conversion/setting callbacks and their mutations are outside
 * this resolved-input contract. Failure preserves delta; no allocation. */
bool ab_slingshot_delta_update(AbVec2 *delta, bool enabled, AbVec2 screen_displacement,
                               const AbCoordinateCamera *camera, float sensitivity,
                               float shoot_range, AbError *error);

/* Enabled reset writes positive zero to both axes; disabled reset preserves
 * all bits, including nonfinite values. Original table replacement/ownership
 * remains caller policy. NULL delta fails. */
bool ab_slingshot_delta_reset(AbVec2 *delta, bool enabled);

typedef struct {
    AbVec2 rest;
    AbVec2 rubber_band;
    float angle;
    float distance;
    float wobble;
    float previous_distance;
    bool scope_sound_eligible;
} AbSlingBandState;

typedef enum {
    AB_SLING_CONTROL_SET_POSITION = 1,
    AB_SLING_CONTROL_QUERY_LASER,
} AbSlingControlOperation;

typedef struct {
    AbSlingControlOperation operation;
    AbShotBird *bird;
    AbVec2 position;
    bool audio_playing;
} AbSlingControlCall;

typedef bool (*AbSlingControlHost)(void *context, AbSlingControlCall *call,
                                   AbError *error);

/* Reset the band to rest and its angle/distance/wobble/previous distance to
 * positive zero, then query laser_sight playback. A stopped laser sound sets
 * scope_sound_eligible true; a playing sound preserves its previous value.
 * The host returns audio_playing for QUERY_LASER. No sound is stopped here.
 * Required input fields must be finite. Failure before work preserves state;
 * a failed host query leaves the numeric reset applied.
 */
bool ab_slingshot_reset_band(AbSlingBandState *band, AbSlingControlHost host,
                             void *context, AbError *error);

/* With a dragged bird, request its position at rest before clearing the dragged
 * reference and selection flag. Then reset the band even if no bird was dragged.
 * Other shot state, including current name/readiness/velocity, is preserved.
 * The host owns the actual position update. A host error stops at that boundary
 * and leaves preceding changes applied. No allocation or audio playback.
 */
bool ab_slingshot_cancel_drag(AbSlingBandState *band, AbShotState *shot,
                              AbSlingControlHost host, void *context, AbError *error);

#endif
