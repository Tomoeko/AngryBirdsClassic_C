#include "ab/block_component.h"
#include "ab/ambient_animation_data.h"
#include "ab/ambient_particle_data.h"
#include "ab/flame_data.h"
#include "ab/target_data.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Block component failed at %d: %s\n", __LINE__,            \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

static AbDataValue string(const char *name) {
    return (AbDataValue){.kind = AB_DATA_STRING,
                         .string = {(const uint8_t *)name, strlen(name)}};
}

static AbDataValue value(uint32_t kind, AbDataTable *table) {
    if (!kind) {
        return (AbDataValue){0};
    }
    if (kind <= 2) {
        return (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = kind == 2};
    }
    if (kind == 3) {
        return (AbDataValue){.kind = AB_DATA_NUMBER};
    }
    if (kind == 4) {
        return string("table");
    }
    return (AbDataValue){.kind = AB_DATA_TABLE, .table = table};
}

static bool edge_checks(void) {
    AbDataTable empty = {0};
    const AbDataTable *output = &empty;
    CHECK(!ab_block_component(NULL, AB_BLOCK_COMPONENT_FLAME, &output, NULL) &&
          output == &empty);
    CHECK(!ab_block_component(&empty, AB_BLOCK_COMPONENT_COUNT, &output, NULL));
    CHECK(!ab_block_component(&empty, AB_BLOCK_COMPONENT_FLAME, NULL, NULL));
    CHECK(ab_block_component(&empty, AB_BLOCK_COMPONENT_FLAME, &output, NULL) &&
          !output);
    AbDataEntry fields[] = {{string("flame"), value(5, &empty)}};
    AbDataTable components = {fields, 1, 1, NULL};
    AbDataEntry entry = {string("components"), value(5, &components)};
    AbDataTable definition = {&entry, 1, 1, NULL};
    CHECK(ab_block_component(&definition, AB_BLOCK_COMPONENT_FLAME, &output, NULL) &&
          output == &empty);
    fields[0].value.table = NULL;
    CHECK(!ab_block_component(&definition, AB_BLOCK_COMPONENT_FLAME, &output, NULL) &&
          output == &empty);
    components.count = 2;
    CHECK(!ab_block_component(&definition, AB_BLOCK_COMPONENT_FLAME, &output, NULL));
    return true;
}

static bool read_words(FILE *file, uint32_t *words, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        CHECK(fscanf(file, "%" SCNx32, words + index) == 1);
    }
    CHECK(fgetc(file) == '\n');
    return true;
}

static bool native_checks(FILE *file) {
    char header[80];
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB_ANDROID_BLOCK_COMPONENT_V1 4096\n") == 0);
    const char *const names[] = {"flame", "ambientAnimations", "ambientParticles"};
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t input[4], expected[3];
        CHECK(read_words(file, input, 4) && read_words(file, expected, 3));
        AbDataTable empty = {0}, children[3] = {{0}};
        AbDataEntry fields[3];
        for (size_t index = 0; index < 3; ++index) {
            fields[index] = (AbDataEntry){string(names[index]),
                                          value(input[index + 1], children + index)};
        }
        AbDataTable components = {fields, 3, 3, NULL};
        AbDataEntry outer = {string("components"), value(input[0], &components)};
        AbDataTable definition = {&outer, 1, 1, NULL};
        for (unsigned kind = 0; kind < AB_BLOCK_COMPONENT_COUNT; ++kind) {
            const AbDataTable *output = &empty;
            CHECK(ab_block_component(&definition, (AbBlockComponentKind)kind, &output,
                                     NULL));
            CHECK((output != NULL) == (expected[kind] != 0));
            CHECK(!output || output == children + kind);
        }
    }
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB block component reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    puts("All 4096 native component configurations and 12288 canApply results match");
    return true;
}

