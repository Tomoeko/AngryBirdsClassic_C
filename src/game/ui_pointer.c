#include "ab/ui_pointer.h"

#include <math.h>

bool ab_ui_pointer_value_truthy(const AbUiPointerValue *value) {
    if (!value || (unsigned)value->kind > AB_UI_POINTER_FUNCTION) {
        return false;
    }
    return value->kind != AB_UI_POINTER_NONE &&
           (value->kind != AB_UI_POINTER_BOOLEAN || value->boolean);
}

bool ab_ui_pointer_value_valid(const AbUiPointerValue *value) {
    return value && (unsigned)value->kind <= AB_UI_POINTER_FUNCTION &&
           (value->kind != AB_UI_POINTER_TEXT || !value->text.size || value->text.data);
}

bool ab_ui_frame_pointer(AbVec2 position, const AbUiPointerChild *children,
                         size_t count, AbSpan event, AbVec2 point,
                         AbUiPointerReply *output, AbError *error) {
    if (!output || count > UINT32_C(0x1000000) || (count && !children) ||
        (event.size && !event.data) || !isfinite(position.x) || !isfinite(position.y) ||
        !isfinite(point.x) || !isfinite(point.y)) {
        return ab_fail(error, "Invalid UI frame pointer input");
    }
    for (size_t cursor = count; cursor > 0;) {
        const AbUiPointerChild *child = children + --cursor;
        if (!child->visible || !child->active) {
            continue;
        }
        AbVec2 local = {point.x - position.x, point.y - position.y};
        if (!child->callback || !isfinite(local.x) || !isfinite(local.y)) {
            return ab_fail(error,
                           "Invalid eligible UI pointer callback or coordinates");
        }
        AbUiPointerReply reply = {0};
        if (!child->callback(child->context, event, local, &reply, error)) {
            return false;
        }
        for (size_t index = 0; index < 3; ++index) {
            if (!ab_ui_pointer_value_valid(reply.values + index)) {
                return ab_fail(error, "Unsupported UI pointer reply value");
            }
        }
        if (reply.values[0].kind != AB_UI_POINTER_NONE) {
            *output = reply;
            return true;
        }
    }
    *output = (AbUiPointerReply){0};
    return true;
}
