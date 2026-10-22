#ifndef AB_TEST_CAMERA_FOLLOW_END_SEQUENCE_MAIN_H
#define AB_TEST_CAMERA_FOLLOW_END_SEQUENCE_MAIN_H

static bool apk_end_reference(const char *apk, const char *trace) {
    AbBuffer bytes = {0};
    AbZip archive = {0};
#if defined(AB_TEST_CAMERA_SHOT_SEQUENCE)
    enum { SOURCE_COUNT = 2 };
#else
    enum { SOURCE_COUNT = 1 };
#endif
    AbTargetData data[SOURCE_COUNT] = {0};
    AbCameraSequence sequence[SOURCE_COUNT] = {0};
    uint8_t key[32] = {0};
    FILE *file = NULL;
    bool okay = false;
    AbError error = {0};
    const char *paths[] = {
        "assets/data/scripts/cameraSequences/FollowShotBirdEndCameraSequence.lua",
        "assets/data/scripts/cameraSequences/FollowShotBirdCameraSequence.lua"};
    if (!ab_file_read(apk, 64 * 1024 * 1024, &bytes, &error) ||
        !ab_zip_open((AbSpan){bytes.data, bytes.size}, &archive, &error) ||
        !ab_target_resource_key(&archive, 16 * 1024 * 1024, key, &error)) {
        goto release_assets;
    }
    for (size_t index = 0; index < SOURCE_COUNT; ++index) {
        if (!ab_target_data_open(&archive, paths[index], (AbSpan){key, sizeof(key)},
                                 1024 * 1024, NULL, data + index, &error)) {
            goto release_assets;
        }
        AbDataValue name = {.kind = AB_DATA_STRING,
                            .string = {(const uint8_t *)"sequence", 8}};
        const AbDataValue *source = ab_data_get(data[index].data.globals, name);
        if (!source || source->kind != AB_DATA_TABLE ||
            !ab_camera_sequence_read(source->table, 65536, sequence + index, &error)) {
            goto release_assets;
        }
    }
    file = fopen(trace, "rb");
    okay = file && reference(file, sequence);

release_assets:
    memset(key, 0, sizeof(key));
    if (file && fclose(file))
        okay = false;
    for (size_t index = 0; index < SOURCE_COUNT; ++index) {
        ab_camera_sequence_free(sequence + index);
        ab_target_data_close(data + index);
    }
    ab_zip_close(&archive);
    ab_buffer_free(&bytes);
    if (!okay && error.message[0])
        fprintf(stderr, "%s\n", error.message);
    return okay;
}

int main(int argc, char **argv) {
    if (argc != 3)
        return 2;
    return apk_end_reference(argv[1], argv[2]) ? 0 : 1;
}

#endif
