#ifndef AB_METAL_H
#define AB_METAL_H

#include "ab/draw.h"
#include "ab/image.h"

typedef enum { AB_METAL_RGBA8, AB_METAL_BGRA8, AB_METAL_RGB565 } AbMetalFormat;
typedef struct AbMetal AbMetal;

/* Apple-only C boundary. Device and encoder are native Objective-C objects.
 * Retains the device and owns pipelines, samplers, and uploaded textures.
 * Encoder must target a single-sample attachment of the selected format.
 * The caller owns command-buffer submission, completion, and presentation.
 * Pixel and already normalized clip commands share this boundary. RGB565 is
 * an optional attachment format; unsupported devices return a creation error.
 */
bool ab_metal_create(void *device, AbMetalFormat format, size_t texture_capacity,
                     size_t allocation_limit, AbMetal **output, AbError *error);
bool ab_metal_upload(AbMetal *renderer, uint32_t texture, const AbImage *image,
                     bool repeat_s, bool repeat_t, AbError *error);
bool ab_metal_remove_texture(AbMetal *renderer, uint32_t texture, AbError *error);
bool ab_metal_render(AbMetal *renderer, void *encoder, const AbDrawList *list,
                     uint32_t width, uint32_t height, AbError *error);
/* Explicit offscreen capture, not a normal per-frame presentation path. Owns
 * successful top-down RGBA8 output; caller releases it with ab_image_free.
 * Clear is RGBA in [0,1]. Checks the pixel budget before allocating, waits for
 * completion and reports native GPU errors. RGB565 captures expand normalized
 * stored channels to RGBA8 with opaque alpha. Output starts empty. */
bool ab_metal_capture(AbMetal *renderer, const AbDrawList *list, uint32_t width,
                      uint32_t height, const float clear[4], size_t decoded_limit,
                      AbImage *output, AbError *error);
void ab_metal_free(AbMetal *renderer);

#endif
