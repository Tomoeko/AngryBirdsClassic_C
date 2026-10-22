#ifndef AB_OBJECT_PROPERTIES_H
#define AB_OBJECT_PROPERTIES_H

#include "ab/game_frame.h"
#include "ab/physics_step.h"

typedef struct {
    AbBodyPose pose;
    AbVec2 velocity;
    bool sleeping;
    bool outside_boundaries;
} AbObjectProperties;

typedef struct {
    AbObjectProperties values;
    bool publish;
} AbObjectPropertyPlan;

/* GameLua.update's resolved updateObjectProperties arguments. Publish when the
 * body is awake or its previous native object-loop visit was awake. Position
 * uses the cached rendered sample, angle uses signed atan2 of body rotation and
 * velocity uses the current body. Positive-mass bodies are outside when Y > 20
 * or X lies strictly outside the supplied signed integer bounds after float
 * conversion. Other masses publish outside=false. No coordinate conversion.
 *
 * The caller supplies the old awake flag and updates it after the enclosing
 * native object work; this plan does not advance it. No world mutation, callback,
 * script assignment or allocation. Inactive objects do not use numeric fields.
 * Used numbers must be finite. Invalid inputs preserve output. Numeric results
 * require nearest rounding and gradual underflow. Body/frame membership and
 * physics-enabled scheduling remain caller responsibilities.
 */
bool ab_object_properties_plan(const AbBodyMotion *body, const AbGameFrameObject *frame,
                               bool previous_awake, int32_t minimum_x,
                               int32_t maximum_x, AbObjectPropertyPlan *output,
                               AbError *error);

#endif
