#ifndef AB_COMPOSITE_DRAW_H
#define AB_COMPOSITE_DRAW_H

#include "ab/composite_bounds.h"
#include "ab/image_rectangle.h"

/* Native CompoSprite/Sprite transformed points for one enabled part, with a
 * finite nonzero signed context scales, native rotation axes and an already
 * resolved composite anchor. The point overload ignores context rotation
 * origins. Input and output borrow no storage. No integer corner truncation
 * here. Direct vertices remain unsupported. */
bool ab_composite_draw_points(const AbCompositeBoundsPart *part,
                              AbSpriteRenderState parent, AbVec2 position,
                              AbVec2 output[4], AbError *error);

#endif
