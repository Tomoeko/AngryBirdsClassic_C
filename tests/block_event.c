#include "ab/block_event.h"
#include "ab/data_order.h"
#include "ab/physics_script.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Block event failed at %d: %s\n", __LINE__, #condition);   \
            return false;                                                              \
        }                                                                              \
    } while (0)

static AbSpan span(const char *name) {
    return (AbSpan){(const uint8_t *)name, strlen(name)};
}

static const char *const names[] = {"first", "second", "third"};

typedef struct EventCheck EventCheck;
typedef struct {
    EventCheck *check;
    uint32_t id;
} EventIdentity;

struct EventCheck {
    uint32_t world, version, handlers, mutation, phase;
    uint32_t events[32][12];
    size_t count, fail_at, definitions_read;
    AbDataTable definitions[6];
    AbBlockEventBinding bindings[2];
    AbBlockEventScript scripts[2];
    EventIdentity script_ids[2], object_ids[3];
    AbBlockEventScript *attached[3][2];
    AbBlockEventObject objects[3];
    AbPhysicsScriptStep physics;
    uint32_t event_selected;
};

static void mutate(EventCheck *check);

static bool event_handler(AbBlockEventScript *script, AbBlockEventObject *object,
                          const AbDataTable *definition, const AbDataValue *arguments,
                          size_t count, AbError *error) {
    (void)error;
    EventIdentity *identity = script->context, *instance = object->context;
    EventCheck *check = identity->check;
    CHECK(check->count < 32 && instance->check == check && count <= 3);
    uint32_t *record = check->events[check->count++];
    memset(record, 0, 12 * sizeof(*record));
    record[0] = check->phase;
    record[1] = instance->id;
    record[2] = identity->id;
    record[3] = (uint32_t)(definition - check->definitions) + 1;
    record[4] = (uint32_t)count + 3;
    for (size_t index = 0; index < count; ++index) {
        const AbDataValue *value = arguments + index;
        record[5 + index * 2] = value->kind == AB_DATA_NUMBER   ? 3u
                                : value->kind == AB_DATA_STRING ? 4u
                                                                : 0u;
        record[6 + index * 2] = value->kind == AB_DATA_NUMBER   ? value->bits
                                : value->kind == AB_DATA_STRING ? 1u
                                                                : 0u;
    }
    record[11] = check->world;
    mutate(check);
    return !check->fail_at || check->count != check->fail_at;
}

static void mutate(EventCheck *check) {
    check->world ^= check->mutation & 7;
    if (check->mutation & 8) {
        check->handlers ^= 2;
        check->bindings[1].function = check->handlers & 2 ? event_handler : NULL;
    }
    if (check->mutation & 16) {
        check->version ^= 1;
    }
    if (check->mutation & 32) {
        check->event_selected ^= 1;
        check->physics.event = span(check->event_selected ? "skip" : "evt");
    }
}

static bool level_step(void *context, float seconds, AbError *error) {
    (void)error;
    EventCheck *check = context;
    CHECK(check->count < 32);
    uint32_t *record = check->events[check->count++];
    memset(record, 0, 12 * sizeof(*record));
    record[0] = check->phase;
    record[4] = 1;
    record[5] = 3;
    memcpy(record + 6, &seconds, sizeof(seconds));
    record[11] = check->world;
    mutate(check);
    return !check->fail_at || check->count != check->fail_at;
}

static void prepare(EventCheck *check, const uint32_t input[8]) {
    *check =
        (EventCheck){.world = input[4], .handlers = input[1], .mutation = input[5]};
    for (size_t index = 0; index < 2; ++index) {
        check->script_ids[index] = (EventIdentity){check, (uint32_t)index + 1};
        check->bindings[index] = (AbBlockEventBinding){
            span("evt"), input[1] & (1u << index) ? event_handler : NULL};
        check->scripts[index] =
            (AbBlockEventScript){check->bindings + index, 1, check->script_ids + index};
    }
    for (size_t index = 0; index < 3; ++index) {
        check->object_ids[index] = (EventIdentity){check, (uint32_t)index + 1};
        check->objects[index] =
            (AbBlockEventObject){.name = span(names[index]),
                                 .scripts = check->attached[index],
                                 .context = check->object_ids + index,
                                 .scripts_present = (input[3] & (1u << index)) != 0,
                                 .tracked = true};
        for (size_t script = 0; script < 2; ++script) {
            if (input[2] & (1u << (index * 2 + script))) {
                size_t slot = check->objects[index].count++;
                check->attached[index][slot] = check->scripts + script;
            }
        }
    }
}

