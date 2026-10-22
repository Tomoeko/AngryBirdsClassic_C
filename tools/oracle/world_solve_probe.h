#ifndef AB_ORACLE_WORLD_SOLVE_PROBE_H
#define AB_ORACLE_WORLD_SOLVE_PROBE_H

#include "island_contacts.h"
#if defined(AB_ORACLE_WORLD_REPORT)
#include "island_reports.h"
#endif

#include "world_graph_probe.h"

static void probe_world_solve(void) {
    typedef void (*ConstructWorld)(void *, const void *);
    typedef void (*WorldOperation)(void *);
    typedef void (*SolveWorld)(void *, const void *);
    ConstructWorld construct = (ConstructWorld)find_function("_ZN7b2WorldC1ERK6b2Vec2");
    WorldOperation close_world = (WorldOperation)find_function("_ZN7b2WorldD1Ev");
    SolveWorld solve = (SolveWorld)find_function("_ZN7b2World5SolveERK10b2TimeStep");
    const uint32_t *translation = (const uint32_t *)find_symbol("b2_maxTranslation", 1);
    const uint32_t *translation_squared =
        (const uint32_t *)find_symbol("b2_maxTranslationSquared", 1);
    static uint64_t world_storage[16384];
    uint32_t *world = (uint32_t *)world_storage;
    const uint32_t steps[] = {0, 0x3d088889, 0x3c23d70a, 0x3e800000, 0x3f800000};
    uint32_t state = UINT32_C(0x520a011d);
#if defined(AB_ORACLE_WORLD_REPORT)
    uint32_t report_vtable[] = {0, 0, 0,
                                0, 0, (uint32_t)(uintptr_t)record_island_report};
    uint32_t listener[] = {(uint32_t)(uintptr_t)report_vtable};
    semihost(4, "AB_ANDROID_WORLD_REPORT_V1 4096\n");
#else
    semihost(4, "AB_ANDROID_WORLD_SOLVE_V1 4096\n");
#endif
    for (size_t index = 0; index < 4096; ++index) {
        uint32_t controls[12] = {
            (uint32_t)(index % 8),
            steps[(index / 8) % 5],
            random_float(&state, 120, 11, 1),
            random_float(&state, 120, 11, 1),
            (uint32_t)(index % 7 != 0),
            (uint32_t)(index % 3 ? 10 : 0),
            (uint32_t)(1 + index % 10),
            *translation,
            *translation_squared,
            (uint32_t)(index % 8 >= 2 ? (index / 8) % 9 : 0),
            (uint32_t)(index % 2),
            (const uint32_t[]){0, 0x3f000000, 0x3f800000, 0x40000000}[(index / 2) % 4],
        };
        uint32_t bodies[7][64] = {0}, body_pointers[7] = {0};
        OracleIslandContact contacts[8] = {0};
        construct(world, controls + 2);
        world[0x19230 / 4] = controls[0];
        world[0x19228 / 4] = controls[0] ? (uint32_t)(uintptr_t)bodies : 0;
        ((uint8_t *)world)[0x19240] = (uint8_t)controls[4];
        for (size_t body = 0; body < controls[0]; ++body) {
            uint32_t *current = bodies[body];
            current[0] = (uint32_t)((index / 8 + body) % 3);
            current[1] = UINT32_C(0xa5000000) | (random_word(&state) & 0x3f);
            current[2] = random_word(&state);
            for (size_t field = 3; field < 22; ++field) {
                current[field] = random_float(&state, 115, 15, 1);
            }
            current[22] = (uint32_t)(uintptr_t)world;
            current[24] =
                body + 1 < controls[0] ? (uint32_t)(uintptr_t)bodies[body + 1] : 0;
            for (size_t field = 29; field <= 36; ++field) {
                current[field] = random_float(&state, 118, 12, 0);
            }
            current[14] = random_float(&state, 118, 12, 1);
            current[30] = current[0] == 2 ? current[30] : 0;
            current[32] = current[0] == 2 ? current[32] : 0;
            if (!current[0] || index % 4 == 1) {
                for (size_t field = 16; field < 22; ++field) {
                    current[field] = 0;
                }
            }
            current[36] = (const uint32_t[]){0, 0x3e7fffff, 0x3e800000,
                                             0x3f000000}[(index / 7 + body) % 4];
            body_pointers[body] = (uint32_t)(uintptr_t)current;
        }
        world[0x19218 / 4] = controls[9];
        world[0x19214 / 4] = controls[9] ? (uint32_t)(uintptr_t)contacts[0].native : 0;
        for (size_t count = controls[9]; count > 0;) {
            size_t contact = --count;
            OracleIslandContact *current = contacts + contact;
            prepare_island_contact(current, body_pointers, controls[0], index, contact,
                                   &state);
            if (current->data[0] == current->data[1]) {
                current->data[1] = (current->data[0] + 1) % controls[0];
                current->fixtures[1][2] = body_pointers[current->data[1]];
            }
            current->native[1] = random_word(&state);
            current->native[3] = contact + 1 < controls[9]
                                     ? (uint32_t)(uintptr_t)contacts[contact + 1].native
                                     : 0;
            current->fixtures[0][9] = index % 11 == contact ? 0x00010000 : 0;
            connect_world_contact(current, body_pointers);
        }
        print_words(controls, 12);
        for (size_t body = 0; body < controls[0]; ++body) {
            print_world_body(bodies[body]);
        }
        for (size_t contact = 0; contact < controls[9]; ++contact) {
            OracleIslandContact *current = contacts + contact;
            const uint32_t metadata[] = {current->native[1],
                                         current->fixtures[0][9] ? 1u : 0u};
            print_words(metadata, 2);
            print_words(current->data, 23);
        }
        uint32_t timestep[] = {controls[1], 0,           controls[11],
                               controls[6], controls[5], controls[10]};
#if defined(AB_ORACLE_WORLD_REPORT)
        uint32_t contact_pointers[8];
        for (size_t contact = 0; contact < controls[9]; ++contact) {
            contact_pointers[contact] = (uint32_t)(uintptr_t)contacts[contact].native;
        }
        begin_island_reports(body_pointers, controls[0], contact_pointers, controls[9]);
        world[0x19220 / 4] = (uint32_t)(uintptr_t)listener;
#endif
        solve(world, timestep);
        for (size_t body = 0; body < controls[0]; ++body) {
            print_world_body(bodies[body]);
        }
        for (size_t contact = 0; contact < controls[9]; ++contact) {
            print_words(contacts[contact].native + 1, 1);
            print_words(contacts[contact].native + 16, 16);
        }
#if defined(AB_ORACLE_WORLD_REPORT)
        print_island_reports();
#endif
        world[0x19228 / 4] = world[0x19230 / 4] = 0;
        world[0x19214 / 4] = world[0x19218 / 4] = 0;
        close_world(world);
    }
#if defined(AB_ORACLE_WORLD_REPORT)
    semihost(4, "AB world report reference complete\n");
#else
    semihost(4, "AB world solve reference complete\n");
#endif
}

#endif
