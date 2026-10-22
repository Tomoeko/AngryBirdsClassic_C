#ifndef AB_ORACLE_GAME_OBJECT_REMOVE_H
#define AB_ORACLE_GAME_OBJECT_REMOVE_H

/* Establish a real native allocation/removal fixture before expanding policy
 * comparisons. Original GameObject/RB-node/world allocation and destruction
 * execute unchanged; only the enclosing GameLua's recovered fields are inputs.
 * No attached renderable, animation, joint or teleporter is introduced here. */
static void probe_game_object_remove(void) {
    void *(*allocate)(unsigned) = (void *(*)(unsigned))find_function("_Znwj");
    void (*world_construct)(void *, const void *) =
        (void (*)(void *, const void *))find_function("_ZN7b2WorldC1ERK6b2Vec2");
    void (*world_destroy)(void *) = (void (*)(void *))find_function("_ZN7b2WorldD1Ev");
    void *(*create_body)(void *, const void *) =
        (void *(*)(void *, const void *))find_function(
            "_ZN7b2World10CreateBodyEPK9b2BodyDef");
    void (*object_construct)(void *, void *) =
        (void (*)(void *, void *))find_function("_ZN10GameObjectC1EP7b2World");
    void **(*insert)(void *, const char **) =
        (void **(*)(void *, const char **))find_function(
            "_ZNSt3mapISsP10GameObjectSt4lessISsESaISt4pairIKSsS1_EEEixERS5_");
    void *(*get_body)(void *, const char **) =
        (void *(*)(void *, const char **))find_function("_ZN7GameLua7getBodyERKSs");
    void (*remove_object)(void *, const char **) =
        (void (*)(void *, const char **))find_function("_ZN7GameLua12removeObjectESs");
    static uint32_t game[0x1a00 / 4];
    static uint64_t world_storage[16384];
    static const struct {
        uint32_t length, capacity, references;
        char bytes[8];
    } name = {5, 5, UINT32_MAX, "first"};
    const char *identifier = name.bytes;
    const uint32_t gravity[2] = {0, 0};
    const uint32_t definition[13] = {
        2, 0, 0, 0, 0, 0, 0, 0, 0, UINT32_C(0x101), 1, 0, UINT32_C(0x3f800000)};
    world_construct(world_storage, gravity);
    uint32_t *world = (uint32_t *)world_storage;
    game[0x1784 / 4] = (uint32_t)(uintptr_t)world;
    const uint32_t maps[] = {0x1790, 0x17a8, 0x17c0};
    for (uint32_t map = 0; map < 3; ++map) {
        uint32_t *header = game + maps[map] / 4;
        header[2] = header[3] = (uint32_t)(uintptr_t)header;
    }
    uint32_t *object = allocate(200);
    if (!object) {
        fail("Cannot allocate original removal GameObject\n");
    }
    object_construct(object, world);
    object[0] = 0;
    object[11] = (uint32_t)(uintptr_t)create_body(world, definition);
    if (!object[11]) {
        fail("Cannot create original removal body\n");
    }
    void **entry = insert(game + 0x17a4 / 4, &identifier);
    if (!entry) {
        fail("Cannot allocate original GameObject map node\n");
    }
    *entry = object;
    if (get_body(game, &identifier) != (void *)(uintptr_t)object[11] ||
        game[0x17b8 / 4] != 1 || world[0x19230 / 4] != 1) {
        fail("Original removal fixture lost its native body/map\n");
    }
    semihost(4, "AB_ANDROID_GAME_OBJECT_REMOVE_BOOTSTRAP_V1 1\n");
    uint32_t before[] = {game[0x17b8 / 4], world[0x19230 / 4]};
    print_words(before, 2);
    remove_object(game, &identifier);
    uint32_t after[] = {game[0x17b8 / 4], world[0x19230 / 4],
                        get_body(game, &identifier) != NULL};
    print_words(after, 3);
    if (after[0] || after[1] || after[2]) {
        fail("Original removeObject left a body or physical registry entry\n");
    }
    remove_object(game, &identifier);
    uint32_t repeated[] = {game[0x17b8 / 4], world[0x19230 / 4]};
    print_words(repeated, 2);
    if (repeated[0] || repeated[1]) {
        fail("Original missing-object removal mutated the empty fixture\n");
    }
    world_destroy(world);
    semihost(4, "AB game object removal bootstrap complete\n");
}

#endif
