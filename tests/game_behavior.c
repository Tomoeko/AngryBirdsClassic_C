#include "ab/game_behavior.h"
#include "ab/bird_collision_notify.h"
#include "game_behavior_cases.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Game behavior failed at %d: %s\n", __LINE__, #condition); \
            return false;                                                              \
        }                                                                              \
    } while (0)

typedef struct Fixture Fixture;
typedef struct {
    Fixture *fixture;
    uint32_t id;
} Receiver;

struct Fixture {
    AbStringMap *map;
    AbGameBehaviorGroup groups[5];
    AbGameBehavior *slots[5][4];
    AbGameBehavior behaviors[20];
    AbGameBehaviorMethod methods[20];
    Receiver receivers[20];
    AbUiPointerValue arguments[8];
    AbBlockCollisionObject objects[2];
    AbBirdCollisionObject *collision_objects;
    uintptr_t object_tokens[2];
    uint32_t input[60], method_kinds[20], events[24][20], count, mutated;
    bool fail_method;
};

static AbSpan span(const char *text) {
    return (AbSpan){(const uint8_t *)text, strlen(text)};
}

static void argument_words(const Fixture *fixture, AbUiPointerValue value,
                           uint32_t output[2]) {
    output[0] = 0;
    output[1] = 0;
    switch (value.kind) {
        case AB_UI_POINTER_NONE:
            break;
        case AB_UI_POINTER_BOOLEAN:
            output[0] = 1;
            output[1] = value.boolean;
            break;
        case AB_UI_POINTER_NUMBER:
            output[0] = 3;
            output[1] = value.number_bits;
            break;
        case AB_UI_POINTER_TEXT:
            output[0] = 4;
            output[1] =
                value.text.size == 7 && !memcmp(value.text.data, "payload", 7) ? 1u
                : value.text.size == 5 && !memcmp(value.text.data, "first", 5) ? 2u
                : value.text.size == 6 && !memcmp(value.text.data, "second", 6)
                    ? 3u
                    : UINT32_MAX;
            break;
        case AB_UI_POINTER_OBJECT:
            output[0] = 5;
            output[1] = value.object == fixture->object_tokens[0] ? 101u
                        : value.object == fixture->object_tokens[1]
                            ? 102u
                            : (uint32_t)value.object;
            break;
        default:
            output[0] = UINT32_MAX;
            break;
    }
}

static bool method_first(void *context, const AbUiPointerValue *arguments, size_t count,
                         AbError *error);
static bool method_second(void *context, const AbUiPointerValue *arguments,
                          size_t count, AbError *error);

static void binding(Fixture *fixture, size_t index, uint32_t kind) {
    fixture->method_kinds[index] = kind;
    fixture->methods[index] =
        (AbGameBehaviorMethod){.event = span(behavior_event_names[fixture->input[6]]),
                               .truthy = kind >= 2,
                               .context = fixture->receivers + index,
                               .call = kind == 2   ? method_first
                                       : kind == 3 ? method_second
                                                   : NULL};
}

static bool mutate(Fixture *fixture, uint32_t id, AbError *error) {
    if (fixture->mutated) {
        return true;
    }
    fixture->mutated = 1;
    uint32_t group = (id - 1u) / 4u, slot = (id - 1u) % 4u;
    uint32_t action = fixture->input[5];
    if (action == 1 || action == 2 || action == 8) {
        uint32_t target = action == 8 ? id : id + 1u;
        CHECK(target <= 20);
        binding(fixture, target - 1u, action == 2 ? 3u : 0u);
    } else if (action >= 3 && action <= 5) {
        CHECK(group < 4 && slot < 3);
        fixture->slots[group][action == 5 ? 3u : slot + 1u] =
            action == 3 ? NULL : fixture->behaviors + (action == 4 ? 16u : 17u);
    } else if (action == 6 || action == 7) {
        CHECK(group < 4);
        uint32_t target = action == 6 ? (group + 1u) % 4u : group;
        uintptr_t value;
        bool present;
        CHECK(ab_string_map_get(fixture->map, span(behavior_group_names[target]),
                                &value, &present, error));
        if (present) {
            CHECK(ab_string_map_set(fixture->map, span(behavior_group_names[target]),
                                    (uintptr_t)(fixture->groups + 4), action == 7,
                                    error));
        }
    }
    return true;
}

static bool method(Receiver *receiver, uint32_t variant,
                   const AbUiPointerValue *arguments, size_t count, AbError *error) {
    Fixture *fixture = receiver->fixture;
    CHECK(count == fixture->input[7] && count <= 8 && fixture->count < 24);
    uint32_t *event = fixture->events[fixture->count++];
    event[0] = receiver->id;
    event[1] = variant;
    event[2] = (uint32_t)count;
    event[3] = fixture->mutated;
    for (size_t argument = 0; argument < count; ++argument) {
        argument_words(fixture, arguments[argument], event + 4 + argument * 2);
    }
    if (!mutate(fixture, receiver->id, error)) {
        return false;
    }
    if (fixture->fail_method) {
        return ab_fail(error, "Authored game behavior method failure");
    }
    return true;
}

