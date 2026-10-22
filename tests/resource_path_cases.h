#ifndef AB_TEST_RESOURCE_PATH_CASES_H
#define AB_TEST_RESOURCE_PATH_CASES_H

#include <stddef.h>
#include <stdint.h>

static size_t resource_path_case(uint32_t index, char output[256]) {
    static const char *const prefixes[] = {"",
                                           "a/",
                                           "a/b/",
                                           "assets/data/images/base/",
                                           "assets/data/images/480x320/",
                                           "assets/data/images/1024x768/",
                                           "assets/data/images/themes_android/",
                                           "path with spaces/"};
    static const char *const names[] = {"SPRITE",
                                        "bird",
                                        "bird-long",
                                        "Bird",
                                        "pig",
                                        "a.b",
                                        "a..b",
                                        "a.b.c",
                                        ".hidden",
                                        "..hidden",
                                        "...hidden",
                                        "file.",
                                        "name with spaces",
                                        "under_score",
                                        "hyphen-name",
                                        "001",
                                        "0",
                                        "UPPER",
                                        "lower",
                                        "MiXeD",
                                        "leading space",
                                        " trailing",
                                        "trailing ",
                                        "a..",
                                        "a...",
                                        "file.dat",
                                        "file.xml",
                                        "file.png",
                                        "name-extended",
                                        "background_layer_01",
                                        "INGAME_BLOCKS_1",
                                        "THEME_01_COMPOSPRITES"};
    static const char *const suffixes[] = {
        "",   ".dat", ".DAT", ".xml",        ".png",  ".pvr", ".tar.gz", ".",
        "..", ".x",   ".x.y", "_suffix.dat", "..dat", ".lua", "-tail",   " "};
    const char *parts[] = {prefixes[(index >> 9) & 7u], names[(index >> 4) & 31u],
                           suffixes[index & 15u]};
    size_t size = 0;
    for (size_t part = 0; part < 3; ++part) {
        for (size_t byte = 0; parts[part][byte]; ++byte) {
            output[size++] = parts[part][byte];
        }
    }
    output[size] = 0;
    return size;
}

static uint32_t resource_path_hash(const char *path, size_t length) {
    uint32_t hash = UINT32_C(2166136261);
    for (size_t index = 0; index < length; ++index) {
        hash = (hash ^ (uint8_t)path[index]) * UINT32_C(16777619);
    }
    return hash;
}

#endif
