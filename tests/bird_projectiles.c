#include "ab/bird_collision_projectiles.h"
#include "bird_projectiles_cases.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Bird projectiles failed at %d: %s\n", __LINE__,           \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

typedef struct {
    AbBirdCollisionObject objects[10];
    AbBirdCollisionObject *items[9];
    AbBirdCollisionSpriteDefinition definitions[2];
    AbBirdCollisionGrenades grenades;
    AbBirdCollisionProjectileHost host;
    AbDataTable table;
    uint32_t input[25], events[2][148], count;
    bool fail_host, mutate_input;
    AbBirdCollisionProjectileInput contact;
} Fixture;

static const char *const names[] = {"first", "second", "other"};
static const char *const values[] = {"truthy", "BOOMERANG", "OTHER"};

static AbSpan span(const char *value) {
    return (AbSpan){(const uint8_t *)value, strlen(value)};
}

static bool named(AbSpan value, const char *wanted) {
    return value.data && value.size == strlen(wanted) &&
           !memcmp(value.data, wanted, value.size);
}

static uint32_t name_id(AbSpan value) {
    for (uint32_t id = 0; id < 3; ++id) {
        if (named(value, names[id])) {
            return id + 1u;
        }
    }
    return 0;
}

static AbDataValue text(const char *value) {
    return (AbDataValue){.kind = AB_DATA_STRING, .string = span(value)};
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
            return (AbDataValue){.kind = AB_DATA_TABLE, .table = &fixture->table};
    }
}

static AbDataValue specialty(Fixture *fixture, uint32_t kind) {
    return kind == 4 || kind == 5 ? text(kind == 4 ? "BOOMERANG" : "OTHER")
                                  : scalar(fixture, kind == 6 ? 5u : kind);
}

static void value_words(AbDataValue value, uint32_t output[2]) {
    static const uint32_t types[] = {0, 1, 3, 4, 5};
    output[0] = types[value.kind];
    output[1] = 0;
    if (value.kind == AB_DATA_BOOLEAN || value.kind == AB_DATA_NUMBER) {
        output[1] = value.bits;
    } else if (value.kind == AB_DATA_STRING) {
        for (uint32_t id = 0; id < 3; ++id) {
            if (named(value.string, values[id])) {
                output[1] = id + 1u;
                break;
            }
        }
    } else if (value.kind == AB_DATA_TABLE) {
        output[1] = 201;
    }
}

static void snapshot(Fixture *fixture, uint32_t output[145]) {
    for (size_t id = 0; id < 10; ++id) {
        AbBirdCollisionObject *object = fixture->objects + id;
        size_t start = id * 13u;
        output[start] = name_id(object->object.name);
        const AbDataValue fields[] = {
            object->boomerang_active, object->collision, object->explode,
            object->controllable,     object->shot,      object->object.sensor};
        for (size_t field = 0; field < 6; ++field) {
            value_words(fields[field], output + start + 1u + field * 2u);
        }
    }
    value_words(fixture->definitions[0].specialty, output + 130);
    value_words(fixture->definitions[1].specialty, output + 132);
    output[134] = fixture->grenades.present;
    output[135] = fixture->grenades.present ? (uint32_t)fixture->grenades.count : 0;
    for (size_t slot = 0; slot < 9; ++slot) {
        output[136 + slot] =
            fixture->grenades.present && slot < fixture->grenades.count
                ? (uint32_t)(fixture->items[slot] - fixture->objects) + 1u
                : 0;
    }
}

