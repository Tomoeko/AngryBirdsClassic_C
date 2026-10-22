#ifndef AB_TEST_LEVEL_RETRY_H
#define AB_TEST_LEVEL_RETRY_H

#include <stdlib.h>

/* Controlled test fixture adapter, before contact discovery. Supply observed
 * leaf identifiers from a reused native world, without importing native poses,
 * bounds, contacts or impulses. Only this owner-side test setup accesses the
 * tree internals; ordinary runtime clients retain the read-only contract.
 * Full preceding-world allocation/destruction policy remains a separate task.
 */
static bool seed_retry_proxy_identifiers(AbPhysicsWorld *world,
                                         const NativeObjectBody records[23],
                                         const uint32_t wanted[23]) {
    AbDynamicTree *tree = &world->broadphase.tree;
    CHECK(tree->capacity == 64 && world->contact_count == 0);
    int32_t mapping[64];
    bool used[64] = {0};
    for (size_t index = 0; index < 64; ++index) {
        mapping[index] = -1;
    }
    for (size_t index = 0; index < 23; ++index) {
        int32_t old =
            world->storage.fixtures[records[index].instance.fixture].proxy.proxy;
        CHECK(old >= 0 && old < 64 && wanted[index] < 64 && !used[wanted[index]]);
        mapping[old] = (int32_t)wanted[index];
        used[wanted[index]] = true;
    }
    size_t next = 0;
    for (size_t index = 0; index < 64; ++index) {
        if (mapping[index] < 0) {
            while (next < 64 && used[next]) {
                ++next;
            }
            CHECK(next < 64);
            mapping[index] = (int32_t)next;
            used[next] = true;
        }
    }
    CHECK(tree->root >= 0 && tree->root < 64 && tree->free_head >= 0 &&
          tree->free_head < 64);
    CHECK(world->broadphase.pair_count == 0);
    for (size_t index = 0; index < 64; ++index) {
        const AbTreeNode *node = tree->nodes + index;
        CHECK(node->parent >= -1 && node->parent < 64);
        if (node->height >= 0) {
            CHECK(node->children[0] >= -1 && node->children[0] < 64 &&
                  node->children[1] >= -1 && node->children[1] < 64);
        }
    }
    for (size_t index = 0; index < world->broadphase.move_count; ++index) {
        CHECK(world->broadphase.moves[index] >= -1 &&
              world->broadphase.moves[index] < 64);
    }
    AbTreeNode *nodes = malloc(64 * sizeof(*nodes));
    CHECK(nodes);
    for (size_t index = 0; index < 64; ++index) {
        AbTreeNode *node = nodes + mapping[index];
        memcpy(node, tree->nodes + index, sizeof(*node));
        if (node->parent >= 0) {
            node->parent = mapping[node->parent];
        }
        if (node->height >= 0) {
            for (size_t child = 0; child < 2; ++child) {
                if (node->children[child] >= 0) {
                    node->children[child] = mapping[node->children[child]];
                }
            }
        }
    }
    free(tree->nodes);
    tree->nodes = nodes;
    tree->root = mapping[tree->root];
    tree->free_head = mapping[tree->free_head];
    for (size_t index = 0; index < world->fixture_slot_count; ++index) {
        if (world->fixture_alive[index]) {
            int32_t *proxy = &world->storage.fixtures[index].proxy.proxy;
            *proxy = mapping[*proxy];
        }
    }
    for (size_t index = 0; index < world->broadphase.move_count; ++index) {
        int32_t *proxy = world->broadphase.moves + index;
        if (*proxy >= 0) {
            *proxy = mapping[*proxy];
        }
    }
    CHECK(world->broadphase.pair_count == 0);
    return true;
}

static bool level_retry_checks(const char *blocks_path, const char *level_path,
                               const char *bodies_path, const char *controls_path,
                               const char *reference_path) {
    AbBuffer inputs[2] = {0};
    AbLuaChunk chunks[2] = {0};
    AbLuaData data[2] = {0};
    AbClassicLevel *owned_level = NULL;
    NativeObjectBody records[23] = {0};
    const char *paths[] = {blocks_path, level_path};
    FILE *bodies = NULL, *controls_file = NULL, *reference = NULL;
    AbError error = {0};
    bool okay = false;
    for (size_t index = 0; index < 2; ++index) {
        if (!ab_file_read(paths[index], 8 * 1024 * 1024, inputs + index, &error) ||
            !ab_lua_read((AbSpan){inputs[index].data, inputs[index].size},
                         8 * 1024 * 1024, chunks + index, &error) ||
            !ab_lua_data(chunks + index, 8 * 1024 * 1024, data + index, &error)) {
            goto release_retry;
        }
    }
    bodies = fopen(bodies_path, "rb");
    controls_file = fopen(controls_path, "rb");
    reference = fopen(reference_path, "rb");
    if (!bodies || !controls_file || !reference) {
        perror("Retry reference");
        goto release_retry;
    }
    char header[80];
    uint32_t controls[8], identifiers[23], rest[2];
    okay = read_body_records(bodies, records) &&
           fgets(header, sizeof(header), controls_file) &&
           strcmp(header, "AB_LEVEL_NATIVE_FRAMES_V2 12 23\n") == 0 &&
           read_body_words(controls_file, controls, 8) &&
           fgets(header, sizeof(header), reference) &&
           strcmp(header, "AB_LEVEL_RETRY_IDENTIFIERS_V1 23\n") == 0 &&
           read_body_words(reference, identifiers, 23) &&
           read_body_words(reference, rest, 2) &&
           open_native_classic_level(data[0].globals, data + 1, records,
                                     level_world_definition(controls), &owned_level,
                                     &error);
    AbClassicLevelView view = ab_classic_level_view(owned_level);
    okay = okay && seed_retry_proxy_identifiers(view.world, records, identifiers);
    if (!okay) {
        goto release_retry;
    }
    const AbObjectBody *bird = NULL;
    for (size_t index = 0; index < 23; ++index) {
        if (records[index].leading_bird) {
            bird = &records[index].instance;
        }
    }
    AbBodyPose pose = {0};
    okay = bird &&
           ab_world_set_body_type(view.world, bird->body, AB_BODY_DYNAMIC, &error) &&
           ab_slingshot_hold_body(view.world, bird,
                                  (AbVec2){from_bits(rest[0]), from_bits(rest[1])},
                                  &error) &&
           ab_body_pose_capture(&view.world->storage.bodies[bird->body].motion, 3,
                                &pose) &&
           replay_level_flight(reference, view.world, records, controls,
                               data[0].globals, view.bodies->objects, owned_level);
release_retry:
    if (!okay && error.message[0]) {
        fprintf(stderr, "Retry replay: %s\n", error.message);
    }
    if (bodies && fclose(bodies) != 0) {
        okay = false;
    }
    if (controls_file && fclose(controls_file) != 0) {
        okay = false;
    }
    if (reference && fclose(reference) != 0) {
        okay = false;
    }
    ab_classic_level_close(owned_level);
    for (size_t index = 0; index < 2; ++index) {
        ab_lua_data_close(data + index);
        ab_lua_close(chunks + index);
        ab_buffer_free(inputs + index);
    }
    return okay;
}

#endif
