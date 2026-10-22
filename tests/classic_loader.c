#include "ab/classic_loader.h"

#include <stdio.h>
#include <inttypes.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Loader failed at %d: %s\n", __LINE__, #condition);        \
            return false;                                                              \
        }                                                                              \
    } while (0)

typedef struct {
    AbClassicLoader *owner;
    AbClassicLoaderState state;
    AbClassicLoaderRequest request;
    AbClassicLoaderCall first, second, completion, *items[2];
    AbClassicLoaderCalls calls, replacement;
    AbEventManager *events;
    AbEventListener listener;
    AbDataValue time, skip, ids[4];
    AbDataEntry completion_entry;
    AbDataTable completion_event;
    unsigned trace[24];
    size_t count, notices;
    const AbDataTable *retained[4];
    bool fail_second, reset_frame, no_completion;
} LoaderTest;

static AbDataValue text(const char *value) {
    return (AbDataValue){.kind = AB_DATA_STRING,
                         .string = {(const uint8_t *)value, strlen(value)}};
}

static AbDataValue number(float value) {
    AbDataValue result = {.kind = AB_DATA_NUMBER};
    memcpy(&result.bits, &value, sizeof(value));
    return result;
}

static AbDataValue field(const AbDataTable *table, const char *name) {
    const AbDataValue *value = ab_data_get(table, text(name));
    return value ? *value : (AbDataValue){0};
}

static bool record(LoaderTest *test, unsigned kind) {
    CHECK(test->count < 24);
    test->trace[test->count++] = kind;
    return true;
}

static bool complete(void *context, AbError *error) {
    LoaderTest *test = context;
    (void)error;
    CHECK(!test->state.loading.bits && record(test, 7));
    return true;
}

static bool first(void *context, AbError *error) {
    LoaderTest *test = context;
    (void)error;
    CHECK(test->state.frame.bits == number(1).bits && test->skip.bits &&
          test->state.loading.bits && record(test, 2) &&
          !ab_classic_loader_update(test->owner, NULL));
    test->items[1] = &test->second;
    test->state.items = &test->replacement;
    return true;
}

static bool second(void *context, AbError *error) {
    LoaderTest *test = context;
    CHECK(test->state.items == &test->replacement && record(test, 3));
    return !test->fail_second || ab_fail(error, "Intentional loader item failure");
}

static bool log_message(void *context, AbClassicLoaderLog kind, AbError *error) {
    LoaderTest *test = context;
    (void)error;
    CHECK(test->state.loading.bits && record(test, kind ? 8u : 4u));
    return true;
}

static bool notice(void *context, AbDataTable *event, AbError *error) {
    LoaderTest *test = context;
    AbDataValue id = field(event, "id");
    CHECK(id.kind == AB_DATA_STRING && id.string.size == 1);
    if (id.string.data[0] == 's') {
        CHECK(!test->state.loading.bits && test->state.frame.bits == number(1).bits &&
              field(event, "loadingPageAlpha").bits == number(0.4f).bits &&
              record(test, 1) &&
              !ab_classic_loader_event(test->owner, &test->request, NULL));
        test->request.items = &test->calls;
    } else if (id.string.data[0] == 'h') {
        CHECK(test->state.loading.bits && record(test, 5));
        if (test->reset_frame)
            test->state.frame = number(10);
    } else {
        CHECK(id.string.data[0] == 'c' && !test->state.loading.bits &&
              event == &test->completion_event && record(test, 6));
        test->state.completion_call = test->no_completion ? NULL : &test->completion;
        return true;
    }
    CHECK(test->notices < 4);
    for (size_t index = 0; index < test->notices; ++index)
        CHECK(test->retained[index] != event);
    test->retained[test->notices++] = event;
    (void)error;
    return true;
}