static bool definition(void *context, AbSpan name,
                       AbBirdCollisionSpriteDefinition **output, AbError *error) {
    Fixture *fixture = context;
    uint32_t id = name_id(name);
    if (id < 1 || id > 2 || fixture->count >= 2) {
        return ab_fail(error, "Unexpected projectile definition boundary");
    }
    uint32_t *event = fixture->events[fixture->count++];
    event[0] = id == 1 ? 747u : 762u;
    event[1] = id;
    event[2] = 1;
    snapshot(fixture, event + 3);
    uint32_t mutation = fixture->input[11];
    if (id == 1) {
        if (mutation & 1u) {
            fixture->objects[1].object.sensor = scalar(fixture, 2);
        }
        if (mutation & 2u) {
            fixture->objects[1].controllable = scalar(fixture, 2);
        }
        if (mutation & 4u) {
            fixture->objects[0].shot = scalar(fixture, 1);
        }
        if (mutation & 8u) {
            fixture->definitions[1].specialty = text("BOOMERANG");
        }
        if ((mutation & 32u) && fixture->grenades.present && fixture->grenades.count) {
            AbBirdCollisionObject *last = fixture->items[fixture->grenades.count - 1u];
            if (last >= fixture->objects + 2) {
                last->object.name = span("first");
            }
        }
        if (mutation & 64u) {
            fixture->grenades.present = false;
        }
    } else {
        if ((mutation & 16u) && fixture->grenades.present &&
            fixture->grenades.count < 9) {
            fixture->items[fixture->grenades.count++] = fixture->objects;
        }
        if (mutation & 128u) {
            fixture->objects[0].controllable = scalar(fixture, 1);
        }
    }
    if (fixture->mutate_input) {
        fixture->contact.first = span("changed");
        fixture->contact.first_object = NULL;
    }
    *output =
        fixture->input[8] & (1u << (id - 1u)) ? fixture->definitions + id - 1u : NULL;
    return fixture->fail_host ? ab_fail(error, "Injected projectile host failure")
                              : true;
}

static void prepare(Fixture *fixture, uint32_t sample) {
    memset(fixture, 0, sizeof(*fixture));
    bird_projectiles_case(sample, fixture->input);
    for (uint32_t id = 0; id < 10; ++id) {
        fixture->objects[id].object.name =
            span(names[bird_projectile_name(sample, id) - 1u]);
        fixture->objects[id].explode = scalar(fixture, (fixture->input[16] + id) % 6u);
    }
    for (size_t id = 0; id < 2; ++id) {
        AbBirdCollisionObject *object = fixture->objects + id;
        object->controllable = scalar(fixture, fixture->input[2 + id]);
        object->shot = scalar(fixture, fixture->input[4 + id]);
        object->boomerang_active = scalar(fixture, fixture->input[12 + id]);
        object->collision = scalar(fixture, fixture->input[14 + id]);
        fixture->definitions[id].specialty = specialty(fixture, fixture->input[6 + id]);
    }
    fixture->objects[1].object.sensor = scalar(fixture, fixture->input[1]);
    for (size_t slot = 0; slot < 8; ++slot) {
        fixture->items[slot] = fixture->objects + fixture->input[17 + slot];
    }
    fixture->grenades = (AbBirdCollisionGrenades){fixture->items, fixture->input[10], 9,
                                                  fixture->input[9] != 0};
    fixture->host = (AbBirdCollisionProjectileHost){fixture, definition};
    fixture->contact = (AbBirdCollisionProjectileInput){
        span("first"), span("second"), fixture->objects, fixture->objects + 1};
}

static bool invoke(Fixture *fixture, AbBirdCollisionProjectileResult *result,
                   AbError *error) {
    return ab_bird_collision_projectiles(&fixture->contact, &fixture->grenades,
                                         &fixture->host, result, error);
}

