#include "ab/block_removal_spawn.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Removal spawn failed at %d: %s\n", __LINE__, #condition); \
            return false;                                                              \
        }                                                                              \
    } while (0)

typedef struct {
    AbDataTable body, spawn, death, target, catalog, blocks, types[2];
    AbDataEntry body_fields[6], spawn_fields[2], death_fields[2], target_fields[2];
    AbDataEntry catalog_fields[2], blocks_field, type_fields[2];
    AbBlockRemovalEntry entry;
    AbBlockRemovalDetached detached;
    AbBlockRemovalSpawnHost host;
    uint8_t names[80];
    float x[5], y[5], angles[5];
    uint32_t variants[5];
    size_t creates, rotations, randoms;
    bool mutate, fail_create;
} Fixture;

static AbDataValue text(const char *value) {
    return (AbDataValue){.kind = AB_DATA_STRING,
                         .string = {(const uint8_t *)value, strlen(value)}};
}

static AbDataValue number(float value) {
    uint32_t word;
    memcpy(&word, &value, sizeof(word));
    return (AbDataValue){.kind = AB_DATA_NUMBER, .bits = word};
}

static AbDataValue table(AbDataTable *value) {
    return (AbDataValue){.kind = AB_DATA_TABLE, .table = value};
}

static bool named(AbDataValue value, const char *name) {
    return value.kind == AB_DATA_STRING && value.string.size == strlen(name) &&
           !memcmp(value.string.data, name, value.string.size);
}

static bool rotate_second(void *context, AbDataValue created, float angle,
                          AbError *error);

static bool random_value(void *context, unsigned count, float lower, float upper,
                         AbDataValue *output, AbError *error) {
    Fixture *fixture = context;
    (void)error;
    ++fixture->randoms;
    if (count == 2) {
        CHECK(lower == 1 && upper == 1);
        if (fixture->mutate) {
            fixture->death_fields[1].value = table(fixture->types + 1);
        }
        *output = number(1);
    } else {
        CHECK(count == 0);
        if (fixture->mutate) {
            fixture->host.rotate = rotate_second;
            fixture->host.scale_factor = number(0.5f);
            fixture->host.math_pi = 1;
        }
        *output = number(0.25f);
    }
    return true;
}

static bool create(void *context, AbDataValue blocks, AbDataValue definition,
                   AbSpan name, float x, float y, AbDataValue *output, AbError *error) {
    Fixture *fixture = context;
    CHECK(blocks.kind == AB_DATA_TABLE && blocks.table == &fixture->blocks &&
          fixture->creates < 5);
    size_t index = fixture->creates++;
    const char *expected = index == 0   ? "body_1"
                           : index == 1 ? (fixture->mutate ? "renamed_2" : "body_2")
                           : index == 2 ? (fixture->mutate ? "renamed_3" : "body_3")
                           : index == 3 ? "queued_generated_1"
                                        : "queued_generated_2";
    CHECK(name.size == strlen(expected) && !memcmp(name.data, expected, name.size));
    CHECK(named(definition, fixture->mutate && index == 4 ? "alternate" : "target"));
    fixture->x[index] = x;
    fixture->y[index] = y;
    if (fixture->mutate && index == 0) {
        fixture->body_fields[0].value = text("renamed");
        fixture->body_fields[1].value = number(22);
        fixture->body_fields[3].value = number(0x1.921fb6p+0f);
        fixture->body_fields[4].value = number(30);
        fixture->body_fields[5].value = number(40);
    }
    *output = number((float)index + 100);
    return fixture->fail_create ? ab_fail(error, "Injected createObject failure")
                                : true;
}

static bool rotate(void *context, AbDataValue created, float angle, uint32_t variant,
                   AbError *error) {
    Fixture *fixture = context;
    (void)error;
    CHECK(fixture->rotations < 5 && created.kind == AB_DATA_NUMBER &&
          created.bits == number((float)fixture->rotations + 100).bits);
    size_t index = fixture->rotations++;
    fixture->angles[index] = angle;
    fixture->variants[index] = variant;
    return true;
}

static bool rotate_first(void *context, AbDataValue created, float angle,
                         AbError *error) {
    return rotate(context, created, angle, 1, error);
}

static bool rotate_second(void *context, AbDataValue created, float angle,
                          AbError *error) {
    return rotate(context, created, angle, 2, error);
}