static bool method_first(void *context, const AbUiPointerValue *arguments, size_t count,
                         AbError *error) {
    return method(context, 1, arguments, count, error);
}

static bool method_second(void *context, const AbUiPointerValue *arguments,
                          size_t count, AbError *error) {
    return method(context, 2, arguments, count, error);
}

static bool prepare(Fixture *fixture, uint32_t sample, AbError *error) {
    AbStringMap *map = fixture->map;
    memset(fixture, 0, sizeof(*fixture));
    fixture->map = map;
    fixture->object_tokens[0] = (uintptr_t)fixture->objects;
    fixture->object_tokens[1] = (uintptr_t)(fixture->objects + 1);
    game_behavior_case(sample, fixture->input);
    CHECK(ab_string_map_reset(map, fixture->input[1], error));
    for (size_t index = 0; index < 20; ++index) {
        fixture->receivers[index] = (Receiver){fixture, (uint32_t)index + 1};
        binding(fixture, index, fixture->input[8 + index]);
        fixture->behaviors[index] = (AbGameBehavior){fixture->methods + index, 1};
    }
    for (size_t group = 0; group < 5; ++group) {
        fixture->groups[group] = (AbGameBehaviorGroup){fixture->slots[group], 4};
        for (size_t slot = 0; slot < 4; ++slot) {
            uint32_t id = group < 4  ? fixture->input[28 + group * 4 + slot]
                          : slot < 3 ? (uint32_t)slot + 17u
                                     : 0;
            fixture->slots[group][slot] = id ? fixture->behaviors + id - 1u : NULL;
        }
    }
    for (uint32_t index = 0; index < fixture->input[2]; ++index) {
        uint32_t group =
            (fixture->input[3] + (fixture->input[4] ? 4u - index : index)) % 4u;
        CHECK(ab_string_map_set(map, span(behavior_group_names[group]),
                                (uintptr_t)(fixture->groups + group), true, error));
    }
    for (size_t index = 0; index < 8; ++index) {
        uint32_t kind = fixture->input[44 + index * 2];
        AbUiPointerValue *value = fixture->arguments + index;
        if (kind == 0) {
            value->kind = AB_UI_POINTER_NONE;
        } else if (kind <= 2) {
            value->kind = AB_UI_POINTER_BOOLEAN;
            value->boolean = kind == 2;
        } else if (kind == 3) {
            value->kind = AB_UI_POINTER_NUMBER;
            value->number_bits = fixture->input[45 + index * 2];
        } else if (kind == 4) {
            value->kind = AB_UI_POINTER_TEXT;
            value->text = span("payload");
        } else {
            value->kind = AB_UI_POINTER_OBJECT;
            value->object = fixture->input[45 + index * 2];
        }
    }
    return true;
}

static bool invoke(Fixture *fixture, AbError *error) {
    if (fixture->input[6] == 2) {
        return ab_game_object_destroy(fixture->map, error);
    }
    if (fixture->input[6] < 2) {
        uint32_t leading = fixture->input[6] == 0 ? 2u : 1u;
        float numbers[6];
        for (size_t index = 0; index < 6; ++index) {
            uint32_t word = fixture->input[45 + (leading + index) * 2];
            memcpy(numbers + index, &word, sizeof(word));
        }
        AbBlockCollisionNotice notice = {
            .first = fixture->objects,
            .second = fixture->objects + 1,
            .other_name = span("payload"),
            .force = numbers[0],
            .damage = numbers[1],
            .normal = {numbers[2], numbers[3]},
            .point = {numbers[4], numbers[5]},
            .normal_present = true,
            .point_present = fixture->input[44 + (leading + 4) * 2] == 3};
        return fixture->input[6] == 0
                   ? ab_game_object_bird_collision(fixture->map, &notice, error)
                   : ab_game_object_block_collision(fixture->map, &notice, error);
    }
    return ab_game_object_delegate(fixture->map,
                                   span(behavior_event_names[fixture->input[6]]),
                                   fixture->arguments, fixture->input[7], error);
}

