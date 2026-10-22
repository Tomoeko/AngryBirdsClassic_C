#ifndef AB_ORACLE_PARTICLE_BOX_H
#define AB_ORACLE_PARTICLE_BOX_H

static uint32_t particle_box_sprite[16];

static void *particle_box_sprite_lookup(void *resources, const void *name) {
    (void)resources;
    (void)name;
    return particle_box_sprite;
}

static uint32_t particle_box_sprite_id(const char *name) {
    const char *const names[] = {"RED_FEATHER_1", "RED_FEATHER_2", "RED_FEATHER_3",
                                 "SMOKE_BUFF_1",  "SMOKE_BUFF_2",  "INGAME_BIRDS_1",
                                 "SMOKE_CLOUD_1", "SMOKE_CLOUD_2", "SMOKE_CLOUD_3",
                                 "SMOKE_CLOUD_4", "SMOKE_CLOUD_5", "SMOKE_CLOUD_6"};
    for (uint32_t index = 0; index < 12; ++index) {
        size_t byte = 0;
        while (name[byte] && names[index][byte] == name[byte])
            ++byte;
        if (!name[byte] && !names[index][byte])
            return index + 1;
    }
    fail("Unexpected native particle sprite\n");
}

#if defined(AB_ORACLE_PARTICLE_DRAW_ONLY)
#include "particle_draw_probe.h"
#endif

#if !defined(AB_ORACLE_PARTICLE_DRAW_ONLY)
static void particle_box_snapshot(uint32_t *particles, uint32_t scene, uint32_t phase,
                                  uint32_t seconds) {
#if defined(AB_ORACLE_PARTICLE_LEVEL_ONLY)
    uint32_t begin = particles[0x3c / 4], end = particles[0x40 / 4];
    if (particles[0x24 / 4] != particles[0x28 / 4])
        fail("Level emission changed normal native particle vector\n");
#else
    uint32_t begin = particles[0x24 / 4], end = particles[0x28 / 4];
#endif
    if (end < begin || (end - begin) % 76 || (end - begin) / 76 > 2)
        fail("Unexpected native particle vector size\n");
    uint32_t count = (end - begin) / 76;
    uint32_t header[] = {scene, phase, seconds, count};
    print_words(header, 4);
    const uint32_t *records = (const uint32_t *)(uintptr_t)begin;
    for (uint32_t index = 0; index < count; ++index) {
        const uint32_t *record = records + index * 19;
        uint32_t words[17];
        words[0] = particle_box_sprite_id((const char *)(uintptr_t)record[1]);
        for (size_t word = 0; word < 10; ++word)
            words[1 + word] = record[2 + word];
        words[11] = record[12] == (uint32_t)(uintptr_t)particle_box_sprite;
        words[12] = record[14] & 255;
        for (size_t word = 0; word < 4; ++word)
            words[13 + word] = record[15 + word];
        print_words(words, 17);
    }
}

#endif

