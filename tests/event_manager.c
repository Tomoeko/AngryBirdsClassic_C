#include "ab/event_manager.h"
#include "ab/object_property_access.h"
#include "event_manager_cases.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Event check at %d: %s\n", __LINE__, #condition);          \
            return false;                                                              \
        }                                                                              \
    } while (0)

typedef struct EventFixture EventFixture;
typedef struct {
    EventFixture *fixture;
    uint32_t role;
} ListenerContext;

struct EventFixture {
    AbEventManager *manager;
    AbEventListener listeners[3];
    ListenerContext contexts[3];
    AbDataTable events[4];
    AbDataEntry entries[4][3];
    uint32_t sample, used, calls, swallowed, trace_count;
    uint32_t trace[EVENT_MANAGER_TRACE_CAPACITY][EVENT_MANAGER_TRACE_WORDS];
};

static AbDataValue text(const char *value) {
    return (AbDataValue){.kind = AB_DATA_STRING,
                         .string = {(const uint8_t *)value, strlen(value)}};
}

static float from_bits(uint32_t bits) {
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static uint32_t id_role(AbDataValue value) {
    if (value.kind == AB_DATA_STRING && value.string.size == 1)
        return value.string.data[0] == 'A' ? 0u : 1u;
    return UINT32_MAX;
}

static AbDataValue field(const AbDataTable *table, const char *name) {
    const AbDataValue *value = ab_data_get(table, text(name));
    return value ? *value : (AbDataValue){0};
}

static void snapshot(EventFixture *fixture,
                     uint32_t output[EVENT_MANAGER_SNAPSHOT_WORDS]) {
    AbEventManagerView view = ab_event_manager_view(fixture->manager);
    output[0] = (uint32_t)view.event_types;
    output[1] = (uint32_t)view.queued_events;
    output[2] = (uint32_t)view.buffered_events;
    output[3] = (uint32_t)view.delayed_events;
    output[4] = view.buffering;
    output[5] = fixture->used;
    output[6] = fixture->calls;
    output[7] = fixture->swallowed;
    for (size_t index = 0; index < 4; ++index) {
        AbDataValue delay = field(fixture->events + index, "delay");
        output[8 + index * 2] = delay.kind == AB_DATA_NUMBER ? 3u : 0u;
        output[9 + index * 2] = delay.bits;
        output[16 + index] = id_role(field(fixture->events + index, "id"));
    }
    for (size_t index = 0; index < 3; ++index)
        output[20 + index] = fixture->listeners[index].event_triggered != NULL;
}

static bool action(EventFixture *fixture, EventFixtureAction request) {
    AbDataValue id = text(request.id ? "B" : "A");
    switch (request.operation) {
        case EVENT_FIXTURE_ADD:
            return ab_event_manager_add(fixture->manager, id,
                                        fixture->listeners + request.object, NULL);
        case EVENT_FIXTURE_REMOVE:
            return ab_event_manager_remove(fixture->manager, id,
                                           fixture->listeners + request.object, NULL);
        case EVENT_FIXTURE_NOTIFY:
            return ab_event_manager_notify(fixture->manager,
                                           fixture->events + request.object, NULL);
        case EVENT_FIXTURE_QUEUE:
            return ab_event_manager_queue(fixture->manager,
                                          fixture->events + request.object, NULL);
        case EVENT_FIXTURE_DELAY:
            return ab_event_manager_delay(fixture->manager,
                                          fixture->events + request.object,
                                          from_bits(request.number), NULL);
        case EVENT_FIXTURE_TICK:
            return ab_event_manager_tick(fixture->manager, from_bits(request.number),
                                         NULL);
        case EVENT_FIXTURE_CHANGE_ID:
            fixture->entries[request.object][0].value = id;
            return true;
    }
    return false;
}

static bool callback(void *context, AbDataTable *event, AbError *error) {
    (void)error;
    ListenerContext *listener = context;
    EventFixture *fixture = listener->fixture;
    CHECK(fixture->trace_count < EVENT_MANAGER_TRACE_CAPACITY);
    uint32_t event_role = UINT32_MAX;
    for (uint32_t index = 0; index < 4; ++index)
        if (event == fixture->events + index)
            event_role = index;
    CHECK(event_role != UINT32_MAX);
    ++fixture->calls;
    uint32_t *row = fixture->trace[fixture->trace_count++];
    row[0] = listener->role;
    row[1] = event_role;
    snapshot(fixture, row + 2);
    for (uint32_t bit = 0; bit < 8; ++bit) {
        uint32_t mask = 1u << bit;
        bool receiver =
            bit < 6 ? listener->role == 0 : listener->role == (bit == 6 ? 1u : 2u);
        if (!receiver || !(fixture->sample & mask) || (fixture->used & mask))
            continue;
        fixture->used |= mask;
        if (bit == 4)
            fixture->listeners[1].event_triggered = NULL;
        else if (!action(fixture, event_fixture_effects[bit]))
            ++fixture->swallowed;
    }
    return true;
}

static bool prepare(EventFixture *fixture, uint32_t sample) {
    fixture->sample = sample;
    CHECK(ab_event_manager_open((AbEventManagerLimits){2, 4, 32, 32, 16, 65536},
                                (AbEventManagerHost){0}, &fixture->manager, NULL));
    static const char *const names[] = {"ListenerA", "ListenerB", "ListenerC"};
    for (uint32_t index = 0; index < 3; ++index) {
        fixture->contexts[index] = (ListenerContext){fixture, index};
        fixture->listeners[index] = (AbEventListener){
            text(names[index]).string, fixture->contexts + index, callback};
    }
    for (size_t index = 0; index < 4; ++index) {
        fixture->entries[index][0] =
            (AbDataEntry){text("id"), text(index == 2 ? "B" : "A")};
        fixture->entries[index][1] =
            (AbDataEntry){text("token"), {.kind = AB_DATA_NUMBER}};
        fixture->events[index] = (AbDataTable){fixture->entries[index], 2, 3, NULL};
    }
    return true;
}

static bool compare(FILE *file, const uint32_t *actual, size_t count, uint32_t sample,
                    size_t step, const char *stage) {
    for (size_t index = 0; index < count; ++index) {
        uint32_t expected;
        CHECK(fscanf(file, "%8" SCNx32, &expected) == 1);
        if (actual[index] != expected) {
            fprintf(stderr,
                    "Event %u step %zu %s %zu: %08" PRIx32 " != %08" PRIx32 "\n",
                    sample, step, stage, index, actual[index], expected);
            return false;
        }
    }
    return true;
}

static bool native_check(FILE *file) {
    char marker[96];
    CHECK(fgets(marker, sizeof(marker), file) &&
          !strcmp(marker, "AB_ANDROID_EVENT_MANAGER_V1 256 24\n"));
    size_t boundaries = 0;
    for (uint32_t sample = 0; sample < EVENT_MANAGER_CASES; ++sample) {
        EventFixture fixture = {0};
        bool okay =
            prepare(&fixture, sample) && compare(file, &sample, 1, sample, 0, "input");
        for (size_t step = 0; okay && step < EVENT_MANAGER_ACTIONS; ++step) {
            fixture.trace_count = 0;
            uint32_t output[25];
            output[0] = action(&fixture, event_fixture_actions[step]);
            output[1] = fixture.trace_count;
            snapshot(&fixture, output + 2);
            okay = compare(file, output, 25, sample, step, "state");
            for (size_t index = 0; okay && index < fixture.trace_count; ++index)
                okay = compare(file, fixture.trace[index], EVENT_MANAGER_TRACE_WORDS,
                               sample, step, "callback");
            boundaries += fixture.trace_count;
        }
        ab_event_manager_close(fixture.manager);
        CHECK(okay);
    }
    CHECK(fscanf(file, " %95[^\n]", marker) == 1 &&
          !strcmp(marker, "AB event manager reference complete") &&
          fgetc(file) == '\n' && fgetc(file) == EOF && !ferror(file));
    printf("Matched 6144 original event actions and %zu ordered listener boundaries.\n",
           boundaries);
    return true;
}

static bool controls(void) {
    AbEventManager *manager = NULL;
    CHECK(!ab_event_manager_open((AbEventManagerLimits){0}, (AbEventManagerHost){0},
                                 &manager, NULL) &&
          !manager);
    EventFixture fixture = {0};
    CHECK(ab_event_manager_open((AbEventManagerLimits){1, 0, 0, 0, 1, 65536},
                                (AbEventManagerHost){0}, &manager, NULL));
    AbEventListener listener = {.context = &fixture, .event_triggered = callback};
    CHECK(!ab_event_manager_add(manager, text("A"), &listener, NULL) &&
          ab_event_manager_view(manager).event_types == 1);
    AbDataEntry entries[2] = {{text("id"), text("A")}};
    AbDataTable event = {entries, 1, 2, NULL};
    CHECK(!ab_event_manager_queue(manager, &event, NULL) &&
          !ab_event_manager_delay(manager, &event, 0.5f, NULL) &&
          field(&event, "delay").bits == UINT32_C(0x3f000000) &&
          !ab_event_manager_view(manager).delayed_events);
    size_t bytes = ab_event_manager_bytes(manager);
    ab_event_manager_close(manager);
    manager = NULL;
    CHECK(!ab_event_manager_open((AbEventManagerLimits){1, 0, 0, 0, 1, bytes - 1},
                                 (AbEventManagerHost){0}, &manager, NULL) &&
          !manager);
    CHECK(ab_event_manager_open((AbEventManagerLimits){1, 0, 0, 0, 1, bytes},
                                (AbEventManagerHost){0}, &manager, NULL) &&
          ab_event_manager_bytes(manager) == bytes);
    ab_event_manager_close(manager);
    return true;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fputs("Usage: ab_event_manager_tests ORIGINAL_EVENT_MANAGER_TRACE\n", stderr);
        return 2;
    }
    FILE *file = fopen(argv[1], "rb");
    bool okay = controls() && file && native_check(file);
    if (file && fclose(file))
        okay = false;
    return okay ? 0 : 1;
}
