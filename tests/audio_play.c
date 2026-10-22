#include "ab/audio_play.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Audio play failed at %d: %s\n", __LINE__, #condition);    \
            return false;                                                              \
        }                                                                              \
    } while (0)

static AbDataValue string(const char *text) {
    return (AbDataValue){.kind = AB_DATA_STRING,
                         .string = {(const uint8_t *)text, strlen(text)}};
}

static AbDataValue value(uint32_t code) {
    static const uint32_t numbers[] = {0, UINT32_C(0x3f000000), UINT32_C(0x3f800000),
                                       UINT32_C(0x40000000)};
    if (!code) {
        return (AbDataValue){0};
    }
    if (code < 3) {
        return (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = code == 2};
    }
    if (code < 7) {
        return (AbDataValue){.kind = AB_DATA_NUMBER, .bits = numbers[code - 3]};
    }
    return string(code == 7 ? "" : "value");
}

static AbDataValue name(uint32_t code) {
    return code < 2    ? value(code)
           : code == 2 ? value(3)
                       : string(code == 3   ? ""
                                : code == 4 ? "clip"
                                            : "missing");
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

static bool words(FILE *file, uint32_t *output, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        CHECK(fscanf(file, "%" SCNx32, output + index) == 1 &&
              fgetc(file) == (index + 1 == count ? '\n' : ' '));
    }
    return true;
}

static bool native_checks(FILE *file) {
    char header[80];
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB_ANDROID_AUDIO_PLAY_V1 4096\n") == 0);
    size_t errors = 0, suppressed = 0, plays = 0;
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t input[10], expected[6];
        CHECK(words(file, input, 10) && words(file, expected, 6) && input[0] < 6 &&
              input[1] < 4 && expected[0] <= 1 && expected[1] <= 1);
        for (size_t field = 2; field < 10; ++field) {
            CHECK(input[field] < 9);
        }
        AbDataEntry fields[] = {{string("channel"), value(input[6])},
                                {string("volume"), value(input[7])},
                                {string("loop"), value(input[8])}};
        AbDataTable metadata = {fields, 3, 3, NULL};
        AbDataValue sound =
            input[1] == 2 ? (AbDataValue){.kind = AB_DATA_TABLE, .table = &metadata}
                          : value(input[1] == 3 ? 2u : input[1]);
        AbDataEntry sound_entry = {string("clip"), sound};
        AbDataTable sounds = {&sound_entry, 1, 1, NULL};
        AbDataEntry volume_entries[8];
        for (uint32_t key = 1; key < 9; ++key) {
            volume_entries[key - 1] = (AbDataEntry){value(key), value(input[9])};
        }
        AbDataTable volumes = {volume_entries, 8, 8, NULL};
        AbAudioPlay request = {name(input[0]), value(input[2]), value(input[3]),
                               value(input[4])};
        AbAudioPlay output = {.name = string("unchanged")};
        bool play = true;
        bool okay =
            ab_audio_play_resolve(&sounds, &volumes, request, &output, &play, NULL);
        CHECK(okay == !expected[0]);
        if (!okay) {
            CHECK(play && same(output.name, string("unchanged")) && !expected[1]);
            ++errors;
        } else if (!expected[1]) {
            CHECK(!play && same(output.name, string("unchanged")));
            ++suppressed;
        } else {
            CHECK(play && same(output.name, name(expected[2])) &&
                  same(output.volume, value(expected[3])) &&
                  same(output.loop, value(expected[4])) &&
                  same(output.channel, value(expected[5])));
            ++plays;
        }
    }
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB audio play reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    printf("All 4096 original audio-play requests match: %zu resource calls, %zu "
           "suppressed names, %zu invalid metadata errors\n",
           plays, suppressed, errors);
    return true;
}

static bool edge_checks(void) {
    AbDataTable empty = {0};
    AbAudioPlay request = {.name = string("missing")};
    AbAudioPlay output = {.name = string("unchanged")};
    bool play = false;
    CHECK(ab_audio_play_resolve(&empty, &empty, request, &output, &play, NULL) &&
          play && same(output.volume, value(5)) && same(output.loop, value(1)) &&
          same(output.channel, value(3)));
    request.name = string("");
    output.name = string("unchanged");
    CHECK(ab_audio_play_resolve(&empty, &empty, request, &output, &play, NULL) &&
          !play && same(output.name, string("unchanged")));
    request.name = string("clip");
    request.volume = value(4);
    request.loop = value(2);
    AbDataEntry fields[] = {{string("channel"), value(6)},
                            {string("volume"), value(3)},
                            {string("loop"), value(1)}};
    AbDataTable metadata = {fields, 3, 3, NULL};
    AbDataEntry sound_entry = {string("clip"),
                               {.kind = AB_DATA_TABLE, .table = &metadata}};
    AbDataTable sounds = {&sound_entry, 1, 1, NULL};
    AbDataEntry channel_entry = {value(6), value(5)};
    AbDataTable channels = {&channel_entry, 1, 1, NULL};
    CHECK(ab_audio_play_resolve(&sounds, &channels, request, &output, &play, NULL) &&
          play && same(output.volume, value(3)) && same(output.loop, value(2)) &&
          same(output.channel, value(6)));
    sound_entry.value = value(1);
    output.name = string("unchanged");
    play = false;
    CHECK(!ab_audio_play_resolve(&sounds, &channels, request, &output, &play, NULL) &&
          !play && same(output.name, string("unchanged")));
    sounds.count = 2;
    CHECK(!ab_audio_play_resolve(&sounds, &channels, request, &output, &play, NULL));
    CHECK(!ab_audio_play_resolve(NULL, &empty, request, &output, &play, NULL));
    CHECK(!ab_audio_play_resolve(&empty, NULL, request, &output, &play, NULL));
    CHECK(!ab_audio_play_resolve(&empty, &empty, request, NULL, &play, NULL));
    CHECK(!ab_audio_play_resolve(&empty, &empty, request, &output, NULL, NULL));
    return true;
}

int main(int argc, char **argv) {
    if (argc == 1) {
        return edge_checks() ? 0 : 1;
    }
    if (argc != 2) {
        fprintf(stderr, "Usage: ab_audio_play_tests [NATIVE_CORPUS]\n");
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
