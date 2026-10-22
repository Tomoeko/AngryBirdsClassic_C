#include "ab/bird_collision_sprites.h"
#include "bird_sprites_cases.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Bird sprites failed at %d: %s\n", __LINE__, #condition);  \
            return false;                                                              \
        }                                                                              \
    } while (0)

typedef struct {
    AbBirdCollisionObject objects[3];
    AbBirdCollisionSpriteDefinition definitions[2];
    AbBirdCollisionSpriteState state;
    AbBirdCollisionSpriteHost host;
    AbDataTable payloads[3];
    uint32_t input[18], events[4][40], count;
    int world_first;
    bool fail_sprite;
} Fixture;

static const char *const strings[] = {"old1",    "old2",    "old3",  "new1",  "new2",
                                      "changed", "GRENADE", "OTHER", "truthy"};

static AbSpan span(const char *text) {
    return (AbSpan){(const uint8_t *)text, strlen(text)};
}

static AbDataValue text(const char *value) {
    return (AbDataValue){.kind = AB_DATA_STRING, .string = span(value)};
}

static bool named(AbSpan name, const char *value) {
    return name.size == strlen(value) && name.data &&
           !memcmp(name.data, value, name.size);
}

static uint32_t name_id(AbSpan name) {
    return named(name, "first")     ? 1u
           : named(name, "second")  ? 2u
           : named(name, "renamed") ? 3u
                                    : 0u;
}

static AbDataValue scalar(Fixture *fixture, uint32_t kind) {
    static AbDataTable empty;
    switch (kind) {
        case 0:
            return (AbDataValue){0};
        case 1:
        case 2:
            return (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = kind == 2};
        case 3:
            return (AbDataValue){.kind = AB_DATA_NUMBER,
                                 .bits =
                                     fixture->input[0] & 1u ? UINT32_C(0x80000000) : 0};
        case 4:
            return text("truthy");
        default:
            return (AbDataValue){.kind = AB_DATA_TABLE, .table = &empty};
    }
}

static AbDataValue specialty(Fixture *fixture, uint32_t kind) {
    return kind == 4   ? text("GRENADE")
           : kind == 5 ? text("OTHER")
                       : scalar(fixture, kind == 6 ? 5u : kind);
}

static AbDataValue sprite_value(Fixture *fixture, uint32_t kind, size_t index) {
    if (kind == 4) {
        return text(index == 0 ? "new1" : "new2");
    }
    if (kind == 5) {
        return (AbDataValue){.kind = AB_DATA_TABLE, .table = fixture->payloads + index};
    }
    return scalar(fixture, kind);
}

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL &&
           (value.kind != AB_DATA_BOOLEAN || value.bits != 0);
}

static void value_words(const Fixture *fixture, AbDataValue value, uint32_t output[2]) {
    static const uint32_t types[] = {0, 1, 3, 4, 5};
    output[0] = types[value.kind];
    output[1] = 0;
    if (value.kind == AB_DATA_BOOLEAN || value.kind == AB_DATA_NUMBER) {
        output[1] = value.bits;
    } else if (value.kind == AB_DATA_STRING) {
        for (uint32_t index = 0; index < 9; ++index) {
            if (named(value.string, strings[index])) {
                output[1] = index + 1u;
                break;
            }
        }
    } else if (value.kind == AB_DATA_TABLE) {
        for (size_t index = 0; index < 3; ++index) {
            if (value.table == fixture->payloads + index) {
                output[1] = (uint32_t)index + 201u;
                break;
            }
        }
    }
}

static void snapshot(const Fixture *fixture, uint32_t output[35]) {
    output[0] = name_id(fixture->objects[0].object.name);
    output[1] = name_id(fixture->objects[1].object.name);
    for (size_t index = 0; index < 2; ++index) {
        value_words(fixture, fixture->objects[index].has_collided,
                    output + 2 + index * 2);
    }
    for (size_t index = 0; index < 3; ++index) {
        value_words(fixture, fixture->objects[index].sprite, output + 6 + index * 2);
    }
    value_words(fixture, fixture->objects[1].object.sensor, output + 12);
    value_words(fixture, fixture->objects[0].dont_update_damage_sprite, output + 14);
    value_words(fixture, fixture->objects[1].dont_update_damage_sprite, output + 16);
    value_words(fixture, fixture->objects[1].frozen_solid, output + 18);
    value_words(fixture, fixture->state.bird_specialty_available, output + 20);
    output[22] = fixture->state.flying_bird
                     ? (uint32_t)(fixture->state.flying_bird - fixture->objects) + 1u
                     : 0u;
    output[23] = fixture->world_first < 0 ? 0u : (uint32_t)fixture->world_first + 1u;
    output[24] = 2;
    value_words(fixture, fixture->definitions[0].sprite_collision, output + 25);
    value_words(fixture, fixture->definitions[1].sprite_collision, output + 27);
    value_words(fixture, fixture->objects[0].object.sensor, output + 29);
    value_words(fixture, fixture->objects[0].shot, output + 31);
    value_words(fixture, fixture->objects[1].shot, output + 33);
}

