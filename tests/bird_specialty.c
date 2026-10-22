#include "ab/bird_collision_specialty.h"
#include "bird_specialty_cases.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Bird specialty failed at %d: %s\n", __LINE__,             \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

typedef struct {
    AbBirdCollisionObject objects[4];
    AbBirdCollisionSpriteDefinition definition;
    AbBirdCollisionSpriteState state;
    AbBirdCollisionSpecialtyHost host;
    AbBirdCollisionHat hat;
    AbBlockCollisionInput contact;
    AbDataTable tables[3];
    AbDataValue cached_specialty;
    uint32_t input[25], bindings[5], events[6][80], count, prints, print_variant;
    int world[2];
    bool fail_call, mutate_contact;
} Fixture;

static const char *const strings[] = {
    "old1",    "old2", "old3",    "normal",   "special",  "changed", "OTHER",
    "GRENADE", "BOMB", "BUBBLES", "handlerA", "handlerB", "truthy"};
static const char *const names[] = {"first", "second", "third", "hat"};

static AbSpan span(const char *value) {
    return (AbSpan){(const uint8_t *)value, strlen(value)};
}

static AbDataValue text(const char *value) {
    return (AbDataValue){.kind = AB_DATA_STRING, .string = span(value)};
}

static bool named(AbSpan value, const char *wanted) {
    return value.size == strlen(wanted) && value.data &&
           !memcmp(value.data, wanted, value.size);
}

static uint32_t name_id(AbSpan value) {
    for (uint32_t id = 0; id < 4; ++id) {
        if (named(value, names[id])) {
            return id + 1u;
        }
    }
    return 0;
}

static uint32_t bits(float value) {
    uint32_t output;
    memcpy(&output, &value, sizeof(output));
    return output;
}

static float number(uint32_t value) {
    float output;
    memcpy(&output, &value, sizeof(output));
    return output;
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
            return (AbDataValue){.kind = AB_DATA_NUMBER, .bits = fixture->input[24]};
        case 4:
            return text("truthy");
        default:
            return table(fixture, 2);
    }
}

static AbDataValue specialty(Fixture *fixture, uint32_t kind) {
    static const char *const values[] = {"OTHER", "GRENADE", "BOMB", "BUBBLES"};
    return kind >= 4 && kind < 8 ? text(values[kind - 4u])
                                 : scalar(fixture, kind == 8 ? 5u : kind);
}

static AbDataValue handler_key(Fixture *fixture, uint32_t kind) {
    if (kind == 3) {
        return (AbDataValue){.kind = AB_DATA_NUMBER};
    }
    if (kind == 4 || kind == 6) {
        return text(kind == 4 ? "handlerA" : "handlerB");
    }
    return kind == 5 ? table(fixture, 0) : scalar(fixture, kind);
}

static AbDataValue sprite_value(Fixture *fixture, uint32_t kind, bool special) {
    return kind == 4 ? text(special ? "special" : "normal") : scalar(fixture, kind);
}

static size_t key_index(Fixture *fixture, AbDataValue key) {
    if (key.kind == AB_DATA_BOOLEAN && key.bits) {
        return 0;
    }
    if (key.kind == AB_DATA_NUMBER && !(key.bits & UINT32_C(0x7fffffff))) {
        return 1;
    }
    if (key.kind == AB_DATA_TABLE && key.table == fixture->tables) {
        return 2;
    }
    if (key.kind == AB_DATA_STRING) {
        return named(key.string, "handlerA")   ? 3u
               : named(key.string, "handlerB") ? 4u
                                               : SIZE_MAX;
    }
    return SIZE_MAX;
}

static AbUiPointerValue binding_value(Fixture *fixture, uint32_t kind) {
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
                                      .object = (uintptr_t)(fixture->tables + 1)};
        default:
            return (AbUiPointerValue){.kind = AB_UI_POINTER_FUNCTION,
                                      .function = kind - 4u};
    }
}

