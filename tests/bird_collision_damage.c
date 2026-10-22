#include "ab/bird_collision_damage.h"
#include "ab/damage_particles.h"
#include "bird_collision_damage_cases.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Bird collision damage failed at %d: %s\n", __LINE__,      \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

typedef struct {
    AbBirdCollisionObject object;
    AbBirdCollisionDamageHost host;
    AbDamageSpriteNames names;
    AbDataTable tables[6];
    AbDataEntry catalog, blocks, raw, descriptor[2];
    uint32_t input[20], events[3][40], count;
    bool fail_sprites, fail_particles;
} Fixture;

static const char *const strings[] = {"sprite", "changed", "first", "second",
                                      "invalid"};

static AbDataValue text(const char *value) {
    return (AbDataValue){.kind = AB_DATA_STRING,
                         .string = {(const uint8_t *)value, strlen(value)}};
}

static AbDataValue table(Fixture *fixture, size_t id) {
    return (AbDataValue){.kind = AB_DATA_TABLE, .table = fixture->tables + id};
}

static AbDataValue value(Fixture *fixture, uint32_t kind) {
    switch (kind) {
        case 0:
            return (AbDataValue){0};
        case 1:
        case 2:
            return (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = kind == 2};
        case 3:
        case 4:
        case 5:
            return (AbDataValue){.kind = AB_DATA_NUMBER,
                                 .bits = kind == 3   ? 0u
                                         : kind == 4 ? UINT32_C(0x80000000)
                                                     : UINT32_C(0x7fc01234)};
        case 6:
        case 8:
            return text(kind == 6 ? "sprite" : "changed");
        default:
            return table(fixture, 0);
    }
}

static AbDataValue gate(Fixture *fixture, uint32_t kind) {
    return value(fixture, kind < 4 ? kind : kind == 4 ? 6u : 7u);
}

static uint32_t bits(float number) {
    uint32_t output;
    memcpy(&output, &number, sizeof(output));
    return output;
}

static float number(uint32_t bits_value) {
    float output;
    memcpy(&output, &bits_value, sizeof(output));
    return output;
}

static void value_words(Fixture *fixture, AbDataValue data, uint32_t output[2]) {
    static const uint32_t types[] = {0, 1, 3, 4, 5};
    output[0] = types[data.kind];
    output[1] = 0;
    if (data.kind == AB_DATA_BOOLEAN || data.kind == AB_DATA_NUMBER) {
        output[1] = data.bits;
    } else if (data.kind == AB_DATA_STRING) {
        for (uint32_t id = 0; id < 5; ++id) {
            if (data.string.size == strlen(strings[id]) &&
                !memcmp(data.string.data, strings[id], data.string.size)) {
                output[1] = id + 1u;
                break;
            }
        }
    } else if (data.kind == AB_DATA_TABLE) {
        output[1] = (uint32_t)(data.table - fixture->tables) + 201u;
    }
}

static void snapshot(Fixture *fixture, uint32_t output[26]) {
    const AbDataValue fields[] = {
        fixture->object.object.sensor, fixture->object.controllable,
        fixture->object.object.damage_sprite, fixture->object.object.blink_sprite,
        fixture->object.object.smile_sprite};
    for (size_t field = 0; field < 5; ++field) {
        value_words(fixture, fields[field], output + field * 2u);
    }
    output[10] = bits(fixture->object.position.x);
    output[11] = bits(fixture->object.position.y);
    const AbDataValue later[] = {
        fixture->host.block_table,    fixture->raw.value,
        fixture->descriptor[0].value, fixture->descriptor[1].value,
        fixture->names.sprite,        fixture->names.blink,
        fixture->names.smile};
    for (size_t field = 0; field < 7; ++field) {
        value_words(fixture, later[field], output + 12 + field * 2u);
    }
}

static uint32_t *record(Fixture *fixture, uint32_t kind, uint32_t pc, uint32_t argc) {
    if (fixture->count >= 3) {
        return NULL;
    }
    uint32_t *event = fixture->events[fixture->count++];
    memset(event, 0, 40 * sizeof(*event));
    event[0] = kind;
    event[1] = pc;
    event[2] = argc;
    snapshot(fixture, event + 14);
    return event;
}