static bool definition(void *context, AbBlockEventObject *object,
                       const AbDataTable **output, AbError *error) {
    (void)error;
    EventCheck *check = context;
    EventIdentity *identity = object->context;
    ++check->definitions_read;
    CHECK(identity->check == check && identity->id >= 1 && identity->id <= 3);
    *output = check->definitions + check->version * 3 + identity->id - 1;
    return true;
}

static bool world_truthy(void *context, AbSpan name, bool *present, AbError *error) {
    (void)error;
    EventCheck *check = context;
    for (size_t index = 0; index < 3; ++index) {
        if (name.size == strlen(names[index]) &&
            memcmp(name.data, names[index], name.size) == 0) {
            *present = (check->world & (1u << index)) != 0;
            return true;
        }
    }
    return false;
}

static bool edge_checks(void) {
    uint32_t input[8] = {0, 1, 3, 1, 7, 24, 0, 0};
    EventCheck check;
    prepare(&check, input);
    AbBlockEventHost host = {&check, definition, world_truthy};
    CHECK(ab_block_event(check.objects, span("evt"), NULL, 0, host, NULL));
    CHECK(check.count == 2 && check.definitions_read == 2 && check.events[0][3] == 1 &&
          check.events[1][3] == 4 && check.version == 0);
    prepare(&check, input);
    check.fail_at = 1;
    CHECK(!ab_block_event(check.objects, span("evt"), NULL, 0, host, NULL) &&
          check.count == 1 && check.version == 1 && check.handlers == 3);
    prepare(&check, input);
    check.objects[0].scripts_present = false;
    check.objects[0].count = SIZE_MAX;
    CHECK(ab_block_event(check.objects, span("evt"), NULL, 0, (AbBlockEventHost){0},
                         NULL) &&
          check.count == 0);
    check.objects[0].scripts_present = true;
    CHECK(!ab_block_event(check.objects, span("evt"), NULL, 0, host, NULL));
    CHECK(!ab_block_event(NULL, span("evt"), NULL, 0, host, NULL));
    CHECK(!ab_block_event(check.objects, (AbSpan){NULL, 1}, NULL, 0, host, NULL));
    CHECK(!ab_block_event(check.objects, span("evt"), NULL, 1, host, NULL));
    CHECK(ab_block_global_event(NULL, 0, span("evt"), NULL, 0, (AbBlockEventHost){0},
                                NULL));
    prepare(&check, input);
    check.world = 0;
    AbBlockEventObject *objects[] = {check.objects};
    CHECK(ab_block_global_event(objects, 1, span("evt"), NULL, 0, host, NULL) &&
          !check.objects[0].tracked && check.count == 0);
    check.world = 7;
    CHECK(ab_block_global_event(objects, 1, span("evt"), NULL, 0, host, NULL) &&
          !check.objects[0].tracked && check.count == 0);
    CHECK(!ab_block_global_event(NULL, 1, span("evt"), NULL, 0, host, NULL));
    CHECK(!ab_physics_script_step(NULL, 0, NULL));
    prepare(&check, input);
    check.fail_at = 1;
    check.physics = (AbPhysicsScriptStep){.level_step = level_step, .context = &check};
    CHECK(!ab_physics_script_step(&check.physics, -0.0f, NULL) && check.count == 1 &&
          check.events[0][6] == UINT32_C(0x80000000) && check.version == 1);
    return true;
}

static bool read_words(FILE *file, uint32_t *values, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        CHECK(fscanf(file, "%" SCNx32, values + index) == 1);
    }
    CHECK(fgetc(file) == '\n');
    return true;
}

