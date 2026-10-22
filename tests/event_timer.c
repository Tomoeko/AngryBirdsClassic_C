#include "ab/event_timer.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    AbEventTimer timer;
    AbEventTimerEntry entries[8];
    AbEventListener listener;
    AbDataEntry fields[6];
    AbDataTable events[6];
    char order[32];
    size_t count;
    bool insert_during_callback;
} Fixture;

static bool called(void *context, AbDataTable *event, AbError *error) {
    Fixture *fixture = context;
    if (!event || event->count != 1 || fixture->count >= sizeof(fixture->order) - 1)
        return ab_fail(error, "Invalid controlled timer event/delivery count");
    char id = (char)event->entries[0].value.string.data[0];
    fixture->order[fixture->count++] = id;
    fixture->order[fixture->count] = 0;
    if (id == 'A' && fixture->insert_during_callback) {
        fixture->insert_during_callback = false;
        return ab_event_timer_once(&fixture->timer, fixture->events + 4,
                                   &fixture->listener, -1, error) &&
               ab_event_timer_once(&fixture->timer, fixture->events + 5,
                                   &fixture->listener, 0, error);
    }
    return true;
}

static bool initialize(Fixture *fixture, float start, AbError *error) {
    static const uint8_t names[] = "ABCDEZ";
    *fixture = (Fixture){0};
    fixture->listener =
        (AbEventListener){.context = fixture, .event_triggered = called};
    for (size_t index = 0; index < 6; ++index) {
        fixture->fields[index] = (AbDataEntry){
            {.kind = AB_DATA_STRING, .string = {(const uint8_t *)"id", 2}},
            {.kind = AB_DATA_STRING, .string = {names + index, 1}}};
        fixture->events[index] = (AbDataTable){fixture->fields + index, 1, 1, NULL};
    }
    return ab_event_timer_init(&fixture->timer, fixture->entries, 8, start, error);
}

static bool boundaries(Fixture *fixture, AbError *error) {
    if (!initialize(fixture, 10, error) ||
        !ab_event_timer_once(&fixture->timer, fixture->events, &fixture->listener, 1,
                             error) ||
        !ab_event_timer_update(&fixture->timer, 1, error) || fixture->count ||
        !ab_event_timer_update(&fixture->timer, 0, error) || fixture->count ||
        !ab_event_timer_update(&fixture->timer, 0.25f, error) ||
        strcmp(fixture->order, "A") || fixture->timer.count)
        return ab_fail(error, "Timer strict deadline boundary differs");
    return true;
}

static bool ties(Fixture *fixture, AbError *error) {
    if (!initialize(fixture, 0, error))
        return false;
    for (size_t index = 0; index < 4; ++index)
        if (!ab_event_timer_once(&fixture->timer, fixture->events + index,
                                 &fixture->listener, 0, error))
            return false;
    if (!ab_event_timer_update(&fixture->timer, 0.5f, error) ||
        strcmp(fixture->order, "ACBD"))
        return ab_fail(error, "Timer heap source tie order differs");
    return true;
}

static bool insertion(Fixture *fixture, AbError *error) {
    if (!initialize(fixture, 0, error))
        return false;
    fixture->insert_during_callback = true;
    for (size_t index = 0; index < 4; ++index)
        if (!ab_event_timer_once(&fixture->timer, fixture->events + index,
                                 &fixture->listener, 0, error))
            return false;
    if (!ab_event_timer_update(&fixture->timer, 0.5f, error) ||
        strcmp(fixture->order, "AEDCB") || fixture->timer.count != 1 ||
        !ab_event_timer_update(&fixture->timer, 0, error) ||
        fixture->timer.count != 1 ||
        !ab_event_timer_update(&fixture->timer, 0.25f, error) ||
        strcmp(fixture->order, "AEDCBZ") || fixture->timer.count)
        return ab_fail(error, "Timer callback insertion/due-event behavior differs");
    return true;
}

static bool failures(Fixture *fixture, AbError *error) {
    if (!initialize(fixture, 0, error) ||
        !ab_event_timer_once(&fixture->timer, fixture->events, NULL, 0, error))
        return false;
    if (ab_event_timer_update(&fixture->timer, 1, error) || !fixture->timer.failed ||
        fixture->timer.time != 1 || fixture->timer.count ||
        ab_event_timer_update(&fixture->timer, 1, error))
        return ab_fail(error, "Timer listener failure lost its dequeue/time prefix");
    AbEventTimer empty = {0};
    if (!ab_event_timer_init(&empty, NULL, 0, 0, error) ||
        !ab_event_timer_update(&empty, 0.25f, error) ||
        ab_event_timer_once(&empty, NULL, NULL, 1, error) || empty.count ||
        empty.failed)
        return ab_fail(error, "Empty timer/capacity boundary differs");
    return true;
}

int main(void) {
    Fixture fixture;
    AbError error = {0};
    if (!boundaries(&fixture, &error) || !ties(&fixture, &error) ||
        !insertion(&fixture, &error) || !failures(&fixture, &error)) {
        fprintf(stderr, "%s\n", error.message);
        return 1;
    }
    puts("Source timer strict deadlines, heap tie order, callback scheduling, "
         "empty owner and dequeue failure prefixes pass.");
    return 0;
}