static void probe_particle_box(void) {
    initialize_damage_lua();
    void *(*allocate)(unsigned) = (void *(*)(unsigned))find_function("_Znwj");
    void (*construct_state)(void *) =
        (void (*)(void *))find_function("_ZN3lua8LuaStateC1Ev");
    void (*claim)(void *) = (void (*)(void *))find_function("_ZN4lang6Object5claimEv");
    void *(*globals)(void *) =
        (void *(*)(void *))find_function("_ZN3lua8LuaState7globalsEv");
    void (*construct)(void *, void *, void *, void *) =
        (void (*)(void *, void *, void *, void *))find_function(
            "_ZN9ParticlesC1EPN3lua8LuaStateEP7GameLuaPN4game9ResourcesE");
#if defined(AB_ORACLE_PARTICLE_LEVEL_ONLY)
    void (*add)(void *, const void *, float, float, float, float, float, float, int) =
        (void (*)(void *, const void *, float, float, float, float, float, float,
                  int))find_function("_ZN9Particles17addLevelParticlesESsffffffb");
    void (*update)(void *, float) =
        (void (*)(void *, float))find_function("_ZN9Particles20updateLevelParticlesEf");
    double (*next)(void) =
        (double (*)(void))find_function("_ZN4math10RandomUtil6randomEv");
#else
    void (*add)(void *, const void *, float, float, float, float, float, float, int,
                int) =
        (void (*)(void *, const void *, float, float, float, float, float, float, int,
                  int))find_function("_ZN9Particles12addParticlesESsffffffbb");
    void (*update)(void *, float, int) =
        (void (*)(void *, float, int))find_function("_ZN9Particles6updateEfb");
#endif
    void (*clear)(void *) = (void (*)(void *))find_function("_ZN9Particles8clearAllEv");
    void (*seed)(uint32_t) =
        (void (*)(uint32_t))find_function("_ZN4math10RandomUtil7setSeedEj");
    uint32_t *wrapper = allocate(20), *particles = allocate(0x70);
    if (!wrapper || !particles)
        fail("Cannot allocate original particle owners\n");
    construct_state(wrapper);
    claim(wrapper);
    void *lua = (void *)(uintptr_t)wrapper[2];
    load_damage_chunk(lua, "/data/local/tmp/ab520/particle-box-data.lua");
    /* Frame changes also perform the real nonvirtual composite lookup. Its
     * SpriteEntry tree at +0x4cc is explicitly empty in this prepared resource. */
    uint32_t resources[0x500 / 4] = {0}, resource_methods[3] = {0};
    resource_methods[2] = (uint32_t)(uintptr_t)particle_box_sprite_lookup;
    resources[0] = (uint32_t)(uintptr_t)resource_methods;
    construct(particles, wrapper, globals(wrapper), resources);
#if defined(AB_ORACLE_PARTICLE_LEVEL_ONLY) && defined(AB_ORACLE_PARTICLE_DRAW_ONLY)
    semihost(4, "AB_ANDROID_PARTICLE_LEVEL_DRAW_V1 3 6\n");
    const uint32_t scenes = 3;
#elif defined(AB_ORACLE_PARTICLE_LEVEL_ONLY)
    semihost(4, "AB_ANDROID_PARTICLE_LEVEL_V1 3 6\n");
    const uint32_t scenes = 3;
#elif defined(AB_ORACLE_PARTICLE_MENU_ONLY)
    semihost(4, "AB_ANDROID_PARTICLE_MENU_DRAW_V1 3 6\n");
    const uint32_t scenes = 3;
#elif defined(AB_ORACLE_PARTICLE_DRAW_ONLY)
    semihost(4, "AB_ANDROID_PARTICLE_DRAW_V1 3 6\n");
    const uint32_t scenes = 3;
#elif defined(AB_ORACLE_PARTICLE_COMMON_ONLY)
    semihost(4, "AB_ANDROID_PARTICLE_COMMON_V1 3 6\n");
    const uint32_t scenes = 3;
#else
    semihost(4, "AB_ANDROID_PARTICLE_BOX_V1 2 6\n");
    const uint32_t scenes = 2;
#endif
    const uint32_t durations[] = {0, 0, 0x3e800000, 0x3f400000, 0, 0x3e000000};
    static const struct {
        uint32_t length, capacity, references;
        char bytes[24];
    } names[] = {{7, 7, UINT32_MAX, "redBuff"},
                 {21, 21, UINT32_MAX, "smokeBuffBirdsDefault"},
                 {9, 9, UINT32_MAX, "smokeBuff"}};
    const uint32_t animated_durations[] = {0,          0x3d75c28f, 0x3d75c28f,
                                           0x3df5c28f, 0x3de147ae, 0x3c23d70a};
    for (uint32_t scene = 0; scene < scenes; ++scene) {
        clear(particles);
        seed(scene);
        const char *name = names[scene].bytes;
#if defined(AB_ORACLE_PARTICLE_MENU_ONLY)
        const float menu_scales[] = {1.375f, 0.625f, -1.25f};
        damage_lua.push_number(lua, menu_scales[scene]);
        damage_lua.set_field(lua, -10002, "g_menu_particle_scale");
#endif
#if defined(AB_ORACLE_PARTICLE_LEVEL_ONLY)
        add(particles, &name, 0, 10, -20, 4, 6, 0.3f, 0);
        add(particles, &name, 1.5f, 10, -20, 4, 6, 0.3f, 0);
#else
        add(particles, &name, 1.5f, 10, -20, 4, 6, 0.3f, 0,
#if defined(AB_ORACLE_PARTICLE_MENU_ONLY)
            1
#else
            0
#endif
        );
#endif
        for (uint32_t phase = 0; phase < 6; ++phase) {
            uint32_t duration =
                scene == 2 ? animated_durations[phase] : durations[phase];
            FloatBits seconds = {.bits = duration};
            if (phase) {
#if defined(AB_ORACLE_PARTICLE_LEVEL_ONLY)
                update(particles, seconds.number);
#else
                update(particles, seconds.number,
#if defined(AB_ORACLE_PARTICLE_MENU_ONLY)
                       1
#else
                       0
#endif
                );
#endif
            }
#if defined(AB_ORACLE_PARTICLE_DRAW_ONLY)
            particle_draw_snapshot(particles, scene, phase, duration);
#else
            particle_box_snapshot(particles, scene, phase, duration);
#endif
        }
#if defined(AB_ORACLE_PARTICLE_LEVEL_ONLY)
        FloatBits following = {.number = (float)next()};
        print_words(&following.bits, 1);
#endif
    }
#if defined(AB_ORACLE_PARTICLE_LEVEL_ONLY) && defined(AB_ORACLE_PARTICLE_DRAW_ONLY)
    semihost(4, "AB particle level draw reference complete\n");
#elif defined(AB_ORACLE_PARTICLE_LEVEL_ONLY)
    semihost(4, "AB particle level reference complete\n");
#elif defined(AB_ORACLE_PARTICLE_MENU_ONLY)
    semihost(4, "AB particle menu draw reference complete\n");
#elif defined(AB_ORACLE_PARTICLE_DRAW_ONLY)
    semihost(4, "AB particle draw reference complete\n");
#elif defined(AB_ORACLE_PARTICLE_COMMON_ONLY)
    semihost(4, "AB particle common reference complete\n");
#else
    semihost(4, "AB particle box reference complete\n");
#endif
}

#endif