static bool sprites(void *context, AbBirdCollisionObject *object, AbDataValue blocks,
                    AbDamageSpriteNames **output, AbError *error) {
    Fixture *fixture = context;
    CHECK(object == &fixture->object);
    uint32_t *event = record(fixture, 1, 959, 2);
    CHECK(event);
    event[3] = 2;
    value_words(fixture, blocks, event + 4);
    uint32_t mutation = fixture->input[12];
    if (mutation & 1u) {
        object->object.damage_sprite = fixture->names.sprite;
    }
    if (mutation & 2u) {
        object->object.sensor = value(fixture, 2);
    }
    if (mutation & 4u) {
        object->controllable = value(fixture, 2);
    }
    if (mutation & 8u) {
        object->position =
            (AbVec2){number(fixture->input[16]), number(fixture->input[17])};
    }
    if (mutation & 16u) {
        fixture->host.block_table = table(fixture, 1);
    }
    if (mutation & 32u) {
        fixture->names.sprite = text("changed");
    }
    if (mutation & 64u) {
        fixture->descriptor[0].value = text("second");
        fixture->descriptor[1].value.bits = fixture->input[19];
    }
    if (mutation & 128u) {
        fixture->host.block_table = (AbDataValue){0};
    }
    *output = fixture->input[5] ? &fixture->names : NULL;
    return fixture->fail_sprites ? ab_fail(error, "Controlled sprite query failure")
                                 : true;
}

static bool transform(void *context, AbVec2 point, AbVec2 *output, AbError *error) {
    Fixture *fixture = context;
    (void)error;
    uint32_t *event = record(fixture, 2, 0, 2);
    CHECK(event);
    event[3] = bits(point.x);
    event[4] = bits(point.y);
    event[5] = fixture->input[16];
    event[6] = fixture->input[17];
    if (fixture->input[12] & 256u) {
        fixture->descriptor[0].value = text("second");
        fixture->descriptor[1].value.bits = fixture->input[19];
    }
    if (fixture->input[12] & 512u) {
        fixture->raw.value = (AbDataValue){0};
    }
    *output = (AbVec2){number(fixture->input[16]), number(fixture->input[17])};
    return true;
}

static bool emit(void *context, const AbAmbientParticleEmission *emission,
                 AbError *error) {
    Fixture *fixture = context;
    (void)error;
    uint32_t *event = record(fixture, 3, 0, 9);
    CHECK(event);
    CHECK(emission->particles.size == 5 || emission->particles.size == 6);
    event[3] = emission->particles.size == 5 ? 3u : 4u;
    event[4] = bits(emission->amount);
    event[5] = bits(emission->position.x);
    event[6] = bits(emission->position.y);
    return true;
}

static const AbDataValue *field(AbDataValue data, const char *key) {
    return data.kind == AB_DATA_TABLE ? ab_data_get(data.table, text(key)) : NULL;
}

static bool particles(void *context, AbBirdCollisionObject *object, AbError *error) {
    Fixture *fixture = context;
    CHECK(object == &fixture->object);
    if (fixture->fail_particles) {
        return ab_fail(error, "Controlled particle host failure");
    }
    const AbDataValue *blocks = field(fixture->host.block_table, "blocks");
    const AbDataValue *raw = blocks ? field(*blocks, "Object") : NULL;
    if (!raw || raw->kind != AB_DATA_TABLE) {
        return ab_fail(error, "Missing raw particle definition");
    }
    return ab_damage_state_particles(raw->table, &object->position,
                                     (AbAmbientParticleHost){fixture, transform, emit},
                                     error);
}

static void prepare(Fixture *fixture, uint32_t sample) {
    memset(fixture, 0, sizeof(*fixture));
    bird_collision_damage_case(sample, fixture->input);
    fixture->object.object.sensor = gate(fixture, fixture->input[1]);
    fixture->object.controllable = gate(fixture, fixture->input[2]);
    fixture->names = (AbDamageSpriteNames){.sprite = value(fixture, fixture->input[6]),
                                           .blink = value(fixture, fixture->input[7]),
                                           .smile = value(fixture, fixture->input[8])};
    fixture->object.object.damage_sprite = value(fixture, fixture->input[9]);
    fixture->object.object.blink_sprite = value(fixture, fixture->input[10]);
    fixture->object.object.smile_sprite = value(fixture, fixture->input[11]);
    fixture->object.position =
        (AbVec2){number(fixture->input[14]), number(fixture->input[15])};
    fixture->descriptor[0] = (AbDataEntry){text("name"), text("first")};
    fixture->descriptor[1] = (AbDataEntry){
        text("amount"), {.kind = AB_DATA_NUMBER, .bits = fixture->input[18]}};
    fixture->tables[5] = (AbDataTable){fixture->descriptor, 2, 2, NULL};
    uint32_t descriptor = fixture->input[13];
    AbDataValue raw = descriptor == 1   ? table(fixture, 5)
                      : descriptor == 2 ? value(fixture, 1)
                      : descriptor == 3 ? value(fixture, 2)
                      : descriptor == 4 ? value(fixture, 3)
                      : descriptor == 5 ? text("invalid")
                                        : (AbDataValue){0};
    fixture->raw = (AbDataEntry){text("damageStateChangedParticles"), raw};
    fixture->tables[4] = (AbDataTable){&fixture->raw, 1, 1, NULL};
    fixture->blocks = (AbDataEntry){text("Object"), table(fixture, 4)};
    fixture->tables[3] = (AbDataTable){&fixture->blocks, 1, 1, NULL};
    uint32_t blocks = fixture->input[4];
    fixture->catalog = (AbDataEntry){
        text("blocks"), blocks == 6 ? table(fixture, 3) : gate(fixture, blocks)};
    fixture->tables[2] = (AbDataTable){&fixture->catalog, 1, 1, NULL};
    uint32_t catalog = fixture->input[3];
    fixture->host = (AbBirdCollisionDamageHost){fixture,
                                                catalog == 6   ? table(fixture, 2)
                                                : catalog == 5 ? table(fixture, 1)
                                                               : gate(fixture, catalog),
                                                sprites, particles};
}

