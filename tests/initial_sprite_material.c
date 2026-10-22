#include "ab/damage_sprite.h"
#include "ab/target_data.h"
#include "initial_sprite_material_cases.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Initial material sprites failed at %d: %s\n", __LINE__,   \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

static AbSpan text(const char *value) {
    return (AbSpan){(const uint8_t *)value, strlen(value)};
}

static bool read_words(FILE *file, uint32_t *output, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        if (fscanf(file, "%" SCNx32, output + index) != 1) {
            return false;
        }
    }
    return fgetc(file) == '\n';
}

static bool selected_value(FILE *file, AbDataValue value) {
    uint32_t tag, size;
    CHECK(fscanf(file, "%" SCNx32 " %" SCNx32, &tag, &size) == 2 && size <= 128 &&
          (tag == 0 || tag == 4));
    uint8_t bytes[128];
    for (size_t index = 0; index < size; ++index) {
        uint32_t byte;
        CHECK(fscanf(file, "%" SCNx32, &byte) == 1 && byte <= UINT8_MAX);
        bytes[index] = (uint8_t)byte;
    }
    CHECK(fgetc(file) == '\n');
    if (!tag) {
        CHECK(!size && value.kind == AB_DATA_NIL);
    } else {
        CHECK(value.kind == AB_DATA_STRING && value.string.size == size &&
              (!size || memcmp(value.string.data, bytes, size) == 0));
    }
    return true;
}

static bool compare(FILE *file, const AbTargetData *catalog) {
    char line[96];
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB_ANDROID_INITIAL_SPRITE_MATERIAL_V1 15\n") == 0);
    size_t indices[64];
    AbStringOrderNode nodes[64];
    AbStringOrderWorkspace workspace = {indices, 64, nodes, 64};
    for (uint32_t sample = 0; sample < 15; ++sample) {
        uint32_t observed[9];
        CHECK(read_words(file, observed, 9) && observed[0] == sample &&
              observed[1] == 1 && observed[2] == 1 && !observed[3] && !observed[4] &&
              observed[5] == 3 && observed[7] == 3 && observed[6] == observed[8]);
        AbObjectDefinition definition;
        CHECK(ab_object_definition_open(catalog->data.globals,
                                        text(initial_material_names[sample]), NULL,
                                        &definition, NULL));
        AbDataValue key = {.kind = AB_DATA_STRING, .string = text("strength")};
        const AbDataValue *raw = ab_data_get(definition.source, key);
        CHECK(!raw || raw->kind == AB_DATA_NIL);
        AbDataValue normalized = ab_object_property(&definition, key.string);
        CHECK(normalized.kind == AB_DATA_NUMBER && normalized.bits == observed[8]);
        AbDamageSpriteNames names;
        CHECK(ab_object_initial_sprites(&definition, &catalog->data, &workspace, &names,
                                        NULL) &&
              selected_value(file, names.sprite) && selected_value(file, names.blink) &&
              selected_value(file, names.smile) && selected_value(file, names.frozen) &&
              ab_data_get(definition.source, key) == raw &&
              (!raw || raw->kind == AB_DATA_NIL));
    }
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB initial sprite material reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    puts("All 15 native constructor material-strength writes and 60 initial sprite "
         "fields match the immutable C adapter.");
    return true;
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr,
                "Usage: ab_initial_sprite_material_tests APK NATIVE_REFERENCE\n");
        return 2;
    }
    AbBuffer bytes = {0};
    AbZip archive = {0};
    AbTargetData catalog = {0};
    uint8_t key[32] = {0};
    AbError error = {0};
    FILE *file = fopen(argv[2], "rb");
    bool okay = file && ab_file_read(argv[1], 64 * 1024 * 1024, &bytes, &error) &&
                ab_zip_open((AbSpan){bytes.data, bytes.size}, &archive, &error) &&
                ab_target_resource_key(&archive, 32 * 1024 * 1024, key, &error) &&
                ab_target_data_open(&archive, "assets/data/scripts/blocks.lua",
                                    (AbSpan){key, sizeof(key)}, 32 * 1024 * 1024, NULL,
                                    &catalog, &error) &&
                compare(file, &catalog);
    if (!okay && error.message[0]) {
        fprintf(stderr, "%s\n", error.message);
    }
    if (file && fclose(file) != 0) {
        okay = false;
    }
    memset(key, 0, sizeof(key));
    ab_target_data_close(&catalog);
    ab_zip_close(&archive);
    ab_buffer_free(&bytes);
    return okay ? 0 : 1;
}
