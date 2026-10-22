#include "ab/mp3.h"

#include <string.h>

typedef struct {
    size_t size;
    uint32_t rate;
    uint16_t channels;
    bool protected;
} Mp3Frame;

static bool leading_tag(AbSpan input, size_t *size, AbError *error) {
    *size = 0;
    if (input.size < 3 || memcmp(input.data, "ID3", 3) != 0) {
        return true;
    }
    if (input.size < 10 || input.data[3] < 2 || input.data[3] > 4 ||
        input.data[4] == 255) {
        return ab_fail(error, "Invalid MP3 ID3v2 header");
    }
    unsigned version = input.data[3];
    unsigned allowed = version == 2 ? 0xc0u : version == 3 ? 0xe0u : 0xf0u;
    if (input.data[5] & ~allowed) {
        return ab_fail(error, "Invalid MP3 ID3v2 flags");
    }
    size_t payload = 0;
    for (size_t index = 6; index < 10; ++index) {
        if (input.data[index] & 128) {
            return ab_fail(error, "Invalid MP3 ID3v2 synchsafe size");
        }
        payload = payload * 128 + input.data[index];
    }
    if (payload > input.size - 10) {
        return ab_fail(error, "Truncated MP3 ID3v2 tag");
    }
    size_t end = payload + 10;
    if (version == 4 && (input.data[5] & 16)) {
        if (input.size - end < 10 || memcmp(input.data + end, "3DI", 3) != 0 ||
            memcmp(input.data + end + 3, input.data + 3, 7) != 0) {
            return ab_fail(error, "Invalid MP3 ID3v2 footer");
        }
        end += 10;
    }
    *size = end;
    return true;
}

static bool frame_header(AbSpan input, Mp3Frame *output, AbError *error) {
    static const uint16_t bitrates[] = {0,  8,  16, 24,  32,  40,  48, 56,
                                        64, 80, 96, 112, 128, 144, 160};
    static const uint32_t rates[] = {22050, 24000, 16000};
    if (input.size < 4) {
        return ab_fail(error, "Truncated MP3 frame header");
    }
    uint32_t bits = ab_u32be(input.data);
    unsigned bitrate = (bits >> 12) & 15;
    unsigned rate = (bits >> 10) & 3;
    if ((bits >> 21) != 2047 || ((bits >> 19) & 3) != 2 || ((bits >> 17) & 3) != 1 ||
        !bitrate || bitrate == 15 || rate == 3 || (bits & 3) == 2) {
        return ab_fail(error, "Unsupported or invalid MPEG-2 Layer III header");
    }
    Mp3Frame frame = {.rate = rates[rate],
                      .channels = ((bits >> 6) & 3) == 3 ? 1 : 2,
                      .protected = (bits & UINT32_C(0x10000)) == 0};
    frame.size = UINT32_C(72000) * bitrates[bitrate] / frame.rate + ((bits >> 9) & 1);
    size_t side_information = frame.channels == 1 ? 9 : 17;
    if (frame.size > input.size ||
        frame.size < 4u + (frame.protected ? 2u : 0u) + side_information) {
        return ab_fail(error, "Truncated or undersized MP3 frame");
    }
    *output = frame;
    return true;
}

