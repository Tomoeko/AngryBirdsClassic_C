#ifndef AB_COMPOSITE_PART_H
#define AB_COMPOSITE_PART_H

#include "ab/composite.h"
#include "ab/composite_bounds.h"

/* Map a parsed KA3D/RVIO part to the original CompoSprite Entry transform. RVIO
 * rotation is degrees, multiplied in binary32 by native 0x3c8efa35; flips are
 * independent +/-1 factors. Legacy fields beyond offset are unused. Borrow the
 * resolved ordinary region. Invalid used values preserve output. */
bool ab_composite_part_state(const AbCompositePart *part, bool extended,
                             const AbSpriteRegion *region,
                             AbCompositeBoundsPart *output, AbError *error);

/* The native child entry name is sprite#tag when an RVIO tag is nonempty,
 * otherwise the sprite identifier. No allocation or normalization. Input spans,
 * output and length must be disjoint. Append NUL; failure preserves output. */
bool ab_composite_part_name(const AbCompositePart *part, bool extended, char *output,
                            size_t capacity, size_t *length, AbError *error);

#endif
