#ifndef AB_CLASSIC_SLING_DRAW_H
#define AB_CLASSIC_SLING_DRAW_H

#include "ab/classic_sling.h"
#include "ab/object_sprite.h"
#include "ab/slingshot_draw.h"
#include "ab/sprite_provider.h"

typedef struct AbClassicSlingDraw AbClassicSlingDraw;

typedef struct {
    AbClassicSling *sling;
    AbClassicShots *shots;
    const AbSpriteProvider *provider;
    const AbDataValue *physics_to_world;
    const float *render_alpha;
    size_t storage_bytes;
} AbClassicSlingDrawConfig;

/* Actual base-sling calculateRubberBand8008 and back/holder/front consumers.
 * Borrow shared sling/shots/level, canonical scalar, inherited alpha and immutable
 * preloaded sprite/texture bindings. Open prepares only cached base images;
 * subsequent calls allocate nothing. Close before borrowed resources are released.
 *
 * Select the source default/KingSling back/front and optional scope front from
 * the live sling. Bind each reached powered image once from preloaded resources,
 * including the actual basic/King scope-front composites and cached parts.
 * Scope accessories/trajectory remain a separate effect draw; the ordinary
 * initializer disables extra trajectory. Used numbers must
 * fit the existing pose/draw helpers. Errors retain prior pose/command effects
 * and stop subsequent calls. No GPU work or complete frame scheduling supplied.
 */
bool ab_classic_sling_draw_open(AbClassicSlingDrawConfig config,
                                AbClassicSlingDraw **output, AbError *error);
void ab_classic_sling_draw_close(AbClassicSlingDraw *draw);
size_t ab_classic_sling_draw_bytes(const AbClassicSlingDraw *draw);
AbClassicLevel *ab_classic_sling_draw_level(const AbClassicSlingDraw *draw);
const float *ab_classic_sling_draw_alpha(const AbClassicSlingDraw *draw);
/* Call after actual controller sling updates, at the calculate boundary.
 * Read radius/type/width from the canonical world table, not initial dimensions.
 * A nil current bird skips those lookups and the conversion scalar. */
bool ab_classic_sling_draw_calculate(AbClassicSlingDraw *draw, AbError *error);
bool ab_classic_sling_draw_pose(const AbClassicSlingDraw *draw,
                                AbSlingshotPose *output);
/* Preserve back -> world/particles -> holder -> foreground -> front order at
 * the caller. Read actual dragged bird/disabled state at each part. Scalar
 * rendering camera zoom only; source values/bindings stay stable during calls. */
bool ab_classic_sling_draw_part(AbClassicSlingDraw *draw, AbCoordinateCamera camera,
                                uint32_t width, uint32_t height,
                                AbSlingshotDrawPart part, AbDrawList *list,
                                AbError *error);

#endif
