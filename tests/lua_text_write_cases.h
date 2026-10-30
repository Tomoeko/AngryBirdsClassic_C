#ifndef AB_LUA_TEXT_WRITE_CASES_H
#define AB_LUA_TEXT_WRITE_CASES_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

static const char *const lua_write_keys[] = {
    "sample", "and", "a-b", "_G", "this", "", "1value", "_value", "goto", "end"};
static const uint32_t lua_write_numbers[] = {
    0,          0x80000000, 0x3dcccccd, 0x3f800001, 0x7f7fffff, 0x00800000,
    0x00000001, 0xbf800000, 0x4b800001, 0x3eaaaaab, 0x42f6e979, 0xc1200000};
static const char *const lua_write_strings[] = {"",
                                                "plain",
                                                "quote\"'\\",
                                                "\a\b\t\n\v\f\r",
                                                "\001"
                                                "9\002a\177"
                                                "8",
                                                "\303\251\377"
                                                "9",
                                                "line one\nline two",
                                                "[]{}=,;"};

enum { LUA_WRITE_CASES = 256 };

/* Own controlled serialization cases, shared with the isolated native probe.
 * These are generated test values, never recovered game implementation. */
typedef struct {
    const char *key, *string;
    uint32_t number;
    unsigned kind;
    bool nested, array;
} LuaWriteCase;

static LuaWriteCase lua_write_case(size_t index) {
    return (LuaWriteCase){.key = lua_write_keys[index % 10],
                          .string = lua_write_strings[(index / 3) % 8],
                          .number = lua_write_numbers[(index + index / 10) % 12],
                          .kind = (unsigned)((index / 10) % 4),
                          .nested = index >= 128,
                          .array = index >= 224};
}

#endif
