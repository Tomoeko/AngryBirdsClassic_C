#ifndef AB_TEST_SHADER_RESOURCES_FIXTURE_H
#define AB_TEST_SHADER_RESOURCES_FIXTURE_H

#include "ab/hash.h"
#include "ab/zip.h"

#include <stdio.h>
#include <string.h>

static bool native_shader_resources_apk(const char *apk) {
    static const struct {
        const char *path, *hash;
    } resources[] = {
        {"2d-sprite.fx",
         "e3ae8e1aa3cae5ceb8718b95274b3858f1b0a49f541d6fe5228bc0ad6fbc02a8"},
        {"2d-sprite-alpha.fx",
         "6e2875bd28e2bf5bc92754d569482ddaafb95a12c197b3ebb0e77b29304898c2"},
        {"pp.fx", "65a3c844973aec9ef01d1427ccfc920f3316f3b859e03b00a922a75ddc126b15"},
        {"pixelShaders/pp.ps",
         "87e55d0a67a123b18115888c91b8c7f162eed77a61017572fd151b7c958c16ff"},
        {"vertexShaders/diff-tex.vs",
         "df45d1d6f76727e2b949baa3501ba3d516fc5aca51d450d61c28f510bc7e51be"},
        {"commons/defines.sh",
         "90801ef6e29fcdb00d87abf908312f7d13be8c29aaeba1d2c1b4947f865d39da"},
        {"commons/attributes.sh",
         "db495f690cce8c81944e37e2ce7418a2e0261c3a9ae9ae0ceae4bb01e0ec9b61"},
        {"commons/uniforms.sh",
         "5b9f1e9339ede7b396cab495806e364150ea37f68209582c0e0b01b05d79a96a"},
        {"commons/functions_vs.sh",
         "fdbb4d2dcb0d47887aa11f78ec86231e0bd64505bf3c341a63b114dd1fa824b7"},
        {"2d-vertexcolor.fx",
         "21a17e69c4ab47d6aeb7f598dcc16d9efd9aea0c30e58ecc656a25ee1c7f34cf"},
        {"2d-vertexcolor-alpha.fx",
         "a521b5a5795b977ff2934db2f71210b59fa16ba1a6c6e90ad536d38543cbbca5"},
        {"commons/varyings.sh",
         "c6e60a99521ca1ea2ed5c3800ee6a61c6ec9e6d74c574f5c8a9dd071f7b53efd"}};
    AbBuffer bytes = {0}, payload = {0};
    AbZip archive = {0};
    AbError error = {0};
    bool okay = ab_file_read(apk, 64 * 1024 * 1024, &bytes, &error) &&
                ab_zip_open((AbSpan){bytes.data, bytes.size}, &archive, &error);
    for (size_t index = 0; okay && index < sizeof(resources) / sizeof(*resources);
         ++index) {
        char path[160];
        int length = snprintf(path, sizeof(path), "assets/data/shaders/gles2/%s",
                              resources[index].path);
        const AbZipEntry *entry = length > 0 && (size_t)length < sizeof(path)
                                      ? ab_zip_find(&archive, path)
                                      : NULL;
        uint8_t digest[32];
        char hexadecimal[65];
        okay = entry &&
               ab_zip_read(&archive, (size_t)(entry - archive.entries), 65536, &payload,
                           &error) &&
               ab_sha256((AbSpan){payload.data, payload.size}, digest, &error);
        if (okay) {
            ab_digest_hex(digest, hexadecimal);
            okay = strcmp(hexadecimal, resources[index].hash) == 0;
        }
        ab_buffer_free(&payload);
    }
    ab_zip_close(&archive);
    ab_buffer_free(&bytes);
    return okay;
}

#endif
