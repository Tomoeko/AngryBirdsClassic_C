#include "ab/ui_image_resources.h"

#include <math.h>

static bool valid_text(AbSpan value) {
    return !value.size || value.data;
}

static bool image_value(AbUiPointerValue value, bool allow_false) {
    return ab_ui_pointer_value_valid(&value) &&
           (value.kind == AB_UI_POINTER_NONE || value.kind == AB_UI_POINTER_TEXT ||
            (allow_false && value.kind == AB_UI_POINTER_BOOLEAN && !value.boolean));
}

static bool finite_point(AbVec2 value) {
    return isfinite(value.x) && isfinite(value.y);
}

bool ab_ui_image_resource_refresh(AbUiImageResources *image,
                                  const AbUiImageResourceBoundaries *boundaries,
                                  AbError *error) {
    if (!image || !boundaries) {
        return ab_fail(error, "Invalid UI image refresh input");
    }
    if (!image->draw.image_present) {
        return true;
    }
    if (!valid_text(image->draw.image) || !boundaries->bounds || !boundaries->pivot) {
        return ab_fail(error, "Missing UI image resource boundary");
    }
    AbVec2 size;
    if (!boundaries->bounds(boundaries->context, image->draw.image, &size, error)) {
        return false;
    }
    if (!finite_point(size)) {
        return ab_fail(error, "Nonfinite UI sprite bounds");
    }
    image->draw.size = size;
    AbVec2 pivot;
    if (!boundaries->pivot(boundaries->context, image->draw.image, &pivot, error)) {
        return false;
    }
    if (!finite_point(pivot)) {
        return ab_fail(error, "Nonfinite UI sprite pivot");
    }
    image->sprite_pivot = pivot;
    image->draw.rotation_pivot = pivot;
    image->draw.pivot_present |= 12;
    return true;
}

bool ab_ui_image_resource_set(AbUiImageResources *image, AbUiPointerValue requested,
                              const AbUiImageResourceBoundaries *boundaries,
                              AbError *error) {
    if (!image || !boundaries || !image_value(requested, false)) {
        return ab_fail(error, "Invalid UI image resource identifier");
    }
    if (requested.kind == AB_UI_POINTER_NONE) {
        image->draw.image = (AbSpan){0};
        image->draw.image_present = false;
    } else {
        if (!boundaries->localize) {
            return ab_fail(error, "Missing UI image localization boundary");
        }
        AbSpan localized;
        if (!boundaries->localize(boundaries->context, requested.text, &localized,
                                  error)) {
            return false;
        }
        if (!valid_text(localized)) {
            return ab_fail(error, "Invalid localized UI image identifier");
        }
        image->draw.image = localized.size ? localized : requested.text;
        image->draw.image_present = true;
    }
    return ab_ui_image_resource_refresh(image, boundaries, error);
}

bool ab_ui_image_resource_size(AbUiImageResources *image, AbVec2 size,
                               uint32_t present) {
    if (!image || (present & ~3u) || ((present & 1) && !isfinite(size.x)) ||
        ((present & 2) && !isfinite(size.y))) {
        return false;
    }
    if (present & 1) {
        image->draw.size.x = size.x;
    }
    if (present & 2) {
        image->draw.size.y = size.y;
    }
    return true;
}

bool ab_ui_image_resource_angle(AbUiImageResources *image, float angle) {
    if (!image || !isfinite(angle)) {
        return false;
    }
    image->draw.frame.angle = angle;
    return true;
}

bool ab_ui_image_button_resource_enable(AbUiImageButtonResources *button,
                                        AbUiPointerValue enabled,
                                        const AbUiImageResourceBoundaries *boundaries,
                                        AbError *error) {
    if (!button || !boundaries || !ab_ui_pointer_value_valid(&enabled)) {
        return ab_fail(error, "Invalid ImageButton enabled value");
    }
    button->enabled = enabled;
    if (ab_ui_pointer_value_truthy(&enabled)) {
        return ab_ui_image_resource_set(&button->image, button->enabled_image,
                                        boundaries, error);
    }
    if (ab_ui_pointer_value_truthy(&button->disabled_image)) {
        return ab_ui_image_resource_set(&button->image, button->disabled_image,
                                        boundaries, error);
    }
    return true;
}

bool ab_ui_image_button_resource_set(AbUiImageButtonResources *button,
                                     AbUiPointerValue enabled_image,
                                     AbUiPointerValue disabled_image,
                                     const AbUiImageResourceBoundaries *boundaries,
                                     AbError *error) {
    if (!button || !boundaries || !image_value(enabled_image, false) ||
        !image_value(disabled_image, true)) {
        return ab_fail(error, "Invalid ImageButton image identifiers");
    }
    if (!ab_ui_image_resource_set(&button->image, enabled_image, boundaries, error)) {
        return false;
    }
    button->enabled_image = enabled_image;
    button->disabled_image = disabled_image;
    return ab_ui_image_button_resource_enable(button, button->enabled, boundaries,
                                              error);
}