static bool snapshot(Fixture *fixture, bool success, uint32_t output[46]) {
    memset(output, 0, 46 * sizeof(*output));
    output[0] = !success;
    output[1] = fixture->mutated;
    memcpy(output + 2, fixture->method_kinds, sizeof(fixture->method_kinds));
    for (size_t group = 0; group < 5; ++group) {
        for (size_t slot = 0; slot < 4; ++slot) {
            AbGameBehavior *behavior = fixture->slots[group][slot];
            output[22 + group * 4 + slot] =
                behavior ? (uint32_t)(behavior - fixture->behaviors) + 1u : 0;
        }
    }
    for (size_t group = 0; group < 4; ++group) {
        uintptr_t value;
        bool present;
        CHECK(ab_string_map_get(fixture->map, span(behavior_group_names[group]), &value,
                                &present, NULL));
        if (present) {
            AbGameBehaviorGroup *entry = (AbGameBehaviorGroup *)value;
            output[42 + group] = (uint32_t)(entry - fixture->groups) + 1u;
        }
    }
    return true;
}

static bool lookup_bird(void *context, AbSpan name, AbBirdCollisionObject **output,
                        AbError *error) {
    (void)error;
    Fixture *fixture = context;
    bool first = name.size == 5 && !memcmp(name.data, "first", 5);
    CHECK(first || (name.size == 6 && !memcmp(name.data, "second", 6)));
    *output = fixture->collision_objects + (first ? 0 : 1);
    return true;
}

static bool lookup_block(void *context, AbSpan name, AbBlockCollisionObject **output,
                         AbError *error) {
    (void)error;
    Fixture *fixture = context;
    bool first = name.size == 5 && !memcmp(name.data, "first", 5);
    CHECK(first || (name.size == 6 && !memcmp(name.data, "second", 6)));
    *output = fixture->objects + (first ? 0 : 1);
    return true;
}

static bool empty_definition(void *context, AbSpan name,
                             AbBlockCollisionDefinition **output, AbError *error) {
    (void)context;
    (void)name;
    (void)error;
    static AbBlockCollisionDefinition empty;
    *output = &empty;
    return true;
}

static bool empty_script_event(void *context, const AbBlockCollisionNotice *notice,
                               AbError *error) {
    (void)context;
    (void)notice;
    (void)error;
    return true;
}

static bool contact_composition(AbStringMap *map) {
    Fixture fixture = {.map = map};
    AbError error = {0};
    CHECK(prepare(&fixture, 4, &error));
    fixture.input[5] = 0;
    for (size_t index = 0; index < 20; ++index) {
        binding(&fixture, index, 2);
    }
    AbBirdCollisionScript bird_script = {map, ab_game_object_bird_collision};
    AbBirdCollisionObject birds[2] = {
        {.object.name = span("first"), .script = &bird_script},
        {.object.name = span("second"),
         .script = &bird_script,
         .controllable = {.kind = AB_DATA_BOOLEAN, .bits = 1}}};
    fixture.collision_objects = birds;
    fixture.object_tokens[0] = (uintptr_t)&birds[0].object;
    fixture.object_tokens[1] = (uintptr_t)&birds[1].object;
    AbBirdCollisionNotifyInput bird_input = {.contact = {.first = span("first"),
                                                         .second = span("second"),
                                                         .normal = {0.0f, -0.0f}}};
    AbBirdCollisionNotifyHost bird_host = {.context = &fixture,
                                           .object = lookup_bird,
                                           .definition = empty_definition,
                                           .scripts = empty_script_event};
    bool skipped = true;
    CHECK(ab_bird_collision_notify(&bird_input, &bird_host, &skipped, &error) &&
          !skipped && fixture.count == 24);
    for (size_t event = 0; event < 24; ++event) {
        CHECK(fixture.events[event][4] == 5 &&
              fixture.events[event][5] == (event < 12 ? 101u : 102u) &&
              fixture.events[event][7] == (event < 12 ? 102u : 101u) &&
              fixture.events[event][14] == 3 &&
              fixture.events[event][15] == UINT32_C(0x80000000) &&
              fixture.events[event][16] == 0 && fixture.events[event][18] == 0);
    }

    CHECK(prepare(&fixture, 7, &error));
    fixture.input[5] = 0;
    /* Use four full groups for both object-method dispatches. */
    for (size_t group = 0; group < 4; ++group) {
        CHECK(ab_string_map_set(map, span(behavior_group_names[group]),
                                (uintptr_t)(fixture.groups + group), true, &error));
        for (size_t slot = 0; slot < 3; ++slot) {
            fixture.slots[group][slot] = fixture.behaviors + group * 4 + slot;
        }
    }
    for (size_t index = 0; index < 20; ++index) {
        binding(&fixture, index, 2);
    }
    AbBlockCollisionScript block_script = {map, ab_game_object_block_collision};
    fixture.objects[0] =
        (AbBlockCollisionObject){.name = span("first"), .script = &block_script};
    fixture.objects[1] =
        (AbBlockCollisionObject){.name = span("second"), .script = &block_script};
    AbBlockCollisionInput block_input = {
        .first = span("first"), .second = span("second"), .normal = {0.0f, -0.0f}};
    AbBlockCollisionHost block_host = {.context = &fixture,
                                       .object = lookup_block,
                                       .definition = empty_definition,
                                       .scripts = empty_script_event};
    CHECK(ab_block_collision_callback(&block_input, &block_host, &error) &&
          fixture.count == 24);
    CHECK(fixture.events[0][4] == 4 && fixture.events[0][5] == 3 &&
          fixture.events[12][4] == 4 && fixture.events[12][5] == 2);
    puts("Production bird/block notifications delegate through actual behavior maps.");
    return true;
}

