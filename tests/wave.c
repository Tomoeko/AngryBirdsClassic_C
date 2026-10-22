#include "ab/wave.h"
#include "ab/hash.h"
#include "ab/zip.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "WAVE check failed at %d: %s\n", __LINE__, #condition);    \
            return false;                                                              \
        }                                                                              \
    } while (0)

static void put_u32(uint8_t *bytes, uint32_t value) {
    for (size_t index = 0; index < 4; ++index) {
        bytes[index] = (uint8_t)(value >> (index * 8));
    }
}

static bool unchanged(AbWave first, AbWave second) {
    return first.pcm.data == second.pcm.data && first.pcm.size == second.pcm.size &&
           first.frames == second.frames && first.sample_rate == second.sample_rate &&
           first.channels == second.channels;
}

static bool edge_checks(void) {
    const uint8_t source[] = {'R', 'I', 'F', 'F', 44,  0,   0,   0,   'W', 'A', 'V',
                              'E', 'f', 'm', 't', ' ', 16,  0,   0,   0,   1,   0,
                              1,   0,   128, 62,  0,   0,   0,   125, 0,   0,   2,
                              0,   16,  0,   'd', 'a', 't', 'a', 8,   0,   0,   0,
                              0,   128, 255, 255, 0,   0,   255, 127};
    AbWave view = {.sample_rate = 99}, sentinel = view;
    CHECK(!ab_wave_read((AbSpan){source, sizeof(source)}, NULL, NULL));
    CHECK(!ab_wave_read((AbSpan){0}, &view, NULL));
    for (size_t size = 0; size < sizeof(source); ++size) {
        CHECK(!ab_wave_read((AbSpan){source, size}, &view, NULL) &&
              unchanged(view, sentinel));
    }
    CHECK(ab_wave_read((AbSpan){source, sizeof(source)}, &view, NULL));
    CHECK(view.sample_rate == 16000 && view.channels == 1 && view.frames == 4 &&
          view.pcm.data == source + 44 && view.pcm.size == 8);
    float samples[] = {2, 3, 4, 5};
    CHECK(!ab_wave_decode(&view, samples, 3, NULL) && samples[0] == 2 &&
          samples[3] == 5);
    CHECK(!ab_wave_decode(&view, NULL, 4, NULL));
    CHECK(ab_wave_decode(&view, samples, 4, NULL));
    const float expected[] = {-1, -0x1p-15f, 0, 0x1.fffcp-1f};
    CHECK(memcmp(samples, expected, sizeof(samples)) == 0);
    view.frames = 5;
    CHECK(!ab_wave_decode(&view, samples, 4, NULL));
    view.frames = 4;
    view.pcm.data = NULL;
    CHECK(!ab_wave_decode(&view, samples, 4, NULL));
    uint8_t bytes[80];
    const size_t offsets[] = {20, 22, 24, 28, 32, 34, 40};
    const uint8_t values[] = {3, 3, 0, 1, 4, 8, 7};
    for (size_t item = 0; item < sizeof(offsets) / sizeof(*offsets); ++item) {
        memcpy(bytes, source, sizeof(source));
        bytes[offsets[item]] = values[item];
        view = sentinel;
        CHECK(!ab_wave_read((AbSpan){bytes, sizeof(source)}, &view, NULL) &&
              unchanged(view, sentinel));
    }
    /* Unknown odd chunks consume their pad, irrespective of its byte value. */
    memcpy(bytes, source, 12);
    memcpy(bytes + 12, "JUNK", 4);
    put_u32(bytes + 16, 1);
    bytes[20] = 8;
    bytes[21] = 127;
    memcpy(bytes + 22, source + 12, sizeof(source) - 12);
    put_u32(bytes + 4, 54);
    CHECK(ab_wave_read((AbSpan){bytes, 62}, &view, NULL) && view.frames == 4);
    /* Data may precede its format. */
    memcpy(bytes, source, 12);
    memcpy(bytes + 12, source + 36, 16);
    memcpy(bytes + 28, source + 12, 24);
    CHECK(ab_wave_read((AbSpan){bytes, 52}, &view, NULL) && view.frames == 4);
    memcpy(bytes, source, sizeof(source));
    memcpy(bytes + 52, "data", 4);
    put_u32(bytes + 56, 0);
    put_u32(bytes + 4, 52);
    CHECK(!ab_wave_read((AbSpan){bytes, 60}, &view, NULL));
    memcpy(bytes, source, 44);
    put_u32(bytes + 4, 36);
    put_u32(bytes + 40, 0);
    CHECK(ab_wave_read((AbSpan){bytes, 44}, &view, NULL) && !view.frames &&
          ab_wave_decode(&view, NULL, 0, NULL));
    return true;
}

