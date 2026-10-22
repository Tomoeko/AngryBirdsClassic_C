#include "ab/ui_scaling.h"

#include "ab/asset_profile.h"

#include <math.h>
#include <string.h>

static bool valid(const AbUiScaling *scaling, AbError *error) {
    return scaling && scaling->initialized && !scaling->failed &&
                   scaling->host.environment
               ? true
               : ab_fail(error, "Invalid or failed UI Scaling owner");
}

static bool dimensions(AbVec2 size) {
    return isfinite(size.x) && isfinite(size.y) && size.x > 0 && size.y > 0;
}

static bool select_scale(AbUiScaling *scaling, AbUiScalingEnvironment environment,
                         AbError *error) {
    if (!dimensions(scaling->default_profile))
        return ab_fail(error, "Unsupported UI Scaling default profile dimensions");
    scaling->default_aspect_ratio =
        scaling->default_profile.x / scaling->default_profile.y;
    if (!environment.use_scaling) {
        scaling->default_scaling = 1;
        return true;
    }
    if (!dimensions(environment.screen))
        return ab_fail(error, "Unsupported UI Scaling logical screen dimensions");
    float ratio = environment.screen.x / environment.screen.y;
    scaling->default_scaling = ratio < scaling->default_aspect_ratio
                                   ? environment.screen.x / scaling->default_profile.x
                                   : environment.screen.y / scaling->default_profile.y;
    return isfinite(scaling->default_scaling) ||
           ab_fail(error, "Nonfinite UI Scaling selection");
}

bool ab_ui_scaling_update(AbUiScaling *scaling, AbError *error) {
    if (!valid(scaling, error) || scaling->busy)
        return ab_fail(error, "Reentrant or failed UI Scaling update");
    scaling->busy = true;
    AbUiScalingEnvironment environment;
    bool okay = scaling->host.environment(scaling->host.context, &environment, error) &&
                select_scale(scaling, environment, error);
    scaling->busy = false;
    scaling->failed = !okay;
    return okay;
}

static bool same(AbDataValue first, AbDataValue second) {
    if (first.kind != second.kind)
        return false;
    if (first.kind == AB_DATA_STRING)
        return first.string.size == second.string.size &&
               (!first.string.size ||
                !memcmp(first.string.data, second.string.data, first.string.size));
    if (first.kind == AB_DATA_NUMBER) {
        float left, right;
        memcpy(&left, &first.bits, sizeof(left));
        memcpy(&right, &second.bits, sizeof(right));
        return left == right;
    }
    return false;
}

static bool event_triggered(void *context, AbDataTable *event, AbError *error) {
    AbUiScaling *scaling = context;
    if (!valid(scaling, error) || scaling->busy)
        return ab_fail(error, "Reentrant or failed UI Scaling event");
    const AbDataValue *id =
        ab_data_get(event, (AbDataValue){.kind = AB_DATA_STRING,
                                         .string = {(const uint8_t *)"id", 2}});
    if (!id || !same(*id, scaling->host.resolution_event))
        return true;
    AbUiScalingEnvironment environment;
    scaling->busy = true;
    bool okay = scaling->host.environment(scaling->host.context, &environment, error);
    if (okay && environment.use_scaling)
        okay = select_scale(scaling, environment, error);
    scaling->busy = false;
    scaling->failed = !okay;
    return okay;
}

bool ab_ui_scaling_init(AbUiScaling *scaling, AbUiScalingHost host, AbError *error) {
    if (!scaling || scaling->initialized || scaling->registered || !host.events ||
        !host.environment)
        return ab_fail(error, "Invalid UI Scaling initialization/host");
    *scaling = (AbUiScaling){.host = host,
                             .default_aspect_ratio = 1,
                             .default_scaling = 1,
                             .initialized = true,
                             .busy = true};
    scaling->listener = (AbEventListener){.name = {(const uint8_t *)"Scaling", 7},
                                          .context = scaling,
                                          .event_triggered = event_triggered};
    bool okay = ab_event_manager_add(host.events, host.resolution_event,
                                     &scaling->listener, error);
    scaling->registered = okay;
    AbUiScalingEnvironment environment;
    if (okay)
        okay = host.environment(host.context, &environment, error);
    if (okay) {
        if (environment.template_layout)
            scaling->default_profile = environment.reference_size;
        else {
            okay = ab_android_default_profile(environment.screen.x,
                                              &scaling->default_profile);
            if (!okay)
                ab_fail(error, "Cannot select UI Scaling Android default profile");
        }
    }
    if (okay)
        okay = select_scale(scaling, environment, error);
    scaling->busy = false;
    scaling->failed = !okay;
    return okay;
}

bool ab_ui_scaling_detach(AbUiScaling *scaling, AbError *error) {
    if (!scaling || scaling->busy)
        return ab_fail(error, "Missing or active UI Scaling lifetime owner");
    if (scaling->registered &&
        !ab_event_manager_remove(scaling->host.events, scaling->host.resolution_event,
                                 &scaling->listener, error))
        return false;
    *scaling = (AbUiScaling){0};
    return true;
}

bool ab_ui_scaling_get(const AbUiScaling *scaling, float *output, AbError *error) {
    if (!valid(scaling, error) || !output)
        return false;
    *output = scaling->default_scaling;
    return true;
}

bool ab_ui_scaling_set_to(const AbUiScaling *scaling, AbUiFrameFields *frame,
                          AbError *error) {
    if (!frame)
        return true;
    float value;
    if (!ab_ui_scaling_get(scaling, &value, error))
        return false;
    AbUiPointerValue field = {.kind = AB_UI_POINTER_NUMBER};
    memcpy(&field.number_bits, &value, sizeof(value));
    frame->scale_x = field;
    frame->scale_y = field;
    return true;
}

bool ab_ui_scaling_sprite(const AbUiScaling *scaling,
                          const AbUiImageResourceBoundaries *resources, AbSpan image,
                          bool pivot, AbVec2 *output, AbError *error) {
    if (!valid(scaling, error) || !resources || !output ||
        (image.size && !image.data) || (pivot ? !resources->pivot : !resources->bounds))
        return ab_fail(error, "Invalid UI Scaling sprite boundary");
    AbVec2 size;
    bool okay = pivot ? resources->pivot(resources->context, image, &size, error)
                      : resources->bounds(resources->context, image, &size, error);
    if (!okay)
        return false;
    AbUiScalingEnvironment environment;
    if (!scaling->host.environment(scaling->host.context, &environment, error))
        return false;
    if (environment.use_scaling) {
        size.x = size.x * scaling->default_scaling;
        size.y = size.y * scaling->default_scaling;
    }
    *output = size;
    return true;
}
