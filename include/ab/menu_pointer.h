#ifndef AB_MENU_POINTER_H
#define AB_MENU_POINTER_H

#include "ab/physics_geometry.h"
#include "ab/base.h"

typedef enum {
    AB_MENU_POINTER_PRESS,
    AB_MENU_POINTER_HOLD,
    AB_MENU_POINTER_RELEASE,
    AB_MENU_POINTER_WHEEL,
} AbMenuPointerEvent;

typedef bool (*AbMenuPointerCallback)(void *context, AbMenuPointerEvent event,
                                      AbVec2 cursor, bool *consumed, AbError *error);

typedef struct {
    AbMenuPointerCallback callback;
    void *context;
} AbMenuPointerRoute;

typedef struct {
    AbVec2 cursor;
    /* Bits follow AbMenuPointerEvent; each active event routes independently. */
    uint32_t events;
    bool allow_input;
} AbMenuPointerInput;

typedef struct {
    /* Press/hold/release slots distinguish absent from consumed false. Wheel
     * is always present, resets to false independently and uses its accepted
     * bit as the value. */
    uint32_t present;
    uint32_t accepted;
    bool copy_touches;
} AbMenuPointerResult;

/* MenuManager resetFilteredInput + the ordinary left-button/wheel portion of
 * delegateClicks. Borrow routes in original input_order; a NULL callback models
 * a missing menu. Stop each event at its first consuming menu. All active event
 * results are ANDed to choose between copying every raw touch and clearing all
 * touches; an empty event set copies touches when input is allowed.
 * Extra mouse buttons, hover scaling, keys, menu mutations, table identity/copy
 * ordering and the surrounding menu update remain separate. Finite logical
 * cursor coordinates are required. No allocation. Callback failure may retain
 * callback effects; result is unchanged until the whole operation succeeds. */
bool ab_menu_pointer_filter(const AbMenuPointerRoute *routes, size_t count,
                            const AbMenuPointerInput *input,
                            AbMenuPointerResult *output, AbError *error);

#endif
