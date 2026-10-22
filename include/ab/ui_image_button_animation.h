#ifndef AB_UI_IMAGE_BUTTON_ANIMATION_H
#define AB_UI_IMAGE_BUTTON_ANIMATION_H

#include "ab/ui_image_button.h"

typedef enum {
    AB_UI_HOVER_NONE,
    AB_UI_HOVER_ENTER,
    AB_UI_HOVER_HOVER,
    AB_UI_HOVER_LEAVE,
    AB_UI_HOVER_OTHER,
} AbUiHoverState;

typedef struct {
    AbVec2 position;
    float hover_scale;
    float hover_count;
    AbUiHoverState hover_state;
    bool hover_enabled;
    bool scale_on_touch;
} AbUiImageButtonAnimation;

typedef struct {
    void *context;
    bool (*image_update)(void *context, float seconds, float game_time, AbError *error);
    bool (*screen_position)(void *context, AbVec2 *position, AbError *error);
    bool (*hit_test)(void *context, AbVec2 point, bool *hit, AbError *error);
    AbVec2 cursor;
    bool scale_buttons_on_touch;
    bool left_button_held;
} AbUiImageButtonAnimationBoundaries;

/* Original enter/leave count is float, unbounded and unclamped: ENTER begins
 * only when the incremented count equals one; LEAVE when it decrements to zero.
 * Disabled hover still changes the count. Finite state/results are required. */
bool ab_ui_image_button_mouse_enter(AbUiImageButtonAnimation *animation);
bool ab_ui_image_button_mouse_leave(AbUiImageButtonAnimation *animation);
/* Original tweenLinear(hover_scale,1,0.1,1), or one when hover is disabled. */
bool ab_ui_image_button_hover_scale(const AbUiImageButtonAnimation *animation,
                                    float *output);

/* Original Image.update boundary runs first, then deferred flags clear. Touch
 * scaling temporarily replaces position with the screen-position query, even
 * when no press qualifies. A qualifying held enabled press decreases scale by
 * seconds*20 down to -1 and suppresses hover transitions for this update.
 * Otherwise NONE returns toward zero only while touch scaling is enabled;
 * ENTER/LEAVE change by seconds*6.66 with the original clamp/state transitions.
 * Boundaries must not mutate button/animation. Finite used arithmetic and time
 * are required. No allocation. Earlier effects remain on failure; temporary
 * position is restored after a hit callback, including a failed callback.
 * Layout, real image/child update effects and drawing remain separate. */
bool ab_ui_image_button_update(AbUiImageButton *button,
                               AbUiImageButtonAnimation *animation,
                               const AbUiImageButtonAnimationBoundaries *boundaries,
                               float seconds, float game_time, AbError *error);

/* Same continuation after the caller has executed actual Image.update. This
 * permits canonical adapters to read fields after child callbacks, preserving
 * their mutations. No image_update call is made by this entry point. */
bool ab_ui_image_button_update_after_image(
    AbUiImageButton *button, AbUiImageButtonAnimation *animation,
    const AbUiImageButtonAnimationBoundaries *boundaries, float seconds,
    AbError *error);

#endif
