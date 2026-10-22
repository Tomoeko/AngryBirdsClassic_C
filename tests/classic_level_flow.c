#include "ab/classic_level_flow.h"
#include "root_query_menu.h"
#include "../tools/oracle/reference_level_start_flow.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Level flow failed at %d: %s\n", __LINE__, #condition);    \
            return false;                                                              \
        }                                                                              \
    } while (0)

typedef struct FlowTest FlowTest;
typedef struct {
    FlowTest *test;
    unsigned index;
} FlowListener;

struct FlowTest {
    AbClassicLevelFlow *owner;
    AbEventManager *events;
    AbEventListener observer;
    AbClassicLevelFlowListener listeners[10];
    FlowListener bindings[10];
    AbClassicLevelFlowRequest *requests[32];
    unsigned shows[10], checks[10];
    unsigned profile, step, created, count, done_events;
    uint32_t trace[64][10];
    bool paused;
};

static AbDataValue text(const char *name) {
    return (AbDataValue){.kind = AB_DATA_STRING,
                         .string = {(const uint8_t *)name, strlen(name)}};
}

static unsigned identity(const FlowTest *test,
                         const AbClassicLevelFlowRequest *request) {
    if (!request)
        return 0;
    for (unsigned index = 0; index < test->created; ++index)
        if (test->requests[index] == request)
            return index + 1;
    return UINT32_MAX;
}

static bool state_words(FlowTest *test, uint32_t words[32]) {
    AbClassicLevelFlowView view = ab_classic_level_flow_view(test->owner);
    memset(words, 0, 32 * sizeof(*words));
    CHECK(view.done.kind == AB_DATA_BOOLEAN);
    words[0] = 1;
    words[1] = view.done.bits;
    words[2] = identity(test, view.showing);
    words[3] = test->paused;
    AbClassicLevelFlowQueue queues[] = {view.staging, view.initialization, view.ingame};
    for (unsigned queue = 0; queue < 3; ++queue) {
        CHECK(queues[queue].count <= 8);
        unsigned offset = 4 + queue * 9;
        words[offset] = (uint32_t)queues[queue].count;
        for (size_t index = 0; index < queues[queue].count; ++index)
            words[offset + index + 1] = identity(test, queues[queue].items[index]);
    }
    words[31] = test->created;
    return true;
}

static bool record(FlowTest *test, unsigned kind, unsigned listener, unsigned type,
                   uint32_t value) {
    uint32_t state[32];
    CHECK(test->count < 64 && state_words(test, state));
    uint32_t *row = test->trace[test->count++];
    row[0] = kind;
    row[1] = listener;
    row[2] = type;
    row[3] = value;
    row[4] = state[2];
    row[5] = state[4];
    row[6] = state[13];
    row[7] = state[22];
    row[8] = state[0];
    row[9] = state[1];
    return true;
}

static bool push(FlowTest *test, unsigned name, unsigned listener, float appearing,
                 AbError *error) {
    CHECK(test->created < 32 && listener < 10 &&
          ab_classic_level_flow_push(test->owner,
                                     text(reference_flow_name(name)).string, appearing,
                                     test->listeners + listener, error));
    AbClassicLevelFlowQueue queue = ab_classic_level_flow_view(test->owner).staging;
    CHECK(queue.count && queue.items[queue.count - 1]);
    test->requests[test->created++] = queue.items[queue.count - 1];
    return true;
}

static bool notify(FlowTest *test, const char *id, AbError *error) {
    AbDataEntry entry = {text("id"), text(id)};
    AbDataTable event = {&entry, 1, 1, NULL};
    return ab_event_manager_notify(test->events, &event, error);
}

static bool show(void *context, AbError *error) {
    FlowListener *listener = context;
    FlowTest *test = listener->test;
    unsigned index = listener->index;
    CHECK(record(test, 1, index, 0, 0));
    ++test->shows[index];
    if (!index && (test->profile & 1) && test->shows[index] == 1)
        CHECK(ab_classic_level_flow_clear_init(test->owner, error));
    if (index == 1 && (test->profile & 2) && test->shows[index] == 1)
        CHECK(push(test, 9, 9, 35, error));
    return true;
}

static bool is_done(void *context, AbDataValue *output, AbError *error) {
    FlowListener *listener = context;
    FlowTest *test = listener->test;
    unsigned index = listener->index;
    bool numeric = !index && (test->profile & 2) && test->step >= 9;
    bool done = index != 0 || test->step >= 9 || (test->profile & 16);
    numeric = !index && (test->profile & 2) && done;
    CHECK(record(test, 2, index, numeric ? 3u : 1u, numeric ? 0u : (unsigned)done));
    ++test->checks[index];
    if (index == 4 && (test->profile & 4) && test->checks[index] == 1)
        CHECK(notify(test, "init", error));
    *output = numeric ? (AbDataValue){.kind = AB_DATA_NUMBER}
                      : (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = done};
    return true;
}