static bool xing_metadata(AbSpan first, const Mp3Frame *frame, AbMp3 *stream,
                          AbError *error) {
    size_t cursor =
        4u + (frame->protected ? 2u : 0u) + (frame->channels == 1 ? 9u : 17u);
    if (first.size - cursor < 4 || (memcmp(first.data + cursor, "Xing", 4) != 0 &&
                                    memcmp(first.data + cursor, "Info", 4) != 0)) {
        return true;
    }
    if (first.size - cursor < 8) {
        return ab_fail(error, "Truncated MP3 Xing header");
    }
    uint32_t flags = ab_u32be(first.data + cursor + 4);
    cursor += 8;
    if (flags & ~UINT32_C(15)) {
        return ab_fail(error, "Unsupported MP3 Xing flags");
    }
    uint32_t count = 0;
    for (unsigned flag = 1; flag <= 8; flag *= 2) {
        if (!(flags & flag)) {
            continue;
        }
        size_t size = flag == 4 ? 100 : 4;
        if (size > first.size - cursor) {
            return ab_fail(error, "Truncated MP3 Xing fields");
        }
        if (flag == 1) {
            count = ab_u32be(first.data + cursor);
        }
        cursor += size;
    }
    if (!(flags & 1) || !count || count != stream->frame_count - 1) {
        return ab_fail(error, "MP3 Xing count differs from complete frame walk");
    }
    stream->has_xing = true;
    stream->sample_frames = (uint64_t)count * 576;
    if (first.size - cursor < 4 || memcmp(first.data + cursor, "LAME", 4) != 0) {
        return true;
    }
    if (first.size - cursor < 36) {
        return ab_fail(error, "Truncated MP3 LAME metadata");
    }
    uint32_t packed = (uint32_t)first.data[cursor + 21] << 16 |
                      (uint32_t)first.data[cursor + 22] << 8 | first.data[cursor + 23];
    stream->encoder_delay = packed >> 12;
    stream->encoder_padding = packed & 4095;
    uint32_t trim = stream->encoder_delay + stream->encoder_padding;
    if (trim > stream->sample_frames) {
        return ab_fail(error, "MP3 LAME delay/padding exceeds audio samples");
    }
    stream->has_lame = true;
    stream->sample_frames -= trim;
    return true;
}

bool ab_mp3_frame_info(AbSpan input, AbMp3FrameInfo *output, AbError *error) {
    if (!output || !input.data) {
        return ab_fail(error, "Missing MPEG-2 frame input or output");
    }
    Mp3Frame frame = {0};
    if (!frame_header(input, &frame, error)) {
        return false;
    }
    size_t offset = 4u + (frame.protected ? 2u : 0u);
    size_t side_bytes = frame.channels == 1 ? 9u : 17u;
    uint16_t crc = 0;
    if (frame.protected) {
        crc = (uint16_t)((uint16_t)input.data[4] << 8 | input.data[5]);
    }
    AbMp3FrameInfo info = {.frame = {input.data, frame.size},
                           .side_information = {input.data + offset, side_bytes},
                           .main_data = {input.data + offset + side_bytes,
                                         frame.size - offset - side_bytes},
                           .header = ab_u32be(input.data),
                           .sample_rate = frame.rate,
                           .channels = frame.channels,
                           .crc = crc,
                           .has_crc = frame.protected};
    if (!ab_mp3_side_info(info.side_information, info.channels, &info.side, error)) {
        return false;
    }
    *output = info;
    return true;
}

bool ab_mp3_read(AbSpan input, AbMp3 *output, AbError *error) {
    if (!output || !input.data || !input.size) {
        return ab_fail(error, "Missing MP3 input or output");
    }
    size_t offset;
    if (!leading_tag(input, &offset, error)) {
        return false;
    }
    AbMp3 stream = {.leading_tag = {input.data, offset}};
    size_t end = input.size;
    if (end - offset >= 128 && memcmp(input.data + end - 128, "TAG", 3) == 0) {
        end -= 128;
        stream.trailing_tag = (AbSpan){input.data + end, 128};
    }
    stream.frames = (AbSpan){input.data + offset, end - offset};
    Mp3Frame first = {0};
    while (offset < end) {
        Mp3Frame frame;
        if (!frame_header((AbSpan){input.data + offset, end - offset}, &frame, error)) {
            return false;
        }
        if (!stream.frame_count) {
            first = frame;
            stream.sample_rate = frame.rate;
            stream.channels = frame.channels;
        } else if (frame.rate != stream.sample_rate ||
                   frame.channels != stream.channels) {
            return ab_fail(error, "Changing MP3 format is unsupported");
        }
        ++stream.frame_count;
        if (stream.sample_frames > UINT64_MAX - 576) {
            return ab_fail(error, "MP3 sample count overflow");
        }
        stream.sample_frames += 576;
        offset += frame.size;
    }
    if (!stream.frame_count) {
        return ab_fail(error, "MP3 contains no complete audio frames");
    }
    if (!xing_metadata((AbSpan){stream.frames.data, first.size}, &first, &stream,
                       error)) {
        return false;
    }
    *output = stream;
    return true;
}
