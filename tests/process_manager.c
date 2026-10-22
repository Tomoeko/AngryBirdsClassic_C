#include "ab/process_manager.h"
#include "ab/object_property_access.h"

#include <math.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "ProcessManager failed at %d: %s\n", __LINE__,             \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

typedef struct ProcessTest ProcessTest;
typedef struct {
    ProcessTest *test;
    unsigned index;
} ActionContext;

struct ProcessTest {
    AbProcessManager *owner;
    AbEventManager *events;
    AbEventListener listener;
    AbDataValue id, skip;
    AbDataEntry entries[2][8];
    AbDataTable tables[2];
    AbProcessAction actions[2], *bindings[2];
    ActionContext contexts[2];
    unsigned trace[64];
    uint32_t recorded[64][5];
    size_t trace_count, calls[2], finishes[2], notices;
    float elapsed[8], durations[8];
    const AbDataTable *retained[8];
    bool mutate, append, fail;
};

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

static bool set(AbDataTable *table, const char *name, AbDataValue value) {
    AbObjectPropertyAccess access = {
        .physics_object = {.kind = AB_DATA_TABLE, .table = table}};
    return ab_object_property_set(&access, text(name).string, value, NULL);
}

static bool record(ProcessTest *test, unsigned kind) {
    CHECK(test->trace_count < 64);
    memset(test->recorded[test->trace_count], 0, sizeof(test->recorded[0]));
    test->recorded[test->trace_count][0] = kind;
    test->trace[test->trace_count++] = kind;
    return true;
}

static bool action(void *context, AbDataValue item, float elapsed, float duration,
                   float seconds, AbDataValue meta, AbError *error) {
    ActionContext *state = context;
    ProcessTest *test = state->test;
    CHECK(item.kind == AB_DATA_NUMBER &&
          item.bits == number((float)state->index).bits &&
          meta.kind == AB_DATA_BOOLEAN && meta.bits && seconds >= 0 &&
          test->skip.kind == AB_DATA_BOOLEAN && test->skip.bits &&
          !ab_process_manager_update(test->owner, seconds, &test->skip, NULL));
    size_t call = test->calls[state->index]++;
    CHECK(call < 8 && record(test, state->index * 10 + 1));
    uint32_t *row = test->recorded[test->trace_count - 1];
    row[1] = number(elapsed).bits;
    row[2] = number(duration).bits;
    row[3] = number(seconds).bits;
    row[4] = item.bits;
    if (!state->index) {
        test->elapsed[call] = elapsed;
        test->durations[call] = duration;
    }
    if (test->append && !state->index && !call)
        CHECK(ab_process_manager_insert(test->owner, test->actions + 1, error));
    if (test->mutate)
        CHECK(set(test->tables + state->index, "duration", number(0.25f)));
    return !test->fail || ab_fail(error, "Intentional process action failure");
}

static bool finished(void *context, AbError *error) {
    ActionContext *state = context;
    ProcessTest *test = state->test;
    (void)error;
    ++test->finishes[state->index];
    CHECK(record(test, state->index * 10 + 2));
    if (test->mutate)
        CHECK(set(test->tables + state->index, "finished",
                  (AbDataValue){.kind = AB_DATA_BOOLEAN}));
    return true;
}

static bool log_action(void *context, AbProcessLog kind, AbDataValue name,
                       AbError *error) {
    ProcessTest *test = context;
    (void)kind;
    (void)error;
    CHECK(name.kind == AB_DATA_STRING && name.string.size == 1);
    unsigned index = name.string.data[0] == 'a' ? 0u : 1u;
    return record(test, index * 10 + 3);
}