static bool edges(void) {
    Fixture fixture;
    prepare(&fixture, 1);
    fixture.object.object.sensor = value(&fixture, 2);
    CHECK(ab_bird_collision_damage_refresh(&fixture.object, NULL, NULL));
    fixture.object.object.sensor = (AbDataValue){0};
    fixture.object.controllable = value(&fixture, 2);
    CHECK(ab_bird_collision_damage_refresh(&fixture.object, NULL, NULL));
    fixture.object.controllable = value(&fixture, 3);
    CHECK(!ab_bird_collision_damage_refresh(&fixture.object, NULL, NULL));
    fixture.input[12] = 0;
    fixture.fail_particles = true;
    fixture.object.object.damage_sprite = text("old");
    fixture.names =
        (AbDamageSpriteNames){text("sprite"), text("changed"), value(&fixture, 1), {0}};
    CHECK(!ab_bird_collision_damage_refresh(&fixture.object, &fixture.host, NULL));
    CHECK(fixture.object.object.damage_sprite.string.size == 6 &&
          fixture.object.object.blink_sprite.string.size == 7);
    CHECK(fixture.object.object.smile_sprite.kind == AB_DATA_BOOLEAN);
    prepare(&fixture, 1);
    fixture.input[12] = 2;
    fixture.fail_sprites = true;
    AbDataValue before = fixture.object.object.damage_sprite;
    CHECK(!ab_bird_collision_damage_refresh(&fixture.object, &fixture.host, NULL));
    CHECK(fixture.object.object.sensor.bits == 1 &&
          fixture.object.object.damage_sprite.kind == before.kind &&
          fixture.object.object.damage_sprite.bits == before.bits);
    CHECK(!ab_bird_collision_damage_refresh(NULL, NULL, NULL));
    return true;
}

static bool words(FILE *file, const uint32_t *actual, size_t count, uint32_t sample,
                  const char *label, size_t *matched) {
    for (size_t field_index = 0; field_index < count; ++field_index) {
        uint32_t expected = 0;
        if (fscanf(file, " %" SCNx32, &expected) != 1 ||
            actual[field_index] != expected) {
            fprintf(stderr, "Bird damage sample %u %s[%zu]: C=%08x original=%08x\n",
                    sample, label, field_index, actual[field_index], expected);
            return false;
        }
        ++*matched;
    }
    return true;
}

static bool reference(FILE *file) {
    char line[96];
    CHECK(fgets(line, sizeof(line), file) &&
          !strcmp(line, "AB_ANDROID_BIRD_COLLISION_DAMAGE_V1 4096\n"));
    size_t matched = 0, failures = 0, counts[4] = {0};
    for (uint32_t sample = 0; sample < 4096; ++sample) {
        Fixture fixture;
        prepare(&fixture, sample);
        CHECK(words(file, fixture.input, 20, sample, "input", &matched));
        AbError error = {0};
        uint32_t failed =
            !ab_bird_collision_damage_refresh(&fixture.object, &fixture.host, &error);
        CHECK(words(file, &failed, 1, sample, "result", &matched));
        uint32_t state[26];
        snapshot(&fixture, state);
        CHECK(words(file, state, 26, sample, "state", &matched) &&
              words(file, &fixture.count, 1, sample, "count", &matched));
        for (uint32_t event = 0; event < fixture.count; ++event) {
            CHECK(words(file, fixture.events[event], 40, sample, "event", &matched));
            ++counts[fixture.events[event][0]];
        }
        failures += failed;
    }
    CHECK(fscanf(file, " %95[^\n]", line) == 1 &&
          !strcmp(line, "AB bird collision damage reference complete") &&
          fscanf(file, " %1s", line) == EOF);
    printf("All 4,096 original bird damage-refresh phases match: %zu words, %zu "
           "failures, %zu queries, %zu conversions, %zu emissions.\n",
           matched, failures, counts[1], counts[2], counts[3]);
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