static bool native_checks(FILE *file, bool physics) {
    char header[80];
    static const size_t permutations[6][3] = {{0, 1, 2}, {0, 2, 1}, {1, 0, 2},
                                              {1, 2, 0}, {2, 0, 1}, {2, 1, 0}};
    const char *expected_header = physics ? "AB_ANDROID_PHYSICS_SCRIPT_V1 4096\n"
                                          : "AB_ANDROID_BLOCK_EVENT_V1 4096\n";
    CHECK(fgets(header, sizeof(header), file) && strcmp(header, expected_header) == 0);
    size_t total = 0;
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t input[9] = {0}, expected[6] = {0};
        CHECK(read_words(file, input, physics ? 9 : 8) &&
              read_words(file, expected, 5) && input[6] < 6 && input[7] <= 3 &&
              expected[4] <= 32);
        CHECK(!physics || read_words(file, expected + 5, 1));
        EventCheck check;
        prepare(&check, input);
        AbBlockEventHost host = {&check, definition, world_truthy};
        AbDataValue arguments[] = {{.kind = AB_DATA_NUMBER, .bits = input[0]},
                                   {0},
                                   {.kind = AB_DATA_STRING, .string = span("payload")}};
        AbDataEntry entries[3];
        size_t indices[3], capacity = 0;
        AbStringOrderNode nodes[4];
        AbStringOrderWorkspace scratch = {indices, 3, nodes, 4};
        for (size_t index = 0; index < 3; ++index) {
            size_t source = permutations[input[6]][index];
            entries[index] =
                (AbDataEntry){{.kind = AB_DATA_STRING, .string = span(names[source])},
                              {.kind = AB_DATA_BOOLEAN, .bits = 1}};
        }
        AbDataTable table = {entries, 3, 3, NULL};
        CHECK(ab_data_string_order_growing(&table, &scratch, &capacity, NULL) &&
              capacity == 4);
        AbBlockEventObject *objects[3];
        for (size_t index = 0; index < 3; ++index) {
            objects[index] = check.objects + permutations[input[6]][indices[index]];
        }
        check.physics =
            (AbPhysicsScriptStep){.objects = objects,
                                  .count = 3,
                                  .host = host,
                                  .event = span("evt"),
                                  .level_step = input[8] == 3 ? level_step : NULL,
                                  .context = &check};
        for (check.phase = 0; check.phase < 2; ++check.phase) {
            if (physics) {
                float seconds;
                memcpy(&seconds, input, sizeof(seconds));
                CHECK(ab_physics_script_step(&check.physics, seconds, NULL));
            } else {
                CHECK(ab_block_global_event(objects, 3, span("evt"), arguments,
                                            input[7], host, NULL));
            }
        }
        check.phase = 2;
        for (size_t index = 0; index < 3; ++index) {
            CHECK(ab_block_event(check.objects + index, span("evt"), arguments,
                                 input[7], host, NULL));
        }
        uint32_t tracked = 0;
        for (size_t index = 0; index < 3; ++index) {
            tracked |= check.objects[index].tracked ? 1u << index : 0;
        }
        uint32_t actual[] = {check.world, check.version,         check.handlers,
                             tracked,     (uint32_t)check.count, check.event_selected};
        if (memcmp(actual, expected, (physics ? 6u : 5u) * sizeof(*actual)) != 0) {
            fprintf(stderr, "Native block event final state %zu differs\n", item);
            return false;
        }
        for (size_t index = 0; index < check.count; ++index) {
            uint32_t record[12];
            CHECK(read_words(file, record, 12));
            if (memcmp(record, check.events[index], sizeof(record)) != 0) {
                fprintf(stderr, "Native block event case %zu call %zu differs\n", item,
                        index);
                return false;
            }
        }
        total += check.count;
    }
    const char *footer = physics ? "AB physics script reference complete\n"
                                 : "AB block event reference complete\n";
    CHECK(fgets(header, sizeof(header), file) && strcmp(header, footer) == 0 &&
          fgetc(file) == EOF && !ferror(file));
    printf("All 4096 native %s cases and %zu handler calls match\n",
           physics ? "physics-script" : "block-event", total);
    return true;
}

int main(int argc, char **argv) {
    bool physics = argc == 3 && strcmp(argv[1], "--physics") == 0;
    if ((argc > 2 && !physics) || !edge_checks()) {
        return 1;
    }
    if (argc >= 2) {
        FILE *file = fopen(argv[physics ? 2 : 1], "rb");
        bool okay = file && native_checks(file, physics);
        if (file && fclose(file) != 0) {
            okay = false;
        }
        return okay ? 0 : 1;
    }
    puts("Block event order, late handlers/definitions, pruning and partial failure "
         "pass");
    return 0;
}