static bool notified(void *context, AbDataTable *event, AbError *error) {
    ProcessTest *test = context;
    (void)error;
    AbDataValue target = field(event, "target");
    bool array = target.kind == AB_DATA_NIL;
    if (target.kind == AB_DATA_NIL) {
        const AbDataValue *slot = ab_data_get(event, number(1));
        CHECK(slot);
        target = *slot;
    }
    CHECK(target.kind == AB_DATA_TABLE && test->notices < 8);
    unsigned index = target.table == test->tables ? 0u : 1u;
    CHECK(target.table == test->tables + index && record(test, index * 10 + 4));
    test->recorded[test->trace_count - 1][4] = array;
    for (size_t earlier = 0; earlier < test->notices; ++earlier)
        CHECK(test->retained[earlier] != event);
    test->retained[test->notices++] = event;
    return true;
}

static bool open_test(ProcessTest *test, AbDataValue first_finished, AbError *error) {
    *test = (ProcessTest){0};
    test->id = text("EID_PROCESS_FINISHED");
    CHECK(ab_event_manager_open((AbEventManagerLimits){2, 2, 2, 2, 4, 32768},
                                (AbEventManagerHost){0}, &test->events, error));
    test->listener = (AbEventListener){text("observer").string, test, notified};
    CHECK(ab_event_manager_add(test->events, test->id, &test->listener, error));
    const char *names[] = {"id",       "name", "start", "duration",
                           "finished", "item", "meta"};
    for (unsigned index = 0; index < 2; ++index) {
        AbDataValue values[] = {text(index ? "second" : "first"),
                                text(index ? "b" : "a"),
                                number(0.25f),
                                number(0.5f),
                                first_finished,
                                number((float)index),
                                {.kind = AB_DATA_BOOLEAN, .bits = 1}};
        for (size_t entry = 0; entry < 7; ++entry)
            test->entries[index][entry] =
                (AbDataEntry){text(names[entry]), values[entry]};
        test->tables[index] = (AbDataTable){test->entries[index], 7, 8, NULL};
        test->contexts[index] = (ActionContext){test, index};
        test->actions[index] = (AbProcessAction){
            test->tables + index, test->contexts + index, action, finished};
        test->bindings[index] = test->actions + index;
    }
    CHECK(ab_process_manager_open((AbProcessManagerConfig){.events = test->events,
                                                           .finished_event = &test->id,
                                                           .bindings = test->bindings,
                                                           .binding_count = 2,
                                                           .actions = 4,
                                                           .generations = 8,
                                                           .notifications = 8,
                                                           .storage_bytes = 32768,
                                                           .context = test,
                                                           .log = log_action},
                                  NULL, &test->owner, error));
    return true;
}

static void close_test(ProcessTest *test) {
    ab_process_manager_close(test->owner);
    ab_event_manager_close(test->events);
}

static bool timing(AbError *error) {
    ProcessTest test;
    CHECK(open_test(&test, (AbDataValue){.kind = AB_DATA_BOOLEAN}, error));
    AbDataTable *state = ab_process_manager_state(test.owner);
    CHECK(ab_process_manager_bytes(test.owner) <= 32768 &&
          !field(state, "running").bits && !field(state, "elapsed").bits &&
          ab_process_manager_update(test.owner, NAN, NULL, error));
    CHECK(ab_process_manager_insert(test.owner, test.actions, error));
    AbDataValue retained = field(state, "actions");
    test.append = true;
    CHECK(ab_process_manager_start(test.owner, error));
    for (unsigned frame = 0; frame < 4; ++frame)
        CHECK(ab_process_manager_update(test.owner, 0.25f, &test.skip, error));
    CHECK(test.calls[0] == 4 && test.calls[1] == 4 && test.finishes[0] == 1 &&
          test.finishes[1] == 1 && !test.notices && field(state, "running").bits &&
          field(state, "elapsed").bits == number(1).bits && test.elapsed[0] == 0 &&
          test.elapsed[2] == 0.5f && test.elapsed[3] == 0.5f);
    CHECK(ab_process_manager_update(test.owner, 0.25f, &test.skip, error) &&
          !field(state, "running").bits && !field(state, "elapsed").bits &&
          test.finishes[0] == 2 && test.finishes[1] == 2 && test.notices == 2 &&
          field(state, "actions").table != retained.table &&
          retained.table->count == 2 && !field(state, "actions").table->count);
    const unsigned end[] = {3, 2, 4, 13, 12, 14};
    CHECK(test.trace_count >= 6 &&
          !memcmp(test.trace + test.trace_count - 6, end, sizeof(end)) &&
          field(test.retained[0], "target").table == test.tables);
    close_test(&test);
    return true;
}

