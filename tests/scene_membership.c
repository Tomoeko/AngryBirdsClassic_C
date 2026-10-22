#include "ab/scene_membership.h"
#include "scene_membership_cases.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(value)                                                                   \
    do {                                                                               \
        if (!(value)) {                                                                \
            fprintf(stderr, "Scene membership check at %d: %s\n", __LINE__, #value);   \
            return false;                                                              \
        }                                                                              \
    } while (0)

static bool controls(void) {
    int objects[3];
    AbSceneMember items[4], removed = {.id = 99};
    AbSceneMembership scene = {items, 0, 4};
    bool found = true;
    CHECK(!ab_scene_membership_insert(NULL, (AbSceneMember){0}, NULL));
    CHECK(!ab_scene_membership_insert(&scene, (AbSceneMember){0}, NULL));
    CHECK(!scene.count);
    CHECK(ab_scene_membership_insert(&scene, (AbSceneMember){objects, 7, 4}, NULL));
    CHECK(ab_scene_membership_insert(&scene, (AbSceneMember){objects + 1, 7, 2}, NULL));
    CHECK(ab_scene_membership_insert(&scene, (AbSceneMember){objects + 2, 8, 2}, NULL));
    CHECK(ab_scene_membership_insert(&scene, (AbSceneMember){objects + 1, 7, 2}, NULL));
    CHECK(items[0].object == objects + 1 && items[1].object == objects + 2 &&
          items[2].object == objects + 1 && items[3].object == objects);
    CHECK(!ab_scene_membership_insert(&scene, (AbSceneMember){objects, 1, -1}, NULL));
    CHECK(scene.count == 4 && items[0].id == 7);
    CHECK(ab_scene_membership_remove_id(&scene, 7, &removed, &found, NULL) && found &&
          removed.object == objects + 1 && scene.count == 3);
    CHECK(items[0].object == objects + 2 && items[1].object == objects + 1);
    CHECK(
        ab_scene_membership_remove_object(&scene, objects, 2, &removed, &found, NULL) &&
        !found && removed.object == objects + 1 && scene.count == 3);
    CHECK(ab_scene_membership_remove_object(&scene, objects + 1, 2, &removed, &found,
                                            NULL) &&
          found && scene.count == 2);
    CHECK(ab_scene_membership_insert(&scene, (AbSceneMember){objects + 1, 7, 2}, NULL));
    CHECK(items[0].object == objects + 2 && items[1].object == objects + 1);
    found = true;
    CHECK(!ab_scene_membership_remove_id(&scene, 8, NULL, &found, NULL) && found &&
          scene.count == 3);
    scene.count = 5;
    CHECK(!ab_scene_membership_remove_id(&scene, 8, &removed, &found, NULL) && found &&
          scene.count == 5);
    scene.count = 0;
    CHECK(ab_scene_membership_insert(&scene, (AbSceneMember){objects, 0, 0}, NULL));
    CHECK(ab_scene_membership_insert(&scene, (AbSceneMember){objects + 1, INT32_MAX, 0},
                                     NULL));
    CHECK(ab_scene_membership_insert(&scene, (AbSceneMember){objects + 2, INT32_MIN, 0},
                                     NULL));
    CHECK(ab_scene_membership_dispose_number(&scene, NAN, &removed, &found, NULL) &&
          found && removed.id == 0);
    CHECK(
        ab_scene_membership_dispose_number(&scene, INFINITY, &removed, &found, NULL) &&
        found && removed.id == INT32_MAX);
    CHECK(
        ab_scene_membership_dispose_number(&scene, -INFINITY, &removed, &found, NULL) &&
        found && removed.id == INT32_MIN);
    return true;
}

static bool word(FILE *file, uint32_t actual, uint32_t sample, uint32_t step) {
    uint32_t expected;
    if (fscanf(file, "%" SCNx32, &expected) != 1 || expected != actual) {
        fprintf(stderr,
                "Scene membership mismatch at scene %u operation %u: %08" PRIx32 "\n",
                sample, step, actual);
        return false;
    }
    return true;
}

static bool corpus(FILE *file) {
    char header[96];
    CHECK(fgets(header, sizeof(header), file) &&
          !strcmp(header, "AB_ANDROID_SCENE_MEMBERSHIP_V1 12288\n"));
    for (uint32_t sample = 0; sample < 256; ++sample) {
        int objects[9];
        AbSceneMember items[48], removed;
        AbSceneMembership scene = {items, 0, 48};
        for (uint32_t step = 0; step < 48; ++step) {
            SceneMembershipOperation operation =
                scene_membership_operation(sample, step);
            uint32_t number;
            memcpy(&number, &operation.number, sizeof(number));
            CHECK(word(file, operation.kind, sample, step) &&
                  word(file, operation.object, sample, step) &&
                  word(file, (uint32_t)operation.id, sample, step) &&
                  word(file, number, sample, step));
            bool found = true;
            if (operation.kind == 0) {
                AbSceneMember member = {
                    objects + operation.object,
                    scene_membership_id(sample, operation.object),
                    scene_membership_layer(sample, operation.object)};
                CHECK(ab_scene_membership_insert(&scene, member, NULL));
            } else if (operation.kind == 1) {
                CHECK(ab_scene_membership_remove_id(&scene, operation.id, &removed,
                                                    &found, NULL));
            } else if (operation.kind == 2) {
                CHECK(ab_scene_membership_remove_object(
                    &scene, objects + operation.object,
                    scene_membership_layer(sample, operation.object), &removed, &found,
                    NULL));
            } else {
                CHECK(ab_scene_membership_dispose_number(&scene, operation.number,
                                                         &removed, &found, NULL));
            }
            CHECK(word(file, found, sample, step) &&
                  word(file, (uint32_t)scene.count, sample, step));
            uint32_t references[9];
            for (size_t index = 0; index < 9; ++index)
                references[index] = 1;
            for (size_t index = 0; index < scene.count; ++index) {
                uint32_t object = (uint32_t)((int *)items[index].object - objects);
                CHECK(word(file, object, sample, step));
                ++references[object];
            }
            for (size_t index = 0; index < 9; ++index)
                CHECK(word(file, references[index], sample, step));
        }
    }
    CHECK(fgetc(file) == '\n' && fgets(header, sizeof(header), file) &&
          !strcmp(header, "AB scene membership reference complete\n") &&
          fgetc(file) == EOF && !ferror(file));
    puts("All 12288 native scene operations, member orders and reference counts match");
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
