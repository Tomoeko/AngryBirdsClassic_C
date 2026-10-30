#ifndef AB_UI_TILED_IMAGE_H
#define AB_UI_TILED_IMAGE_H

#include "ab/ui_widget.h"

typedef struct {
    AbUiWidget widget;
    AbSpan image;
    float repeat_x, repeat_y;
} AbUiTiledImage;

/* Target TiledImage.lua (SHA-256 494c7244...cb00b86), a Frame-derived class.
 * Borrow resource identifiers/context/children through the widget lifetime.
 * Tile positions and spacing ignore frame/parent scale, angle and local factor;
 * ordinary Frame children still receive the full draw input afterward. Layout
 * refreshes unscaled w/h. Repeat values use the source's positive numeric loop,
 * including truncation of fractional endpoints. No allocation or asset loading. */
bool ab_ui_tiled_image_init(AbUiTiledImage *image, AbUiWidgetContext *context,
                            AbUiWidgetChildren children,
                            const AbUiFrameFields *arguments, AbError *error);
bool ab_ui_tiled_image_set_image(AbUiTiledImage *image, AbSpan name, AbError *error);
bool ab_ui_tiled_image_repeat(AbUiTiledImage *image, float x, float y, AbError *error);

#endif
