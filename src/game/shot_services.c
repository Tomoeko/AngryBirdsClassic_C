#include "ab/shot_services.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    AbDataTable table;
    AbDataEntry entries[5];
} ShotNotification;

struct AbShotServices {
    AbShotServicesConfig config;
    ShotNotification *notifications;
    AbDataTable *position, *impulse;
    size_t count, bytes;
    bool dispatching;
};

static AbSpan span(const char *text) {
    return (AbSpan){(const uint8_t *)text, strlen(text)};
}

static AbDataValue text(const char *value) {
    return (AbDataValue){.kind = AB_DATA_STRING, .string = span(value)};
}

static AbDataValue number(float value) {
    AbDataValue result = {.kind = AB_DATA_NUMBER};
    memcpy(&result.bits, &value, sizeof(value));
    return result;
}

static bool vector_read(const AbDataTable *table, AbVec2 *output, AbError *error) {
    if (!table || table->count > table->capacity || (table->count && !table->entries))
        return ab_fail(error, "Invalid shot event vector table");
    const AbDataValue *x = ab_data_get(table, text("x"));
    const AbDataValue *y = ab_data_get(table, text("y"));
    if (!x || !y || x->kind != AB_DATA_NUMBER || y->kind != AB_DATA_NUMBER)
        return ab_fail(error, "Shot event vector requires numeric x/y fields");
    memcpy(&output->x, &x->bits, sizeof(output->x));
    memcpy(&output->y, &y->bits, sizeof(output->y));
    return true;
}

static bool same_vector(AbVec2 first, AbVec2 second) {
    return !memcmp(&first.x, &second.x, sizeof(float)) &&
           !memcmp(&first.y, &second.y, sizeof(float));
}

bool ab_shot_services_open(AbShotServicesConfig config, AbShotServices **output,
                           AbError *error) {
    if (!output)
        return ab_fail(error, "Missing shot services output");
    *output = NULL;
    if (!config.statistics || !config.events ||
        config.storage_bytes < sizeof(AbShotServices) ||
        config.notifications >
            (config.storage_bytes - sizeof(AbShotServices)) / sizeof(ShotNotification))
        return ab_fail(error, "Invalid shot services bindings or storage limit");
    AbShotServices *services = calloc(1, sizeof(*services));
    if (!services)
        return ab_fail(error, "Cannot allocate shot services");
    services->config = config;
    services->bytes =
        sizeof(*services) + config.notifications * sizeof(ShotNotification);
    if (config.notifications) {
        services->notifications =
            calloc(config.notifications, sizeof(*services->notifications));
        if (!services->notifications) {
            ab_shot_services_close(services);
            return ab_fail(error, "Cannot allocate shot notification storage");
        }
    }
    *output = services;
    return true;
}

void ab_shot_services_close(AbShotServices *services) {
    if (!services)
        return;
    free(services->notifications);
    free(services);
}

size_t ab_shot_services_bytes(const AbShotServices *services) {
    return services ? services->bytes : 0;
}

size_t ab_shot_services_notification_count(const AbShotServices *services) {
    return services ? services->count : 0;
}

bool ab_shot_services_bind_vectors(AbShotServices *services, AbDataTable *position,
                                   AbDataTable *impulse, AbError *error) {
    AbVec2 p, i;
    if (!services || services->dispatching)
        return ab_fail(error, "Invalid or reentrant shot vector binding");
    if (!vector_read(position, &p, error) || !vector_read(impulse, &i, error))
        return false;
    services->position = position;
    services->impulse = impulse;
    return true;
}