static bool digest_matches(AbSpan bytes, const char *expected, AbError *error) {
    uint8_t digest[32];
    char text[65];
    if (!ab_sha256(bytes, digest, error)) {
        return false;
    }
    ab_digest_hex(digest, text);
    return strcmp(text, expected) == 0;
}

static bool compare_asset(const AbZip *archive, const char *path,
                          const uint32_t expected[4], const char *encoded_hash,
                          const char *pcm_hash) {
    AbBuffer bytes = {0};
    AbWave wave;
    AbError error = {0};
    float *samples = NULL;
    bool okay = false;
    const AbZipEntry *entry = ab_zip_find(archive, path);
    if (!entry ||
        !ab_zip_read(archive, (size_t)(entry - archive->entries), 1024 * 1024, &bytes,
                     &error) ||
        bytes.size != expected[0] ||
        !digest_matches((AbSpan){bytes.data, bytes.size}, encoded_hash, &error) ||
        !ab_wave_read((AbSpan){bytes.data, bytes.size}, &wave, &error) ||
        wave.sample_rate != expected[1] || wave.channels != expected[2] ||
        wave.frames != expected[3]) {
        goto release_wave;
    }
    size_t count = wave.pcm.size / 2;
    if (count > SIZE_MAX / sizeof(*samples)) {
        goto release_wave;
    }
    samples = malloc(count * sizeof(*samples));
    if (!samples || !ab_wave_decode(&wave, samples, count, &error) ||
        !digest_matches((AbSpan){(const uint8_t *)samples, count * sizeof(*samples)},
                        pcm_hash, &error)) {
        goto release_wave;
    }
    okay = true;
release_wave:
    if (!okay) {
        fprintf(stderr, "WAVE asset comparison failed for %s: %s\n", path,
                error.message);
    }
    free(samples);
    ab_buffer_free(&bytes);
    return okay;
}

static bool native_checks(const AbZip *archive, FILE *reference) {
    char header[80];
    CHECK(fgets(header, sizeof(header), reference) &&
          strcmp(header, "AB_WAVE_PCM16_REFERENCE_V1 3\n") == 0);
    size_t frames = 0;
    for (size_t item = 0; item < 3; ++item) {
        char path[256], encoded[65], pcm[65];
        uint32_t expected[4];
        CHECK(fscanf(reference, "%255s", path) == 1 && fgetc(reference) == '\n');
        for (size_t word = 0; word < 4; ++word) {
            CHECK(fscanf(reference, "%" SCNx32, expected + word) == 1 &&
                  fgetc(reference) == (word == 3 ? '\n' : ' '));
        }
        CHECK(fscanf(reference, "%64s", encoded) == 1 && fgetc(reference) == '\n' &&
              strlen(encoded) == 64);
        CHECK(fscanf(reference, "%64s", pcm) == 1 && fgetc(reference) == '\n' &&
              strlen(pcm) == 64);
        CHECK(compare_asset(archive, path, expected, encoded, pcm));
        frames += expected[3];
    }
    CHECK(fgets(header, sizeof(header), reference) &&
          strcmp(header, "AB wave pcm16 reference complete\n") == 0 &&
          fgetc(reference) == EOF && !ferror(reference));
    printf(
        "All three APK WAVs match independently converted PCM bits: %zu mono frames\n",
        frames);
    return true;
}

int main(int argc, char **argv) {
    if (argc == 1) {
        return edge_checks() ? 0 : 1;
    }
    if (argc != 3) {
        fprintf(stderr, "Usage: ab_wave_tests [APK REFERENCE_MANIFEST]\n");
        return 2;
    }
    AbBuffer bytes = {0};
    AbZip archive = {0};
    AbError error = {0};
    FILE *reference = fopen(argv[2], "rb");
    bool okay = reference && ab_file_read(argv[1], 64 * 1024 * 1024, &bytes, &error) &&
                ab_zip_open((AbSpan){bytes.data, bytes.size}, &archive, &error) &&
                native_checks(&archive, reference);
    if (reference && fclose(reference) != 0) {
        okay = false;
    }
    ab_zip_close(&archive);
    ab_buffer_free(&bytes);
    return okay ? 0 : 1;
}
