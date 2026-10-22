#ifndef AB_IMAGE_H
#define AB_IMAGE_H

#include "ab/base.h"

typedef struct {
    uint32_t width;
    uint32_t height;
    /* Top-down RGBA8; retains stored channels without alpha-association changes. */
    AbBuffer rgba;
} AbImage;

typedef struct {
    uint32_t width, height;
    uint8_t color_type;
    bool has_transparency;
} AbPngInfo;

/* Inspect the supported noninterlaced eight-bit PNG chunk layout/CRCs without
 * allocation or DEFLATE decoding. Color type is the PNG wire value (0/2/3/4/6);
 * has_transparency reports a tRNS chunk, separately from alpha-bearing channels.
 * Invalid input preserves output. This does not validate compressed pixels. */
bool ab_png_read_info(AbSpan input, AbPngInfo *output, AbError *error);
bool ab_png_decode(AbSpan input, size_t decoded_limit, AbImage *output, AbError *error);
bool ab_pvr_decode(AbSpan input, size_t decoded_limit, AbImage *output, AbError *error);
/* Target WebP still images: profile-zero VP8 color, optional raw/palette VP8L
 * alpha, native RGBA interpolation and stored alpha. Bound all simultaneously
 * live decoder heap/output by allocation_limit. Output starts empty; failure
 * frees partial storage. Full VP8L color and alpha preprocessing are unsupported. */
bool ab_webp_decode(AbSpan input, size_t allocation_limit, AbImage *output,
                    AbError *error);
/* Inspect the supported PVR container layout without decoding pixels. RGBA4444/
 * RGBA8888 formats carry alpha; RGB565/ETC1 do not. Check dimensions and payload
 * length, without validating compressed pixel contents. Failure preserves
 * output; no allocation. Native PNG format selection remains separate. */
bool ab_pvr_has_alpha(AbSpan input, bool *output, AbError *error);
void ab_image_free(AbImage *image);

#endif
