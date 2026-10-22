#ifndef AB_SCENE_ID_H
#define AB_SCENE_ID_H

#include "ab/base.h"

typedef struct {
    int32_t value;
    float script_number;
} AbSceneId;

/* gfx::Scene.newId uses one application-wide counter, shared across scenes.
 * Initialize the owner at application startup; scene clearing/retry does not
 * reset it. Increment modulo 2^32, retain signed ID bits and expose the original
 * binary32 numeric return under round-to-nearest. Large IDs can alias after
 * rounding; do not replace
 * that value with a higher-precision representation. No allocation. Counter
 * and output must be disjoint. Invalid bindings preserve both outputs.
 * Scene storage, renderer registration and float-ID disposal remain separate. */
bool ab_scene_id_next(uint32_t *counter, AbSceneId *output, AbError *error);

/* Native signed ARM VCVT conversion used for layer numbers and ID disposal.
 * Truncate finite fractions, saturate overflow/infinities, map NaN to zero. */
int32_t ab_scene_id_integer(float number);

#endif