static bool notify(AbShotServices *services, AbShotCall *call, AbError *error) {
    if (services->count == services->config.notifications)
        return ab_fail(error, "Shot notification storage exhausted");
    bool bird = call->operation == AB_SHOT_NOTIFY_BIRD;
    if (bird) {
        AbVec2 position, impulse;
        if (!call->launch_position || !call->launch_impulse || !isfinite(call->value) ||
            !isfinite(call->backwards_count))
            return ab_fail(error, "Invalid bird-shot event inputs");
        if (!vector_read(services->position, &position, error) ||
            !vector_read(services->impulse, &impulse, error))
            return false;
        if (!same_vector(position, *call->launch_position) ||
            !same_vector(impulse, *call->launch_impulse))
            return ab_fail(error, "Shot event tables differ from live launch vectors");
    }
    ShotNotification *event = services->notifications + services->count++;
    event->table = (AbDataTable){event->entries, bird ? 5u : 1u, 5, NULL};
    event->entries[0] =
        (AbDataEntry){text("id"), text(bird ? "EID_BIRD_SHOT" : "EID_ALL_BIRDS_SHOT")};
    if (bird) {
        event->entries[1] = (AbDataEntry){text("birdsShooted"), number(call->value)};
        event->entries[2] =
            (AbDataEntry){text("backwardsBirdCount"), number(call->backwards_count)};
        event->entries[3] = (AbDataEntry){
            text("launchImpulse"), {.kind = AB_DATA_TABLE, .table = services->impulse}};
        event->entries[4] =
            (AbDataEntry){text("launchPosition"),
                          {.kind = AB_DATA_TABLE, .table = services->position}};
    }
    bool okay = ab_event_manager_notify(services->config.events, &event->table, error);
    if (bird) {
        /* These are the original tables, even if the event's fields changed.
         * Preserve representable vector writes after a partial listener failure. */
        bool position = vector_read(services->position, call->launch_position, error);
        bool impulse = vector_read(services->impulse, call->launch_impulse, error);
        if (!position || !impulse)
            return false;
    }
    return okay;
}

static bool shot_service(AbShotServices *services, AbShotCall *call, AbError *error) {
    switch (call->operation) {
        case AB_SHOT_READ_SHOT_COUNT:
        case AB_SHOT_READ_BACKWARDS_COUNT:
            return ab_settings_statistics_device_sum(
                services->config.statistics,
                span(call->operation == AB_SHOT_READ_SHOT_COUNT ? "birdsShooted"
                                                                : "backwardsBirdCount"),
                &call->value, error);
        case AB_SHOT_NOTIFY_BIRD:
        case AB_SHOT_NOTIFY_ALL:
            return notify(services, call, error);
        default:
            if (!services->config.remaining_shot)
                return ab_fail(error, "Missing shot service for operation %u",
                               (unsigned)call->operation);
            return services->config.remaining_shot(services->config.context, call,
                                                   error);
    }
}

bool ab_shot_services_shot(void *context, AbShotCall *call, AbError *error) {
    AbShotServices *services = context;
    if (!services || !call || services->dispatching)
        return ab_fail(error, "Invalid or reentrant shot service request");
    services->dispatching = true;
    bool okay = shot_service(services, call, error);
    services->dispatching = false;
    return okay;
}

bool ab_shot_services_sling(void *context, AbSlingLaunchCall *call, AbError *error) {
    AbShotServices *services = context;
    if (!services || !call || services->dispatching)
        return ab_fail(error, "Invalid or reentrant sling service request");
    switch (call->operation) {
        case AB_SLING_LAUNCH_BACKWARDS:
        case AB_SLING_LAUNCH_SHOT_COUNT:
            return ab_settings_statistics_increment(
                services->config.statistics,
                span(call->operation == AB_SLING_LAUNCH_BACKWARDS ? "backwardsBirdCount"
                                                                  : "birdsShooted"),
                error);
        default:
            if (!services->config.remaining_sling)
                return ab_fail(error, "Missing sling service for operation %u",
                               (unsigned)call->operation);
            services->dispatching = true;
            bool okay =
                services->config.remaining_sling(services->config.context, call, error);
            services->dispatching = false;
            return okay;
    }
}