static bool pause_game(void *context, bool paused, AbError *error) {
    FlowTest *test = context;
    (void)error;
    test->paused = paused;
    return record(test, 3, 0, 1, paused);
}

static bool event(void *context, AbDataTable *payload, AbError *error) {
    FlowTest *test = context;
    (void)payload;
    ++test->done_events;
    CHECK(record(test, 4, 0, 0, 0));
    if ((test->profile & 8) && test->done_events == 1)
        CHECK(push(test, 8, 8, 50, error));
    return true;
}

static bool compare(FILE *reference, FlowTest *test, const uint32_t *actual,
                    size_t count) {
    for (size_t index = 0; reference && index < count; ++index) {
        uint32_t expected;
        CHECK(fscanf(reference, "%" SCNx32, &expected) == 1);
        if (expected != actual[index]) {
            fprintf(stderr, "Flow %u/%u word %zu: %08" PRIx32 " != %08" PRIx32 "\n",
                    test->profile, test->step, index, actual[index], expected);
            return false;
        }
    }
    return true;
}

static bool corpus(FILE *reference) {
    if (reference) {
        char header[64];
        CHECK(fgets(header, sizeof(header), reference) &&
              !strcmp(header, "AB_ANDROID_LEVEL_FLOW_V1 32 27\n"));
    }
    for (unsigned profile = 0; profile < 32; ++profile) {
        FlowTest test = {.profile = profile};
        QueryMenu menu;
        const char *child_names[] = {"pausePage"};
        CHECK(query_menu_prepare_named((ReferenceRootQuery){.root = 5, .children = 1},
                                       child_names, &menu, NULL));
        menu.frames[1].fields.visible = query_ui_flag(1);
        AbUiFrameState notification_state = {0};
        AbUiFrameNode notifications = {.state = &notification_state};
        CHECK(ab_event_manager_open((AbEventManagerLimits){.event_types = 4,
                                                           .listeners_per_type = 2,
                                                           .notification_depth = 16,
                                                           .storage_bytes = 16384},
                                    (AbEventManagerHost){0}, &test.events, NULL));
        AbClassicLevelFlowConfig config = {.menus = menu.manager,
                                           .notifications = &notifications,
                                           .events = test.events,
                                           .loading_init = text("init"),
                                           .loading_done = text("loaded"),
                                           .flow_done = text("done"),
                                           .requests = 64,
                                           .queue_generations = 128,
                                           .notifications_count = 32,
                                           .storage_bytes = 131072,
                                           .depth_limit = 32,
                                           .work_limit = 4096,
                                           .context = &test,
                                           .set_paused = pause_game};
        CHECK(ab_classic_level_flow_open(config, &test.owner, NULL) &&
              ab_classic_level_flow_initialize(test.owner, NULL));
        for (unsigned index = 0; index < 10; ++index) {
            test.bindings[index] = (FlowListener){&test, index};
            test.listeners[index] =
                (AbClassicLevelFlowListener){test.bindings + index, show, is_done};
        }
        test.observer = (AbEventListener){.name = text("observer").string,
                                          .context = &test,
                                          .event_triggered = event};
        CHECK(ab_event_manager_add(test.events, text("done"), &test.observer, NULL));
        bool stopped = false;
        for (test.step = 0; test.step < 27; ++test.step) {
            test.count = 0;
            ReferenceLevelFlow input = reference_level_flow(profile, test.step);
            AbError error = {0};
            bool okay = true;
            if (stopped) {
                CHECK(!ab_classic_level_flow_update(test.owner, NULL));
            } else if (input.operation == REFERENCE_FLOW_PUSH)
                okay = push(&test, input.name, input.listener, input.appearing, &error);
            else if (input.operation == REFERENCE_FLOW_UPDATE)
                okay = ab_classic_level_flow_update(test.owner, &error);
            else if (input.operation == REFERENCE_FLOW_CLEAR)
                okay = ab_classic_level_flow_clear_init(test.owner, &error);
            else if (input.operation != REFERENCE_FLOW_SNAPSHOT)
                okay = notify(&test,
                              input.operation == REFERENCE_FLOW_LOADING_INIT ? "init"
                                                                             : "loaded",
                              &error);
            if (!okay) {
                CHECK(profile >= 16);
                stopped = true;
            }
            uint32_t header[] = {profile, test.step, test.count, stopped}, state[32];
            CHECK(state_words(&test, state) && compare(reference, &test, header, 4) &&
                  compare(reference, &test, state, 32));
            for (unsigned row = 0; row < test.count; ++row)
                CHECK(compare(reference, &test, test.trace[row], 10));
        }
        ab_classic_level_flow_close(test.owner);
        ab_event_manager_close(test.events);
        ab_ui_menu_manager_close(menu.manager);
    }
    if (reference) {
        char marker[64];
        CHECK(fscanf(reference, " %63[^\n]", marker) == 1 &&
              !strcmp(marker, "AB level-flow reference complete"));
        int byte;
        do {
            byte = fgetc(reference);
        } while (byte == '\n' || byte == '\r');
        CHECK(byte == EOF && !ferror(reference));
    }
    return true;
}