static bool edges(void) {
    Fixture fixture;
    prepare(&fixture, 0);
    fixture.input[11] = 0;
    fixture.objects[0].controllable = scalar(&fixture, 3);
    fixture.objects[1].controllable = scalar(&fixture, 1);
    fixture.items[0] = fixture.objects + 2;
    fixture.items[1] = fixture.objects + 3;
    fixture.objects[2].object.name = fixture.objects[3].object.name = span("first");
    fixture.objects[2].explode = fixture.objects[3].explode = scalar(&fixture, 1);
    fixture.grenades.count = 2;
    fixture.mutate_input = true;
    AbBirdCollisionProjectileResult result = {0};
    AbError error = {0};
    CHECK(invoke(&fixture, &result, &error) && !result.first_present &&
          !result.return_early);
    CHECK(fixture.objects[3].explode.bits == 1 && fixture.objects[2].explode.bits == 0);
    CHECK(named(fixture.objects[0].object.name, "first"));
    prepare(&fixture, 0);
    fixture.objects[0].controllable = scalar(&fixture, 3);
    fixture.objects[1].controllable = scalar(&fixture, 2);
    fixture.grenades.count = 2;
    fixture.items[0] = fixture.objects;
    fixture.items[1] = fixture.objects + 1;
    CHECK(invoke(&fixture, &result, &error) && result.first_present &&
          result.return_early);
    CHECK(fixture.objects[1].explode.kind == AB_DATA_BOOLEAN &&
          fixture.objects[1].explode.bits);
    CHECK(fixture.objects[0].boomerang_active.kind == AB_DATA_BOOLEAN &&
          !fixture.objects[0].boomerang_active.bits);
    prepare(&fixture, 0);
    fixture.objects[0].controllable = scalar(&fixture, 2);
    fixture.fail_host = true;
    result = (AbBirdCollisionProjectileResult){true, true};
    CHECK(!invoke(&fixture, &result, &error) && result.first_present &&
          result.return_early && fixture.count == 1);
    prepare(&fixture, 0);
    fixture.objects[0].controllable = scalar(&fixture, 2);
    fixture.grenades.present = false;
    CHECK(!invoke(&fixture, &result, &error) && fixture.objects[0].collision.bits == 1);
    prepare(&fixture, 0);
    fixture.grenades.count = 1;
    fixture.items[0] = NULL;
    CHECK(!invoke(&fixture, &result, &error));
    CHECK(!ab_bird_collision_projectiles(NULL, NULL, NULL, &result, &error));
    return true;
}

static bool words(FILE *file, const uint32_t *values_to_check, size_t count,
                  uint32_t sample, const char *label, size_t *matched) {
    for (size_t field = 0; field < count; ++field) {
        uint32_t expected = 0;
        if (fscanf(file, " %" SCNx32, &expected) != 1 ||
            expected != values_to_check[field]) {
            fprintf(stderr, "Projectile sample %u %s[%zu]: C=%08x original=%08x\n",
                    sample, label, field, values_to_check[field], expected);
            return false;
        }
        ++*matched;
    }
    return true;
}

static bool reference(FILE *file) {
    char line[96];
    CHECK(fgets(line, sizeof(line), file) &&
          !strcmp(line, "AB_ANDROID_BIRD_PROJECTILES_V1 4096\n"));
    size_t matched = 0, failures = 0, early = 0, absent = 0, queries = 0;
    for (uint32_t sample = 0; sample < 4096; ++sample) {
        Fixture fixture;
        prepare(&fixture, sample);
        CHECK(words(file, fixture.input, 25, sample, "input", &matched));
        AbBirdCollisionProjectileResult result = {0};
        AbError error = {0};
        bool success = invoke(&fixture, &result, &error);
        uint32_t status[] = {!success, success && result.first_present,
                             success && result.return_early};
        CHECK(words(file, status, 3, sample, "result", &matched));
        uint32_t pc = 0, first_type = 0, second_type = 0;
        CHECK(fscanf(file, " %" SCNx32 " %" SCNx32 " %" SCNx32, &pc, &first_type,
                     &second_type) == 3);
        CHECK(!success ? !pc && !first_type && !second_type
                       : (result.return_early ? pc == 819 || pc == 825 : pc == 973) &&
                             first_type == (result.first_present ? 4u : 0u) &&
                             second_type == 4);
        matched += 3;
        uint32_t state[145];
        snapshot(&fixture, state);
        CHECK(words(file, state, 145, sample, "state", &matched) &&
              words(file, &fixture.count, 1, sample, "count", &matched));
        for (uint32_t event = 0; event < fixture.count; ++event) {
            CHECK(words(file, fixture.events[event], 148, sample, "event", &matched));
        }
        failures += !success;
        early += success && result.return_early;
        absent += success && !result.first_present;
        queries += fixture.count;
    }
    CHECK(fscanf(file, " %95[^\n]", line) == 1 &&
          !strcmp(line, "AB bird projectiles reference complete") &&
          fscanf(file, " %1s", line) == EOF);
    printf("All 4,096 original bird projectile phases match: %zu words, %zu failures, "
           "%zu early returns, %zu absent first names, %zu definitions.\n",
           matched, failures, early, absent, queries);
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
