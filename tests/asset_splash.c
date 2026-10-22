#include "ab/asset_profile.h"
#include "asset_splash_cases.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Splash profile check failed at %d: %s\n", __LINE__,       \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

static bool same(AbSpan span, const char *value) {
    return span.size == strlen(value) &&
           (!span.size || memcmp(span.data, value, span.size) == 0);
}

static bool read_words(FILE *file, uint32_t *output, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        if (fscanf(file, "%" SCNx32, output + index) != 1) {
            return false;
        }
    }
    return fgetc(file) == '\n';
}

static bool native_checks(FILE *file) {
    char line[100];
    size_t undefined = 0;
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB_ANDROID_ASSET_SPLASH_V1 4096\n") == 0);
    for (uint32_t sample = 0; sample < 4096; ++sample) {
        uint32_t input[2], generated[2], expected[5], actual[5] = {0};
        asset_splash_case(sample, generated);
        CHECK(read_words(file, input, 2) &&
              memcmp(input, generated, sizeof(input)) == 0 &&
              read_words(file, expected, 5));
        float width, height;
        memcpy(&width, input, sizeof(width));
        memcpy(&height, input + 1, sizeof(height));
        AbAssetSplashProfile result = {.resolution = {7, 8}};
        if (ab_android_splash_profile(width, height, &result, NULL)) {
            actual[0] = actual[3] = 1;
            memcpy(actual + 1, &result.resolution.x, sizeof(float));
            memcpy(actual + 2, &result.resolution.y, sizeof(float));
            for (uint32_t profile = 0; profile < 4; ++profile) {
                if (same(result.profile, splash_profiles[profile])) {
                    actual[4] = profile + 1;
                }
            }
            CHECK(actual[4]);
        } else {
            CHECK(result.resolution.x == 7 && result.resolution.y == 8 &&
                  !result.profile.data && !result.profile.size);
            ++undefined;
        }
        if (memcmp(actual, expected, sizeof(actual)) != 0) {
            fprintf(stderr, "Original splash sample %" PRIu32 " differs\n", sample);
            return false;
        }
    }
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB asset splash reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    printf("Matched 4,096 original splash resolution/profile pairs, including %zu "
           "undefined zero/zero ratios.\n",
           undefined);
    return true;
}

static bool edge_checks(void) {
    AbAssetSplashProfile profile;
    CHECK(ab_android_splash_profile(480, 320, &profile, NULL) &&
          profile.resolution.x == 1440 && profile.resolution.y == 960 &&
          same(profile.profile, "1440x960_splash"));
    CHECK(!ab_android_splash_profile(0, 0, &profile, NULL) &&
          !ab_android_splash_profile(NAN, 320, &profile, NULL) &&
          !ab_android_splash_profile(480, INFINITY, &profile, NULL) &&
          !ab_android_splash_profile(480, 320, NULL, NULL) &&
          same(profile.profile, "1440x960_splash") && profile.resolution.x == 1440);
    CHECK(ab_android_splash_profile(1, 0, &profile, NULL) &&
          same(profile.profile, "1024x550_splash"));
    CHECK(ab_android_splash_profile(-1, 0, &profile, NULL) &&
          same(profile.profile, "1024x768_splash"));
    return true;
}

int main(int argc, char **argv) {
    if (argc == 1) {
        return edge_checks() ? 0 : 1;
    }
    if (argc != 2) {
        return 2;
    }
    FILE *file = fopen(argv[1], "r");
    if (!file) {
        return 1;
    }
    bool okay = native_checks(file);
    if (fclose(file) != 0) {
        okay = false;
    }
    return okay ? 0 : 1;
}
