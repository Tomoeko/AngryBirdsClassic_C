#ifndef AB_LUA_CHUNK_H
#define AB_LUA_CHUNK_H

#include "ab/base.h"

typedef enum {
    AB_LUA_NIL = 0,
    AB_LUA_BOOLEAN = 1,
    AB_LUA_NUMBER = 3,
    AB_LUA_STRING = 4,
} AbLuaKind;

typedef struct {
    AbLuaKind kind;
    uint32_t bits;
    AbSpan string;
} AbLuaConstant;

typedef struct AbLuaPrototype {
    size_t file_offset;
    AbSpan source;
    AbSpan instructions;
    uint32_t first_line;
    uint32_t last_line;
    uint8_t upvalue_count;
    uint8_t parameter_count;
    uint8_t vararg_flags;
    uint8_t stack_size;
    AbLuaConstant *constants;
    size_t constant_count;
    struct AbLuaPrototype *children;
    size_t child_count;
} AbLuaPrototype;

typedef struct {
    /* Original bytes are borrowed. Arrays and the prototype tree are owned. */
    AbSpan input;
    AbLuaPrototype root;
    size_t function_count;
    size_t instruction_count;
    size_t constant_count;
    size_t allocated_bytes;
} AbLuaChunk;

/* Static bytecode structure only: this API never executes Lua instructions. */
bool ab_lua_read(AbSpan input, size_t memory_limit, AbLuaChunk *output, AbError *error);
void ab_lua_close(AbLuaChunk *chunk);

#endif
