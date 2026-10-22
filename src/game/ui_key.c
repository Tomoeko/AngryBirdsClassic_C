#include "ab/ui_key.h"

bool ab_ui_frame_key(const AbUiKeyChild *children, size_t count, AbSpan event,
                     AbSpan key, AbUiPointerReply *output, AbError *error) {
    if (!output || (count && !children) || count > UINT32_C(0x1000000) ||
        (event.size && !event.data) || (key.size && !key.data)) {
        return ab_fail(error, "Invalid frame key routes or text");
    }
    for (size_t remaining = count; remaining; --remaining) {
        const AbUiKeyChild *child = children + remaining - 1;
        if (!child->visible || !child->active) {
            continue;
        }
        if (!child->callback) {
            return ab_fail(error, "Missing eligible frame key method");
        }
        AbUiPointerReply reply = {0};
        if (!child->callback(child->context, event, key, &reply, error)) {
            return false;
        }
        for (size_t index = 0; index < 3; ++index) {
            if (!ab_ui_pointer_value_valid(reply.values + index)) {
                return ab_fail(error, "Invalid frame key reply value");
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
