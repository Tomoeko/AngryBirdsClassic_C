#ifndef AB_CLASSIC_OBJECT_ANIMATION_H
#define AB_CLASSIC_OBJECT_ANIMATION_H

#include "ab/classic_visuals.h"

/* Ordinary updateAnimation over canonical physical fields and retained catalog
 * frame tables. Advance at most one frame per call, including overshoot; a
 * finished nonlooping animation retains its last frame. Publish the damage
 * override before the real getDamageSprite/visual boundary. No allocation,
 * clocks, RNG or frame interpolation. Mutable frame tables/numeric strings are
 * outside this domain. Earlier field/visual changes survive a later error. */
bool ab_classic_object_animation_update(AbClassicLevel *level,
                                        AbClassicVisuals *visuals,
                                        AbStringOrderWorkspace *workspace,
                                        size_t object, float seconds, AbError *error);

#endif
