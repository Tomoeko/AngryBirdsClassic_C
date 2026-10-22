#include "android_io.h"

extern void *dlopen(const char *path, int mode);
extern void *dlsym(void *library, const char *name);

typedef void LuaState;
static LuaState *(*new_state)(void);
static int (*load_buffer)(LuaState *, const char *, unsigned, const char *);
static int (*protected_call)(LuaState *, int, int, int);
static void (*close_state)(LuaState *);
static int (*get_top)(LuaState *);
static void (*set_top)(LuaState *, int);
static void (*push_nil)(LuaState *);
static int (*next_value)(LuaState *, int);
static int (*value_type)(LuaState *, int);
static float (*to_number)(LuaState *, int);
static int (*to_boolean)(LuaState *, int);
static const char *(*to_string)(LuaState *, int, unsigned *);
static char input[1024 * 1024];
static char output[32768];
static size_t output_count;

static void emit(char byte) {
    output[output_count++] = byte;
    if (output_count == sizeof(output)) {
        write_bytes(output, output_count);
        output_count = 0;
    }
}

static void hex_word(uint32_t word) {
    static const char digits[] = "0123456789abcdef";
    for (unsigned index = 0; index < 8; ++index) {
        emit(digits[(word >> (28 - index * 4)) & 15]);
    }
}

static void export_value(LuaState *state, int index, unsigned depth) {
    if (depth >= 64) {
        system_exit(10);
    }
    switch (value_type(state, index)) {
        case 0:
            emit('N');
            break;
        case 1:
            emit(to_boolean(state, index) ? 'Y' : 'F');
            break;
        case 3: {
            union {
                float number;
                uint32_t bits;
            } value = {.number = to_number(state, index)};
            emit('D');
            hex_word(value.bits);
            break;
        }
        case 4: {
            unsigned length = 0;
            const char *string = to_string(state, index, &length);
            if ((!string && length) || length > sizeof(input)) {
                system_exit(11);
            }
            emit('S');
            static const char digits[] = "0123456789abcdef";
            for (unsigned offset = 0; offset < length; ++offset) {
                unsigned byte = (unsigned char)string[offset];
                emit(digits[byte >> 4]);
                emit(digits[byte & 15]);
            }
            break;
        }
        case 5: {
            emit('T');
            emit('\n');
            push_nil(state);
            while (next_value(state, index)) {
                export_value(state, get_top(state) - 1, depth + 1);
                export_value(state, get_top(state), depth + 1);
                set_top(state, get_top(state) - 1);
            }
            emit('E');
            break;
        }
        default:
            system_exit(12);
    }
    emit('\n');
}

#define LOAD(function, symbol)                                                         \
    do {                                                                               \
        *(void **)&function = dlsym(library, symbol);                                  \
        if (!function) {                                                               \
            system_exit(2);                                                            \
        }                                                                              \
    } while (0)

void android_lua_data_main(void) {
    void *library = dlopen("/data/local/tmp/ab520/libAngryBirdsClassic.so", 2);
    if (!library) {
        system_exit(1);
    }
    LOAD(new_state, "_Z13luaL_newstatev");
    LOAD(load_buffer, "_Z15luaL_loadbufferP9lua_StatePKcjS2_");
    LOAD(protected_call, "_Z9lua_pcallP9lua_Stateiii");
    LOAD(close_state, "_Z9lua_closeP9lua_State");
    LOAD(get_top, "_Z10lua_gettopP9lua_State");
    LOAD(set_top, "_Z10lua_settopP9lua_Statei");
    LOAD(push_nil, "_Z11lua_pushnilP9lua_State");
    LOAD(next_value, "_Z8lua_nextP9lua_Statei");
    LOAD(value_type, "_Z8lua_typeP9lua_Statei");
    LOAD(to_number, "_Z12lua_tonumberP9lua_Statei");
    LOAD(to_boolean, "_Z13lua_tobooleanP9lua_Statei");
    LOAD(to_string, "_Z13lua_tolstringP9lua_StateiPj");
    uint32_t controls = 0;
    __asm__ volatile("vmsr fpscr, %0" : : "r"(controls) : "memory");
    const char *path = "/data/local/tmp/ab520/serialized.lua";
    int file = system_call(5, (int)(uintptr_t)path, NULL, 0);
    if (file < 0) {
        system_exit(3);
    }
    size_t length = 0;
    while (length < sizeof(input)) {
        int count = system_call(3, file, input + length, sizeof(input) - length);
        if (count < 0) {
            system_exit(4);
        }
        if (!count) {
            break;
        }
        length += (size_t)count;
    }
    if (length == sizeof(input) || system_call(6, file, NULL, 0) != 0) {
        system_exit(5);
    }
    LuaState *state = new_state();
    if (!state || load_buffer(state, input, (unsigned)length, "serialized") ||
        protected_call(state, 0, 0, 0)) {
        system_exit(6);
    }
    export_value(state, -10002, 0);
    close_state(state);
    write_bytes(output, output_count);
    static const char marker[] = "AB serialized reference complete\n";
    write_bytes(marker, sizeof(marker) - 1);
    system_exit(0);
}