static void value_words(const Fixture *fixture, AbDataValue value, uint32_t output[2]) {
    static const uint32_t types[] = {0, 1, 3, 4, 5};
    output[0] = types[value.kind];
    output[1] = 0;
    if (value.kind == AB_DATA_BOOLEAN || value.kind == AB_DATA_NUMBER) {
        output[1] = value.bits;
    } else if (value.kind == AB_DATA_STRING) {
        for (uint32_t id = 0; id < 13; ++id) {
            if (named(value.string, strings[id])) {
                output[1] = id + 1u;
                break;
            }
        }
    } else if (value.kind == AB_DATA_TABLE) {
        for (size_t id = 0; id < 3; ++id) {
            if (value.table == fixture->tables + id) {
                output[1] = (uint32_t)id + 201u;
                break;
            }
        }
    }
}

static void pointer_words(Fixture *fixture, AbUiPointerValue value,
                          uint32_t output[2]) {
    AbDataValue data = {0};
    switch (value.kind) {
        case AB_UI_POINTER_NONE:
            break;
        case AB_UI_POINTER_BOOLEAN:
            data = (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = value.boolean};
            break;
        case AB_UI_POINTER_NUMBER:
            data = (AbDataValue){.kind = AB_DATA_NUMBER, .bits = value.number_bits};
            break;
        case AB_UI_POINTER_TEXT:
            data = (AbDataValue){.kind = AB_DATA_STRING, .string = value.text};
            break;
        case AB_UI_POINTER_OBJECT:
            data = (AbDataValue){.kind = AB_DATA_TABLE,
                                 .table = (AbDataTable *)value.object};
            break;
        case AB_UI_POINTER_FUNCTION:
            output[0] = 6;
            output[1] = (uint32_t)value.function;
            return;
    }
    value_words(fixture, data, output);
}

static uint32_t object_id(const Fixture *fixture, const AbBirdCollisionObject *object) {
    return object ? (uint32_t)(object - fixture->objects) + 1u : 0;
}

static void snapshot(const Fixture *fixture, uint32_t output[67]) {
    output[0] = object_id(fixture, fixture->state.flying_bird);
    value_words(fixture, fixture->state.bird_specialty_available, output + 1);
    output[3] = fixture->world[0] < 0 ? 0u : (uint32_t)fixture->world[0] + 1u;
    output[4] = fixture->world[1] < 0 ? 0u : (uint32_t)fixture->world[1] + 1u;
    output[5] = fixture->input[17] == 1;
    output[6] = fixture->input[17] != 0;
    value_words(fixture, fixture->definition.specialty, output + 7);
    value_words(fixture, fixture->definition.on_flying_bird_collided, output + 9);
    value_words(fixture, fixture->definition.specialty_available_after_collision,
                output + 11);
    value_words(fixture, fixture->definition.sprite_collision, output + 13);
    value_words(fixture, fixture->definition.explosion_timer, output + 15);
    value_words(fixture, fixture->definition.sprite_special1, output + 17);
    memcpy(output + 19, fixture->bindings, sizeof(fixture->bindings));
    output[24] = fixture->print_variant;
    for (size_t id = 0; id < 3; ++id) {
        const AbBirdCollisionObject *object = fixture->objects + id;
        AbDataValue fields[] = {
            object->collision,  object->sprite,     object->object.damage_sprite,
            object->bomb_timer, object->pink_timer, object->dont_update_damage_sprite,
            object->in_bubble};
        for (size_t field = 0; field < 7; ++field) {
            value_words(fixture, fields[field], output + 25 + id * 14 + field * 2);
        }
    }
}

static uint32_t *record(Fixture *fixture, uint32_t kind, uint32_t pc, uint32_t variant,
                        uint32_t arguments) {
    if (fixture->count >= 6) {
        return NULL;
    }
    uint32_t *event = fixture->events[fixture->count++];
    event[0] = kind;
    event[1] = pc;
    event[2] = variant;
    event[12] = arguments;
    snapshot(fixture, event + 13);
    return event;
}

static bool definition(void *context, AbSpan name,
                       AbBirdCollisionSpriteDefinition **output, AbError *error) {
    (void)error;
    Fixture *fixture = context;
    uint32_t *event = record(fixture, 1, 522, 1, 1);
    CHECK(event);
    event[3] = 4;
    event[4] = name_id(name);
    CHECK(event[4]);
    if (fixture->input[19] & 16384u) {
        fixture->state.flying_bird = NULL;
    }
    if (fixture->mutate_contact) {
        fixture->contact.first = span("third");
        fixture->contact.force = 17.0f;
        fixture->contact.normal = (AbVec2){19.0f, 23.0f};
    }
    fixture->cached_specialty = fixture->definition.specialty;
    *output = fixture->input[18] ? &fixture->definition : NULL;
    return true;
}

