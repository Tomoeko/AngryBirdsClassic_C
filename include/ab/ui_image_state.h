#ifndef AB_UI_IMAGE_STATE_H
#define AB_UI_IMAGE_STATE_H

#include "ab/ui_frame_tree.h"
#include "ab/ui_image_resources.h"

typedef struct {
    AbUiFrameState frame;
    AbUiPointerValue image, alpha, floor_coordinates;
    AbUiPointerValue resource_pivot_x, resource_pivot_y;
    AbUiPointerValue rotation_pivot_x, rotation_pivot_y;
    AbUiPointerValue pivot_x, pivot_y;
} AbUiImageState;

typedef struct {
    AbUiFrameFields frame;
    AbUiPointerValue image;
} AbUiImageArguments;

typedef struct {
    AbUiImageState image;
    AbUiPointerValue enabled, hover_enabled, hover_scale, hover_state, hover_count;
    AbUiPointerValue scale_on_touch, padding, click_sound, activate_on_release;
    AbUiPointerValue return_value, disabled_return_value, enabled_image, disabled_image;
    AbUiPointerValue meta, click_started, clear_pointer_flags;
} AbUiImageButtonState;

typedef struct {
    AbUiImageArguments image;
    AbUiPointerValue scale_on_touch, padding, click_sound, activate_on_release;
    AbUiPointerValue return_value, disabled_return_value, enabled_image, disabled_image;
    AbUiPointerValue meta;
} AbUiImageButtonArguments;

typedef struct {
    AbUiInitBoundaries frame;
    AbUiImageResourceBoundaries resources;
    /* Caller prepares a fresh empty padding object for each initialization
     * that needs one. Its identity/storage remains borrowed by the button.
     * Allocation timing and generic Lua-table behavior are outside this model. */
    AbUiPointerValue empty_padding;
} AbUiImageInitBoundaries;

typedef struct {
    void *context;
    bool (*set_image)(void *context, AbUiPointerValue image, AbError *error);
} AbUiImageInitSetter;

/* Typed Image resource state, retaining absent/false and arbitrary unused old
 * fields. Refresh skips false/absent image. Present resource identifiers must be
 * text. Assign bounds before pivot lookup; assign resource/rotation pivots after
 * that callback. setImage localizes, falls back from empty localization to the
 * request, then refreshes. Clearing preserves bounds/pivots. Providers are
 * borrowed; output geometry must be finite. Earlier effects remain on failure.
 * This value model and the resolved draw/resource structs are separate views. */
bool ab_ui_image_state_refresh(AbUiImageState *state,
                               const AbUiImageResourceBoundaries *boundaries,
                               AbError *error);
bool ab_ui_image_state_set_image(AbUiImageState *state, AbUiPointerValue image,
                                 const AbUiImageResourceBoundaries *boundaries,
                                 AbError *error);
bool ab_ui_image_button_state_enable(AbUiImageButtonState *state,
                                     AbUiPointerValue enabled,
                                     const AbUiImageResourceBoundaries *boundaries,
                                     AbError *error);
bool ab_ui_image_button_state_set_image(AbUiImageButtonState *state,
                                        AbUiPointerValue enabled_image,
                                        AbUiPointerValue disabled_image,
                                        const AbUiImageResourceBoundaries *boundaries,
                                        AbError *error);

/* Builtin Image/ImageButton constructor composition. NULL arguments cover
 * absent/false tables. Image calls Frame.init, then resets angle/alpha/floor and
 * rotation pivots. A truthy image argument invokes the builtin class's setter;
 * no argument leaves an old image/resource pivot/draw pivot untouched. Button
 * uses its own setter during that Image phase, before resetting enabled=true.
 * It then assigns only its original constructor fields, retaining clickStarted
 * and deferred pointer flags. Input fields remain typed; callback/custom-class
 * overrides and generic property copying are outside this contract. No allocation
 * or ownership transfer. Stable disjoint inputs; prefix effects remain on failure. */
bool ab_ui_image_state_init(AbUiImageState *state, const AbUiImageArguments *arguments,
                            const AbUiImageInitBoundaries *boundaries, AbError *error);
bool ab_ui_image_button_state_init(AbUiImageButtonState *state,
                                   const AbUiImageButtonArguments *arguments,
                                   const AbUiImageInitBoundaries *boundaries,
                                   AbError *error);
/* The source constructor's virtual self:setImage call can enter a concrete C
 * subclass before the remaining ImageButton fields are assigned. Invoke an
 * actual class setter at that point; an absent setter selects the builtin.
 * Canonical state is available; hierarchy construction has not finished. */
bool ab_ui_image_state_init_derived(AbUiImageState *state,
                                    const AbUiImageArguments *arguments,
                                    const AbUiImageInitBoundaries *boundaries,
                                    AbUiImageInitSetter setter, AbError *error);
bool ab_ui_image_button_state_init_derived(AbUiImageButtonState *state,
                                           const AbUiImageButtonArguments *arguments,
                                           const AbUiImageInitBoundaries *boundaries,
                                           AbUiImageInitSetter setter, AbError *error);

/* Builtin ImageButton.onEntry resets clickStarted/hover scale/state before
 * inherited Frame.onEntry. Hover count and deferred pointer flags survive.
 * The node must refer to this image's frame; arrays/lifecycle remain borrowed. */
bool ab_ui_image_button_state_enter(AbUiImageButtonState *state, AbUiFrameNode *node,
                                    AbUiFrameLifecycle *lifecycle, AbError *error);

#endif
