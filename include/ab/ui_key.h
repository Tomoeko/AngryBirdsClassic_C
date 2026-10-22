#ifndef AB_UI_KEY_H
#define AB_UI_KEY_H

#include "ab/ui_pointer.h"

typedef bool (*AbUiKeyCallback)(void *context, AbSpan event, AbSpan key,
                                AbUiPointerReply *reply, AbError *error);

typedef struct {
    AbUiKeyCallback callback;
    void *context;
    bool visible, active;
} AbUiKeyChild;

/* Original Frame.onKeyEvent over stable borrowed child routes. Visit children
 * last to first, skip invisible/inactive children and return all three values
 * from the first non-absent primary, including false. Self visibility/activity
 * is unused. No allocation. Text/replies remain borrowed; callbacks must not
 * mutate the traversed routes. Failure preserves output and prior effects. */
bool ab_ui_frame_key(const AbUiKeyChild *children, size_t count, AbSpan event,
                     AbSpan key, AbUiPointerReply *output, AbError *error);

#endif
