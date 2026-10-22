#include "ab/object_removal_notify.h"
#include "object_removal_notify_cases.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Removal notification failed at %d: %s\n", __LINE__,       \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

typedef struct {
    AbObjectRemovalObject object;
    AbObjectRemovalNotifyHost host;
    AbObjectRemovalLevel levels[2];
    AbDataTable tables[6];
    AbDataEntry catalog, blocks, raw;
    AbDataValue marker;
    uint32_t input[15], bindings[5], level_binding, level_kinds[2];
    uint32_t events[2][24], count, calls[2], binding_queries, level_queries;
    bool fail_call;
} Fixture;

static const char *const strings[] = {"Object", "Missing", "handlerA", "handlerB",
                                      "level",  "next",    "truthy"};

static AbDataValue text(const char *value) {
    return (AbDataValue){.kind = AB_DATA_STRING,
                         .string = {(const uint8_t *)value, strlen(value)}};
}

static bool named(AbDataValue value, const char *wanted) {
    return value.kind == AB_DATA_STRING && value.string.data &&
           value.string.size == strlen(wanted) &&
           !memcmp(value.string.data, wanted, value.string.size);
}

static AbDataValue table(Fixture *fixture, size_t index) {
    return (AbDataValue){.kind = AB_DATA_TABLE, .table = fixture->tables + index};
}

static AbDataValue scalar(Fixture *fixture, uint32_t kind) {
    switch (kind) {
        case 0:
            return (AbDataValue){0};
        case 1:
        case 2:
            return (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = kind == 2};
        case 3:
            return (AbDataValue){.kind = AB_DATA_NUMBER};
        case 4:
            return text("truthy");
        default:
            return table(fixture, 0);
    }
}

static AbDataValue handler_key(Fixture *fixture, uint32_t kind) {
    return kind == 4   ? text("handlerA")
           : kind == 5 ? table(fixture, 5)
           : kind == 6 ? text("handlerB")
                       : scalar(fixture, kind);
}

static AbDataValue level_key(Fixture *fixture, uint32_t kind) {
    return kind == 4   ? text("level")
           : kind == 5 ? table(fixture, 5)
           : kind == 6 ? text("next")
                       : scalar(fixture, kind);
}

static AbUiPointerValue function_value(Fixture *fixture, uint32_t kind) {
    switch (kind) {
        case 0:
            return (AbUiPointerValue){0};
        case 1:
        case 2:
            return (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN,
                                      .boolean = kind == 2};
        case 3:
            return (AbUiPointerValue){.kind = AB_UI_POINTER_NUMBER};
        case 4:
            return (AbUiPointerValue){.kind = AB_UI_POINTER_OBJECT,
                                      .object = (uintptr_t)fixture->tables};
        default:
            return (AbUiPointerValue){.kind = AB_UI_POINTER_FUNCTION,
                                      .function = kind - 4u};
    }
}

static void set_level(Fixture *fixture, size_t index, uint32_t kind) {
    fixture->level_kinds[index] = kind;
    fixture->levels[index].on_remove = function_value(fixture, kind);
}

static void value_words(Fixture *fixture, AbDataValue value, uint32_t output[2]) {
    static const uint32_t types[] = {0, 1, 3, 4, 5};
    output[0] = types[value.kind];
    output[1] = 0;
    if (value.kind == AB_DATA_BOOLEAN || value.kind == AB_DATA_NUMBER) {
        output[1] = value.bits;
    } else if (value.kind == AB_DATA_STRING) {
        for (uint32_t index = 0; index < 7; ++index) {
            if (named(value, strings[index])) {
                output[1] = index + 1u;
                break;
            }
        }
    } else if (value.kind == AB_DATA_TABLE) {
        output[1] = (uint32_t)(value.table - fixture->tables) + 201u;
    }
}

static void snapshot(Fixture *fixture, uint32_t output[20]) {
    const AbDataValue fields[] = {fixture->object.definition, fixture->marker,
                                  fixture->host.block_table, fixture->host.level_name,
                                  fixture->raw.value};
    for (size_t field = 0; field < 5; ++field) {
        value_words(fixture, fields[field], output + field * 2u);
    }
    memcpy(output + 10, fixture->bindings, sizeof(fixture->bindings));
    output[15] = fixture->level_binding;
    output[16] = fixture->level_kinds[0];
    output[17] = fixture->level_kinds[1];
    output[18] = fixture->calls[0];
    output[19] = fixture->calls[1];
}

