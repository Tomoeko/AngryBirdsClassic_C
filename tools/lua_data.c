#include "ab/lua_data_json.h"

#include <stdio.h>

typedef struct {
    AbBuffer bytes;
    AbLuaChunk chunk;
    AbLuaData data;
} RecoveredFile;

static bool recover_file(const char *path, size_t *remaining,
                         const AbDataTable *globals, RecoveredFile *output,
                         AbError *error) {
    if (!ab_file_read(path, *remaining, &output->bytes, error)) {
        return false;
    }
    *remaining -= output->bytes.size;
    if (!ab_lua_read((AbSpan){output->bytes.data, output->bytes.size}, *remaining,
                     &output->chunk, error)) {
        return false;
    }
    *remaining -= output->chunk.allocated_bytes;
    if (!ab_lua_data_with_globals(&output->chunk, *remaining, globals, &output->data,
                                  error)) {
        return false;
    }
    *remaining -= output->data.allocated_bytes;
    return true;
}

int main(int argc, char **argv) {
    enum { MAX_FILES = 32 };
    if (argc < 2 || argc > MAX_FILES + 1) {
        fprintf(stderr,
                "Usage: ab_lua_data TARGET_BYTECODE [PREREQUISITE_BYTECODE ...]\n"
                "Prerequisites run left to right, then target; at most 32 files.\n");
        return 2;
    }
    size_t remaining = 32 * 1024 * 1024;
    RecoveredFile files[MAX_FILES] = {0};
    size_t count = (size_t)argc - 1;
    AbError error = {0};
    const AbDataTable *globals = NULL;
    bool okay = true;
    for (size_t index = 0; index < count; ++index) {
        const char *path = index + 1 == count ? argv[1] : argv[index + 2];
        if (!recover_file(path, &remaining, globals, files + index, &error)) {
            fprintf(stderr, "Cannot recover %s: %s\n", path, error.message);
            okay = false;
            break;
        }
        globals = files[index].data.globals;
    }
    if (okay) {
        okay = ab_lua_data_json(&files[count - 1].data, stdout, &error);
        if (!okay) {
            fprintf(stderr, "%s\n", error.message);
        }
    }
    if (fflush(stdout) != 0 || ferror(stdout)) {
        okay = ab_fail(&error, "Cannot flush static data output");
        fprintf(stderr, "%s\n", error.message);
    }
    for (size_t index = count; index > 0; --index) {
        RecoveredFile *file = files + index - 1;
        ab_lua_data_close(&file->data);
        ab_lua_close(&file->chunk);
        ab_buffer_free(&file->bytes);
    }
    return okay ? 0 : 1;
}
