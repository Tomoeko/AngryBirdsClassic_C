#include "ab/lua_chunk.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
    AbSpan input;
    size_t offset;
    size_t memory_limit;
    AbLuaChunk *chunk;
    AbError *error;
} ChunkReader;

static bool take(ChunkReader *reader, size_t size, AbSpan *output) {
    if (!ab_slice(reader->input, reader->offset, size, output)) {
        return ab_fail(reader->error, "Truncated Lua chunk at byte %zu",
                       reader->offset);
    }
    reader->offset += size;
    return true;
}

static bool read_word(ChunkReader *reader, uint32_t *output) {
    AbSpan bytes;
    if (!take(reader, 4, &bytes)) {
        return false;
    }
    *output = ab_u32le(bytes.data);
    return true;
}

static bool read_string(ChunkReader *reader, bool nullable, AbSpan *output) {
    uint32_t size = 0;
    if (!read_word(reader, &size)) {
        return false;
    }
    *output = (AbSpan){0};
    if (!size) {
        return nullable || ab_fail(reader->error, "Lua constant or debug name is null");
    }
    AbSpan bytes;
    if (!take(reader, size, &bytes)) {
        return false;
    }
    if (bytes.data[size - 1] != 0) {
        return ab_fail(reader->error, "Lua string lacks its stored terminator");
    }
    *output = (AbSpan){bytes.data, size - 1};
    return true;
}

static void *allocate_array(ChunkReader *reader, size_t count, size_t item_size) {
    if (!count) {
        return NULL;
    }
    if (count > (reader->memory_limit - reader->chunk->allocated_bytes) / item_size) {
        ab_fail(reader->error, "Lua prototype metadata exceeds memory limit");
        return NULL;
    }
    void *allocation = calloc(count, item_size);
    if (!allocation) {
        ab_fail(reader->error, "Cannot allocate Lua prototype metadata");
        return NULL;
    }
    reader->chunk->allocated_bytes += count * item_size;
    return allocation;
}

static bool read_constants(ChunkReader *reader, AbLuaPrototype *prototype) {
    uint32_t count = 0;
    if (!read_word(reader, &count)) {
        return false;
    }
    if (count > reader->input.size - reader->offset) {
        return ab_fail(reader->error, "Lua constant count exceeds remaining input");
    }
    prototype->constants = allocate_array(reader, count, sizeof(*prototype->constants));
    if (count && !prototype->constants) {
        return false;
    }
    prototype->constant_count = count;
    reader->chunk->constant_count += count;
    for (size_t index = 0; index < count; ++index) {
        AbSpan tag;
        if (!take(reader, 1, &tag)) {
            return false;
        }
        AbLuaConstant *constant = prototype->constants + index;
        constant->kind = (AbLuaKind)tag.data[0];
        if (constant->kind == AB_LUA_BOOLEAN) {
            AbSpan value;
            if (!take(reader, 1, &value)) {
                return false;
            }
            if (value.data[0] > 1) {
                return ab_fail(reader->error, "Invalid Lua boolean constant");
            }
            constant->bits = value.data[0];
        } else if (constant->kind == AB_LUA_NUMBER) {
            if (!read_word(reader, &constant->bits)) {
                return false;
            }
        } else if (constant->kind == AB_LUA_STRING) {
            if (!read_string(reader, false, &constant->string)) {
                return false;
            }
        } else if (constant->kind != AB_LUA_NIL) {
            return ab_fail(reader->error, "Unsupported Lua constant type %u",
                           tag.data[0]);
        }
    }
    return true;
}

static bool read_debug(ChunkReader *reader, const AbLuaPrototype *prototype) {
    uint32_t lines = 0;
    AbSpan ignored;
    if (!read_word(reader, &lines)) {
        return false;
    }
    if ((lines && lines != prototype->instructions.size / 4) ||
        lines > (reader->input.size - reader->offset) / 4 ||
        !take(reader, (size_t)lines * 4, &ignored)) {
        return ab_fail(reader->error, "Invalid Lua debug line count");
    }
    uint32_t locals = 0;
    if (!read_word(reader, &locals) ||
        locals > (reader->input.size - reader->offset) / 12) {
        return ab_fail(reader->error, "Invalid Lua local-variable count");
    }
    for (uint32_t index = 0; index < locals; ++index) {
        AbSpan name;
        uint32_t start = 0;
        uint32_t end = 0;
        if (!read_string(reader, false, &name) || !read_word(reader, &start) ||
            !read_word(reader, &end)) {
            return false;
        }
        if (start > end || end > prototype->instructions.size / 4) {
            return ab_fail(reader->error, "Lua local-variable lifetime exceeds code");
        }
    }
    uint32_t upvalues = 0;
    if (!read_word(reader, &upvalues)) {
        return false;
    }
    if (upvalues && upvalues != prototype->upvalue_count) {
        return ab_fail(reader->error, "Lua debug upvalue count differs from prototype");
    }
    for (uint32_t index = 0; index < upvalues; ++index) {
        AbSpan name;
        if (!read_string(reader, false, &name)) {
            return false;
        }
    }
    return true;
}

