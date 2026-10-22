#ifndef AB_ORACLE_SCENE_ID_H
#define AB_ORACLE_SCENE_ID_H

#include "../../tests/scene_id_cases.h"

static void probe_scene_id(void) {
    initialize_damage_lua();
    uint32_t (*next)(void) = (uint32_t (*)(void))find_function("_ZN3gfx5Scene5newIdEv");
    void (*push_integer)(void *, int32_t) =
        (void (*)(void *, int32_t))find_function("_Z15lua_pushintegerP9lua_Statei");
    /* The complete original ARM method contains one PC-relative counter word.
     * Control only that global in this isolated process; restore it afterward. */
    const uint32_t *code = (const uint32_t *)(uintptr_t)next;
    uint32_t *counter = (uint32_t *)((uintptr_t)code + 12 + code[6]);
    uint32_t saved = *counter;
    void *lua = damage_lua.new_state();
    if (!lua)
        fail("Cannot create scene identifier numeric observer\n");
    semihost(4, "AB_ANDROID_SCENE_ID_V1 4096\n");
    for (uint32_t sample = 0; sample < 4096; ++sample) {
        uint32_t input = scene_id_case(sample);
        *counter = input;
        uint32_t id = next();
        int32_t signed_id;
        /* The target and oracle use the pinned ARM two's-complement ABI. */
        signed_id = (int32_t)id;
        push_integer(lua, signed_id);
        FloatBits number = {.number = damage_lua.to_number(lua, 1)};
        uint32_t output[] = {id, number.bits, *counter};
        print_words(&input, 1);
        print_words(output, 3);
        damage_lua.set_top(lua, 0);
    }
    *counter = saved;
    damage_lua.close_state(lua);
    semihost(4, "AB scene identifier reference complete\n");
}

#endif
