#include "ab/classic_loader.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    AbDataTable table;
    AbDataEntry entries[2];
} LoaderNotice;

struct AbClassicLoader {
    AbClassicLoaderConfig config;
    LoaderNotice *notices;
    size_t count, bytes;
    bool eventing, finishing, updating, failed;
};

static AbDataValue text(const char *value) {
    return (AbDataValue){.kind = AB_DATA_STRING,
                         .string = {(const uint8_t *)value, strlen(value)}};
}

static AbDataValue number(float value) {
    AbDataValue output = {.kind = AB_DATA_NUMBER};
    memcpy(&output.bits, &value, sizeof(value));
    return output;
}

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL && (value.kind != AB_DATA_BOOLEAN || value.bits);
}

static bool same(AbDataValue a, AbDataValue b) {
    if (a.kind != b.kind)
        return false;
    switch (a.kind) {
        case AB_DATA_NIL:
            return true;
        case AB_DATA_BOOLEAN:
            return !!a.bits == !!b.bits;
        case AB_DATA_NUMBER: {
            float x, y;
            memcpy(&x, &a.bits, sizeof(x));
            memcpy(&y, &b.bits, sizeof(y));
            return x == y;
        }
        case AB_DATA_STRING:
            return a.string.size == b.string.size &&
                   (!a.string.size ||
                    (a.string.data && b.string.data &&
                     !memcmp(a.string.data, b.string.data, a.string.size)));
        case AB_DATA_TABLE:
            return a.table == b.table;
        case AB_DATA_OBJECT:
            return a.object == b.object;
    }
    return false;
}

static bool numeric(AbDataValue value, float *output, AbError *error) {
    if (value.kind != AB_DATA_NUMBER)
        return ab_fail(error, "Nonnumeric reached Loader field");
    memcpy(output, &value.bits, sizeof(*output));
    return isfinite(*output) || ab_fail(error, "Nonfinite reached Loader field");
}

bool ab_classic_loader_open(AbClassicLoaderConfig config, AbClassicLoader **output,
                            AbError *error) {
    if (!output)
        return ab_fail(error, "Missing Loader output");
    *output = NULL;
    if (!config.state || !config.script_time || !config.do_loading ||
        !config.show_loading || !config.hide_loading || !config.skip_input ||
        !config.events || config.storage_bytes < sizeof(AbClassicLoader) ||
        config.notifications >
            (config.storage_bytes - sizeof(AbClassicLoader)) / sizeof(LoaderNotice))
        return ab_fail(error, "Invalid Loader bindings or storage");
    AbClassicLoader *loader = calloc(1, sizeof(*loader));
    if (!loader)
        return ab_fail(error, "Cannot allocate Loader owner");
    if (config.notifications)
        loader->notices = calloc(config.notifications, sizeof(*loader->notices));
    if (config.notifications && !loader->notices) {
        free(loader);
        return ab_fail(error, "Cannot allocate retained Loader events");
    }
    loader->config = config;
    loader->bytes = sizeof(*loader) + config.notifications * sizeof(LoaderNotice);
    *output = loader;
    return true;
}

void ab_classic_loader_close(AbClassicLoader *loader) {
    if (loader) {
        free(loader->notices);
        free(loader);
    }
}

size_t ab_classic_loader_bytes(const AbClassicLoader *loader) {
    return loader ? loader->bytes : 0;
}

static bool notify(AbClassicLoader *loader, AbDataValue id, const AbDataValue *alpha,
                   AbError *error) {
    if (loader->count == loader->config.notifications)
        return ab_fail(error, "Loader retained notification capacity exhausted");
    LoaderNotice *notice = loader->notices + loader->count++;
    notice->entries[0] = (AbDataEntry){text("id"), id};
    if (alpha)
        notice->entries[1] = (AbDataEntry){text("loadingPageAlpha"), *alpha};
    notice->table = (AbDataTable){notice->entries, alpha ? 2u : 1u, 2, NULL};
    return ab_event_manager_notify(loader->config.events, &notice->table, error);
}

static bool event(AbClassicLoader *loader, AbClassicLoaderRequest *request,
                  AbError *error) {
    AbClassicLoaderConfig *config = &loader->config;
    if (!same(request->id, *config->do_loading))
        return true;
    if (!truthy(request->hidden) &&
        !notify(loader, *config->show_loading, &request->loading_page_alpha, error))
        return false;
    AbClassicLoaderState *state = config->state;
    state->items = request->items;
    state->loading = (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1};
    state->completion_event = request->completion_event;
    state->completion_call = request->completion_call;
    state->frame = number(0);
    state->loading_frame = number(1);
    if (truthy(request->minimum_display_time)) {
        float time, duration;
        if (!numeric(*config->script_time, &time, error) ||
            !numeric(request->minimum_display_time, &duration, error) ||
            !isfinite(time + duration))
            return ab_fail(error, "Unsupported Loader minimum display arithmetic");
        state->minimum_display_time = number(time + duration);
    } else {
        state->minimum_display_time = (AbDataValue){0};
    }
    return true;
}

