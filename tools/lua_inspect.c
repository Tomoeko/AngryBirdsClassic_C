#include "ab/json.h"
#include "ab/lua_chunk.h"

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

static const AbLuaPrototype *find_prototype(const AbLuaPrototype *prototype,
                                            size_t offset) {
    if (prototype->file_offset == offset) {
        return prototype;
    }
    for (size_t child = 0; child < prototype->child_count; ++child) {
        const AbLuaPrototype *found =
            find_prototype(prototype->children + child, offset);
        if (found) {
            return found;
        }
    }
    return NULL;
}

static bool print_prototype(const AbLuaPrototype *prototype) {
    static const char *const operations[] = {
        "MOVE",     "LOADK",     "LOADBOOL", "LOADNIL",  "GETUPVAL", "GETGLOBAL",
        "GETTABLE", "SETGLOBAL", "SETUPVAL", "SETTABLE", "NEWTABLE", "SELF",
        "ADD",      "SUB",       "MUL",      "DIV",      "MOD",      "POW",
        "UNM",      "NOT",       "LEN",      "CONCAT",   "JMP",      "EQ",
        "LT",       "LE",        "TEST",     "TESTSET",  "CALL",     "TAILCALL",
        "RETURN",   "FORLOOP",   "FORPREP",  "TFORLOOP", "SETLIST",  "CLOSE",
        "CLOSURE",  "VARARG",
    };
    printf("prototype %zu parameters %u upvalues %u stack %u lines %u..%u\n",
           prototype->file_offset, prototype->parameter_count, prototype->upvalue_count,
           prototype->stack_size, prototype->first_line, prototype->last_line);
    for (size_t index = 0; index < prototype->constant_count; ++index) {
        const AbLuaConstant *constant = prototype->constants + index;
        printf("K%zu type %u ", index, (unsigned)constant->kind);
        if (constant->kind == AB_LUA_STRING) {
            if (!ab_json_bytes(stdout, constant->string)) {
                return false;
            }
        } else {
            printf("%08" PRIx32, constant->bits);
        }
        putchar('\n');
    }
    for (size_t child = 0; child < prototype->child_count; ++child) {
        printf("child %zu offset %zu\n", child, prototype->children[child].file_offset);
    }
    bool literal = false;
    for (size_t offset = 0; offset < prototype->instructions.size; offset += 4) {
        uint32_t word = ab_u32le(prototype->instructions.data + offset);
        if (literal) {
            printf("%5zu DATA       word=%08" PRIx32 "\n", offset / 4, word);
            literal = false;
            continue;
        }
        uint32_t opcode = word & 63;
        uint32_t first = (word >> 6) & 255;
        uint32_t second = word >> 23;
        uint32_t third = (word >> 14) & 511;
        uint32_t extended = word >> 14;
        const char *name = opcode < sizeof(operations) / sizeof(*operations)
                               ? operations[opcode]
                               : "DATA";
        printf("%5zu %-10s A=%u B=%u C=%u Bx=%u sBx=%d word=%08" PRIx32 "\n",
               offset / 4, name, first, second, third, extended, (int)extended - 131071,
               word);
        literal = opcode == 34 && third == 0;
    }
    return true;
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "Usage: ab_lua_inspect BYTECODE PROTOTYPE_OFFSET\n");
        return 2;
    }
    errno = 0;
    char *end = NULL;
    uintmax_t offset = strtoumax(argv[2], &end, 0);
    if (errno || end == argv[2] || *end || argv[2][0] == '-' || offset > SIZE_MAX) {
        fprintf(stderr, "Invalid prototype offset\n");
        return 2;
    }
    AbBuffer input = {0};
    AbLuaChunk chunk = {0};
    AbError error = {0};
    bool okay =
        ab_file_read(argv[1], 32 * 1024 * 1024, &input, &error) &&
        ab_lua_read((AbSpan){input.data, input.size}, 32 * 1024 * 1024, &chunk, &error);
    if (okay) {
        const AbLuaPrototype *prototype = find_prototype(&chunk.root, (size_t)offset);
        okay = prototype ? print_prototype(prototype)
                         : ab_fail(&error, "Prototype offset is absent");
    }
    if (fflush(stdout) != 0 || ferror(stdout)) {
        okay = ab_fail(&error, "Cannot flush bytecode inspection output");
    }
    if (!okay) {
        fprintf(stderr, "%s\n", error.message);
    }
    ab_lua_close(&chunk);
    ab_buffer_free(&input);
    return okay ? 0 : 1;
}