static bool raw_states(AbError *error) {
    ProcessTest test;
    CHECK(open_test(&test, (AbDataValue){0}, error) &&
          ab_process_manager_insert(test.owner, test.actions, error));
    AbDataTable *state = ab_process_manager_state(test.owner);
    CHECK(set(state, "running", number(0)) &&
          ab_process_manager_update(test.owner, NAN, NULL, error) &&
          !test.trace_count && field(state, "running").kind == AB_DATA_NUMBER);
    CHECK(ab_process_manager_start(test.owner, error) &&
          ab_process_manager_update(test.owner, 0.25f, &test.skip, error) &&
          test.calls[0] == 1 && test.finishes[0] == 1 && test.notices == 1 &&
          !field(state, "running").bits);
    close_test(&test);
    CHECK(open_test(&test, (AbDataValue){.kind = AB_DATA_BOOLEAN}, error) &&
          ab_process_manager_insert(test.owner, test.actions, error) &&
          set(test.tables, "id", number(0)) &&
          ab_process_manager_reset_process(test.owner, (AbDataValue){0}, error) &&
          !test.trace_count &&
          ab_process_manager_reset_process(test.owner, number(0), error) &&
          test.trace_count == 3 && test.trace[0] == 2 && test.trace[1] == 3 &&
          test.trace[2] == 4 && field(test.retained[0], "target").kind == AB_DATA_NIL &&
          ab_data_get(test.retained[0], number(1))->table == test.tables);
    close_test(&test);
    return true;
}

static bool mutation_and_failure(AbError *error) {
    ProcessTest test;
    CHECK(open_test(&test, (AbDataValue){.kind = AB_DATA_BOOLEAN}, error) &&
          ab_process_manager_insert(test.owner, test.actions, error) &&
          ab_process_manager_start(test.owner, error));
    test.mutate = true;
    CHECK(ab_process_manager_update(test.owner, 1, &test.skip, error) &&
          test.calls[0] == 2 && test.finishes[0] == 1 && test.elapsed[0] == 0.5f &&
          test.durations[0] == 0.5f && test.elapsed[1] == 0.5f &&
          test.durations[1] == 0.25f && !test.notices);
    close_test(&test);
    CHECK(open_test(&test, (AbDataValue){.kind = AB_DATA_BOOLEAN}, error) &&
          ab_process_manager_insert(test.owner, test.actions, error) &&
          ab_process_manager_start(test.owner, error));
    test.fail = true;
    AbError late = {0};
    AbDataTable *state = ab_process_manager_state(test.owner);
    CHECK(!ab_process_manager_update(test.owner, 0.25f, &test.skip, &late) &&
          strstr(late.message, "Intentional process action failure") &&
          field(state, "elapsed").bits == number(0.25f).bits && test.skip.bits &&
          test.calls[0] == 1 && !test.finishes[0] &&
          !ab_process_manager_update(test.owner, 0.25f, &test.skip, NULL) &&
          field(state, "elapsed").bits == number(0.25f).bits);
    close_test(&test);
    return true;
}

static uint32_t native_tag(AbDataValue value) {
    return value.kind == AB_DATA_NUMBER ? 3u : value.kind == AB_DATA_BOOLEAN ? 1u : 0u;
}

static bool read_words(FILE *file, uint32_t *words, size_t count) {
    for (size_t index = 0; index < count; ++index)
        if (fscanf(file, "%" SCNx32, words + index) != 1)
            return false;
    return true;
}