static void prepare(Fixture *fixture) {
    memset(fixture, 0, sizeof(*fixture));
    const char *const fields[] = {"name", "width", "height", "angle", "x", "y"};
    float numbers[] = {0, 5, 4, 0, 10, 20};
    for (size_t field = 0; field < 6; ++field) {
        fixture->body_fields[field] = (AbDataEntry){
            text(fields[field]), field ? number(numbers[field]) : text("body")};
    }
    fixture->body = (AbDataTable){fixture->body_fields, 6, 6, NULL};
    fixture->spawn_fields[0] = (AbDataEntry){text("definition"), text("target")};
    fixture->spawn_fields[1] = (AbDataEntry){text("amount"), number(3)};
    fixture->spawn = (AbDataTable){fixture->spawn_fields, 2, 2, NULL};
    fixture->target_fields[0] = (AbDataEntry){text("width"), number(2)};
    fixture->target_fields[1] = (AbDataEntry){text("height"), {0}};
    fixture->target = (AbDataTable){fixture->target_fields, 2, 2, NULL};
    fixture->catalog_fields[0] = (AbDataEntry){text("target"), table(&fixture->target)};
    fixture->catalog_fields[1] =
        (AbDataEntry){text("alternate"), table(&fixture->target)};
    fixture->catalog = (AbDataTable){fixture->catalog_fields, 2, 2, NULL};
    fixture->blocks_field = (AbDataEntry){text("blocks"), table(&fixture->catalog)};
    fixture->blocks = (AbDataTable){&fixture->blocks_field, 1, 1, NULL};
    for (size_t type = 0; type < 2; ++type) {
        fixture->type_fields[type] =
            (AbDataEntry){number(1), text(type ? "alternate" : "target")};
        fixture->types[type] = (AbDataTable){fixture->type_fields + type, 1, 1, NULL};
    }
    fixture->death_fields[0] = (AbDataEntry){text("amount"), number(2)};
    fixture->death_fields[1] = (AbDataEntry){text("types"), table(fixture->types)};
    fixture->death = (AbDataTable){fixture->death_fields, 2, 2, NULL};
    fixture->entry =
        (AbBlockRemovalEntry){{(const uint8_t *)"queued", 6}, table(&fixture->body)};
    fixture->detached = (AbBlockRemovalDetached){
        table(&fixture->spawn), table(&fixture->death), number(7), number(-3)};
    fixture->host = (AbBlockRemovalSpawnHost){fixture,
                                              table(&fixture->blocks),
                                              number(2),
                                              0x1.921fb6p+1f,
                                              fixture->names,
                                              sizeof(fixture->names),
                                              5,
                                              random_value,
                                              create,
                                              rotate_first};
}

static bool controls(void) {
    Fixture fixture;
    prepare(&fixture);
    CHECK(
        ab_block_removal_spawn(&fixture.entry, &fixture.detached, &fixture.host, NULL));
    CHECK(fixture.creates == 5 && fixture.rotations == 5 && fixture.randoms == 4);
    CHECK(fixture.x[0] == 8.75f && fixture.y[0] == 19 && fixture.x[1] == 11.25f &&
          fixture.y[1] == 19 && fixture.x[2] == 8.75f && fixture.y[2] == 21);
    CHECK(fixture.x[3] == 14 && fixture.y[3] == -6 && fixture.x[4] == 14 &&
          fixture.y[4] == -6);
    prepare(&fixture);
    fixture.mutate = true;
    CHECK(
        ab_block_removal_spawn(&fixture.entry, &fixture.detached, &fixture.host, NULL));
    CHECK(fixture.x[1] == 31 && fixture.y[1] == 41.25f && fixture.x[2] == 29 &&
          fixture.y[2] == 38.75f);
    CHECK(fixture.x[3] == 14 && fixture.y[3] == -6 && fixture.x[4] == 3.5f &&
          fixture.y[4] == -1.5f);
    CHECK(fixture.variants[3] == 1 && fixture.variants[4] == 2 &&
          fixture.angles[3] == 0.5f && fixture.angles[4] == 0.5f);
    prepare(&fixture);
    fixture.host.maximum_instances = 2;
    CHECK(!ab_block_removal_spawn(&fixture.entry, &fixture.detached, &fixture.host,
                                  NULL));
    CHECK(fixture.creates == 2 && fixture.rotations == 2);
    prepare(&fixture);
    fixture.fail_create = true;
    CHECK(!ab_block_removal_spawn(&fixture.entry, &fixture.detached, &fixture.host,
                                  NULL));
    CHECK(fixture.creates == 1 && fixture.rotations == 0);
    prepare(&fixture);
    fixture.detached.spawn = fixture.detached.death_spawn = (AbDataValue){0};
    fixture.host.math_pi = 0;
    CHECK(
        ab_block_removal_spawn(&fixture.entry, &fixture.detached, &fixture.host, NULL));
    return true;
}

int main(void) {
    return controls() ? 0 : 1;
}
