#include "ab/shot_services.h"
#include "ab/object_property_access.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Shot service check failed at %d: %s\n", __LINE__,         \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

typedef struct {
    AbDataTable *impulse, *position, replacement;
    AbDataTable *retained;
    AbEventManager *events;
    AbShotServices *services;
    size_t calls;
    bool fail, queued;
} Observer;

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

static bool write(AbDataTable *table, const char *name, AbDataValue value) {
    AbObjectPropertyAccess object = {
        .physics_object = {.kind = AB_DATA_TABLE, .table = table}};
    return ab_object_property_set(&object, span(name), value, NULL);
}

static bool mutate(void *context, AbDataTable *event, AbError *error) {
    Observer *observer = context;
    ++observer->calls;
    observer->retained = event;
    CHECK(write(observer->impulse, "x", number(-7)) &&
          write(observer->position, "y", number(8)) &&
          write(event, "launchImpulse",
                (AbDataValue){.kind = AB_DATA_TABLE, .table = &observer->replacement}));
    if (!observer->queued) {
        observer->queued = true;
        CHECK(ab_event_manager_queue(observer->events, event, error));
    }
    AbShotCall reentry = {.operation = AB_SHOT_READ_SHOT_COUNT};
    CHECK(!ab_shot_services_shot(observer->services, &reentry, NULL) &&
          !ab_shot_services_bind_vectors(observer->services, observer->position,
                                         observer->impulse, NULL));
    return observer->fail ? ab_fail(error, "Requested listener failure") : true;
}

static bool check_notifications(bool alias, bool fail) {
    AbSettingsStatistics *statistics = NULL;
    AbEventManager *events = NULL;
    AbShotServices *services = NULL;
    CHECK(ab_settings_statistics_open(
        (AbSettingsStatisticsConfig){span("fixture"), span("fixture"), 8, 8, 65536},
        &statistics, NULL));
    CHECK(ab_event_manager_open((AbEventManagerLimits){2, 2, 2, 0, 4, 8192},
                                (AbEventManagerHost){0}, &events, NULL));
    CHECK(ab_shot_services_open(
        (AbShotServicesConfig){statistics, events, NULL, NULL, NULL, 1, 4096},
        &services, NULL));
    AbDataEntry entries[4] = {{text("x"), number(2)},
                              {text("y"), number(3)},
                              {text("x"), number(4)},
                              {text("y"), number(5)}};
    AbDataTable vectors[] = {{entries, 2, 2, NULL}, {entries + 2, 2, 2, NULL}};
    AbDataTable *impulse_table = vectors + (alias ? 0 : 1);
    CHECK(ab_shot_services_bind_vectors(services, vectors, impulse_table, NULL));
    Observer observer = {.impulse = impulse_table,
                         .position = vectors,
                         .events = events,
                         .services = services,
                         .fail = fail};
    AbEventListener listener = {span("fixture"), &observer, mutate};
    CHECK(ab_event_manager_add(events, text("EID_BIRD_SHOT"), &listener, NULL));
    AbVec2 position = {2, 3}, impulse = alias ? position : (AbVec2){4, 5};
    AbShotCall call = {.operation = AB_SHOT_NOTIFY_BIRD,
                       .launch_position = &position,
                       .launch_impulse = &impulse,
                       .value = 12,
                       .backwards_count = 3};
    CHECK(ab_shot_services_shot(services, &call, NULL) == !fail);
    CHECK(observer.calls == 1 && position.y == 8 && impulse.x == -7 &&
          (!alias || (position.x == -7 && impulse.y == 8)) &&
          ab_event_manager_view(events).queued_events == 1 &&
          ab_shot_services_notification_count(services) == 1);
    const AbDataValue *retained =
        ab_data_get(observer.retained, text("launchPosition"));
    CHECK(retained && retained->table == vectors &&
          ab_data_get(observer.retained, text("launchImpulse"))->table ==
              &observer.replacement);
    /* The original input impulse table survives the replaced event field.
     * Capacity failure fires no later notification and preserves that payload. */
    call.operation = AB_SHOT_NOTIFY_ALL;
    CHECK(!ab_shot_services_shot(services, &call, NULL) && observer.calls == 1);
    call.operation = AB_SHOT_ATTACH_PARTICLES;
    CHECK(!ab_shot_services_shot(services, &call, NULL));
    AbSlingLaunchCall sling = {.operation = AB_SLING_LAUNCH_POWER};
    CHECK(!ab_shot_services_sling(services, &sling, NULL));
    size_t bytes = ab_shot_services_bytes(services);
    ab_shot_services_close(services);
    AbShotServices *limited = NULL;
    /* No callbacks run while checking owner allocation accounting. */
    CHECK(!ab_shot_services_open(
        (AbShotServicesConfig){statistics, events, NULL, NULL, NULL, 1, bytes - 1},
        &limited, NULL));
    CHECK(!limited);
    ab_event_manager_close(events);
    ab_settings_statistics_close(statistics);
    return true;
}

int main(void) {
    return check_notifications(false, false) && check_notifications(false, true) &&
                   check_notifications(true, false) && check_notifications(true, true)
               ? 0
               : 1;
}