bool ab_classic_loader_event(AbClassicLoader *loader, AbClassicLoaderRequest *request,
                             AbError *error) {
    if (!loader || loader->failed || loader->eventing || !request)
        return ab_fail(error, "Invalid or reentrant Loader event");
    loader->eventing = true;
    bool okay = event(loader, request, error);
    loader->eventing = false;
    loader->failed = loader->failed || !okay;
    return !loader->failed;
}

static bool call(AbClassicLoaderCall *function, AbError *error) {
    return (function && function->call)
               ? function->call(function->context, error)
               : ab_fail(error, "Missing reached actual Loader callable");
}

static bool finish(AbClassicLoader *loader, AbError *error) {
    AbClassicLoaderConfig *config = &loader->config;
    if (!notify(loader, *config->hide_loading, NULL, error))
        return false;
    config->state->loading = (AbDataValue){.kind = AB_DATA_BOOLEAN};
    AbDataValue completion = config->state->completion_event;
    if (truthy(completion)) {
        if (completion.kind != AB_DATA_TABLE || !completion.table)
            return ab_fail(error, "Unsupported reached Loader completion event");
        if (!ab_event_manager_notify(config->events, completion.table, error))
            return false;
    }
    AbClassicLoaderCall *function = config->state->completion_call;
    return !function || call(function, error);
}

bool ab_classic_loader_finish(AbClassicLoader *loader, AbError *error) {
    if (!loader || loader->failed || loader->finishing)
        return ab_fail(error, "Invalid or reentrant Loader finish");
    loader->finishing = true;
    bool okay = finish(loader, error);
    loader->finishing = false;
    loader->failed = loader->failed || !okay;
    return !loader->failed;
}

static bool load_items(AbClassicLoaderState *state, AbError *error) {
    if (!state->items || (state->items->capacity && !state->items->items))
        return ab_fail(error, "Missing actual Loader item list");
    AbClassicLoaderCalls captured = *state->items;
    for (size_t index = 0; index < captured.capacity && captured.items[index]; ++index)
        if (!call(captured.items[index], error))
            return false;
    return true;
}

static bool log_message(AbClassicLoader *loader, AbClassicLoaderLog kind,
                        AbError *error) {
    return !loader->config.log ||
           loader->config.log(loader->config.context, kind, error);
}

static bool update(AbClassicLoader *loader, AbError *error) {
    AbClassicLoaderConfig *config = &loader->config;
    AbClassicLoaderState *state = config->state;
    float frame;
    if (truthy(state->loading)) {
        *config->skip_input = (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1};
        float target;
        if (!numeric(state->frame, &frame, error) ||
            !numeric(state->loading_frame, &target, error))
            return false;
        if (frame == target) {
            if (!load_items(state, error))
                return false;
            if (!truthy(state->minimum_display_time) &&
                (!log_message(loader, AB_CLASSIC_LOADER_NO_MINIMUM, error) ||
                 !ab_classic_loader_finish(loader, error)))
                return false;
        }
        if (!numeric(state->frame, &frame, error) ||
            !numeric(state->loading_frame, &target, error))
            return false;
        if (target <= frame && truthy(state->minimum_display_time)) {
            float time, minimum;
            if (!numeric(*config->script_time, &time, error) ||
                !numeric(state->minimum_display_time, &minimum, error))
                return false;
            if (minimum <= time &&
                (!log_message(loader, AB_CLASSIC_LOADER_MINIMUM_PASSED, error) ||
                 !ab_classic_loader_finish(loader, error)))
                return false;
        }
    }
    if (!numeric(state->frame, &frame, error) || !isfinite(frame + 1))
        return ab_fail(error, "Unsupported Loader frame arithmetic");
    state->frame = number(frame + 1);
    return true;
}

bool ab_classic_loader_update(AbClassicLoader *loader, AbError *error) {
    if (!loader || loader->failed || loader->updating || loader->eventing ||
        loader->finishing)
        return ab_fail(error, "Invalid or reentrant Loader update");
    loader->updating = true;
    bool okay = update(loader, error);
    loader->updating = false;
    loader->failed = loader->failed || !okay;
    return !loader->failed;
}