static size_t binding_index(Fixture *fixture, AbDataValue key) {
    if (key.kind == AB_DATA_BOOLEAN && key.bits) {
        return 0;
    }
    if (key.kind == AB_DATA_NUMBER && !(key.bits & UINT32_C(0x7fffffff))) {
        return 1;
    }
    return named(key, "handlerA")                                          ? 2u
           : key.kind == AB_DATA_TABLE && key.table == fixture->tables + 5 ? 3u
           : named(key, "handlerB")                                        ? 4u
                                                                           : SIZE_MAX;
}

static bool binding(void *context, AbDataValue key, AbUiPointerValue *output,
                    AbError *error) {
    Fixture *fixture = context;
    (void)error;
    ++fixture->binding_queries;
    size_t index = binding_index(fixture, key);
    *output = index < 5 ? function_value(fixture, fixture->bindings[index])
                        : (AbUiPointerValue){0};
    return true;
}

static bool level(void *context, AbDataValue current_name,
                  AbObjectRemovalLevel **output, AbError *error) {
    Fixture *fixture = context;
    (void)current_name;
    (void)error;
    ++fixture->level_queries;
    *output = fixture->level_binding == 1 || fixture->level_binding == 2
                  ? fixture->levels + fixture->level_binding - 1u
                  : NULL;
    return true;
}

static bool call(void *context, AbObjectRemovalNotice notice, AbUiPointerValue function,
                 AbObjectRemovalObject *object, AbError *error) {
    Fixture *fixture = context;
    CHECK(object == &fixture->object && fixture->count < 2 &&
          function.kind == AB_UI_POINTER_FUNCTION && function.function >= 1 &&
          function.function <= 2);
    uint32_t *event = fixture->events[fixture->count++];
    event[0] = (uint32_t)notice + 1u;
    event[1] = (uint32_t)function.function;
    event[2] = 501;
    event[3] = 1;
    snapshot(fixture, event + 4);
    ++fixture->calls[notice];
    uint32_t mutation =
        fixture->input[notice == AB_OBJECT_REMOVAL_DEFINITION ? 10 : 11];
    if (notice == AB_OBJECT_REMOVAL_DEFINITION) {
        if (mutation & 1u) {
            fixture->host.level_name = text("next");
        }
        if (mutation & 2u) {
            fixture->host.level_name = scalar(fixture, 1);
        }
        if (mutation & 4u) {
            fixture->level_binding = 2;
        }
        if (mutation & 8u) {
            set_level(fixture, 1, 6);
        }
        if (mutation & 16u) {
            fixture->raw.value = text("handlerB");
        }
        if (mutation & 32u) {
            object->definition = text("Missing");
        }
        if (mutation & 64u) {
            fixture->marker = scalar(fixture, 2);
        }
        if (mutation & 128u) {
            fixture->host.block_table = (AbDataValue){0};
        }
    } else {
        if (mutation & 1u) {
            fixture->marker = scalar(fixture, 1);
        }
        if (mutation & 2u) {
            object->definition = text("Object");
        }
        if (mutation & 4u) {
            fixture->host.level_name = (AbDataValue){0};
        }
        if (mutation & 8u) {
            fixture->raw.value = scalar(fixture, 1);
        }
        if (mutation & 16u) {
            set_level(fixture, fixture->level_binding - 1u, 0);
        }
        if (mutation & 32u) {
            fixture->bindings[2] = 1;
        }
    }
    return fixture->fail_call ? ab_fail(error, "Injected removal notification failure")
                              : true;
}

static void prepare(Fixture *fixture, uint32_t sample) {
    memset(fixture, 0, sizeof(*fixture));
    object_removal_notify_case(sample, fixture->input);
    fixture->raw =
        (AbDataEntry){text("onRemoved"), handler_key(fixture, fixture->input[4])};
    fixture->tables[4] = (AbDataTable){&fixture->raw, 1, 1, NULL};
    uint32_t raw = fixture->input[3];
    fixture->blocks = (AbDataEntry){text("Object"), raw == 6   ? table(fixture, 4)
                                                    : raw == 5 ? table(fixture, 1)
                                                               : scalar(fixture, raw)};
    fixture->tables[3] = (AbDataTable){&fixture->blocks, 1, 1, NULL};
    uint32_t blocks = fixture->input[2];
    fixture->catalog =
        (AbDataEntry){text("blocks"), blocks == 6   ? table(fixture, 3)
                                      : blocks == 5 ? table(fixture, 1)
                                                    : scalar(fixture, blocks)};
    fixture->tables[2] = (AbDataTable){&fixture->catalog, 1, 1, NULL};
    uint32_t definition = fixture->input[12];
    fixture->object =
        (AbObjectRemovalObject){definition == 0   ? text("Object")
                                : definition == 4 ? text("Missing")
                                                  : scalar(fixture, definition - 1u),
                                fixture};
    fixture->marker = scalar(fixture, fixture->input[13]);
    for (size_t index = 0; index < 5; ++index) {
        fixture->bindings[index] = fixture->input[index == 4 ? 14 : 5];
    }
    fixture->level_binding = fixture->input[7];
    set_level(fixture, 0, fixture->input[8]);
    set_level(fixture, 1, fixture->input[9]);
    uint32_t catalog = fixture->input[1];
    fixture->host =
        (AbObjectRemovalNotifyHost){fixture,
                                    catalog == 6   ? table(fixture, 2)
                                    : catalog == 5 ? table(fixture, 1)
                                                   : scalar(fixture, catalog),
                                    level_key(fixture, fixture->input[6]),
                                    binding,
                                    level,
                                    call};
}

