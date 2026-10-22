#ifndef AB_UI_POINTER_H
#define AB_UI_POINTER_H

#include "ab/base.h"
#include "ab/physics_geometry.h"

typedef enum {
    AB_UI_POINTER_NONE,
    AB_UI_POINTER_BOOLEAN,
    AB_UI_POINTER_NUMBER,
    AB_UI_POINTER_TEXT,
    AB_UI_POINTER_OBJECT,
    AB_UI_POINTER_FUNCTION,
} AbUiPointerKind;

typedef struct {
    AbUiPointerKind kind;
    union {
        bool boolean;
        uint32_t number_bits;
        AbSpan text;
        uintptr_t object;
        uintptr_t function;
    };
} AbUiPointerValue;

typedef struct {
    AbUiPointerValue values[3];
} AbUiPointerReply;

typedef bool (*AbUiPointerCallback)(void *context, AbSpan event, AbVec2 point,
                                    AbUiPointerReply *reply, AbError *error);

typedef struct {
    AbUiPointerCallback callback;
    void *context;
    bool visible;
    bool active;
} AbUiPointerChild;

/* Original UI truthiness: only absent and Boolean false are false. Numbers
 * (including zero/nonfinite payloads), empty text and opaque objects are true.
 * Text bytes remain borrowed; object/function tokens remain caller-defined.
 * Functions carry identity only; invoking them requires an explicit boundary. */
bool ab_ui_pointer_value_truthy(const AbUiPointerValue *value);
bool ab_ui_pointer_value_valid(const AbUiPointerValue *value);

/* Base Frame.onPointerEvent over a dense, stable borrowed child array. Visit
 * children last to first; skip invisible/inactive children. Each eligible child
 * receives point minus this frame's position, without scale, scroll or angle.
 * Return all three values from the first reply with a non-absent primary, even
 * Boolean false. No reply writes an all-absent result. Self visibility/activity
 * is a caller gate; this base method does not inspect it.
 * Geometry must be finite, event text and reply text/object lifetimes are
 * borrowed, and callbacks must not mutate the array or geometry during dispatch.
 * Eligible missing callbacks reject when reached. No allocation or UI layout.
 * Child count is bounded to the exact binary32 integer loop range (2^24).
 * Failure preserves output but preceding callback effects remain. */
bool ab_ui_frame_pointer(AbVec2 position, const AbUiPointerChild *children,
                         size_t count, AbSpan event, AbVec2 point,
                         AbUiPointerReply *output, AbError *error);

#endif
