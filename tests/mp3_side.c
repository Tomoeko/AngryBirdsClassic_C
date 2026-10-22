#include "ab/mp3.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "MP3 side information failed at %d: %s\n", __LINE__,       \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

static bool words(FILE *file, uint32_t *output, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        CHECK(fscanf(file, "%" SCNx32, output + index) == 1 &&
              fgetc(file) == (index + 1 == count ? '\n' : ' '));
    }
    return true;
}

static bool native_checks(FILE *file) {
    char line[128];
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB_ANDROID_MP3_SIDE_V1\n") == 0);
    size_t frames = 0, granules = 0;
    for (;;) {
        uint32_t header[5];
        CHECK(fgets(line, sizeof(line), file));
        int count =
            sscanf(line, "%" SCNx32 " %" SCNx32 " %" SCNx32 " %" SCNx32 " %" SCNx32,
                   header, header + 1, header + 2, header + 3, header + 4);
        if (count == 1) {
            CHECK(header[0] == frames);
            break;
        }
        CHECK(count == 5 && (header[0] >> 21) == 2047 && ((header[0] >> 19) & 3) == 2 &&
              ((header[0] >> 17) & 3) == 1 && (header[2] == 1 || header[2] == 2) &&
              header[3] == (header[2] == 1 ? 9u : 17u));
        uint32_t raw[17], common[2];
        uint8_t bytes[17];
        CHECK(words(file, raw, header[3]) && words(file, common, 2));
        for (size_t index = 0; index < header[3]; ++index) {
            CHECK(raw[index] <= 255);
            bytes[index] = (uint8_t)raw[index];
        }
        AbMp3SideInfo side;
        CHECK(ab_mp3_side_info((AbSpan){bytes, header[3]}, (uint16_t)header[2], &side,
                               NULL) &&
              side.main_data_begin == common[0] && side.private_bits == common[1]);
        for (size_t channel = 0; channel < side.channels; ++channel) {
            uint32_t expected[16];
            CHECK(words(file, expected, 16));
            const AbMp3Granule *gr = side.channel + channel;
            static const uint32_t rates[] = {22050, 24000, 16000};
            uint32_t rate_index = header[0] >> 10 & 3u;
            uint16_t regions[2];
            CHECK(rate_index < 3 &&
                  ab_mp3_huffman_regions(gr, rates[rate_index], regions, NULL) &&
                  regions[0] == expected[8] && regions[1] == expected[9]);
            CHECK(gr->part23_length == expected[0] && gr->big_values == expected[1] &&
                  gr->scalefactor_compress == expected[2] &&
                  gr->block_type == expected[3] &&
                  gr->mixed_block == (expected[4] != 0) &&
                  gr->window_switch == (expected[3] != 0) &&
                  gr->table_select[0] == expected[5] &&
                  gr->table_select[1] == expected[6] &&
                  gr->table_select[2] == expected[7] &&
                  gr->scalefactor_scale == (expected[10] != 0) &&
                  gr->count1_table == (expected[11] != 0));
            uint32_t ms = ((header[0] >> 6) & 3) == 1 && (header[0] & 32) ? 8u : 0u;
            CHECK(expected[12] == UINT32_C(0x2ea8) - 4u * gr->global_gain + ms);
            for (size_t window = 0; window < 3; ++window) {
                CHECK(gr->window_switch ? expected[13 + window] ==
                                              (uint32_t)gr->subblock_gain[window] * 32
                                        : expected[13 + window] == UINT32_MAX &&
                                              !gr->subblock_gain[window]);
            }
            ++granules;
        }
        ++frames;
    }
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB MP3 side-information reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    printf("Compared %zu native MPEG-2 frames and %zu side-information granules\n",
           frames, granules);
    return true;
}

static bool edge_checks(void) {
    uint8_t bytes[17] = {0};
    AbMp3SideInfo side = {.main_data_begin = 7};
    CHECK(!ab_mp3_side_info((AbSpan){0}, 1, &side, NULL) && side.main_data_begin == 7 &&
          !ab_mp3_side_info((AbSpan){bytes, 9}, 1, NULL, NULL) &&
          !ab_mp3_side_info((AbSpan){bytes, 9}, 0, &side, NULL) &&
          ab_mp3_side_info((AbSpan){bytes, 17}, 2, &side, NULL) && side.channels == 2);
    for (size_t size = 0; size < sizeof(bytes); ++size) {
        CHECK(ab_mp3_side_info((AbSpan){bytes, size}, 1, &side, NULL) == (size == 9));
    }
    bytes[0] = 0xa5;
    bytes[1] = 0x80;
    bytes[8] = 1;
    CHECK(ab_mp3_side_info((AbSpan){bytes, 9}, 1, &side, NULL) &&
          side.main_data_begin == 0xa5 && side.private_bits == 1 &&
          side.channel[0].count1_table && !side.channel[0].scalefactor_scale);
    AbMp3SideInfo prior = side;
    bytes[5] = 1;
    CHECK(!ab_mp3_side_info((AbSpan){bytes, 9}, 1, &side, NULL) &&
          memcmp(&side, &prior, sizeof(side)) == 0);
    bytes[5] = 0;
    bytes[2] = 7;
    bytes[3] = 0xfc;
    CHECK(!ab_mp3_side_info((AbSpan){bytes, 9}, 1, &side, NULL) &&
          memcmp(&side, &prior, sizeof(side)) == 0);
    return true;
}

int main(int argc, char **argv) {
    bool okay = edge_checks();
    for (int index = 1; okay && index < argc; ++index) {
        FILE *file = fopen(argv[index], "rb");
        okay = file && native_checks(file);
        if (file && fclose(file) != 0) {
            okay = false;
        }
    }
    return okay ? 0 : 1;
}