static bool object(void *context, AbSpan name, AbBirdCollisionObject **output,
                   AbError *error) {
    (void)error;
    Fixture *fixture = context;
    uint32_t id = name_id(name);
    CHECK(id);
    int index = id < 3                    ? fixture->world[id - 1u]
                : id == 3                 ? 2
                : fixture->input[17] == 1 ? 3
                                          : -1;
    *output = index < 0 ? NULL : fixture->objects + index;
    return true;
}

static bool binding(void *context, AbDataValue key, AbUiPointerValue *output,
                    AbError *error) {
    (void)error;
    Fixture *fixture = context;
    size_t index = key_index(fixture, key);
    *output = binding_value(fixture, index == SIZE_MAX ? 0u : fixture->bindings[index]);
    return true;
}

static bool print_first(void *context, AbUiPointerValue value, AbError *error);
static bool print_second(void *context, AbUiPointerValue value, AbError *error);

static bool print_value(Fixture *fixture, uint32_t variant, AbUiPointerValue value) {
    CHECK(fixture->prints < 2);
    uint32_t *event = record(fixture, fixture->prints + 2u,
                             fixture->prints ? 538u : 534u, variant, 1);
    CHECK(event);
    pointer_words(fixture, value, event + 3);
    uint32_t mutation = fixture->input[19];
    if (fixture->prints++ == 0) {
        if (mutation & 2u) {
            fixture->definition.on_flying_bird_collided = text("handlerB");
        }
        if (mutation & 8u) {
            fixture->host.print = print_second;
            fixture->print_variant = 2;
        }
    } else {
        if (mutation & 4u) {
            size_t index =
                key_index(fixture, fixture->definition.on_flying_bird_collided);
            CHECK(index != SIZE_MAX);
            fixture->bindings[index] = 6;
        }
        if (mutation & 16u) {
            fixture->state.flying_bird = NULL;
        }
    }
    return true;
}

static bool print_first(void *context, AbUiPointerValue value, AbError *error) {
    (void)error;
    return print_value(context, 1, value);
}

static bool print_second(void *context, AbUiPointerValue value, AbError *error) {
    (void)error;
    return print_value(context, 2, value);
}

static bool call(void *context, AbUiPointerValue function,
                 AbBirdCollisionObject *flying_argument,
                 const AbBirdCollisionNamedInput *contact, AbError *error) {
    Fixture *fixture = context;
    CHECK(function.kind == AB_UI_POINTER_FUNCTION && function.function >= 1 &&
          function.function <= 2);
    uint32_t *event = record(fixture, 4, 549, (uint32_t)function.function, 7);
    CHECK(event);
    event[3] = flying_argument ? 5u : 0u;
    event[4] = object_id(fixture, flying_argument);
    event[5] = name_id(contact->first);
    event[6] = name_id(contact->second);
    event[7] = bits(contact->force);
    event[8] = bits(contact->damage);
    event[9] = bits(contact->normal.x);
    event[10] = bits(contact->normal.y);
    event[11] = (uint32_t)function.function;
    uint32_t mutation = fixture->input[19];
    if (mutation & 32u) {
        fixture->state.flying_bird = fixture->objects + 1;
    }
    if (mutation & 1u) {
        fixture->definition.specialty = text("BOMB");
    }
    if (mutation & 64u) {
        fixture->definition.specialty_available_after_collision = scalar(fixture, 1);
    }
    if (mutation & 128u) {
        fixture->definition.sprite_collision = text("changed");
    }
    if (mutation & 256u) {
        fixture->definition.explosion_timer = scalar(fixture, 1);
        fixture->definition.sprite_special1 = text("changed");
    }
    AbBirdCollisionObject *current = fixture->state.flying_bird;
    if (current) {
        uint32_t id = name_id(current->object.name);
        if (id >= 1 && id <= 2) {
            if (mutation & 512u) {
                fixture->world[id - 1u] = 2;
            }
            if (mutation & 1024u) {
                fixture->world[id - 1u] = -1;
            }
        }
    }
    if (fixture->fail_call) {
        return ab_fail(error, "Authored specialty handler failure");
    }
    return true;
}

