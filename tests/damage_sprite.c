#include "ab/damage_sprite.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Damage sprite failed at %d: %s\n", __LINE__, #condition); \
            return false;                                                              \
        }                                                                              \
    } while (0)

static AbDataValue number(float value) {
    AbDataValue result = {.kind = AB_DATA_NUMBER};
    memcpy(&result.bits, &value, sizeof(value));
    return result;
}

static AbDataValue string(const char *text) {
    return (AbDataValue){.kind = AB_DATA_STRING,
                         .string = {(const uint8_t *)text, strlen(text)}};
}

static AbDataValue value(uint32_t code) {
    const char *names[] = {"base", "rangeA", "rangeB", "rangeC"};
    return code == 0   ? (AbDataValue){0}
           : code == 1 ? (AbDataValue){.kind = AB_DATA_BOOLEAN}
           : code == 2 ? number(0)
                       : string(names[code - 3]);
}

static bool same_value(AbDataValue first, AbDataValue second) {
    return first.kind == second.kind &&
           (first.kind == AB_DATA_STRING
                ? first.string.size == second.string.size &&
                      memcmp(first.string.data, second.string.data,
                             first.string.size) == 0
                : first.bits == second.bits);
}

static bool words(FILE *file, uint32_t *output, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        CHECK(fscanf(file, "%" SCNx32, output + index) == 1);
        CHECK(fgetc(file) == (index + 1 == count ? '\n' : ' '));
    }
    return true;
}

static bool native_checks(FILE *file) {
    char header[80];
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB_ANDROID_DAMAGE_SPRITE_V1 4096\n") == 0);
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t input[24], native_order[3], expected[4];
        CHECK(words(file, input, 24) && words(file, native_order, 3) &&
              words(file, expected, 4) && input[0] <= 6 && input[1] <= 1 &&
              input[3] <= 2);
        AbDataEntry fields[3][6], entries[3];
        AbDataTable intervals[3];
        size_t order[3];
        const char *names[] = {"min",         "max",         "sprite",
                               "blinkSprite", "smileSprite", "frozenSprite"};
        for (size_t index = 0; index < 3; ++index) {
            CHECK(native_order[index] < 3);
            order[index] = native_order[index];
            for (size_t field = 0; field < 6; ++field) {
                uint32_t word = input[6 + index * 6 + field];
                CHECK(field < 2 || word <= 6);
                fields[index][field] = (AbDataEntry){
                    string(names[field]),
                    field < 2 ? (AbDataValue){.kind = AB_DATA_NUMBER, .bits = word}
                              : value(word)};
            }
            intervals[index] = (AbDataTable){fields[index], 6, 6, NULL};
            entries[index] =
                (AbDataEntry){number((float)index),
                              {.kind = AB_DATA_TABLE, .table = intervals + index}};
        }
        AbDataTable ranges = {entries, 3, 3, NULL};
        AbDataValue strength =
            input[1] ? (AbDataValue){.kind = AB_DATA_NUMBER, .bits = input[2]}
                     : (AbDataValue){0};
        AbDataValue initial =
            input[3] == 0   ? (AbDataValue){0}
            : input[3] == 1 ? (AbDataValue){.kind = AB_DATA_BOOLEAN}
                            : (AbDataValue){.kind = AB_DATA_NUMBER, .bits = input[4]};
        AbDamageSpriteNames selected;
        AbError error = {0};
        CHECK(ab_damage_sprite_names(
            value(input[0]), strength, initial,
            (AbDataValue){.kind = AB_DATA_NUMBER, .bits = input[5]}, &ranges, order,
            &selected, &error));
        AbDataValue actual[] = {selected.sprite, selected.blink, selected.smile,
                                selected.frozen};
        for (size_t field = 0; field < 4; ++field) {
            CHECK(expected[field] <= 6);
            if (!same_value(actual[field], value(expected[field]))) {
                fprintf(stderr, "Damage-sprite native case %zu field %zu diverged\n",
                        item, field);
                return false;
            }
        }
    }
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB damage sprite reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    puts(
        "All 4096 native damage-sprite typed selections match in supplied range order");
    return true;
}

