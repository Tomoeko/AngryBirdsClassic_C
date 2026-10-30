#include "ab/water_effect.h"
#include "ab/target_data.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Water effect failed at %d: %s\n", __LINE__, #condition);  \
            return false;                                                              \
        }                                                                              \
    } while (0)

static AbDataValue string(const char *text) {
    return (AbDataValue){.kind = AB_DATA_STRING,
                         .string = {(const uint8_t *)text, strlen(text)}};
}

static uint32_t bits(float number) {
    uint32_t result;
    memcpy(&result, &number, sizeof(result));
    return result;
}

static AbDataValue typed(uint32_t kind, uint32_t value, AbDataTable *empty) {
    if (kind == 0) {
        return (AbDataValue){0};
    }
    if (kind <= 2) {
        return (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = kind == 2};
    }
    if (kind == 3) {
        return (AbDataValue){.kind = AB_DATA_NUMBER, .bits = value};
    }
    return (AbDataValue){.kind = AB_DATA_TABLE, .table = empty};
}

static bool edge_checks(void) {
    AbDataTable empty = {0};
    AbWaterEffect effect = {.level = 7};
    CHECK(ab_water_effect(&empty, &effect, NULL) && !effect.enabled &&
          bits(effect.level) == 0 && !effect.freezing && !effect.kill_level_present);
    AbDataEntry nested[] = {{string("freezing"), typed(2, 0, NULL)},
                            {string("killLevel"), typed(3, bits(5), NULL)}};
    AbDataTable water = {nested, 2, 2, NULL};
    AbDataEntry root[] = {{string("waterLevel"), typed(3, bits(-0.0f), NULL)},
                          {string("water"), typed(4, 0, &water)}};
    AbDataTable level = {root, 2, 2, NULL};
    CHECK(ab_water_effect(&level, &effect, NULL) && effect.enabled &&
          bits(effect.level) == bits(-0.0f) && !effect.freezing &&
          !effect.kill_level_present && bits(effect.kill_level) == 0);
    root[0].value = typed(0, 0, NULL);
    CHECK(ab_water_effect(&level, &effect, NULL) && effect.enabled &&
          bits(effect.level) == 0 && effect.freezing && effect.kill_level_present &&
          effect.kill_level == 5);
    AbWaterEffect saved = effect;
    root[0].value = string("0");
    CHECK(!ab_water_effect(&level, &effect, NULL) &&
          memcmp(&effect, &saved, sizeof(effect)) == 0);
    root[0].value = typed(3, bits(NAN), NULL);
    CHECK(!ab_water_effect(&level, &effect, NULL));
    CHECK(!ab_water_effect(NULL, &effect, NULL));
    CHECK(!ab_water_effect(&level, NULL, NULL));
    root[0].value = typed(0, 0, NULL);
    water.count = 3;
    CHECK(!ab_water_effect(&level, &effect, NULL));
    return true;
}

static bool read_words(FILE *file, uint32_t *values, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        CHECK(fscanf(file, "%" SCNx32, values + index) == 1);
    }
    CHECK(fgetc(file) == '\n');
    return true;
}

static bool native_checks(FILE *file) {
    char header[80];
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB_ANDROID_WATER_EFFECT_V1 4096\n") == 0);
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t input[8], expected[5];
        CHECK(read_words(file, input, 8) && read_words(file, expected, 5));
        AbDataTable empty = {0};
        AbDataEntry nested[] = {
            {string("waterLevel"), typed(input[3], input[4], &empty)},
            {string("freezing"), typed(input[5], input[4], &empty)},
            {string("killLevel"), typed(input[6], input[7], &empty)}};
        AbDataTable water = {nested, 3, 3, NULL};
        AbDataEntry root[] = {{string("waterLevel"), typed(input[0], input[1], &empty)},
                              {string("water"), typed(input[2], input[4], &water)}};
        AbDataTable level = {root, 2, 2, NULL};
        AbWaterEffect effect = {0};
        CHECK(ab_water_effect(&level, &effect, NULL));
        uint32_t actual[] = {effect.enabled, effect.freezing, effect.kill_level_present,
                             bits(effect.level), bits(effect.kill_level)};
        if (memcmp(actual, expected, sizeof(actual)) != 0) {
            fprintf(stderr, "Native water configuration case %zu differs\n", item);
            return false;
        }
    }
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB water effect reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    puts("All 4096 native water-effect configurations match");
    return true;
}

static bool level_name(AbSpan name) {
    const char prefix[] = "assets/data/levels/", suffix[] = ".lua";
    return name.size >= sizeof(prefix) + sizeof(suffix) - 2 &&
           memcmp(name.data, prefix, sizeof(prefix) - 1) == 0 &&
           memcmp(name.data + name.size - sizeof(suffix) + 1, suffix,
                  sizeof(suffix) - 1) == 0;
}

static bool apk_checks(const char *path) {
    AbBuffer bytes = {0};
    AbZip archive = {0};
    AbError error = {0};
    uint8_t key[32] = {0};
    bool okay = ab_file_read(path, 64 * 1024 * 1024, &bytes, &error) &&
                ab_zip_open((AbSpan){bytes.data, bytes.size}, &archive, &error) &&
                ab_target_resource_key(&archive, 16 * 1024 * 1024, key, &error);
    size_t count = 0, enabled = 0, freezing = 0, kill = 0;
    if (!okay) {
        goto release_archive;
    }
    for (size_t index = 0; index < archive.count; ++index) {
        AbSpan name = archive.entries[index].name;
        if (!level_name(name)) {
            continue;
        }
        char resource[1024];
        if (name.size >= sizeof(resource)) {
            okay = ab_fail(&error, "Level path exceeds its bound");
            goto release_archive;
        }
        memcpy(resource, name.data, name.size);
        resource[name.size] = 0;
        AbTargetData level = {0};
        AbWaterEffect effect = {0};
        okay = ab_target_data_open(&archive, resource, (AbSpan){key, sizeof(key)},
                                   32 * 1024 * 1024, NULL, &level, &error) &&
               ab_water_effect(level.data.globals, &effect, &error);
        ab_target_data_close(&level);
        if (!okay) {
            fprintf(stderr, "Water configuration in %s\n", resource);
            goto release_archive;
        }
        ++count;
        enabled += effect.enabled;
        freezing += effect.freezing;
        kill += effect.kill_level_present;
    }
    okay = count == 655;
    printf("Initialized %zu APK level water configurations: %zu enabled, %zu freezing, "
           "%zu kill levels\n",
           count, enabled, freezing, kill);

release_archive:
    memset(key, 0, sizeof(key));
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
    puts("Water presence, legacy precedence, Boolean gates and invalid inputs pass");
    return 0;
}
