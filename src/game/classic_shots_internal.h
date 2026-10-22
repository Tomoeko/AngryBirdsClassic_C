#ifndef AB_CLASSIC_SHOTS_INTERNAL_H
#define AB_CLASSIC_SHOTS_INTERNAL_H

#include "ab/classic_birds.h"

/* The bird registry borrows shots and exclusively manages this binding during
 * its own open/close lifetime. NULL unbinds; a second live owner rejects. */
bool ab_classic_shots_bind_birds(AbClassicShots *shots, AbClassicBirds *birds,
                                 AbError *error);

#endif