static bool native_compare(const char *path, AbError *error) {
    FILE *file = fopen(path, "rb");
    CHECK(file);
    char header[80];
    CHECK(fgets(header, sizeof(header), file) &&
          !strcmp(header, "AB_ANDROID_PROCESS_MANAGER_V1 8 9\n"));
    for (uint32_t sample = 0; sample < 8; ++sample) {
        ProcessTest test;
        AbDataValue initial = {.kind = AB_DATA_BOOLEAN};
        if (sample == 2)
            initial = (AbDataValue){0};
        if (sample == 3)
            initial.bits = 1;
        if (sample == 4)
            initial = number(0);
        CHECK(open_test(&test, initial, error));
        if (sample == 5)
            CHECK(set(test.tables, "start", number(1)) &&
                  set(test.tables, "duration", number(-0.5f)));
        test.mutate = sample == 6;
        test.append = sample == 7;
        AbDataTable *state = ab_process_manager_state(test.owner);
        for (uint32_t step = 0; step < 9; ++step) {
            test.trace_count = 0;
            if (step == 1)
                CHECK(ab_process_manager_insert(test.owner, test.actions, error));
            else if (step == 2) {
                if (sample)
                    CHECK(ab_process_manager_start(test.owner, error));
                else
                    CHECK(set(state, "running", number(0)));
            } else if (step >= 3 && step <= 7) {
                if (sample == 7 && step == 5)
                    CHECK(ab_process_manager_reset_process(test.owner, text("first"),
                                                           error));
                else
                    CHECK(ab_process_manager_update(test.owner, 0.25f, &test.skip,
                                                    error));
            } else if (step == 8) {
                CHECK(ab_process_manager_reset(test.owner, error));
            }
            AbDataValue running = field(state, "running");
            AbDataValue elapsed = field(state, "elapsed");
            AbDataValue finished_value = field(test.tables, "finished");
            AbDataValue actions = field(state, "actions");
            uint32_t actual[] = {native_tag(running),
                                 running.bits,
                                 elapsed.bits,
                                 native_tag(test.skip),
                                 test.skip.bits,
                                 (uint32_t)actions.table->count,
                                 native_tag(finished_value),
                                 finished_value.bits,
                                 (uint32_t)test.trace_count};
            uint32_t meta[2], expected[9];
            CHECK(read_words(file, meta, 2) && meta[0] == sample && meta[1] == step &&
                  read_words(file, expected, 9));
            if (memcmp(actual, expected, sizeof(actual))) {
                fprintf(stderr, "Process native case %u step %u state\n", sample, step);
                return false;
            }
            for (size_t index = 0; index < test.trace_count; ++index) {
                uint32_t row[5];
                CHECK(read_words(file, row, 5));
                if (memcmp(row, test.recorded[index], sizeof(row))) {
                    fprintf(stderr, "Process native case %u step %u event %zu\n",
                            sample, step, index);
                    return false;
                }
            }
        }
        close_test(&test);
    }
    CHECK(fscanf(file, " %79s", header) == 1 && !strcmp(header, "AB") &&
          fgets(header, sizeof(header), file) &&
          !strcmp(header, " process manager reference complete\n") &&
          fgetc(file) == EOF);
    CHECK(!fclose(file));
    puts("ProcessManager matches 72 original checkpoints and ordered action/finish/log/"
         "event records, including callback mutation and retained payload shape.");
    return true;
}

int main(int argc, char **argv) {
    if (argc > 2)
        return 2;
    AbError error = {0};
    bool okay = timing(&error) && raw_states(&error) && mutation_and_failure(&error);
    if (okay && argc == 2)
        okay = native_compare(argv[1], &error);
    if (!okay && error.message[0])
        fprintf(stderr, "%s\n", error.message);
    return okay ? 0 : 1;
}
