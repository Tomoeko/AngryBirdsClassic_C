#ifndef AB_UI_IMAGE_RESOURCES_H
#define AB_UI_IMAGE_RESOURCES_H

#include "ab/ui_image_draw.h"

typedef struct {
    AbUiImageDraw draw;
    /* Original px/py resource pivot, separate from pivotX/pivotY draw overrides. */
    AbVec2 sprite_pivot;
} AbUiImageResources;

typedef struct {
    void *context;
    bool (*localize)(void *context, AbSpan requested, AbSpan *localized,
                     AbError *error);
    bool (*bounds)(void *context, AbSpan image, AbVec2 *size, AbError *error);
    bool (*pivot)(void *context, AbSpan image, AbVec2 *pivot, AbError *error);
} AbUiImageResourceBoundaries;

/* Original Image refresh/setImage over explicit stable localization/sprite
 * lookups. Only absent or text image values are supported. Empty localization
 * falls back to the requested identifier. Clearing an image preserves size and
 * pivots. A present image refreshes bounds then resource pivot, copying that
 * pivot into rotatePivotX/Y while preserving pivotX/Y draw overrides.
 * Output text borrows its provider and must remain valid while bound. Finite
 * geometry is required. No allocation, ownership transfer, decoding or GPU.
 * Earlier state/callback effects remain on failure, matching successful prefix
 * order; native malformed/error/unwinding behavior is outside this contract. */
bool ab_ui_image_resource_refresh(AbUiImageResources *image,
                                  const AbUiImageResourceBoundaries *boundaries,
                                  AbError *error);
bool ab_ui_image_resource_set(AbUiImageResources *image, AbUiPointerValue requested,
                              const AbUiImageResourceBoundaries *boundaries,
                              AbError *error);
/* Optional numeric width/height selected by mask bits1/2. Zero is present.
 * Angle stores the supplied finite number without normalization. */
bool ab_ui_image_resource_size(AbUiImageResources *image, AbVec2 size,
                               uint32_t present);
bool ab_ui_image_resource_angle(AbUiImageResources *image, float angle);

typedef struct {
    AbUiImageResources image;
    AbUiPointerValue enabled;
    AbUiPointerValue enabled_image;
    AbUiPointerValue disabled_image;
} AbUiImageButtonResources;

/* Original ImageButton setEnabled/setImage resource paths. Enabled retains its
 * complete typed value. Truthy enabled always sets enabledImage, even absent;
 * false/absent enabled only sets a truthy disabledImage, otherwise leaves image.
 * setImage first performs Image.setImage(enabledImage), then stores raw image
 * arguments and reapplies current enabled, preserving repeated refresh calls.
 * Enabled image is absent/text; disabled image may also be Boolean false.
 * Pointer click/animation state and constructors are separate value models. */
bool ab_ui_image_button_resource_enable(AbUiImageButtonResources *button,
                                        AbUiPointerValue enabled,
                                        const AbUiImageResourceBoundaries *boundaries,
                                        AbError *error);
bool ab_ui_image_button_resource_set(AbUiImageButtonResources *button,
                                     AbUiPointerValue enabled_image,
                                     AbUiPointerValue disabled_image,
                                     const AbUiImageResourceBoundaries *boundaries,
                                     AbError *error);

#endif
