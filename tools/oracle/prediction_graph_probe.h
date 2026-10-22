#ifndef AB_ORACLE_PREDICTION_GRAPH_H
#define AB_ORACLE_PREDICTION_GRAPH_H

static inline uint32_t predict_native_pc(void *lua, uint32_t *size) {
    uintptr_t ci = *(const uint32_t *)((const uint8_t *)lua + 0x14);
    uintptr_t base = *(const uint32_t *)((const uint8_t *)lua + 0x28);
    if (ci <= base || (ci - base) % 24u) {
        fail("Missing original impact-prediction caller\n");
    }
    const uint32_t *caller = (const uint32_t *)(ci - 24u);
    const uint32_t *function = (const uint32_t *)(uintptr_t)caller[1];
    if (!function || function[1] != 6 || !function[0]) {
        fail("Missing original impact-prediction function\n");
    }
    const uint8_t *closure = (const uint8_t *)(uintptr_t)function[0];
    const uint32_t *prototype =
        (const uint32_t *)(uintptr_t)((const uint32_t *)closure)[4];
    if (closure[6] || !prototype || (prototype[11] != 115 && prototype[11] != 22)) {
        fail("Unexpected original impact-prediction closure\n");
    }
    *size = prototype[11];
    if (closure[7] != (*size == 115 ? 1u : 5u)) {
        fail("Unexpected original impact-prediction upvalue count\n");
    }
    const uint32_t *code = (const uint32_t *)(uintptr_t)prototype[3];
    uintptr_t begin = prototype[3], saved = caller[3];
    uint32_t first = *size == 115 ? UINT32_C(0x0000409a) : UINT32_C(0x00000044);
    if (!code || code[0] != first || code[*size - 1u] != UINT32_C(0x0080001e) ||
        saved <= begin || saved > begin + *size * 4u || (saved - begin) % 4u) {
        fail("Unexpected original impact-prediction instructions\n");
    }
    return (uint32_t)((saved - begin) / 4u - 1u);
}

static void predict_native_load_function(void *lua, const char *path,
                                         const char *name) {
    char chunk[512];
    int file = system_call(5, (int)(uintptr_t)path, NULL, 0);
    int size = file < 0 ? -1 : system_call(3, file, chunk, sizeof(chunk));
    if (size <= 0 || size == sizeof(chunk) || system_call(6, file, NULL, 0) ||
        damage_lua.load(lua, chunk, (unsigned)size, "original-prediction-helper")) {
        fail("Cannot load original impact-prediction helper\n");
    }
    damage_lua.set_field(lua, -10002, name);
}

static void load_prediction_graph(void *lua) {
    (void)damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    (void)damage_lua.open_math(lua);
    damage_lua.set_top(lua, 0);
    damage_function(lua, -10002, "requireFile", damage_require);
    damage_lua.create_table(lua, 0, 2);
    damage_lua.set_field(lua, -10002, "lua");
    load_damage_chunk(lua, "/data/local/tmp/ab520/collision/predict-class.lua");
    load_damage_chunk(lua, "/data/local/tmp/ab520/collision/predict-vector2.lua");
    predict_native_load_function(
        lua, "/data/local/tmp/ab520/collision/predict-vector-create.lua",
        "vectorCreate");
    predict_native_load_function(
        lua, "/data/local/tmp/ab520/collision/predict-vector-perpendicular.lua",
        "vectorPerpendicular");
    load_damage_chunk(lua, "/data/local/tmp/ab520/collision/physics-queries.lua");
}

#endif