static bool record(Fixture *fixture, uint32_t kind, uint32_t pc, AbSpan name,
                   AbDataValue value) {
    CHECK(fixture->count < 4);
    uint32_t *event = fixture->events[fixture->count++];
    event[0] = kind;
    event[1] = pc;
    event[2] = name_id(name);
    CHECK(event[2]);
    value_words(fixture, value, event + 3);
    snapshot(fixture, event + 5);
    return true;
}

static bool definition(void *context, AbSpan name,
                       AbBirdCollisionSpriteDefinition **output, AbError *error) {
    (void)error;
    Fixture *fixture = context;
    uint32_t id = name_id(name) == 2 ? 2u : 1u;
    CHECK(record(fixture, id, id == 1 ? 378u : 444u, name, (AbDataValue){0}));
    uint32_t mutation = fixture->input[14];
    if (id == 1) {
        if (mutation & 1u) {
            fixture->objects[1].object.sensor = scalar(fixture, 2);
        }
        if (mutation & 2u) {
            fixture->objects[1].shot = scalar(fixture, 0);
        }
        if (mutation & 4u) {
            fixture->objects[0].dont_update_damage_sprite = scalar(fixture, 1);
        }
        if (mutation & 8u) {
            fixture->world_first = 2;
        }
        if (mutation & 16u) {
            fixture->state.flying_bird = fixture->objects;
        }
        if (mutation & 32u) {
            fixture->state.bird_specialty_available = scalar(fixture, 1);
        }
        if (mutation & 1024u) {
            fixture->world_first = -1;
        }
    }
    *output =
        fixture->input[13] & (1u << (id - 1u)) ? fixture->definitions + id - 1u : NULL;
    return true;
}

static bool object(void *context, AbSpan name, AbBirdCollisionObject **output,
                   AbError *error) {
    (void)error;
    Fixture *fixture = context;
    uint32_t id = name_id(name);
    CHECK(id);
    int index = id == 1 ? fixture->world_first : id == 2 ? 1 : 2;
    *output = index < 0 ? NULL : fixture->objects + index;
    return true;
}

static bool sprite(void *context, AbSpan name, AbDataValue value, AbError *error) {
    Fixture *fixture = context;
    uint32_t id = name_id(name) == 2 ? 2u : 1u;
    bool grenade = fixture->definitions[id - 1u].specialty.kind == AB_DATA_STRING &&
                   named(fixture->definitions[id - 1u].specialty.string, "GRENADE");
    uint32_t pc = id == 1 ? grenade ? 412u : 433u : grenade ? 478u : 499u;
    CHECK(record(fixture, id + 2u, pc, name, value));
    uint32_t mutation = fixture->input[14];
    if (id == 1) {
        if (mutation & 64u) {
            fixture->objects[1].frozen_solid = scalar(fixture, 1);
        }
        if (mutation & 128u) {
            fixture->objects[1].dont_update_damage_sprite = scalar(fixture, 1);
        }
        if (mutation & 256u) {
            fixture->definitions[1].sprite_collision = text("changed");
        }
        if (mutation & 512u) {
            fixture->objects[0].object.name = span("renamed");
        }
        if (mutation & 2048u) {
            fixture->objects[1].object.sensor = scalar(fixture, 2);
        }
    }
    if (fixture->fail_sprite) {
        return ab_fail(error, "Authored bird sprite failure");
    }
    return true;
}

static void prepare(Fixture *fixture, uint32_t sample) {
    memset(fixture, 0, sizeof(*fixture));
    bird_sprites_case(sample, fixture->input);
    for (size_t index = 0; index < 3; ++index) {
        fixture->objects[index].object.name = span(index == 1 ? "second" : "first");
        fixture->objects[index].sprite = text(strings[index]);
    }
    for (size_t index = 0; index < 2; ++index) {
        fixture->objects[index].shot = scalar(fixture, fixture->input[1 + index]);
        fixture->objects[index].dont_update_damage_sprite =
            scalar(fixture, fixture->input[4 + index]);
        fixture->objects[index].has_collided =
            scalar(fixture, fixture->input[16 + index]);
        fixture->definitions[index] = (AbBirdCollisionSpriteDefinition){
            .specialty = specialty(fixture, fixture->input[9 + index]),
            .sprite_collision =
                sprite_value(fixture, fixture->input[11 + index], index)};
    }
    fixture->objects[0].object.sensor = scalar(fixture, fixture->input[15]);
    fixture->objects[1].object.sensor = scalar(fixture, fixture->input[3]);
    fixture->objects[0].frozen_solid = scalar(fixture, (sample / 11u) % 6u);
    fixture->objects[1].frozen_solid = scalar(fixture, fixture->input[6]);
    fixture->state = (AbBirdCollisionSpriteState){
        scalar(fixture, fixture->input[7]),
        fixture->input[8] ? fixture->objects + fixture->input[8] - 1u : NULL};
    fixture->host = (AbBirdCollisionSpriteHost){fixture, definition, object, sprite};
}

