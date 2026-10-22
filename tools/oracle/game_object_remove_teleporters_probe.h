#ifndef AB_ORACLE_GAME_OBJECT_REMOVE_TELEPORTERS_H
#define AB_ORACLE_GAME_OBJECT_REMOVE_TELEPORTERS_H

#include "../../tests/game_object_remove_teleporters_cases.h"

typedef struct {
    float x, y;
} RemovalTeleportVector;

static void *removal_teleporter(uint32_t *game, const char **key) {
    uint32_t *(*find)(void *, const char **) =
        (uint32_t *(*)(void *, const char **))find_function(
            "_ZNSt8_Rb_treeISsSt4pairIKSsP16ObjectTeleporterESt10_Select1stIS4_"
            "ESt4lessISsESaIS4_EE4findERS1_");
    uint32_t *node = find(game + 0x17bc / 4, key);
    return node == game + 0x17c0 / 4 ? NULL : (void *)(uintptr_t)node[5];
}

static void removal_teleporter_snapshot(uint32_t *game, const char *const names[4]) {
    void *(*get_body)(void *, const char **) =
        (void *(*)(void *, const char **))find_function("_ZN7GameLua7getBodyERKSs");
    uint32_t *world = (uint32_t *)(uintptr_t)game[0x1784 / 4];
    uint32_t output[7] = {game[0x17b8 / 4],
                          world[0x19230 / 4],
                          0,
                          game[0x17d0 / 4],
                          0,
                          0,
                          (world[0x191d4 / 4] & 2u) != 0};
    for (uint32_t index = 0; index < 3; ++index) {
        const char *key = names[index];
        output[2] |= get_body(game, &key) ? 1u << index : 0u;
        uint32_t *teleporter = removal_teleporter(game, &key);
        if (!teleporter) {
            continue;
        }
        output[4] |= 1u << index;
        const uintptr_t *vtable = (const uintptr_t *)(uintptr_t)teleporter[0];
        int (*remove_on_object_remove)(void *) = (int (*)(void *))vtable[4];
        output[5] |= remove_on_object_remove(teleporter) ? 1u << index : 0u;
    }
    print_words(output, 7);
}