static bool edge_show(void *context, AbError *error) {
    (void)error;
    ++*(unsigned *)context;
    return true;
}

static bool edge_not_done(void *context, AbDataValue *output, AbError *error) {
    (void)context;
    (void)error;
    *output = (AbDataValue){.kind = AB_DATA_BOOLEAN};
    return true;
}

static bool edges(void) {
    AbClassicLevelFlow *owner = NULL;
    CHECK(!ab_classic_level_flow_open((AbClassicLevelFlowConfig){0}, &owner, NULL) &&
          !owner);
    QueryMenu menu;
    CHECK(query_menu_prepare((ReferenceRootQuery){.root = 1}, &menu, NULL));
    AbEventManager *events = NULL;
    CHECK(ab_event_manager_open((AbEventManagerLimits){.event_types = 4,
                                                       .listeners_per_type = 2,
                                                       .notification_depth = 8,
                                                       .storage_bytes = 8192},
                                (AbEventManagerHost){0}, &events, NULL));
    AbClassicLevelFlowConfig config = {.menus = menu.manager,
                                       .events = events,
                                       .loading_init = text("init"),
                                       .loading_done = text("loaded"),
                                       .flow_done = text("done"),
                                       .requests = 4,
                                       .queue_generations = 3,
                                       .depth_limit = 8,
                                       .work_limit = 128,
                                       .storage_bytes = 8192};
    unsigned shows = 0;
    AbClassicLevelFlowListener listener = {&shows, edge_show, edge_not_done};
    CHECK(ab_classic_level_flow_open(config, &owner, NULL));
    size_t bytes = ab_classic_level_flow_bytes(owner);
    CHECK(bytes <= config.storage_bytes && !ab_classic_level_flow_update(owner, NULL) &&
          ab_classic_level_flow_initialize(owner, NULL) &&
          ab_classic_level_flow_push(owner, text("same").string, 10, &listener, NULL) &&
          ab_classic_level_flow_push(owner, text("same").string, 20, &listener, NULL));
    AbError error = {0};
    CHECK(!ab_classic_level_flow_update(owner, &error) &&
          strstr(error.message, "queue generations exhausted"));
    AbClassicLevelFlowView view = ab_classic_level_flow_view(owner);
    CHECK(view.staging.count == 2 && view.initialization.count == 1 &&
          view.staging.items[0] == view.initialization.items[0] && !view.showing &&
          !shows && !ab_classic_level_flow_update(owner, NULL));
    ab_classic_level_flow_close(owner);
    config.storage_bytes = bytes - 1;
    CHECK(!ab_classic_level_flow_open(config, &owner, NULL) && !owner);
    config.storage_bytes = 8192;
    config.queue_generations = 8;
    CHECK(
        ab_classic_level_flow_open(config, &owner, NULL) &&
        ab_classic_level_flow_initialize(owner, NULL) &&
        !ab_classic_level_flow_push(owner, text("bad").string, 25, &listener, &error) &&
        !ab_classic_level_flow_view(owner).staging.count &&
        !ab_classic_level_flow_initialize(owner, NULL));
    ab_classic_level_flow_close(owner);
    CHECK(
        ab_classic_level_flow_open(config, &owner, NULL) &&
        ab_classic_level_flow_initialize(owner, NULL) &&
        ab_classic_level_flow_push(owner, text("active").string, 10, &listener, NULL) &&
        ab_classic_level_flow_update(owner, NULL));
    AbDataEntry entry = {text("id"), text("init")};
    AbDataTable payload = {&entry, 1, 1, NULL};
    CHECK(!ab_event_manager_notify(events, &payload, &error) &&
          strstr(error.message, "Missing reached level-start pause owner"));
    view = ab_classic_level_flow_view(owner);
    CHECK(shows == 1 && view.showing && !view.initialization.count &&
          !ab_classic_level_flow_update(owner, NULL));
    ab_classic_level_flow_close(owner);
    ab_event_manager_close(events);
    ab_ui_menu_manager_close(menu.manager);
    return true;
}

int main(int argc, char **argv) {
    FILE *reference = argc == 2 ? fopen(argv[1], "rb") : NULL;
    if (argc > 2 || (argc == 2 && !reference))
        return 2;
    bool okay = edges() && corpus(reference);
    if (reference)
        okay = fclose(reference) == 0 && okay;
    if (!okay)
        return 1;
    puts(reference ? "Level-start flow matches864 original checkpoints and callbacks"
                   : "Level-start flow callback and queue checks passed");
    return 0;
}
