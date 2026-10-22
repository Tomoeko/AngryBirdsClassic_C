#include "ab/sprite_scene.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Sprite scene check failed at line %d: %s\n", __LINE__,    \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

static bool scene_checks(void) {
    AbSpriteRegion region = {.width = 16, .height = 16};
    AbSceneSprite items[5];
    for (size_t index = 0; index < 5; ++index) {
        items[index] = (AbSceneSprite){.region = &region,
                                       .pose = {{2, 2}, 0, {1, 1}},
                                       .texture = (uint32_t)index + 1,
                                       .texture_width = 64,
                                       .texture_height = 64,
                                       .layer = 2,
                                       .blend = AB_BLEND_PREMULTIPLIED,
                                       .visible = true};
    }
    items[1].layer = -1;
    items[2].visible = false;
    items[2].region = NULL;
    items[3].collider = 7;
    items[3].region = NULL;
    size_t order[5] = {99, 99, 99, 99, 99};
    CHECK(!ab_sprite_scene_order(items, 5, order, 4, NULL));
    CHECK(order[0] == 99);
    CHECK(ab_sprite_scene_order(items, 5, order, 5, NULL));
    CHECK(order[0] == 1 && order[1] == 0 && order[2] == 2 && order[3] == 3 &&
          order[4] == 4);
    AbDrawCommand commands[4] = {{.texture = 99}};
    AbDrawList list = {commands, 1, 4};
    AbCoordinateCamera camera = {{0, 0}, {1, 1}};
    CHECK(ab_sprite_scene_draw(items, 5, order, camera, 480, 320, &list, NULL));
    CHECK(list.count == 4 && commands[0].texture == 99 && commands[1].texture == 2 &&
          commands[2].texture == 1 && commands[3].texture == 5);
    CHECK(commands[1].coordinates == AB_DRAW_CLIP && commands[1].color[0] == 1 &&
          commands[1].opacity == 1 && commands[1].quad.vertices[0].x < 0);
    list.count = 1;
    items[4].special_animation = true;
    CHECK(!ab_sprite_scene_draw(items, 5, order, camera, 480, 320, &list, NULL));
    CHECK(list.count == 1 && commands[0].texture == 99);
    items[4].special_animation = false;
    items[4].pose.physics_position.x = 500;
    CHECK(ab_sprite_scene_draw(items, 5, order, camera, 480, 320, &list, NULL));
    CHECK(list.count == 3 && commands[1].texture == 2 && commands[2].texture == 1);
    list.count = 1;
    order[4] = 5;
    CHECK(!ab_sprite_scene_draw(items, 5, order, camera, 480, 320, &list, NULL));
    CHECK(list.count == 1);
    CHECK(ab_sprite_scene_order(NULL, 0, NULL, 0, NULL));
    CHECK(ab_sprite_scene_draw(NULL, 0, NULL, camera, 480, 320, &list, NULL));
    return true;
}

static bool compare_reference(FILE *file) {
    size_t count;
    CHECK(fscanf(file, "AB_NATIVE_SCENE_ORDER_V1 %zu\n", &count) == 1);
    CHECK(count > 0 && count <= 1024);
    uint32_t identifiers[1024], expected[1024];
    AbSceneSprite items[1024] = {0};
    for (size_t index = 0; index < count; ++index) {
        CHECK(fscanf(file, "%" SCNu32 " %" SCNd32 "\n", expected + index,
                     &items[index].layer) == 2);
        identifiers[index] = expected[index];
    }
    CHECK(fgetc(file) == EOF && !ferror(file));
    /* Scene::newId is monotonic in this retained lifetime; reconstruct insertion
     * chronology, then compare with the actual layer-map/vector traversal. */
    for (size_t index = 0; index < count; ++index) {
        size_t smallest = index;
        for (size_t next = index + 1; next < count; ++next) {
            if (identifiers[next] < identifiers[smallest]) {
                smallest = next;
            }
        }
        uint32_t identifier = identifiers[index];
        identifiers[index] = identifiers[smallest];
        identifiers[smallest] = identifier;
        AbSceneSprite item = items[index];
        items[index] = items[smallest];
        items[smallest] = item;
        CHECK(index == 0 || identifiers[index] > identifiers[index - 1]);
    }
    size_t order[1024];
    CHECK(ab_sprite_scene_order(items, count, order, 1024, NULL));
    for (size_t index = 0; index < count; ++index) {
        CHECK(identifiers[order[index]] == expected[index]);
    }
    printf("Matched retained native scene order for %zu renderers\n", count);
    return true;
}

int main(int argc, char **argv) {
    if (!scene_checks()) {
        return 1;
    }
    if (argc == 1) {
        return 0;
    }
    if (argc != 2) {
        return 2;
    }
    FILE *file = fopen(argv[1], "rb");
    if (!file) {
        return 1;
    }
    bool okay = compare_reference(file);
    if (fclose(file)) {
        okay = false;
    }
    return okay ? 0 : 1;
}
