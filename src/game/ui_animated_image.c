#include "ab/ui_animated_image.h"

#include <math.h>
#include <string.h>

static AbUiPointerValue number(float value) {
    AbUiPointerValue output = {.kind = AB_UI_POINTER_NUMBER};
    memcpy(&output.number_bits, &value, sizeof(value));
    return output;
}

static bool numeric(AbUiPointerValue value, float *output, AbError *error) {
    if (value.kind == AB_UI_POINTER_NUMBER) {
        memcpy(output, &value.number_bits, sizeof(*output));
        if (isfinite(*output))
            return true;
    }
    return ab_fail(error, "AnimatedImage requires a finite numeric field");
}

static bool count_frames(const AbDataTable *frames, size_t *output, AbError *error) {
    if (!frames || frames->count > 16777216 || (frames->count && !frames->entries) ||
        frames->count > frames->capacity)
        return ab_fail(error, "Missing or unsupported AnimatedImage frame table");
    size_t count = 0;
    while (count < frames->count) {
        AbDataValue key = {.kind = AB_DATA_NUMBER,
                           .bits = number((float)(count + 1)).number_bits};
        const AbDataValue *value = ab_data_get(frames, key);
        if (!value || value->kind == AB_DATA_NIL)
            break;
        ++count;
    }
    for (size_t index = 0; index < frames->count; ++index) {
        const AbDataEntry *entry = frames->entries + index;
        if (entry->key.kind != AB_DATA_NUMBER || entry->value.kind == AB_DATA_NIL)
            continue;
        float key;
        if (!numeric((AbUiPointerValue){.kind = AB_UI_POINTER_NUMBER,
                                        .number_bits = entry->key.bits},
                     &key, error))
            return false;
        if (key >= 1 && truncf(key) == key && key > (float)count)
            return ab_fail(error, "Sparse AnimatedImage frame table is unsupported");
    }
    *output = count;
    return true;
}

static bool healthy(AbUiAnimatedImage *image, AbError *error) {
    return (image && image->initialized && image->widget.context &&
            !image->widget.context->failed) ||
           ab_fail(error, "Invalid or stopped AnimatedImage");
}

static bool finish(AbUiAnimatedImage *image, bool okay) {
    if (!okay)
        image->widget.context->failed = true;
    return okay;
}

bool ab_ui_animated_image_frames(AbUiAnimatedImage *image, const AbDataTable *frames,
                                 AbError *error) {
    if (!healthy(image, error))
        return false;
    image->frames = frames;
    image->timer = number(0);
    return true;
}

bool ab_ui_animated_image_current(AbUiAnimatedImage *image, float frame,
                                  AbError *error) {
    if (!healthy(image, error))
        return false;
    size_t count;
    if (!isfinite(frame) || !count_frames(image->frames, &count, error) || !count)
        return finish(image, ab_fail(error, "Invalid AnimatedImage frame selection"));
    float selected = frame < 1 ? 1 : frame;
    if (selected > (float)count)
        selected = (float)count;
    image->timer = number((selected - 1) / (float)count);
    return true;
}

static bool update(void *context, AbUiWidget *widget, float seconds, float time,
                   AbError *error) {
    AbUiAnimatedImage *image = context;
    if (!ab_ui_widget_update_base(widget, seconds, time, error))
        return false;
    if (ab_ui_pointer_value_truthy(&image->stopped))
        return true;
    float timer, speed;
    if (!numeric(image->timer, &timer, error) || !numeric(image->speed, &speed, error))
        return false;
    timer = timer + seconds * speed;
    image->timer = number(timer);
    if (!isfinite(timer))
        return ab_fail(error, "AnimatedImage timer overflow");
    if (timer >= 1) {
        if (ab_ui_pointer_value_truthy(&image->looping)) {
            timer -= 1;
            image->timer = number(timer);
        } else {
            image->stopped =
                (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN, .boolean = true};
            image->current_frame = number(0);
            return true;
        }
    }
    size_t count;
    if (!count_frames(image->frames, &count, error))
        return false;
    float selected = floorf(timer * (float)count) + 1;
    image->current_frame = number(selected);
    return isfinite(selected) || ab_fail(error, "AnimatedImage frame overflow");
}

static bool draw(void *context, AbUiWidget *widget, const AbUiDrawInput *input,
                 AbError *error) {
    AbUiAnimatedImage *image = context;
    if (!image->frames)
        return ab_fail(error, "AnimatedImage draw indexes an absent frames table");
    AbDataValue key = {0};
    if (image->current_frame.kind == AB_UI_POINTER_NUMBER)
        key = (AbDataValue){.kind = AB_DATA_NUMBER,
                            .bits = image->current_frame.number_bits};
    else if (image->current_frame.kind != AB_UI_POINTER_NONE)
        return ab_fail(error, "Unsupported AnimatedImage frame index type");
    const AbDataValue *selected = ab_data_get(image->frames, key);
    AbUiPointerValue sprite = {.kind = AB_UI_POINTER_TEXT};
    if (selected && selected->kind != AB_DATA_NIL &&
        (selected->kind != AB_DATA_BOOLEAN || selected->bits)) {
        if (selected->kind != AB_DATA_STRING)
            return ab_fail(error, "Unsupported AnimatedImage frame image type");
        sprite.text = selected->string;
    }
    return ab_ui_image_state_set_image(&widget->state.image, sprite,
                                       &widget->context->images, error) &&
           ab_ui_widget_draw_base(widget, input, error);
}

bool ab_ui_animated_image_init(AbUiAnimatedImage *image, AbUiWidgetContext *context,
                               AbUiWidgetChildren children,
                               const AbUiImageArguments *arguments, AbError *error) {
    if (!image)
        return ab_fail(error, "Missing AnimatedImage storage");
    if (!ab_ui_widget_init(&image->widget, context, AB_UI_WIDGET_IMAGE, children, NULL,
                           arguments, NULL, error))
        return false;
    image->frames = &image->empty_frames;
    image->timer = number(0);
    image->speed = number(1);
    image->looping = image->stopped = (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN};
    image->widget.methods =
        (AbUiWidgetMethods){.context = image, .draw = draw, .update = update};
    image->initialized = true;
    return true;
}