static bool apk_checks(const char *path) {
    AbBuffer bytes = {0};
    AbZip archive = {0};
    AbTargetData catalog = {0};
    AbError error = {0};
    uint8_t key[32] = {0};
    bool okay = ab_file_read(path, 64 * 1024 * 1024, &bytes, &error) &&
                ab_zip_open((AbSpan){bytes.data, bytes.size}, &archive, &error) &&
                ab_target_resource_key(&archive, 16 * 1024 * 1024, key, &error) &&
                ab_target_data_open(&archive, "assets/data/scripts/blocks.lua",
                                    (AbSpan){key, sizeof(key)}, 32 * 1024 * 1024, NULL,
                                    &catalog, &error);
    size_t definitions = 0, counts[3] = {0}, maximum[3] = {0}, ambient_frames = 0;
    if (!okay) {
        goto release_catalog;
    }
    const AbDataValue *blocks = ab_data_get(catalog.data.globals, string("blocks"));
    if (!blocks || blocks->kind != AB_DATA_TABLE || !blocks->table) {
        okay = ab_fail(&error, "Missing component definition catalog");
        goto release_catalog;
    }
    for (size_t index = 0; index < blocks->table->count; ++index) {
        const AbDataValue *definition = &blocks->table->entries[index].value;
        if (definition->kind != AB_DATA_TABLE) {
            okay = ab_fail(&error, "Invalid block component definition");
            goto release_catalog;
        }
        for (unsigned kind = 0; kind < AB_BLOCK_COMPONENT_COUNT; ++kind) {
            const AbDataTable *component = NULL;
            if (!ab_block_component(definition->table, (AbBlockComponentKind)kind,
                                    &component, &error)) {
                okay = false;
                goto release_catalog;
            }
            if (component) {
                ++counts[kind];
                if (kind == AB_BLOCK_COMPONENT_FLAME) {
                    AbSpan sprites[8];
                    AbFlameOffset offsets[8];
                    AbFlameConfiguration flame;
                    if (!ab_flame_data(component,
                                       (AbFlameStorage){sprites, 8, offsets, 8}, &flame,
                                       &error) ||
                        !flame.definition.default_present ||
                        flame.definition.offset_count ||
                        !((flame.definition.count == 6 && flame.timer_speed == 2 &&
                           flame.scale == 0.7f &&
                           flame.definition.default_offset.y == -1.6f) ||
                          (flame.definition.count == 3 && flame.timer_speed == 3 &&
                           flame.scale == 1.7f &&
                           flame.definition.default_offset.y == -3.5f))) {
                        okay = ab_fail(&error, "Unexpected APK flame configuration");
                        goto release_catalog;
                    }
                }
                if (kind == AB_BLOCK_COMPONENT_AMBIENT_ANIMATIONS) {
                    AbSpan sprites[8];
                    AbAmbientAnimationVariant variants[1];
                    AbAmbientAnimationConfiguration animation;
                    size_t order[] = {0};
                    if (!ab_ambient_animation_data(
                            component, order,
                            (AbAmbientAnimationStorage){variants, 1, sprites, 8},
                            &animation, &error) ||
                        animation.count != 1 || variants[0].delay_present ||
                        variants[0].timer_present || !variants[0].count) {
                        okay = ab_fail(&error, "Unexpected APK ambient configuration");
                        goto release_catalog;
                    }
                    ambient_frames += animation.sprite_count;
                }
                if (kind == AB_BLOCK_COMPONENT_AMBIENT_PARTICLES) {
                    AbAmbientParticle particle;
                    size_t order[] = {0}, particle_count = 0;
                    if (!ab_ambient_particle_data(component, order, &particle, 1,
                                                  &particle_count, &error) ||
                        particle_count != 1 || particle.timer_present ||
                        particle.amount != 1 || particle.offset.x != 0 ||
                        particle.offset.y != 0 ||
                        !((particle.delay_present && particle.delay == 0.5f) ||
                          (!particle.delay_present && particle.min_delay == 3.02f &&
                           particle.max_delay == 5.02f))) {
                        okay = ab_fail(&error, "Unexpected APK particle configuration");
                        goto release_catalog;
                    }
                }
                if (component->count > maximum[kind]) {
                    maximum[kind] = component->count;
                }
            }
        }
        ++definitions;
    }
    okay = definitions == 618 && counts[0] == 2 && counts[1] == 8 && counts[2] == 5 &&
           ambient_frames == 20;
    printf("All %zu APK definitions: flame %zu, ambient animations %zu, ambient "
           "particles %zu; maximum table entries %zu/%zu/%zu\n",
           definitions, counts[0], counts[1], counts[2], maximum[0], maximum[1],
           maximum[2]);
    printf("Read both flame configurations and all eight ambient descriptors / %zu "
           "sprite entries, plus all five ambient particle descriptors\n",
           ambient_frames);

release_catalog:
    memset(key, 0, sizeof(key));
    ab_target_data_close(&catalog);
    ab_zip_close(&archive);
    ab_buffer_free(&bytes);
    if (!okay) {
        fprintf(stderr, "%s\n", error.message);
    }
    return okay;
}

int main(int argc, char **argv) {
    if (!edge_checks()) {
        return 1;
    }
    if (argc == 3 && strcmp(argv[1], "--apk") == 0) {
        return apk_checks(argv[2]) ? 0 : 1;
    }
    if (argc == 2) {
        FILE *file = fopen(argv[1], "rb");
        bool okay = file && native_checks(file);
        if (file && fclose(file) != 0) {
            okay = false;
        }
        return okay ? 0 : 1;
    }
    if (argc != 1) {
        return 2;
    }
    puts("Component table-type predicates and invalid inputs pass");
    return 0;
}
