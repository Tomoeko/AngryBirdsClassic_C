#include "ab/ui_frame_draw.h"

#include <math.h>
#include <string.h>

static bool same_tag(const AbUiPointerValue *first, const AbUiPointerValue *second) {
    if (first->kind != second->kind) {
        return false;
    }
    switch (first->kind) {
        case AB_UI_POINTER_NONE:
            return true;
        case AB_UI_POINTER_BOOLEAN:
            return first->boolean == second->boolean;
        case AB_UI_POINTER_NUMBER: {
            float a, b;
            memcpy(&a, &first->number_bits, sizeof(a));
            memcpy(&b, &second->number_bits, sizeof(b));
            return a == b;
        }
        case AB_UI_POINTER_TEXT:
            return first->text.size == second->text.size &&
                   (!first->text.size ||
                    memcmp(first->text.data, second->text.data, first->text.size) == 0);
        case AB_UI_POINTER_OBJECT:
            return first->object == second->object;
        case AB_UI_POINTER_FUNCTION:
            return first->function == second->function;
    }
    return false;
}

bool ab_ui_draw_resolve(const AbUiDrawInput *input, AbUiDrawTransform *output) {
    if (!input || !output || (input->present & ~63u)) {
        return false;
    }
    AbUiDrawTransform values = input->transform;
    values.position.x = input->present & 1 ? values.position.x : 0;
    values.position.y = input->present & 2 ? values.position.y : 0;
    values.scale.x = input->present & 4 ? values.scale.x : 1;
    values.scale.y = input->present & 8 ? values.scale.y : 1;
    values.angle = input->present & 16 ? values.angle : 0;
    values.local_factor = input->present & 32 ? values.local_factor : 1;
    if (!isfinite(values.position.x) || !isfinite(values.position.y) ||
        !isfinite(values.scale.x) || !isfinite(values.scale.y) ||
        !isfinite(values.angle) || !isfinite(values.local_factor)) {
        return false;
    }
    *output = values;
    return true;
}

bool ab_ui_frame_draw(const AbUiDrawFrame *frame, const AbUiDrawChild *children,
                      size_t count, const AbUiDrawInput *input,
                      const AbUiPointerValue *filter, const AbSpan *method,
                      AbError *error) {
    AbUiDrawTransform values;
    if (!frame || !ab_ui_draw_resolve(input, &values) || count > UINT32_C(0x1000000) ||
        (count && !children) || (filter && !ab_ui_pointer_value_valid(filter)) ||
        (method && method->size && !method->data)) {
        return ab_fail(error, "Invalid UI frame draw input");
    }
    if (!isfinite(frame->position.x) || !isfinite(frame->position.y) ||
        !isfinite(frame->scale.x) || !isfinite(frame->scale.y) ||
        !isfinite(frame->angle)) {
        return ab_fail(error, "Nonfinite UI frame draw geometry");
    }
    AbUiDrawTransform next = {
        .position = {values.position.x + frame->position.x,
                     values.position.y + frame->position.y},
        .scale = {values.scale.x * frame->scale.x, values.scale.y * frame->scale.y},
        .angle = values.angle + frame->angle,
        .local_factor = values.local_factor,
    };
    if (!isfinite(next.position.x) || !isfinite(next.position.y) ||
        !isfinite(next.scale.x) || !isfinite(next.scale.y) || !isfinite(next.angle)) {
        return ab_fail(error, "Nonfinite UI frame draw composition");
    }
    const AbSpan default_method = {(const uint8_t *)"draw", 4};
    AbSpan selected_method = method ? *method : default_method;
    for (size_t index = 0; index < count; ++index) {
        const AbUiDrawChild *child = children + index;
        if (!child->visible) {
            continue;
        }
        if (filter && filter->kind != AB_UI_POINTER_NONE) {
            if (!ab_ui_pointer_value_valid(&child->tag)) {
                return ab_fail(error, "Invalid UI child draw tag");
            }
            if (!same_tag(filter, &child->tag)) {
                continue;
            }
        }
        if (!child->callback) {
            return ab_fail(error, "Missing eligible UI child draw callback");
        }
        if (!child->callback(child->context, selected_method, &next, error)) {
            return false;
        }
    }
    return true;
}
