#ifndef AB_CLASSIC_TOUCHES_H
#define AB_CLASSIC_TOUCHES_H

#include "ab/classic_behaviors.h"
#include "ab/camera_system.h"

typedef struct AbClassicTouches AbClassicTouches;

typedef struct {
    int32_t id;
    AbVec2 position;
    AbDataValue consumed;
} AbClassicTouch;

typedef struct {
    AbClassicLevel *level;
    AbClassicBehaviors *behaviors;
    AbCameraSystem *camera;
    size_t capacity, storage_bytes;
} AbClassicTouchesConfig;

typedef struct {
    size_t previous_count;
    int32_t primary;
    bool primary_present;
} AbClassicTouchesView;

/* BehaviorTouchHandler152/945 specialized to the actual fixed Museum behavior
 * classes constructed by AbClassicBehaviors. They have no active touch methods;
 * extra platform payload fields therefore have no observable reader here. The
 * projection preserves numeric touch IDs, x/y and scalar consumed (including
 * numeric zero). Active touch classes/nested consumed values remain unsupported.
 *
 * Input to handle is the actual getFilteredTouches copy in native traversal
 * order, after MenuManager filtering. Cancellation takes the cached raw filtered
 * input, in its own native order. This owner does not run menu filtering, create
 * default touches, or replace the native platform input boundary. Strings and
 * every configured owner remain borrowed through the following call/close.
 * IDs obey integer_order.h. Current/previous copies and scratch are bounded at
 * open; calls allocate nothing. Errors preserve earlier effects and stop calls.
 */
bool ab_classic_touches_open(AbClassicTouchesConfig config, AbClassicTouches **output,
                             AbError *error);
void ab_classic_touches_close(AbClassicTouches *touches);
size_t ab_classic_touches_bytes(const AbClassicTouches *touches);
AbClassicTouchesView ab_classic_touches_view(const AbClassicTouches *touches);
bool ab_classic_touches_handle(AbClassicTouches *touches, const AbClassicTouch *input,
                               size_t count, float seconds, AbDataValue *consumed,
                               AbError *error);
/* No world transform, touchId assignment or previous/primary reset on cancel. */
bool ab_classic_touches_cancel(AbClassicTouches *touches, const AbClassicTouch *input,
                               size_t count, AbError *error);

#endif