static bool open_test(LoaderTest *test, AbDataValue duration, AbDataValue hidden,
                      AbError *error) {
    *test = (LoaderTest){0};
    test->state.loading = (AbDataValue){.kind = AB_DATA_BOOLEAN};
    test->state.frame = number(0);
    const char *names[] = {"d", "s", "h", "c"};
    for (size_t index = 0; index < 4; ++index)
        test->ids[index] = text(names[index]);
    CHECK(ab_event_manager_open((AbEventManagerLimits){4, 2, 2, 2, 4, 32768},
                                (AbEventManagerHost){0}, &test->events, error));
    test->listener = (AbEventListener){text("LoaderObserver").string, test, notice};
    for (size_t index = 1; index < 4; ++index)
        CHECK(ab_event_manager_add(test->events, test->ids[index], &test->listener,
                                   error));
    test->first = (AbClassicLoaderCall){test, first};
    test->second = (AbClassicLoaderCall){test, second};
    test->completion = (AbClassicLoaderCall){test, complete};
    test->items[0] = &test->first;
    test->calls = (AbClassicLoaderCalls){test->items, 2};
    test->completion_entry = (AbDataEntry){text("id"), test->ids[3]};
    test->completion_event = (AbDataTable){&test->completion_entry, 1, 1, NULL};
    test->request = (AbClassicLoaderRequest){
        .id = test->ids[0],
        .hidden = hidden,
        .loading_page_alpha = number(0.4f),
        .minimum_display_time = duration,
        .completion_event = {.kind = AB_DATA_TABLE, .table = &test->completion_event},
        .items = &test->calls};
    CHECK(ab_classic_loader_open((AbClassicLoaderConfig){.state = &test->state,
                                                         .script_time = &test->time,
                                                         .do_loading = test->ids,
                                                         .show_loading = test->ids + 1,
                                                         .hide_loading = test->ids + 2,
                                                         .skip_input = &test->skip,
                                                         .events = test->events,
                                                         .notifications = 4,
                                                         .storage_bytes = 4096,
                                                         .context = test,
                                                         .log = log_message},
                                 &test->owner, error));
    CHECK(ab_classic_loader_bytes(test->owner) <= 4096 &&
          ab_classic_loader_update(test->owner, error) &&
          test->state.frame.bits == number(1).bits && test->skip.kind == AB_DATA_NIL);
    return true;
}

static void close_test(LoaderTest *test) {
    ab_classic_loader_close(test->owner);
    ab_event_manager_close(test->events);
}

static bool immediate(AbError *error) {
    LoaderTest test;
    CHECK(open_test(&test, (AbDataValue){0}, (AbDataValue){0}, error));
    test.request.items = NULL; /* The real show observer replaces this field. */
    CHECK(ab_classic_loader_event(test.owner, &test.request, error) &&
          test.state.items == &test.calls && test.state.loading.bits &&
          !test.state.frame.bits && test.state.loading_frame.bits == number(1).bits &&
          ab_classic_loader_update(test.owner, error) && test.count == 1 &&
          ab_classic_loader_update(test.owner, error) && test.count == 7 &&
          !test.state.loading.bits && test.state.frame.bits == number(2).bits);
    const unsigned expected[] = {1, 2, 3, 4, 5, 6, 7};
    CHECK(!memcmp(test.trace, expected, sizeof(expected)) &&
          field(test.retained[0], "loadingPageAlpha").bits == number(0.4f).bits &&
          test.skip.kind == AB_DATA_BOOLEAN && test.skip.bits);
    close_test(&test);
    return true;
}

static bool delayed(AbError *error) {
    LoaderTest test;
    CHECK(open_test(&test, number(0.5f), number(0), error));
    test.time = number(100);
    CHECK(ab_classic_loader_event(test.owner, &test.request, error) && !test.count &&
          test.state.minimum_display_time.bits == number(100.5f).bits &&
          ab_classic_loader_update(test.owner, error) &&
          ab_classic_loader_update(test.owner, error) && test.count == 2 &&
          test.state.loading.bits);
    test.time = number(100.25f);
    CHECK(ab_classic_loader_update(test.owner, error) && test.count == 2);
    test.time = number(100.5f);
    test.reset_frame = true;
    CHECK(ab_classic_loader_update(test.owner, error) && test.count == 6 &&
          test.trace[2] == 8 && test.trace[3] == 5 && test.trace[4] == 6 &&
          test.trace[5] == 7 && !test.state.loading.bits &&
          test.state.frame.bits == number(11).bits);
    test.state.frame = number(16777216);
    test.time = (AbDataValue){0};
    CHECK(ab_classic_loader_update(test.owner, error) &&
          test.state.frame.bits == number(16777216).bits);
    close_test(&test);
    return true;
}