static bool sprite(void *context, AbSpan name, AbDataValue value, AbError *error) {
    (void)error;
    Fixture *fixture = context;
    bool bomb = fixture->cached_specialty.kind == AB_DATA_STRING &&
                named(fixture->cached_specialty.string, "BOMB");
    bool grenade = fixture->cached_specialty.kind == AB_DATA_STRING &&
                   named(fixture->cached_specialty.string, "GRENADE");
    uint32_t *event = record(fixture, 5, bomb ? 678u : grenade ? 595u : 619u, 1, 2);
    CHECK(event);
    event[3] = 4;
    event[4] = name_id(name);
    value_words(fixture, value, event + 5);
    if (fixture->input[19] & 2048u) {
        fixture->state.bird_specialty_available = scalar(fixture, 1);
    }
    if (fixture->input[19] & 4096u) {
        fixture->state.flying_bird = NULL;
    }
    return true;
}

static bool hat(void *context, AbSpan name, AbBirdCollisionHat **output,
                AbError *error) {
    (void)error;
    Fixture *fixture = context;
    uint32_t id = name_id(name);
    CHECK(id);
    *output = fixture->input[17] && id <= 2 ? &fixture->hat : NULL;
    return true;
}

static bool release_hat(void *context, AbSpan name, AbError *error) {
    (void)error;
    Fixture *fixture = context;
    uint32_t *event = record(fixture, 6, 736, 1, 1);
    CHECK(event);
    event[3] = 4;
    event[4] = name_id(name);
    if (fixture->input[19] & 8192u) {
        fixture->state.flying_bird->collision = scalar(fixture, 1);
        fixture->state.bird_specialty_available = scalar(fixture, 1);
    }
    return true;
}

static void prepare(Fixture *fixture, uint32_t sample) {
    memset(fixture, 0, sizeof(*fixture));
    bird_specialty_case(sample, fixture->input);
    for (size_t id = 0; id < 4; ++id) {
        fixture->objects[id].object.name = span(names[id]);
        if (id < 3) {
            fixture->objects[id].sprite = text(strings[id]);
            fixture->objects[id].object.damage_sprite = text(strings[id]);
        }
    }
    fixture->objects[0].object.sensor = scalar(fixture, fixture->input[3]);
    fixture->objects[1].object.sensor = scalar(fixture, fixture->input[2]);
    fixture->state = (AbBirdCollisionSpriteState){
        scalar(fixture, fixture->input[16]),
        fixture->input[1] ? fixture->objects + fixture->input[1] - 1u : NULL};
    if (fixture->state.flying_bird) {
        AbBirdCollisionObject *object = fixture->state.flying_bird;
        object->collision = scalar(fixture, fixture->input[4]);
        object->dont_update_damage_sprite = scalar(fixture, fixture->input[11]);
        object->in_bubble = scalar(fixture, fixture->input[14]);
        object->pink_timer = scalar(fixture, fixture->input[15]);
    }
    fixture->definition = (AbBirdCollisionSpriteDefinition){
        .specialty = specialty(fixture, fixture->input[5]),
        .on_flying_bird_collided = handler_key(fixture, fixture->input[6]),
        .specialty_available_after_collision = scalar(fixture, fixture->input[9]),
        .sprite_collision = sprite_value(fixture, fixture->input[10], false),
        .explosion_timer = scalar(fixture, fixture->input[12]),
        .sprite_special1 = sprite_value(fixture, fixture->input[13], true)};
    for (size_t index = 0; index < 5; ++index) {
        fixture->bindings[index] = fixture->input[index == 4 ? 8 : 7];
    }
    fixture->world[0] = 0;
    fixture->world[1] = 1;
    fixture->hat.object_name = span("hat");
    fixture->print_variant = 1;
    fixture->contact = (AbBlockCollisionInput){
        .first = span("first"),
        .second = span("second"),
        .force = number(fixture->input[20]),
        .damage = number(fixture->input[21]),
        .normal = {number(fixture->input[22]), number(fixture->input[23])}};
    fixture->host = (AbBirdCollisionSpecialtyHost){.context = fixture,
                                                   .definition = definition,
                                                   .object = object,
                                                   .binding = binding,
                                                   .print = print_first,
                                                   .call = call,
                                                   .sprite = sprite,
                                                   .hat = hat,
                                                   .release_hat = release_hat};
}