static bool edges(void) {
    Fixture fixture;
    prepare(&fixture, 1);
    fixture.raw.value = text("handlerA");
    fixture.bindings[2] = 5;
    fixture.host.level_name = (AbDataValue){0};
    fixture.input[10] = 64;
    fixture.fail_call = true;
    CHECK(!ab_object_removal_notify(&fixture.object, &fixture.host, NULL));
    CHECK(fixture.marker.kind == AB_DATA_BOOLEAN && fixture.marker.bits &&
          fixture.binding_queries == 2 && !fixture.level_queries);
    prepare(&fixture, 1);
    fixture.raw.value = (AbDataValue){0};
    fixture.host.level_name = text("level");
    fixture.level_binding = 1;
    set_level(&fixture, 0, 5);
    fixture.input[11] = 0;
    CHECK(ab_object_removal_notify(&fixture.object, &fixture.host, NULL));
    CHECK(fixture.level_queries == 3 && fixture.calls[1] == 1 &&
          !fixture.binding_queries);
    CHECK(!ab_object_removal_notify(NULL, &fixture.host, NULL));
    return true;
}

static bool words(FILE *file, const uint32_t *actual, size_t count, uint32_t sample,
                  const char *label, size_t *matched) {
    for (size_t field = 0; field < count; ++field) {
        uint32_t expected = 0;
        if (fscanf(file, " %" SCNx32, &expected) != 1 || expected != actual[field]) {
            fprintf(stderr, "Removal notify sample %u %s[%zu]: C=%08x original=%08x\n",
                    sample, label, field, actual[field], expected);
            return false;
        }
        ++*matched;
    }
    return true;
}

static bool reference(FILE *file) {
    char line[96];
    CHECK(fgets(line, sizeof(line), file) &&
          !strcmp(line, "AB_ANDROID_OBJECT_REMOVAL_NOTIFY_V1 4096\n"));
    size_t matched = 0, failures = 0, calls[2] = {0};
    for (uint32_t sample = 0; sample < 4096; ++sample) {
        Fixture fixture;
        prepare(&fixture, sample);
        CHECK(words(file, fixture.input, 15, sample, "input", &matched));
        uint32_t failed =
                     !ab_object_removal_notify(&fixture.object, &fixture.host, NULL),
                 state[20];
        CHECK(words(file, &failed, 1, sample, "result", &matched));
        snapshot(&fixture, state);
        CHECK(words(file, state, 20, sample, "state", &matched) &&
              words(file, &fixture.count, 1, sample, "count", &matched));
        for (uint32_t event = 0; event < fixture.count; ++event) {
            CHECK(words(file, fixture.events[event], 24, sample, "event", &matched));
        }
        failures += failed;
        calls[0] += fixture.calls[0];
        calls[1] += fixture.calls[1];
    }
    CHECK(fscanf(file, " %95[^\n]", line) == 1 &&
          !strcmp(line, "AB object removal notify reference complete") &&
          fscanf(file, " %1s", line) == EOF);
    printf("All 4,096 original removal notification helpers match: %zu words, %zu "
           "failures, %zu definition and %zu level calls.\n",
           matched, failures, calls[0], calls[1]);
    return true;
}

int main(int argc, char **argv) {
    if (argc == 1) {
        return edges() ? 0 : 1;
    }
    if (argc != 2) {
        return 2;
    }
    FILE *file = fopen(argv[1], "r");
    bool okay = file && reference(file);
    if (file) {
        fclose(file);
    }
    return okay ? 0 : 1;
}
