#include "ab/audio_definition.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Audio definition failed at %d: %s\n", __LINE__,           \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

static AbDataValue string(const char *text) {
    return (AbDataValue){.kind = AB_DATA_STRING,
                         .string = {(const uint8_t *)text, strlen(text)}};
}

static AbDataValue value(uint32_t code) {
    if (code <= 2) {
        return code == 0 ? (AbDataValue){0}
                         : (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = code == 2};
    }
    if (code <= 4) {
        return (AbDataValue){.kind = AB_DATA_NUMBER,
                             .bits = code == 3 ? 0 : UINT32_C(0x3f000000)};
    }
    return string(code == 5 ? "" : "value");
}

static bool same(AbDataValue actual, AbDataValue expected) {
    return actual.kind == expected.kind &&
           (actual.kind == AB_DATA_STRING
                ? actual.string.size == expected.string.size &&
                      (!actual.string.size ||
                       memcmp(actual.string.data, expected.string.data,
                              actual.string.size) == 0)
                : actual.bits == expected.bits);
}

static bool path_is(AbSpan path, const char *expected) {
    return path.size == strlen(expected) && memcmp(path.data, expected, path.size) == 0;
}

static bool words(FILE *file, uint32_t *output, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        CHECK(fscanf(file, "%" SCNx32, output + index) == 1 &&
              fgetc(file) == (index + 1 == count ? '\n' : ' '));
    }
    return true;
}

static bool native_checks(FILE *file) {
    static const char *const extensions[] = {"wav", "mp3", "", "custom"};
    char header[80];
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB_ANDROID_AUDIO_DEFINITION_V1 4096\n") == 0);
    size_t created_count = 0;
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t input[8], expected[6];
        CHECK(words(file, input, 8) && words(file, expected, 6) && input[0] <= 1 &&
              input[1] < 6 && input[2] < 7 && input[3] < 4 && expected[0] <= 1);
        AbDataEntry entries[] = {
            {string("fileName"), string("/sfx/source")},
            {string("clipName"), string("clip")},
            {string("extension"),
             input[1] < 2 ? value(input[1]) : string(extensions[input[1] - 2])},
            {string("stream"), value(input[2])},
            {string("channel"), value(input[4])},
            {string("volume"), value(input[5])},
            {string("canBeOverWritten"), value(input[6])},
            {string("loop"), value(input[7])},
        };
        for (size_t field = 4; field < 8; ++field) {
            CHECK(input[field] < 7);
        }
        AbDataTable properties = {entries, 8, 8, NULL};
        uint8_t storage[128];
        memset(storage, 0xa5, sizeof(storage));
        AbAudioDefinition output = {.path = string("unchanged").string};
        bool created = true;
        CHECK(ab_audio_definition_read(
                  &properties, value(input[3]), string("data/audio").string,
                  input[0] != 0, storage, sizeof(storage), &output, &created, NULL) &&
              created == (expected[0] != 0));
        if (!created) {
            CHECK(path_is(output.path, "unchanged"));
            for (size_t byte = 0; byte < sizeof(storage); ++byte) {
                CHECK(storage[byte] == 0xa5);
            }
            continue;
        }
        CHECK(same(output.stream, value(expected[1])) &&
              same(output.name, string("clip")) &&
              same(output.channel, value(expected[2])) &&
              same(output.volume, value(expected[3])) &&
              same(output.can_be_overwritten, value(expected[4])) &&
              same(output.loop, value(expected[5])));
        uint32_t size;
        char bytes[128];
        CHECK(words(file, &size, 1) && size < sizeof(bytes) &&
              fread(bytes, 1, size, file) == size && fgetc(file) == '\n' &&
              output.path.size == size && memcmp(output.path.data, bytes, size) == 0);
        ++created_count;
    }
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB audio definition reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    printf("All 4096 original audio definitions match: %zu creations, %zu duplicate "
           "skips, typed metadata and resource paths\n",
           created_count, 4096 - created_count);
    return true;
}

static bool edge_checks(void) {
    AbDataEntry entries[] = {{string("fileName"), string("/a")},
                             {string("clipName"), string("clip")},
                             {string("extension"), {0}}};
    AbDataTable properties = {entries, 3, 3, NULL};
    uint8_t storage[32], before[32];
    memset(storage, 0xa5, sizeof(storage));
    memcpy(before, storage, sizeof(before));
    AbAudioDefinition output = {.path = string("unchanged").string};
    bool created = false;
    CHECK(!ab_audio_definition_read(&properties, value(0), string("root").string, true,
                                    storage, 9, &output, &created, NULL) &&
          !created && path_is(output.path, "unchanged") &&
          memcmp(storage, before, sizeof(storage)) == 0);
    CHECK(ab_audio_definition_read(&properties, value(0), string("root").string, true,
                                   storage, 10, &output, &created, NULL) &&
          created && path_is(output.path, "root/a.mp3") &&
          output.stream.kind == AB_DATA_BOOLEAN && !output.stream.bits &&
          output.channel.kind == AB_DATA_NIL && output.volume.kind == AB_DATA_NIL);
    entries[2].value = value(1);
    CHECK(ab_audio_definition_read(&properties, value(0), string("root").string, true,
                                   storage, 10, &output, &created, NULL) &&
          path_is(output.path, "root/a.wav"));
    entries[2].value = string("");
    CHECK(ab_audio_definition_read(&properties, value(0), string("root").string, true,
                                   storage, 7, &output, &created, NULL) &&
          path_is(output.path, "root/a."));
    entries[2].value = value(3);
    memcpy(before, storage, sizeof(storage));
    created = false;
    CHECK(!ab_audio_definition_read(&properties, value(0), string("root").string, true,
                                    storage, sizeof(storage), &output, &created,
                                    NULL) &&
          !created && path_is(output.path, "root/a.") &&
          memcmp(storage, before, sizeof(storage)) == 0);
    CHECK(ab_audio_definition_read(NULL, value(1), (AbSpan){NULL, SIZE_MAX}, true, NULL,
                                   0, &output, &created, NULL) &&
          !created && path_is(output.path, "root/a."));
    entries[2].value = value(0);
    CHECK(!ab_audio_definition_read(&properties, value(0),
                                    (AbSpan){(const uint8_t *)"x", SIZE_MAX}, true,
                                    storage, sizeof(storage), &output, &created, NULL));
    entries[0].value = (AbDataValue){.kind = AB_DATA_STRING, .string = {NULL, 1}};
    CHECK(!ab_audio_definition_read(&properties, value(0), (AbSpan){0}, false, storage,
                                    sizeof(storage), &output, &created, NULL));
    CHECK(!ab_audio_definition_read(NULL, value(0), (AbSpan){0}, false, storage,
                                    sizeof(storage), &output, &created, NULL));
    CHECK(!ab_audio_definition_read(&properties, value(0), (AbSpan){0}, false, storage,
                                    sizeof(storage), NULL, &created, NULL));
    CHECK(!ab_audio_definition_read(&properties, value(0), (AbSpan){0}, false, storage,
                                    sizeof(storage), &output, NULL, NULL));
    return true;
}

int main(int argc, char **argv) {
    if (argc == 1) {
        return edge_checks() ? 0 : 1;
    }
    if (argc != 2) {
        fprintf(stderr, "Usage: ab_audio_definition_tests [NATIVE_CORPUS]\n");
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