static void probe_game_object_remove_teleporters(void) {
    typedef void (*Construct)(void *);
    void *(*allocate)(unsigned) = (void *(*)(unsigned))find_function("_Znwj");
    Construct state_construct = (Construct)find_function("_ZN3lua8LuaStateC1Ev");
    void (*table_construct)(void *, void *) =
        (void (*)(void *, void *))find_function("_ZN3lua8LuaTableC1EPNS_8LuaStateE");
    Construct table_destroy = (Construct)find_function("_ZN3lua8LuaTableD1Ev");
    Construct claim = (Construct)find_function("_ZN4lang6Object5claimEv");
    Construct release = (Construct)find_function("_ZN4lang6Object7releaseEv");
    void (*world_construct)(void *, const void *) =
        (void (*)(void *, const void *))find_function("_ZN7b2WorldC1ERK6b2Vec2");
    Construct world_destroy = (Construct)find_function("_ZN7b2WorldD1Ev");
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
    void **(*insert_teleporter)(void *, const char **) =
        (void **(*)(void *, const char **))find_function(
            "_ZNSt3mapISsP16ObjectTeleporterSt4lessISsESaISt4pairIKSsS1_EEEixERS5_");
    void (*set_rolling)(void *, const char **, const char **) =
        (void (*)(void *, const char **, const char **))find_function(
            "_ZN7GameLua15setRollingSoundESsSs");
    void (*remove_object)(void *, const char **) =
        (void (*)(void *, const char **))find_function("_ZN7GameLua12removeObjectESs");
    void (*construct_base)(void *, const char **, void *, RemovalTeleportVector,
                           RemovalTeleportVector, float) =
        (void (*)(
            void *, const char **, void *, RemovalTeleportVector, RemovalTeleportVector,
            float))find_function("_ZN16ObjectTeleporterC1ESsP10GameObject6b2Vec2S2_f");
    void (*construct_portal)(
        void *, void *, RemovalTeleportVector, float, float, const char **,
        RemovalTeleportVector, RemovalTeleportVector, float, const char **,
        RemovalTeleportVector, float, float, void *, void *, float, void *,
        void *) = (void (*)(void *, void *, RemovalTeleportVector, float, float,
                            const char **, RemovalTeleportVector, RemovalTeleportVector,
                            float, const char **, RemovalTeleportVector, float, float,
                            void *, void *, float, void *, void *))
        find_function("_ZN22PortalObjectTeleporterC1EP10GameObject6b2Vec2ffSsS2_S2_"
                      "fSsS2_ffPN4game9ResourcesEP9ParticlesfN3lua8LuaTableEP7GameLua");
    uint32_t *wrapper = allocate(20);
    if (!wrapper) {
        fail("Cannot allocate original teleporter Lua wrapper\n");
    }
    state_construct(wrapper);
    claim(wrapper);
    static uint32_t game[0x1a00 / 4];
    static uint64_t world_storage[16384];
    static const struct {
        uint32_t length, capacity, references;
        char bytes[8];
    } strings[] = {{5, 5, UINT32_MAX, "first"},
                   {6, 6, UINT32_MAX, "second"},
                   {5, 5, UINT32_MAX, "third"},
                   {7, 7, UINT32_MAX, "missing"}};
    const char *names[4];
    for (uint32_t index = 0; index < 4; ++index) {
        names[index] = strings[index].bytes;
    }
    const uint32_t gravity[2] = {0, 0};
    const uint32_t definition[13] = {
        2, 0, 0, 0, 0, 0, 0, 0, 0, UINT32_C(0x101), 1, 0, UINT32_C(0x3f800000)};
    world_construct(world_storage, gravity);
    uint32_t *world = (uint32_t *)world_storage;
    game[4] = (uint32_t)(uintptr_t)wrapper;
    game[0x1784 / 4] = (uint32_t)(uintptr_t)world;
    table_construct((uint8_t *)game + 0x17f8, wrapper);
    const uint32_t maps[] = {0x1790, 0x17a8, 0x17c0};
    for (uint32_t map = 0; map < 3; ++map) {
        uint32_t *header = game + maps[map] / 4;
        header[2] = header[3] = (uint32_t)(uintptr_t)header;
    }
    semihost(4, "AB_ANDROID_GAME_OBJECT_REMOVE_TELEPORTERS_V1 1024\n");
    for (uint32_t sample = 0; sample < 1024; ++sample) {
        uint32_t input[5];
        game_object_remove_teleporters_case(sample, input);
        print_words(input, 5);
        for (uint32_t index = 0; index < 3; ++index) {
            const char *key = names[index];
            uint32_t *object = allocate(200);
            if (!object) {
                fail("Cannot allocate original teleporter physical object\n");
            }
            object_construct(object, world);
            object[0] = 0;
            object[11] = (uint32_t)(uintptr_t)create_body(world, definition);
            if (!object[11]) {
                fail("Cannot create original teleporter body\n");
            }
            *insert(game + 0x17a4 / 4, &key) = object;
            set_rolling(game, &key, &key);
            uint32_t empty = object[32];
            object[32] = object[23];
            object[23] = empty;
            uint32_t kind = (input[1] >> (index * 2u)) & 3u;
            if (!kind) {
                continue;
            }
            void *teleporter = allocate(kind == 1 ? 32u : 140u);
            if (!teleporter) {
                fail("Cannot allocate original teleporter\n");
            }
            const RemovalTeleportVector zero = {0, 0};
            if (kind == 1) {
                construct_base(teleporter, &key, object, zero, zero, 0);
            } else {
                construct_portal(teleporter, object, zero, 0, 0, &key, zero, zero, 0,
                                 &key, zero, 0, 0, NULL, NULL, 0,
                                 (uint8_t *)game + 0x17f8, game);
                ((uint8_t *)teleporter)[0x86] = kind == 3;
            }
            *insert_teleporter(game + 0x17bc / 4, &key) = teleporter;
        }
        world[0x191d4 / 4] |= input[3] ? 2u : 0u;
        removal_teleporter_snapshot(game, names);
        for (uint32_t pass = 0; pass < 3; ++pass) {
            uint32_t requested = pass < 2 ? input[2] : (input[2] + 1u) % 4u;
            const char *key = names[requested];
            if (pass == 1 && input[4] && requested < 3 &&
                ((input[1] >> (requested * 2u)) & 3u) == 3) {
                uint8_t *teleporter = removal_teleporter(game, &key);
                if (!teleporter) {
                    fail("Original protected portal was prematurely removed\n");
                }
                teleporter[0x86] = 0;
            }
            remove_object(game, &key);
            removal_teleporter_snapshot(game, names);
        }
        world[0x191d4 / 4] &= ~UINT32_C(2);
        for (uint32_t index = 0; index < 3; ++index) {
            const char *key = names[index];
            uint8_t *teleporter = removal_teleporter(game, &key);
            if (teleporter && ((input[1] >> (index * 2u)) & 3u) >= 2) {
                teleporter[0x86] = 0;
            }
            remove_object(game, &key);
        }
        while (world[0x19228 / 4]) {
            destroy_body(world, (void *)(uintptr_t)world[0x19228 / 4]);
        }
        if (game[0x17b8 / 4] || game[0x17d0 / 4]) {
            fail("Original teleporter fixture retained a registry entry\n");
        }
    }
    table_destroy((uint8_t *)game + 0x17f8);
    world_destroy(world);
    release(wrapper);
    semihost(4, "AB game object removal teleporters reference complete\n");
}

#endif
