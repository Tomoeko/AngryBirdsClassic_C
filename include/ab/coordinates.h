#ifndef AB_COORDINATES_H
#define AB_COORDINATES_H

#include "ab/physics_geometry.h"

typedef enum {
    AB_SPACE_SCREEN,
    AB_SPACE_WORLD,
    AB_SPACE_PHYSICS,
} AbCoordinateSpace;

typedef struct {
    AbVec2 top_left;
    AbVec2 zoom;
} AbCoordinateCamera;

/* Target CoordinateTransforms arithmetic. World-to-physics multiplies by
 * binary32 0.05; its inverse multiplies by twenty. World-to-screen subtracts
 * top_left before multiplying by zoom; its inverse divides before adding.
 * Screen/physics conversions compose those operations in that order. Same-
 * space conversion is an identity defined here. The camera may be NULL when
 * no screen conversion is needed. Used values/intermediates must be finite;
 * screen-to-world requires both zoom components nonzero. Failure preserves
 * output. Logical camera setup, viewport and device input mapping are separate.
 */
bool ab_coordinate_point(const AbCoordinateCamera *camera, AbCoordinateSpace source,
                         AbCoordinateSpace target, AbVec2 point, AbVec2 *output);

/* Convert zero and delta independently, then subtract transformed zero from
 * transformed delta. Do not cancel camera translation algebraically: it affects
 * rounding. This is a vector delta, not a Euclidean scalar length. The same
 * finite-domain/ownership rules as ab_coordinate_point apply. */
bool ab_coordinate_distance(const AbCoordinateCamera *camera, AbCoordinateSpace source,
                            AbCoordinateSpace target, AbVec2 delta, AbVec2 *output);

/* Original screenDistanceToPhysicsScale uses only camera.zoom.x: divide the
 * scalar by that component, then multiply by binary32 0.05. Camera translation
 * and zoom.y are unused. Finite used values/result and nonzero zoom.x required;
 * failure preserves output. */
bool ab_coordinate_screen_scale(const AbCoordinateCamera *camera, float distance,
                                float *output);

#endif
