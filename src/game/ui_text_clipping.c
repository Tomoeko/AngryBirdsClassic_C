#include "ab/ui_text_clipping.h"

#include "ab/utf8.h"

#include <stdio.h>
#include <string.h>

static bool default_name(void *context, AbUiPointerValue *output, AbError *error) {
    AbUiTextClipping *owner = context;
    size_t index = owner->next_name;
    if (index >= owner->storage.line_capacity)
        return ab_fail(error, "Clipped Text names exceed retained line storage");
    char *name = owner->storage.names[index];
    int count = snprintf(name, 40, "table: %p", (void *)(owner->storage.lines + index));
    if (count < 0 || count >= 40)
        return ab_fail(error, "Clipped Text name exceeds storage");
    ++owner->next_name;
    *output = (AbUiPointerValue){.kind = AB_UI_POINTER_TEXT,
                                 .text = {(const uint8_t *)name, (size_t)count}};
    return true;
}

static bool set_font(void *context, AbUiPointerValue font, AbError *error) {
    AbUiTextClipping *owner = context;
    AbUiInitBoundaries *resources = &owner->resources.resources;
    return resources->set_font(resources->context, font, error);
}

static bool localize(void *context, AbUiPointerValue group, AbUiPointerValue label,
                     AbUiPointerValue *output, AbError *error) {
    AbUiTextClipping *owner = context;
    AbUiInitBoundaries *resources = &owner->resources.resources;
    return resources->localize(resources->context, group, label, output, error);
}

static bool measure(void *context, AbUiPointerValue label, AbUiPointerValue *output,
                    AbError *error) {
    AbUiTextClipping *owner = context;
    AbUiInitBoundaries *resources = &owner->resources.resources;
    return resources->measure(resources->context, label, output, error);
}

static bool call_font(void *context, uintptr_t function, AbUiPointerValue *output,
                      AbError *error) {
    AbUiTextClipping *owner = context;
    AbUiInitBoundaries *resources = &owner->resources.resources;
    return resources->call_font(resources->context, function, output, error);
}

static bool cumulative_scale(void *context, AbVec2 *output, AbError *error) {
    AbUiTextClipping *owner = context;
    return ab_ui_widget_cumulative_scale(&owner->text->widget, output, error);
}

static bool leading(void *context, float *output, AbError *error) {
    AbUiTextClipping *owner = context;
    return owner->resources.leading(owner->resources.resources.context, output, error);
}

static bool wrap(void *context, AbUiPointerValue group, AbUiPointerValue label,
                 float width, AbUiWrappedText *output, AbError *error) {
    AbUiTextClipping *owner = context;
    AbUiTextClipStorage *storage = &owner->storage;
    if (group.kind != AB_UI_POINTER_TEXT || label.kind != AB_UI_POINTER_TEXT)
        return ab_fail(error, "Concrete clipText requires ordinary string arguments");
    AbUiPointerValue localized;
    if (!localize(owner, group, label, &localized, error))
        return false;
    if (localized.kind != AB_UI_POINTER_TEXT ||
        (localized.text.size &&
         (!localized.text.data || memchr(localized.text.data, 0, localized.text.size))))
        return ab_fail(error, "Unsupported concrete clipText localized string");
    size_t count;
    AbUiFontResources *fonts = owner->text->fonts;
    if (!fonts->current || !fonts->current->font ||
        !ab_utf8_to_codes(localized.text, storage->codes, storage->code_capacity,
                          &count))
        return ab_fail(error, "Missing clipText font or bounded UTF workspace");
    AbFontWrap wrapped = {.lines = storage->wrapped,
                          .capacity = storage->wrapped_capacity};
    if (!ab_font_wrap_codes(fonts->current->font, storage->codes, count, width,
                            &wrapped, error))
        return false;
    for (size_t index = 0; index < wrapped.count; ++index) {
        AbFontWrappedLine line = wrapped.lines[index];
        size_t written;
        uint8_t *bytes = storage->bytes + owner->used_bytes;
        if (!ab_utf8_from_codes(storage->codes + line.start, line.count, bytes,
                                storage->byte_capacity - owner->used_bytes, &written))
            return ab_fail(error, "Clipped Text exceeds retained UTF byte storage");
        storage->wrapped_text[index] = (AbSpan){bytes, written};
        owner->used_bytes += written;
    }
    *output = (AbUiWrappedText){storage->wrapped_text, wrapped.count,
                                (float)wrapped.widest_line};
    return true;
}

bool ab_ui_text_clipping_init(AbUiTextClipping *owner, AbUiTextClipStorage storage,
                              AbError *error) {
    if (!owner || owner->initialized || !storage.lines || !storage.names ||
        !storage.line_capacity || storage.line_capacity > UINT32_C(0x1000000) ||
        !storage.wrapped || !storage.wrapped_text || !storage.wrapped_capacity ||
        storage.wrapped_capacity > UINT32_C(0x1000000) || !storage.codes ||
        !storage.code_capacity || storage.code_capacity > INT32_MAX || !storage.bytes ||
        !storage.byte_capacity)
        return ab_fail(error, "Invalid concrete Text clipping pools");
    owner->storage = storage;
    owner->initialized = true;
    return true;
}

bool ab_ui_text_widget_clip(AbUiTextWidget *text, AbUiTextClipping *owner,
                            AbUiPointerValue own_scale, AbUiPointerValue spacing,
                            AbError *error) {
    if (!text || !text->widget.initialized || !text->widget.context ||
        text->widget.context->failed || !text->fonts || !owner || !owner->initialized ||
        owner->busy || owner->failed || (owner->text && owner->text != text) ||
        (text->clipping && text->clipping != owner))
        return ab_fail(error, "Invalid or active concrete Text clipping owner");
    owner->busy = true;
    owner->text = text;
    text->clipping = owner;
    owner->resources = ab_ui_font_resources_boundaries(text->fonts);
    /* The real builtin font/localization/measurement providers do not mutate
     * widget fields. The root projection is read-only during this call. */
    owner->generation.text = text->state;
    owner->generation.lines = owner->storage.lines + owner->used_lines;
    owner->generation.capacity = owner->storage.line_capacity - owner->used_lines;
    owner->next_name = owner->used_lines;
    AbUiInitBoundaries resources = owner->resources.resources;
    resources.context = owner;
    resources.default_name = default_name;
    resources.set_font = set_font;
    resources.localize = localize;
    resources.measure = measure;
    resources.call_font = call_font;
    AbUiTextLineBoundaries boundaries = {.init = resources,
                                         .cumulative_scale = cumulative_scale,
                                         .wrap = wrap,
                                         .font_leading = leading};
    bool okay = ab_ui_text_lines_build(&owner->generation, own_scale, spacing,
                                       &boundaries, error);
    owner->used_lines = owner->next_name;
    if (okay)
        text->clipped =
            (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN, .boolean = true};
    owner->busy = false;
    owner->failed = !okay;
    if (!okay)
        text->widget.context->failed = true;
    return okay;
}
