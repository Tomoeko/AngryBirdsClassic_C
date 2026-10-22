#ifndef AB_UI_IMAGE_BUTTON_H
#define AB_UI_IMAGE_BUTTON_H

#include "ab/ui_pointer.h"

typedef struct {
    bool visible;
    bool image_present;
    bool activate_on_release;
    bool click_started;
    bool should_clear_pointer_flags;
    AbUiPointerValue enabled;
    AbUiPointerValue return_value;
    AbUiPointerValue disabled_return_value;
    AbUiPointerValue meta;
    AbUiPointerValue click_sound;
    uintptr_t object;
} AbUiImageButton;

typedef struct {
    void *context;
    bool (*hit_test)(void *context, AbVec2 point, bool *hit, AbError *error);
    bool (*play_audio)(void *context, AbUiPointerValue sound, float volume, bool loop,
                       AbError *error);
    bool device_ipad;
    float touch_count;
    AbVec2 frame_position;
    const AbUiPointerChild *children;
    size_t child_count;
} AbUiImageButtonBoundaries;

/* Original ImageButton pointer state over explicit nonmutating hit/audio/child
 * boundaries. Geometry/resource selection, construction, animation and menu
 * scheduling are separate. Hit callbacks run in original order, including
 * repeated queries. HOVER returns immediately before touch cancellation.
 * Activation distinguishes enabled=false from absent, and absent return values
 * from false. No extra visibility gate is applied to an already-started release.
 * Event/value text and object identities remain borrowed, points/touch count
 * must be finite. No allocation. On failure preserve reply; earlier state and
 * callback effects remain. Callbacks must not mutate the button or boundaries.
 */
bool ab_ui_image_button_handle(AbUiImageButton *button,
                               const AbUiImageButtonBoundaries *boundaries,
                               AbSpan event, AbVec2 point, AbUiPointerReply *output,
                               AbError *error);
/* Wrapper sets the deferred-clear flag for any non-absent primary other than
 * the exact text CLICK. A nil primary falls back to base child traversal. */
bool ab_ui_image_button_pointer(AbUiImageButton *button,
                                const AbUiImageButtonBoundaries *boundaries,
                                AbSpan event, AbVec2 point, AbUiPointerReply *output,
                                AbError *error);
void ab_ui_image_button_clear_flags(AbUiImageButton *button);
void ab_ui_image_button_clear_events(AbUiImageButton *button);

#endif