static bool validate_code(const AbLuaPrototype *prototype, AbError *error) {
    size_t count = prototype->instructions.size / 4;
    for (size_t index = 0; index < count; ++index) {
        uint32_t word = ab_u32le(prototype->instructions.data + index * 4);
        unsigned opcode = word & 63;
        uint32_t extended = word >> 14;
        if (opcode >= 38) {
            return ab_fail(error, "Unknown Lua opcode at instruction %zu", index);
        }
        if (opcode == 1 || opcode == 5 || opcode == 7) {
            if (extended >= prototype->constant_count) {
                return ab_fail(error, "Lua instruction constant index exceeds table");
            }
            if (opcode != 1 && prototype->constants[extended].kind != AB_LUA_STRING) {
                return ab_fail(error, "Lua global instruction key is not a string");
            }
        } else if (opcode == 36 && extended >= prototype->child_count) {
            return ab_fail(error, "Lua closure index exceeds child prototypes");
        } else if (opcode == 34 && ((word >> 14) & 511) == 0) {
            if (++index == count ||
                !ab_u32le(prototype->instructions.data + index * 4)) {
                return ab_fail(error, "Lua SETLIST lacks its extended list index");
            }
        }
    }
    return true;
}

static bool read_prototype(ChunkReader *reader, AbLuaPrototype *prototype,
                           size_t depth) {
    if (depth >= 64) {
        return ab_fail(reader->error, "Lua prototype nesting exceeds supported depth");
    }
    prototype->file_offset = reader->offset;
    if (!read_string(reader, true, &prototype->source) ||
        !read_word(reader, &prototype->first_line) ||
        !read_word(reader, &prototype->last_line)) {
        return false;
    }
    AbSpan attributes;
    if (!take(reader, 4, &attributes)) {
        return false;
    }
    prototype->upvalue_count = attributes.data[0];
    prototype->parameter_count = attributes.data[1];
    prototype->vararg_flags = attributes.data[2];
    prototype->stack_size = attributes.data[3];
    if (prototype->stack_size < 2 ||
        prototype->parameter_count > prototype->stack_size ||
        prototype->vararg_flags > 7) {
        return ab_fail(reader->error,
                       "Invalid Lua prototype stack or argument metadata");
    }
    uint32_t code_count = 0;
    if (!read_word(reader, &code_count) ||
        code_count > (reader->input.size - reader->offset) / 4 ||
        !take(reader, (size_t)code_count * 4, &prototype->instructions)) {
        return ab_fail(reader->error, "Lua instruction count exceeds input bounds");
    }
    ++reader->chunk->function_count;
    reader->chunk->instruction_count += code_count;
    if (!code_count || !read_constants(reader, prototype)) {
        return ab_fail(reader->error, "Lua prototype has no code or invalid constants");
    }
    uint32_t child_count = 0;
    if (!read_word(reader, &child_count) ||
        child_count > (reader->input.size - reader->offset) / 32) {
        return ab_fail(reader->error, "Lua child count exceeds input bounds");
    }
    prototype->children =
        allocate_array(reader, child_count, sizeof(*prototype->children));
    if (child_count && !prototype->children) {
        return false;
    }
    prototype->child_count = child_count;
    for (size_t index = 0; index < child_count; ++index) {
        if (!read_prototype(reader, prototype->children + index, depth + 1)) {
            return false;
        }
    }
    return read_debug(reader, prototype) && validate_code(prototype, reader->error);
}

static void release_prototype(AbLuaPrototype *prototype) {
    for (size_t index = 0; index < prototype->child_count; ++index) {
        release_prototype(prototype->children + index);
    }
    free(prototype->children);
    free(prototype->constants);
}

void ab_lua_close(AbLuaChunk *chunk) {
    if (chunk) {
        release_prototype(&chunk->root);
        *chunk = (AbLuaChunk){0};
    }
}

bool ab_lua_read(AbSpan input, size_t memory_limit, AbLuaChunk *output,
                 AbError *error) {
    static const uint8_t header[12] = {
        0x1b, 'L', 'u', 'a', 0x51, 0, 1, 4, 4, 4, 4, 0,
    };
    if (!output) {
        return ab_fail(error, "Missing Lua chunk output");
    }
    *output = (AbLuaChunk){0};
    if (!input.data || input.size < sizeof(header) ||
        memcmp(input.data, header, sizeof(header)) != 0) {
        return ab_fail(error,
                       "Unsupported Lua bytecode version or numeric representation");
    }
    output->input = input;
    ChunkReader reader = {input, sizeof(header), memory_limit, output, error};
    bool parsed = read_prototype(&reader, &output->root, 0);
    if (parsed && reader.offset != input.size) {
        parsed = ab_fail(error, "Lua chunk contains trailing bytes");
    }
    if (!parsed) {
        ab_lua_close(output);
        return false;
    }
    return true;
}