static bool invoke(Fixture *fixture, AbError *error) {
    return ab_bird_collision_sprites(fixture->objects, fixture->objects + 1,
                                     &fixture->state, &fixture->host, error);
}

static uint32_t cutoff(const Fixture *fixture, bool success) {
    if (!success) {
        return 0;
    }
    AbBirdCollisionObject *flying = fixture->state.flying_bird;
    return !truthy(fixture->objects[1].object.sensor) && flying &&
                   (named(flying->object.name, "first") ||
                    named(flying->object.name, "second"))
               ? 522u
               : 789u;
}

static bool edges(void) {
    Fixture fixture;
    AbError error = {0};
    prepare(&fixture, 2);
    fixture.objects[1].object.sensor = scalar(&fixture, 3);
    AbDataValue before = fixture.objects[0].has_collided;
    CHECK(ab_bird_collision_sprites(fixture.objects, fixture.objects + 1, NULL, NULL,
                                    &error) &&
          fixture.count == 0 && fixture.objects[0].has_collided.kind == before.kind &&
          fixture.objects[0].has_collided.bits == before.bits);
    prepare(&fixture, 2);
    fixture.host.definition = NULL;
    CHECK(!invoke(&fixture, &error) && fixture.objects[0].has_collided.bits == 1 &&
          !fixture.count);
    prepare(&fixture, 2);
    fixture.input[14] = 0;
    fixture.objects[0].dont_update_damage_sprite = scalar(&fixture, 1);
    fixture.objects[1].frozen_solid = scalar(&fixture, 1);
    fixture.definitions[0].specialty = text("OTHER");
    fixture.host.sprite = NULL;
    CHECK(!invoke(&fixture, &error) && fixture.count == 1 &&
          fixture.objects[0].sprite.kind == AB_DATA_STRING &&
          named(fixture.objects[0].sprite.string, "new1"));
    prepare(&fixture, 2);
    fixture.input[14] = 2048u;
    fixture.objects[0].dont_update_damage_sprite = scalar(&fixture, 1);
    fixture.objects[1].frozen_solid = scalar(&fixture, 1);
    fixture.definitions[0].specialty = text("OTHER");
    fixture.fail_sprite = true;
    CHECK(!invoke(&fixture, &error) && fixture.count == 2 &&
          truthy(fixture.objects[1].object.sensor));
    puts("Bird sprite gates, prior flag/write state and callback failures pass.");
    return true;
}

static bool words(FILE *file, const uint32_t *actual, size_t count, uint32_t sample,
                  const char *phase, size_t *matched) {
    for (size_t field = 0; field < count; ++field) {
        uint32_t expected = 0;
        if (fscanf(file, "%" SCNx32, &expected) != 1 || expected != actual[field]) {
            fprintf(stderr,
                    "Bird sprites case %u %s word %zu: %08" PRIx32
                    " expected %08" PRIx32 "\n",
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
          !strcmp(line, "AB_ANDROID_BIRD_SPRITES_V1 4096\n"));
    size_t matched = 0, failures = 0, counts[4] = {0}, cutoffs[2] = {0};
    for (uint32_t sample = 0; sample < 4096; ++sample) {
        Fixture fixture;
        prepare(&fixture, sample);
        CHECK(words(file, fixture.input, 18, sample, "input", &matched));
        AbError error = {0};
        bool success = invoke(&fixture, &error);
        uint32_t result[] = {!success, cutoff(&fixture, success)};
        CHECK(words(file, result, 2, sample, "result", &matched));
        uint32_t state[35];
        snapshot(&fixture, state);
        CHECK(words(file, state, 35, sample, "state", &matched) &&
              words(file, &fixture.count, 1, sample, "count", &matched));
        for (uint32_t event = 0; event < fixture.count; ++event) {
            CHECK(words(file, fixture.events[event], 40, sample, "event", &matched));
            ++counts[fixture.events[event][0] - 1u];
        }
        failures += !success;
        if (success) {
            ++cutoffs[result[1] == 522 ? 0 : 1];
        }
    }
    CHECK(fscanf(file, " %95[^\n]", line) == 1 &&
          !strcmp(line, "AB bird sprites reference complete") &&
          fscanf(file, " %1s", line) == EOF);
    printf("All 4,096 original bird sprite phases match: %zu words, %zu first/%zu "
           "second definitions, %zu first/%zu second sprites, %zu phase failures, "
           "%zu definition/%zu length suffix cutoffs.\n",
           matched, counts[0], counts[1], counts[2], counts[3], failures, cutoffs[0],
           cutoffs[1]);
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
