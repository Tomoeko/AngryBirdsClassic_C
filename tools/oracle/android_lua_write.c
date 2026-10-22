/* Isolated calls into the pinned original serializer. No original library is
 * linked into the first-party runtime or tests. All table values are ours. */
#include "android_io.h"
#include <stdbool.h>
#include "../../tests/lua_text_write_cases.h"

extern void *dlopen(const char *path, int mode);
extern void *dlsym(void *library, const char *name);

void __aeabi_memclr8(void *destination, size_t size) {
    volatile uint8_t *bytes = destination;
    for (size_t index = 0; index < size; ++index)
        bytes[index] = 0;
}

static uint8_t serialized[32768];
static size_t serialized_size;

static void stream_write(void *context, const void *data, size_t size) {
    (void)context;
    if (size > sizeof(serialized) - serialized_size)
        system_exit(10);
    const uint8_t *bytes = data;
    for (size_t index = 0; index < size; ++index)
        serialized[serialized_size++] = bytes[index];
}

static void emit_hex(const uint8_t *data, size_t size) {
    static const char digits[] = "0123456789abcdef";
    char text[512];
    while (size) {
        size_t batch = size < sizeof(text) / 2 ? size : sizeof(text) / 2;
        for (size_t index = 0; index < batch; ++index) {
            text[index * 2] = digits[data[index] >> 4];
            text[index * 2 + 1] = digits[data[index] & 15];
        }
        write_bytes(text, batch * 2);
        data += batch;
        size -= batch;
    }
    write_bytes("\n", 1);
}

#define LOAD(function, symbol)                                                         \
    do {                                                                               \
        *(void **)&function = dlsym(library, symbol);                                  \
        if (!function) {                                                               \
            write_bytes(symbol, sizeof(symbol) - 1);                                   \
            system_exit(2);                                                            \
        }                                                                              \
    } while (0)

void android_lua_write_main(void) {
    void *library = dlopen("/data/local/tmp/ab520/libAngryBirdsClassic.so", 2);
    if (!library)
        system_exit(1);
    void *(*new_state)(void);
    void (*set_top)(void *, int), (*create_table)(void *, int, int);
    void (*push_boolean)(void *, int), (*push_number)(void *, float);
    void (*push_string)(void *, const char *), (*set_field)(void *, int, const char *);
    void (*raw_seti)(void *, int, int);
    void (*close_state)(void *), (*state_open)(void *, void *), (*claim)(void *);
    void (*table_open)(void *, void *, int), (*table_close)(void *);
    void (*table_write)(void *, void *, int, int);
    LOAD(new_state, "_Z13luaL_newstatev");
    LOAD(set_top, "_Z10lua_settopP9lua_Statei");
    LOAD(create_table, "_Z15lua_createtableP9lua_Stateii");
    LOAD(push_boolean, "_Z15lua_pushbooleanP9lua_Statei");
    LOAD(push_number, "_Z14lua_pushnumberP9lua_Statef");
    LOAD(push_string, "_Z14lua_pushstringP9lua_StatePKc");
    LOAD(set_field, "_Z12lua_setfieldP9lua_StateiPKc");
    LOAD(raw_seti, "_Z11lua_rawsetiP9lua_Stateii");
    LOAD(close_state, "_Z9lua_closeP9lua_State");
    LOAD(state_open, "_ZN3lua8LuaStateC1EP9lua_State");
    LOAD(claim, "_ZN4lang6Object5claimEv");
    LOAD(table_open, "_ZN3lua8LuaTableC1EPNS_8LuaStateEi");
    LOAD(table_close, "_ZN3lua8LuaTableD1Ev");
    LOAD(table_write, "_ZN3lua8LuaTable5writeERN2io12OutputStreamEib");
    uint32_t wrapper[8] = {0}, table[16] = {0}, vtable[3] = {0};
    uint32_t stream[] = {(uint32_t)(uintptr_t)vtable};
    vtable[2] = (uint32_t)(uintptr_t)stream_write;
    void *parent_state = new_state();
    if (!parent_state)
        system_exit(3);
    state_open(wrapper, parent_state);
    claim(wrapper);
    void *state = (void *)(uintptr_t)wrapper[2];
    write_bytes("AB_ANDROID_LUA_WRITE_V1 256\n", 28);
    for (size_t index = 0; index < LUA_WRITE_CASES; ++index) {
        LuaWriteCase recipe = lua_write_case(index);
        set_top(state, 0);
        create_table(state, 0, 0);
        if (recipe.nested)
            create_table(state, 0, 0);
        switch (recipe.kind) {
            case 0: {
                union {
                    uint32_t word;
                    float number;
                } value = {.word = recipe.number};
                push_number(state, value.number);
                break;
            }
            case 1:
                push_boolean(state, (int)(index % 2));
                break;
            case 2:
                push_string(state, recipe.string);
                break;
            default:
                create_table(state, 0, 0);
                if (recipe.array) {
                    union {
                        uint32_t word;
                        float number;
                    } value = {.word = recipe.number};
                    push_number(state, value.number);
                    raw_seti(state, -2, 1);
                    push_boolean(state, (int)(index % 2));
                    raw_seti(state, -2, 2);
                    if (index % 2) {
                        push_string(state, recipe.string);
                        set_field(state, -2, "tail");
                    }
                }
                break;
        }
        set_field(state, -2, recipe.key);
        if (recipe.nested)
            set_field(state, -2, "outer");
        table_open(table, wrapper, 1);
        serialized_size = 0;
        table_write(table, stream, 0, 0);
        emit_hex(serialized, serialized_size);
        table_close(table);
    }
    close_state(parent_state);
    static const char marker[] = "AB Lua write reference complete\n";
    write_bytes(marker, sizeof(marker) - 1);
    system_exit(0);
}
