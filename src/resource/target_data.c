#include "ab/target_data.h"

bool ab_target_data_open(const AbZip *archive, const char *path, AbSpan key,
                         size_t allocation_limit, const AbDataTable *globals,
                         AbTargetData *output, AbError *error) {
    if (!output) {
        return ab_fail(error, "Missing target data output");
    }
    *output = (AbTargetData){0};
    if (!ab_target_packed_read(archive, path, key, allocation_limit, &output->bytes,
                               error)) {
        return false;
    }
    size_t remaining = allocation_limit - output->bytes.size;
    if (!ab_lua_read((AbSpan){output->bytes.data, output->bytes.size}, remaining,
                     &output->chunk, error)) {
        goto release_data;
    }
    if (output->chunk.allocated_bytes > remaining) {
        ab_fail(error, "Target data chunk exceeds allocation limit");
        goto release_data;
    }
    remaining -= output->chunk.allocated_bytes;
    if (!ab_lua_data_with_globals(&output->chunk, remaining, globals, &output->data,
                                  error)) {
        goto release_data;
    }
    return true;

release_data:
    ab_target_data_close(output);
    return false;
}

void ab_target_data_close(AbTargetData *data) {
    if (!data) {
        return;
    }
    ab_lua_data_close(&data->data);
    ab_lua_close(&data->chunk);
    ab_buffer_free(&data->bytes);
    *data = (AbTargetData){0};
}
