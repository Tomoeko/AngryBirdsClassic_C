#include "ab/ui_font_resources.h"

#include "ab/utf8.h"

#include <math.h>
#include <string.h>

static AbUiPointerValue number(float value) {
    AbUiPointerValue output = {.kind = AB_UI_POINTER_NUMBER};
    memcpy(&output.number_bits, &value, sizeof(value));
    return output;
}

static bool text(AbUiPointerValue value, AbSpan *output, AbError *error) {
    if (value.kind != AB_UI_POINTER_TEXT || (value.text.size && !value.text.data)) {
        ab_fail(error, "Expected concrete UI font resource text");
        return false;
    }
    *output = value.text;
    return true;
}

static bool valid(const AbUiFontResources *resources, AbError *error) {
    return resources && resources->count <= UINT32_C(16777216) &&
                   (!resources->count || resources->bindings) && resources->capacity &&
                   resources->codes && resources->glyphs && resources->strings &&
                   resources->getter_count <= UINT32_C(16777216) &&
                   (!resources->getter_count || resources->getters)
               ? true
               : ab_fail(error, "Missing concrete UI font resources/workspace");
}

static bool select_font(void *context, AbUiPointerValue value, AbError *error) {
    AbUiFontResources *resources = context;
    AbSpan name;
    if (!valid(resources, error) || !text(value, &name, error))
        return false;
    for (size_t index = resources->count; index; --index) {
        const AbUiFontBinding *binding = resources->bindings + index - 1;
        if (binding->name.size == name.size &&
            (!name.size || (binding->name.data &&
                            !memcmp(binding->name.data, name.data, name.size)))) {
            if (!binding->font)
                return ab_fail(error, "Missing concrete UI font metadata");
            resources->current = binding;
            break;
        }
    }
    return true;
}

static bool localize(void *context, AbUiPointerValue group, AbUiPointerValue label,
                     AbUiPointerValue *output, AbError *error) {
    AbUiFontResources *resources = context;
    AbSpan group_name, identifier, localized;
    if (!valid(resources, error) || !text(group, &group_name, error) ||
        !text(label, &identifier, error) ||
        !ab_text_resources_get(resources->strings, group_name, identifier, &localized,
                               error))
        return false;
    *output = (AbUiPointerValue){.kind = AB_UI_POINTER_TEXT, .text = localized};
    return true;
}

static bool decode(AbUiFontResources *resources, AbSpan label, size_t *length,
                   AbError *error) {
    return ab_utf8_to_codes(label, resources->codes, resources->capacity, length) ||
           ab_fail(error, "Concrete UI text exceeds UTF workspace");
}

static bool measure(void *context, AbUiPointerValue value, AbUiPointerValue *output,
                    AbError *error) {
    AbUiFontResources *resources = context;
    AbSpan label;
    size_t length;
    AbFontMeasure measured;
    if (!valid(resources, error) || !text(value, &label, error) ||
        !decode(resources, label, &length, error))
        return false;
    if (!resources->current ||
        !ab_font_measure_codes(resources->current->font, resources->codes, length, 0,
                               length, &measured))
        return ab_fail(error, "Missing or invalid current UI font measurement");
    *output = number((float)measured.width);
    return true;
}

static bool leading(void *context, float *output, AbError *error) {
    AbUiFontResources *resources = context;
    if (!valid(resources, error) || !resources->current || !resources->current->font)
        return ab_fail(error, "Missing current UI font leading");
    *output = (float)resources->current->font->leading;
    return true;
}

static bool call_font(void *context, uintptr_t identity, AbUiPointerValue *output,
                      AbError *error) {
    AbUiFontResources *resources = context;
    if (!valid(resources, error) || !output)
        return false;
    for (size_t index = 0; index < resources->getter_count; ++index) {
        const AbUiFontGetter *getter = resources->getters + index;
        if (identity != (uintptr_t)getter)
            continue;
        return getter->call
                   ? getter->call(getter->context, output, error)
                   : ab_fail(error, "Missing actual concrete Text font getter");
    }
    return ab_fail(error, "Unregistered concrete Text font getter identity");
}

AbUiTextMetricBoundaries ab_ui_font_resources_boundaries(AbUiFontResources *resources) {
    return (AbUiTextMetricBoundaries){
        .resources = {.context = resources,
                      .set_font = select_font,
                      .localize = localize,
                      .measure = measure,
                      .call_font = call_font,
                      .screen_width = number(resources ? resources->screen_width : 0),
                      .basic_font = {.kind = AB_UI_POINTER_TEXT,
                                     .text = {(const uint8_t *)"FONT_BASIC", 10}},
                      .font_setting_available = true},
        .leading = leading};
}

bool ab_ui_font_resources_draw(AbUiFontResources *resources, AbUiWidgetContext *context,
                               const AbUiTextDrawPlan *plan, AbError *error) {
    if (!plan || !context || context->failed || !context->render_state ||
        !context->draw_list)
        return ab_fail(error, "Missing concrete UI text draw context");
    if (!plan->draw)
        return true;
    AbUiPointerValue selected = {.kind = AB_UI_POINTER_TEXT, .text = plan->font};
    if (!select_font(resources, selected, error))
        return false;
    if (!resources->current)
        return ab_fail(error, "No font selected for concrete UI text");
    AbUiPointerValue localized;
    if (!localize(resources,
                  (AbUiPointerValue){.kind = AB_UI_POINTER_TEXT, .text = plan->group},
                  (AbUiPointerValue){.kind = AB_UI_POINTER_TEXT, .text = plan->text},
                  &localized, error))
        return false;
    size_t length, count;
    if (!decode(resources, localized.text, &length, error))
        return false;
    AbUiImageRenderState *state = context->render_state;
    state->alpha = plan->draw_alpha;
    state->geometry.scale_x = plan->scale.x;
    state->geometry.scale_y = plan->scale.y;
    state->geometry.angle_radians = plan->angle;
    state->geometry.rotation_origin_x = plan->layout.rotation_origin_x;
    state->geometry.rotation_origin_y = plan->layout.rotation_origin_y;
    if (!ab_font_layout_codes(resources->current->font, resources->codes, length, 0,
                              length, plan->layout, resources->glyphs,
                              resources->capacity, &count))
        return ab_fail(error, "Cannot lay out concrete UI font glyphs");
    const AbUiFontBinding *binding = resources->current;
    AbFontNativeDrawStyle style = {.texture = binding->texture,
                                   .texture_width = binding->texture_width,
                                   .texture_height = binding->texture_height,
                                   .viewport_width = context->render.viewport_width,
                                   .viewport_height = context->render.viewport_height,
                                   .glyph_scale_x = 1,
                                   .glyph_scale_y = 1,
                                   .state = state->geometry,
                                   .blend = context->render.blend,
                                   .opacity = state->alpha};
    memcpy(style.color, context->render.color, sizeof(style.color));
    size_t first = context->draw_list->count;
    if (!ab_font_draw_native(resources->glyphs, count, style, context->draw_list,
                             error))
        return false;
    for (size_t index = first; index < context->draw_list->count; ++index)
        context->draw_list->commands[index].scissor = state->clip;
    state->alpha = plan->after_alpha;
    return true;
}