static bool invoke(Fixture *fixture, AbError *error) {
    return ab_bird_collision_specialty(&fixture->contact, fixture->objects + 1,
                                       &fixture->state, &fixture->host, error);
}

static bool edges(void) {
    Fixture fixture;
    AbError error = {0};
    prepare(&fixture, 1);
    fixture.objects[1].object.sensor = scalar(&fixture, 3);
    CHECK(ab_bird_collision_specialty(&fixture.contact, fixture.objects + 1, NULL, NULL,
                                      &error) &&
          !fixture.count);
    prepare(&fixture, 1);
    fixture.definition.specialty = text("BOMB");
    fixture.objects[0].collision = scalar(&fixture, 1);
    fixture.definition.explosion_timer = scalar(&fixture, 1);
    fixture.host.sprite = NULL;
    CHECK(!invoke(&fixture, &error) && fixture.count == 1 &&
          fixture.objects[0].bomb_timer.bits == UINT32_C(0x3fc00000) &&
          fixture.objects[0].object.damage_sprite.kind == AB_DATA_STRING &&
          fixture.objects[0].collision.kind == AB_DATA_BOOLEAN &&
          fixture.objects[0].collision.bits == 0);
    prepare(&fixture, 1);
    fixture.input[17] = 1;
    fixture.host.release_hat = NULL;
    CHECK(!invoke(&fixture, &error) &&
          fixture.objects[0].collision.kind == AB_DATA_BOOLEAN &&
          fixture.objects[0].collision.bits == 1);
    prepare(&fixture, 1025);
    fixture.input[19] = 32;
    fixture.fail_call = true;
    CHECK(!invoke(&fixture, &error) && fixture.count == 4 &&
          fixture.state.flying_bird == fixture.objects + 1);
    prepare(&fixture, 1025);
    fixture.input[19] = 0;
    fixture.mutate_contact = true;
    CHECK(invoke(&fixture, &error) && fixture.events[3][0] == 4 &&
          fixture.events[3][5] == 1 && fixture.events[3][7] == fixture.input[20] &&
          fixture.events[3][9] == fixture.input[22] &&
          fixture.events[3][10] == fixture.input[23]);
    puts(
        "Bird specialty inactive, delayed-host and prior-state failure controls pass.");
    return true;
}

static bool words(FILE *file, const uint32_t *actual, size_t count, uint32_t sample,
                  const char *phase, size_t *matched) {
    for (size_t field = 0; field < count; ++field) {
        uint32_t expected = 0;
        if (fscanf(file, "%" SCNx32, &expected) != 1 || expected != actual[field]) {
            fprintf(stderr,
                    "Specialty case %u %s word %zu: %08" PRIx32 " expected %08" PRIx32
                    "\n",
                    sample, phase, field, actual[field], expected);
            return false;
        }
        ++*matched;
    }
    return true;
}

static bool reference(FILE *file) {
    char line[96];
    CHECK(fgets(line, sizeof(line), file) &&
          !strcmp(line, "AB_ANDROID_BIRD_SPECIALTY_V1 8192\n"));
    size_t matched = 0, failures = 0, counts[7] = {0};
    for (uint32_t sample = 0; sample < 8192; ++sample) {
        Fixture fixture;
        prepare(&fixture, sample);
        CHECK(words(file, fixture.input, 25, sample, "input", &matched));
        AbError error = {0};
        bool success = invoke(&fixture, &error);
        uint32_t result = !success;
        CHECK(words(file, &result, 1, sample, "result", &matched));
        uint32_t state[67];
        snapshot(&fixture, state);
        CHECK(words(file, state, 67, sample, "state", &matched) &&
              words(file, &fixture.count, 1, sample, "count", &matched));
        for (uint32_t event = 0; event < fixture.count; ++event) {
            CHECK(words(file, fixture.events[event], 80, sample, "event", &matched));
            ++counts[fixture.events[event][0]];
        }
        failures += !success;
    }
    CHECK(fscanf(file, " %95[^\n]", line) == 1 &&
          !strcmp(line, "AB bird specialty reference complete") &&
          fscanf(file, " %1s", line) == EOF);
    printf("All 8,192 original bird specialty phases match: %zu words, %zu failures.\n",
           matched, failures);
    for (size_t kind = 1; kind < 7; ++kind) {
        printf("Specialty boundary %zu: %zu calls\n", kind, counts[kind]);
    }
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