static bool edges(AbStringMap *map) {
    Fixture fixture = {.map = map};
    AbError error = {0};
    CHECK(prepare(&fixture, 4, &error));
    fixture.input[5] = 0;
    for (size_t index = 0; index < 20; ++index) {
        binding(&fixture, index, 2);
    }
    fixture.fail_method = true;
    CHECK(!invoke(&fixture, &error) && fixture.count == 1 && fixture.mutated == 1);
    CHECK(prepare(&fixture, 4, &error));
    fixture.input[5] = 0;
    for (size_t index = 0; index < 20; ++index) {
        binding(&fixture, index, 2);
        fixture.methods[index].event = span("differentEvent");
    }
    CHECK(invoke(&fixture, &error) && !fixture.count);
    CHECK(ab_string_map_reset(map, 0, &error) &&
          ab_string_map_set(map, span("invalid"), 0, true, &error) &&
          !invoke(&fixture, &error));
    puts(
        "Game behavior failure-prefix, missing-event and invalid-group controls pass.");
    return contact_composition(map);
}

static bool words(FILE *file, const uint32_t *actual, size_t count, uint32_t sample,
                  const char *phase, size_t *matched) {
    for (size_t field = 0; field < count; ++field) {
        uint32_t expected = 0;
        if (fscanf(file, "%" SCNx32, &expected) != 1 || expected != actual[field]) {
            fprintf(stderr,
                    "Game behavior case %u %s word %zu: %08" PRIx32
                    " expected %08" PRIx32 "\n",
                    sample, phase, field, actual[field], expected);
            return false;
        }
        ++*matched;
    }
    return true;
}

static bool reference(AbStringMap *map, FILE *file) {
    char header[96];
    CHECK(fgets(header, sizeof(header), file) &&
          !strcmp(header, "AB_ANDROID_GAME_BEHAVIOR_V1 4096\n"));
    size_t matched = 0, calls = 0, failures = 0, mutated = 0, nil_args = 0;
    size_t actions[9] = {0};
    for (uint32_t sample = 0; sample < 4096; ++sample) {
        Fixture fixture = {.map = map};
        AbError error = {0};
        CHECK(prepare(&fixture, sample, &error) &&
              words(file, fixture.input, 60, sample, "input", &matched));
        bool success = invoke(&fixture, &error);
        uint32_t output[46];
        CHECK(snapshot(&fixture, success, output) &&
              words(file, output, 46, sample, "state", &matched) &&
              words(file, &fixture.count, 1, sample, "count", &matched));
        for (uint32_t event = 0; event < fixture.count; ++event) {
            CHECK(words(file, fixture.events[event], 20, sample, "event", &matched));
            for (size_t argument = 0; argument < fixture.input[7]; ++argument) {
                nil_args += fixture.events[event][4 + argument * 2] == 0;
            }
        }
        calls += fixture.count;
        failures += !success;
        mutated += fixture.mutated != 0;
        actions[fixture.input[5]] += fixture.mutated != 0;
    }
    char footer[96];
    CHECK(fscanf(file, " %95[^\n]", footer) == 1 &&
          !strcmp(footer, "AB game behavior reference complete") &&
          fscanf(file, " %1s", header) == EOF);
    printf("All 4,096 original GameObject delegates match: %zu words, %zu methods, "
           "%zu noncallable failures, %zu first-call actions, %zu nil arguments.\n",
           matched, calls, failures, mutated, nil_args);
    for (size_t action = 0; action < 9; ++action) {
        printf("Behavior action %zu: %zu cases\n", action, actions[action]);
    }
    return true;
}

int main(int argc, char **argv) {
    AbStringMap *map = NULL;
    if (argc > 2 || !ab_string_map_open(0, 8, 65536, &map, NULL)) {
        return 2;
    }
    bool okay;
    if (argc == 1) {
        okay = edges(map);
    } else {
        FILE *file = fopen(argv[1], "r");
        okay = file && reference(map, file);
        if (file) {
            fclose(file);
        }
    }
    ab_string_map_close(map);
    return okay ? 0 : 1;
}