static bool edge_checks(void) {
    AbDamageSpriteNames output = {.sprite = number(99)};
    CHECK(!ab_damage_sprite_names(value(3), number(1), (AbDataValue){0}, number(1),
                                  NULL, NULL, NULL, NULL));
    CHECK(ab_damage_sprite_names(value(3), number(NAN), (AbDataValue){0}, number(1),
                                 NULL, NULL, &output, NULL));
    CHECK(same_value(output.sprite, value(3)) && same_value(output.blink, value(3)) &&
          output.frozen.kind == AB_DATA_NIL);
    AbDataEntry bounds[] = {{string("min"), number(0)},
                            {string("max"), number(100)},
                            {string("sprite"), value(4)}};
    AbDataTable interval = {bounds, 3, 3, NULL};
    AbDataEntry entry[] = {
        {string("first"), {.kind = AB_DATA_TABLE, .table = &interval}},
        {string("second"), {.kind = AB_DATA_TABLE, .table = &interval}}};
    AbDataTable ranges = {entry, 2, 2, NULL};
    size_t order[] = {0, 0};
    output.sprite = number(99);
    CHECK(!ab_damage_sprite_names(value(3), number(1), (AbDataValue){0}, number(1),
                                  &ranges, order, &output, NULL));
    CHECK(same_value(output.sprite, number(99)));
    order[1] = 2;
    CHECK(!ab_damage_sprite_names(value(3), number(1), (AbDataValue){0}, number(1),
                                  &ranges, order, &output, NULL));
    order[1] = 1;
    CHECK(ab_damage_sprite_names(value(3), number(0), (AbDataValue){0}, number(1),
                                 &ranges, order, &output, NULL));
    CHECK(same_value(output.sprite, value(3)));
    CHECK(ab_damage_sprite_names(value(3), number(1), (AbDataValue){0}, number(1),
                                 &ranges, order, &output, NULL));
    CHECK(same_value(output.sprite, value(4)));
    AbDataEntry source_fields[] = {
        {string("sprite"), value(3)},
        {string("strength"), number(1)},
        {string("damageSprites"), {.kind = AB_DATA_TABLE, .table = &ranges}}};
    AbDataTable source = {source_fields, 3, 3, NULL};
    AbObjectDefinition definition = {.source = &source};
    AbDataTableLayout layout = {
        .table = &ranges, .hash_capacity = 2, .no_nil_assignments = true};
    AbLuaData catalog = {.layouts = &layout};
    size_t indices[2];
    AbStringOrderNode nodes[2];
    AbStringOrderWorkspace workspace = {indices, 2, nodes, 2};
    CHECK(!ab_object_initial_sprites(&definition, NULL, &workspace, &output, NULL));
    CHECK(same_value(output.sprite, value(4)));
    CHECK(ab_object_initial_sprites(&definition, &catalog, &workspace, &output, NULL));
    CHECK(same_value(output.sprite, value(4)));
    source_fields[1].value = (AbDataValue){0};
    AbDataEntry material_field = {string("strength"), number(2)};
    AbDataTable material = {&material_field, 1, 1, NULL};
    definition.material = &material;
    CHECK(ab_object_initial_sprites(&definition, &catalog, &workspace, &output, NULL) &&
          same_value(output.sprite, value(4)) &&
          source_fields[1].value.kind == AB_DATA_NIL);
    material_field.value = number(0);
    CHECK(ab_object_initial_sprites(&definition, &catalog, &workspace, &output, NULL) &&
          same_value(output.sprite, value(3)) &&
          source_fields[1].value.kind == AB_DATA_NIL);
    layout.no_nil_assignments = false;
    CHECK(!ab_object_initial_sprites(&definition, &catalog, &workspace, &output, NULL));
    return true;
}

int main(int argc, char **argv) {
    if (argc == 1) {
        return edge_checks() ? 0 : 1;
    }
    if (argc != 2) {
        fprintf(stderr, "Usage: ab_damage_sprite_tests [NATIVE_CORPUS]\n");
        return 2;
    }
    FILE *file = fopen(argv[1], "rb");
    if (!file) {
        return 1;
    }
    bool okay = native_checks(file);
    if (fclose(file) != 0) {
        okay = false;
    }
    return okay ? 0 : 1;
}