static bool failure(AbError *error) {
    LoaderTest test;
    CHECK(open_test(&test, (AbDataValue){0}, (AbDataValue){0}, error));
    test.fail_second = true;
    CHECK(ab_classic_loader_event(test.owner, &test.request, error) &&
          ab_classic_loader_update(test.owner, error));
    AbError late = {0};
    CHECK(!ab_classic_loader_update(test.owner, &late) &&
          strstr(late.message, "Intentional loader item failure") &&
          test.state.loading.bits && test.state.frame.bits == number(1).bits &&
          test.skip.bits && test.count == 3 && test.notices == 1 &&
          !ab_classic_loader_update(test.owner, NULL));
    close_test(&test);
    return true;
}

static uint32_t native_tag(AbDataValue value) {
    return value.kind == AB_DATA_NUMBER ? 3u : value.kind == AB_DATA_BOOLEAN ? 1u : 0u;
}

static bool words(FILE *file, uint32_t *output, size_t count) {
    for (size_t index = 0; index < count; ++index)
        if (fscanf(file, "%" SCNx32, output + index) != 1)
            return false;
    return true;
}

static bool native_compare(const char *path, AbError *error) {
    FILE *file = fopen(path, "rb");
    CHECK(file);
    char header[80];
    CHECK(fgets(header, sizeof(header), file) &&
          !strcmp(header, "AB_ANDROID_LOADER_V1 6 7\n"));
    for (uint32_t sample = 0; sample < 6; ++sample) {
        LoaderTest test;
        AbDataValue duration = sample == 1 || sample == 4 ? number(0.5f)
                               : sample == 2              ? number(0)
                               : sample == 3 ? (AbDataValue){.kind = AB_DATA_BOOLEAN}
                                             : (AbDataValue){0};
        AbDataValue hidden = sample == 1 ? number(0) : (AbDataValue){0};
        CHECK(open_test(&test, duration, hidden, error));
        test.time = number(100);
        test.reset_frame = sample == 4;
        test.no_completion = sample == 5;
        for (uint32_t step = 0; step < 7; ++step) {
            test.count = 0;
            if (step == 1)
                CHECK(ab_classic_loader_event(test.owner, &test.request, error));
            else if (step >= 2) {
                if (step == 4)
                    test.time = number(100.25f);
                if (step >= 5)
                    test.time = number(100.5f);
                CHECK(ab_classic_loader_update(test.owner, error));
            }
            AbClassicLoaderState *state = &test.state;
            uint32_t actual[] = {native_tag(state->loading),
                                 state->loading.bits,
                                 state->frame.bits,
                                 native_tag(state->loading_frame),
                                 state->loading_frame.bits,
                                 native_tag(state->minimum_display_time),
                                 state->minimum_display_time.bits,
                                 native_tag(test.skip),
                                 test.skip.bits,
                                 state->items == &test.calls         ? 1u
                                 : state->items == &test.replacement ? 2u
                                                                     : 0u,
                                 state->completion_call != NULL,
                                 (uint32_t)test.count,
                                 test.time.bits};
            uint32_t meta[2], expected[13];
            CHECK(words(file, meta, 2) && meta[0] == sample && meta[1] == step &&
                  words(file, expected, 13));
            if (memcmp(actual, expected, sizeof(actual))) {
                fprintf(stderr, "Original loader case %u step %u state\n", sample,
                        step);
                return false;
            }
            uint32_t events[24];
            CHECK(words(file, events, test.count));
            for (size_t index = 0; index < test.count; ++index)
                CHECK(events[index] == test.trace[index]);
        }
        close_test(&test);
    }
    CHECK(fscanf(file, " %79s", header) == 1 && !strcmp(header, "AB") &&
          fgets(header, sizeof(header), file) &&
          !strcmp(header, " loader reference complete\n") && fgetc(file) == EOF &&
          !fclose(file));
    puts("Loader matches 42 original checkpoints and ordered notification/item/"
         "completion calls.");
    return true;
}

int main(int argc, char **argv) {
    if (argc > 2)
        return 2;
    AbError error = {0};
    bool okay = immediate(&error) && delayed(&error) && failure(&error);
    if (okay && argc == 2)
        okay = native_compare(argv[1], &error);
    if (!okay && error.message[0])
        fprintf(stderr, "%s\n", error.message);
    return okay ? 0 : 1;
}
