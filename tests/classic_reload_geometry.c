#include "ab/classic_launch_host.h"
#include "../tools/oracle/reference_reload_geometry.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Reload geometry failed at %d: %s\n", __LINE__,            \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

static uint32_t bits(float value) {
    uint32_t word;
    memcpy(&word, &value, sizeof(word));
    return word;
}

static bool corpus(FILE *reference) {
    if (reference) {
        char header[64];
        CHECK(fgets(header, sizeof(header), reference) &&
              !strcmp(header, "AB_ANDROID_RELOAD_GEOMETRY_V1 128\n"));
    }
    for (unsigned profile = 0; profile < 128; ++profile) {
        float values[4];
        reference_reload_geometry(profile, values);
        AbCameraSnap snaps[] = {{{values[0], values[1]}, 1},
                                {{values[2], values[3]}, 0.5f}};
        AbCameraSystem *camera = NULL;
        CHECK(ab_camera_system_open(snaps, 2, (AbVec2){480, 320}, 200, 200, 65536,
                                    &camera, NULL));
        /* Isolate the reached geometry query over an actual owned CameraSystem.
         * Other launch-host fields are unused here; full initialized host/audio/
         * sling composition runs independently in classic_shots_live_android. */
        AbClassicLaunchHost host = {.initialized = true, .config = {.camera = &camera}};
        float range = -1, from_screen = -1;
        CHECK(
            ab_classic_launch_host_camera_distances(&host, &range, &from_screen, NULL));
        uint32_t row[] = {profile,          bits(values[0]), bits(values[1]),
                          bits(values[2]),  bits(values[3]), bits(range),
                          bits(from_screen)};
        for (unsigned index = 0; reference && index < 7; ++index) {
            uint32_t expected;
            CHECK(fscanf(reference, "%" SCNx32, &expected) == 1);
            if (row[index] != expected) {
                fprintf(stderr, "Reload %u/%u: %08" PRIx32 " != %08" PRIx32 "\n",
                        profile, index, row[index], expected);
                return false;
            }
        }
        CHECK(!host.failed);
        ab_camera_system_close(camera);
    }
    if (reference) {
        char marker[64];
        CHECK(fscanf(reference, " %63[^\n]", marker) == 1 &&
              !strcmp(marker, "AB reload geometry reference complete"));
        int byte;
        do {
            byte = fgetc(reference);
        } while (byte == '\n' || byte == '\r');
        CHECK(byte == EOF && !ferror(reference));
    }
    return true;
}

static bool edges(void) {
    AbCameraSystem *camera = NULL;
    AbClassicLaunchHost host = {.initialized = true, .config = {.camera = &camera}};
    float range = 91, from_screen = 92;
    AbError error = {0};
    CHECK(
        !ab_classic_launch_host_camera_distances(&host, &range, &from_screen, &error) &&
        strstr(error.message, "Missing actual two-snap Museum reload camera") &&
        range == 91 && from_screen == 92 && host.failed &&
        !ab_classic_launch_host_camera_distances(&host, &range, &from_screen, NULL));
    host.failed = false;
    CHECK(!ab_classic_launch_host_camera_distances(&host, NULL, &from_screen, NULL) &&
          !host.failed && from_screen == 92);
    return true;
}

int main(int argc, char **argv) {
    FILE *reference = argc == 2 ? fopen(argv[1], "rb") : NULL;
    if (argc > 2 || (argc == 2 && !reference))
        return 2;
    bool okay = edges() && corpus(reference);
    if (reference)
        okay = fclose(reference) == 0 && okay;
    if (!okay)
        return 1;
    puts(reference ? "Reload geometry matches 128 original camera query pairs"
                   : "Reload geometry actual-owner checks passed");
    return 0;
}
