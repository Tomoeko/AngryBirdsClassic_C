#ifndef AB_ORACLE_GAME_OBJECT_REMOVE_POLICY_H
#define AB_ORACLE_GAME_OBJECT_REMOVE_POLICY_H

#include "../../tests/game_object_remove_cases.h"

/* Real native objects/map nodes/world bodies are constructed and destroyed
 * by the original library. Controlled enclosing GameLua fields exclude scene,
 * animation, joints and teleporters. Static vector slots retain pointer identity
 * after deletion; snapshots compare addresses and never dereference freed data. */
static void probe_game_object_remove_policy(void) {
    void *(*allocate)(unsigned) = (void *(*)(unsigned))find_function("_Znwj");
    void (*world_construct)(void *, const void *) =
        (void (*)(void *, const void *))find_function("_ZN7b2WorldC1ERK6b2Vec2");
    void (*world_destroy)(void *) = (void (*)(void *))find_function("_ZN7b2WorldD1Ev");
    void *(*create_body)(void *, const void *) =
        (void *(*)(void *, const void *))find_function(
            "_ZN7b2World10CreateBodyEPK9b2BodyDef");
    void (*destroy_body)(void *, void *) =
        (void (*)(void *, void *))find_function("_ZN7b2World11DestroyBodyEP6b2Body");
    void (*object_construct)(void *, void *) =
        (void (*)(void *, void *))find_function("_ZN10GameObjectC1EP7b2World");
    void **(*insert)(void *, const char **) =
        (void **(*)(void *, const char **))find_function(
            "_ZNSt3mapISsP10GameObjectSt4lessISsESaISt4pairIKSsS1_EEEixERS5_");
    float *(*rolling)(void *, const char **) =
        (float *(*)(void *, const char **))find_function(
            "_ZNSt3mapISsfSt4lessISsESaISt4pairIKSsfEEEixERS3_");
    void *(*get_body)(void *, const char **) =
        (void *(*)(void *, const char **))find_function("_ZN7GameLua7getBodyERKSs");
    void (*remove_object)(void *, const char **) =
        (void (*)(void *, const char **))find_function("_ZN7GameLua12removeObjectESs");
    void (*set_rolling)(void *, const char **, const char **) =
        (void (*)(void *, const char **, const char **))find_function(
            "_ZN7GameLua15setRollingSoundESsSs");
    static uint32_t game[0x1a00 / 4], birds[3];
    static uint64_t world_storage[16384];
    static const struct {
        uint32_t length, capacity, references;
        char bytes[8];
    } names[] = {{5, 5, UINT32_MAX, "first"},   {6, 6, UINT32_MAX, "second"},
                 {7, 7, UINT32_MAX, "missing"}, {0, 0, UINT32_MAX, ""},
                 {5, 5, UINT32_MAX, "rollA"},   {5, 5, UINT32_MAX, "rollB"},
                 {5, 5, UINT32_MAX, "rollC"}};
    const uint32_t maps[] = {0x1790, 0x17a8, 0x17c0};
    for (uint32_t map = 0; map < 3; ++map) {
        uint32_t *header = game + maps[map] / 4;
        header[2] = header[3] = (uint32_t)(uintptr_t)header;
    }
    const uint32_t gravity[2] = {0, 0};
    const uint32_t definition[13] = {
        2, 0, 0, 0, 0, 0, 0, 0, 0, UINT32_C(0x101), 1, 0, UINT32_C(0x3f800000)};
    world_construct(world_storage, gravity);
    uint32_t *world = (uint32_t *)world_storage;
    game[0x1784 / 4] = (uint32_t)(uintptr_t)world;
    game[0x17d4 / 4] = game[0x17d8 / 4] = (uint32_t)(uintptr_t)birds;
    game[0x17dc / 4] = (uint32_t)(uintptr_t)(birds + 3);
    /* Three process-owned cache nodes remain bounded and rooted until the
     * isolated oracle exits. Every per-case physical node/object is freed by
     * original removeObject, and every native body is destroyed below. */
    semihost(4, "AB_ANDROID_GAME_OBJECT_REMOVE_POLICY_V1 256\n");
    for (uint32_t sample = 0; sample < 256; ++sample) {
        uint32_t input[11], objects[2], bodies[2];
        game_object_remove_case(sample, input);
        print_words(input, 11);
        for (uint32_t sound = 0; sound < 3; ++sound) {
            const char *key = names[sound + 4u].bytes;
            FloatBits value = {.bits = input[sound + 8u]};
            *rolling(game + 0x178c / 4, &key) = value.number;
        }
        for (uint32_t index = 0; index < 2; ++index) {
            const char *key = names[index].bytes;
            uint32_t *object = allocate(200);
            if (!object) {
                fail("Cannot allocate original object-removal policy fixture\n");
            }
            object_construct(object, world);
            object[0] = index ? 0u : input[1];
            object[11] = (uint32_t)(uintptr_t)create_body(world, definition);
            bodies[index] = object[11];
            objects[index] = (uint32_t)(uintptr_t)object;
            if (!object[11]) {
                fail("Cannot create original object-removal policy body\n");
            }
            *insert(game + 0x17a4 / 4, &key) = object;
            /* The original public setter performs managed string assignment.
             * Swap its result into name, preserving the default empty string,
             * then set the requested rolling sound through that same setter. */
            set_rolling(game, &key, &key);
            uint32_t empty = object[32];
            object[32] = object[23];
            object[23] = empty;
            const char *sound = names[3u + input[index + 5u]].bytes;
            set_rolling(game, &key, &sound);
        }
        game[0x17d8 / 4] = (uint32_t)(uintptr_t)(birds + input[3]);
        for (uint32_t slot = 0; slot < 3; ++slot) {
            uint32_t id = (input[4] >> (slot * 2u)) & 3u;
            birds[slot] = objects[id - 1u];
        }
        if (input[7]) {
            world[0x191d4 / 4] |= 2u;
        }
        const char *requested = names[input[2]].bytes;
        remove_object(game, &requested);
        uint32_t output[13] = {game[0x17b8 / 4],
                               world[0x19230 / 4],
                               0,
                               (game[0x17d8 / 4] - game[0x17d4 / 4]) / 4,
                               0,
                               0,
                               0,
                               game[0x17a0 / 4],
                               0,
                               0,
                               0,
                               (world[0x191d4 / 4] & 2u) != 0,
                               0};
        for (uint32_t index = 0; index < 2; ++index) {
            const char *key = names[index].bytes;
            output[2] |= get_body(game, &key) ? 1u << index : 0u;
        }
        for (uint32_t slot = 0; slot < output[3]; ++slot) {
            output[4 + slot] = birds[slot] == objects[0] ? 1u : 2u;
            if (birds[slot] != objects[output[4 + slot] - 1u]) {
                fail("Unexpected original remaining bird pointer\n");
            }
        }
        for (uint32_t sound = 0; sound < 3; ++sound) {
            const char *key = names[sound + 4u].bytes;
            FloatBits value = {.number = *rolling(game + 0x178c / 4, &key)};
            output[8 + sound] = value.bits;
        }
        const uint32_t *body = (const uint32_t *)(uintptr_t)world[0x19228 / 4];
        while (body) {
            uint32_t id = (uint32_t)(uintptr_t)body == bodies[0] ? 0u : 1u;
            if ((uint32_t)(uintptr_t)body != bodies[id]) {
                fail("Unexpected original remaining body pointer\n");
            }
            output[12] |= 1u << id;
            body = (const uint32_t *)(uintptr_t)body[24];
        }
        print_words(output, 13);
        world[0x191d4 / 4] &= ~UINT32_C(2);
        for (uint32_t index = 0; index < 2; ++index) {
            const char *key = names[index].bytes;
            if (get_body(game, &key)) {
                ((uint32_t *)(uintptr_t)objects[index])[0] = 0;
                remove_object(game, &key);
            }
        }
        while (world[0x19228 / 4]) {
            destroy_body(world, (void *)(uintptr_t)world[0x19228 / 4]);
        }
        if (game[0x17b8 / 4] || world[0x19230 / 4]) {
            fail("Original removal policy fixture leaked a body/map entry\n");
        }
    }
    world_destroy(world);
    semihost(4, "AB game object removal policy complete\n");
}

#endif
