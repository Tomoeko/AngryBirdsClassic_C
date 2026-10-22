#ifndef AB_COMMON_RENDERER_H
#define AB_COMMON_RENDERER_H

#include "ab/draw.h"
#include "ab/image.h"

#include "console_common/platform/platform.h"

typedef struct AbCommonRenderer AbCommonRenderer;

/* The borrowed Common platform outlives the renderer. Resources and fixed
 * command/vertex capacity are prepared outside frames; render never allocates
 * application storage. Both Common backends consume the same AbDrawList.
 * Texture identifiers remain caller-selected, from one through texture_capacity. */
bool ab_common_create(CcPlatform *platform, size_t draw_capacity,
                      size_t texture_capacity, size_t allocation_limit,
                      AbCommonRenderer **output, AbError *error);
bool ab_common_upload(AbCommonRenderer *renderer, uint32_t texture,
                      const AbImage *image, bool repeat_s, bool repeat_t,
                      AbError *error);
/* Release a texture slot on its owning thread outside a frame. Empty slots
 * succeed. Common retains already submitted GPU work through completion,
 * without waiting here; subsequent draws cannot use the released ID. */
bool ab_common_unload(AbCommonRenderer *renderer, uint32_t texture, AbError *error);
bool ab_common_render(AbCommonRenderer *renderer, const AbDrawList *list,
                      const float clear[4], AbError *error);
/* Full-drawable presentation in native backing pixels. The caller queries the
 * platform before layout and supplies those same dimensions here. Both shaders
 * and clipping use this view; the legacy render entry retains its fixed profile. */
bool ab_common_render_native(AbCommonRenderer *renderer, const AbDrawList *list,
                             const float clear[4], int width, int height,
                             AbError *error);
bool ab_common_wait(AbCommonRenderer *renderer, AbError *error);
void ab_common_free(AbCommonRenderer *renderer);

#endif
