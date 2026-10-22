#include "ab/scene_id.h"
#include "scene_id_cases.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(value)                                                                   \
    do {                                                                               \
        if (!(value)) {                                                                \
            fprintf(stderr, "Scene identifier check at %d: %s\n", __LINE__, #value);   \
            return false;                                                              \
        }                                                                              \
    } while (0)

static bool controls(void) {
    uint32_t counter = UINT32_MAX;
    AbSceneId output = {.value = 123, .script_number = 4};
    CHECK(!ab_scene_id_next(NULL, &output, NULL) && output.value == 123);
    CHECK(!ab_scene_id_next(&counter, NULL, NULL) && counter == UINT32_MAX);
    CHECK(ab_scene_id_next(&counter, &output, NULL) && counter == 0 &&
          output.value == 0 && output.script_number == 0);
    counter = UINT32_C(0x01000000);
    CHECK(ab_scene_id_next(&counter, &output, NULL));
    CHECK(output.value == 16777217 && output.script_number == 16777216);
    counter = UINT32_C(0x7fffffff);
    CHECK(ab_scene_id_next(&counter, &output, NULL));
    CHECK(output.value == INT32_MIN && output.script_number == -2147483648.0f);
    return true;
}

static bool corpus(FILE *file) {
    char header[96];
    CHECK(fgets(header, sizeof(header), file) &&
          !strcmp(header, "AB_ANDROID_SCENE_ID_V1 4096\n"));
    for (uint32_t sample = 0; sample < 4096; ++sample) {
        uint32_t input, expected[3], actual[3];
        CHECK(fscanf(file, "%" SCNx32, &input) == 1 && input == scene_id_case(sample));
        for (size_t index = 0; index < 3; ++index)
            CHECK(fscanf(file, "%" SCNx32, expected + index) == 1);
        uint32_t counter = input;
        AbSceneId output;
        CHECK(ab_scene_id_next(&counter, &output, NULL));
        memcpy(actual, &output.value, sizeof(output.value));
        memcpy(actual + 1, &output.script_number, sizeof(output.script_number));
        actual[2] = counter;
        for (size_t index = 0; index < 3; ++index) {
            if (actual[index] != expected[index]) {
                fprintf(stderr,
                        "Scene identifier %u word %zu: %08" PRIx32 " != %08" PRIx32
                        "\n",
                        sample, index, actual[index], expected[index]);
                return false;
            }
        }
    }
    CHECK(fgetc(file) == '\n' && fgets(header, sizeof(header), file) &&
          !strcmp(header, "AB scene identifier reference complete\n") &&
          fgetc(file) == EOF && !ferror(file));
    puts("All 4096 native scene ID increments and original integer-to-number results "
         "match");
    return true;
}

int main(int argc, char **argv) {
    if (argc == 1)
        return controls() ? 0 : 1;
    if (argc != 2)
        return 2;
    FILE *file = fopen(argv[1], "rb");
    if (!file)
        return 1;
    bool okay = corpus(file);
    fclose(file);
    return okay ? 0 : 1;
}
