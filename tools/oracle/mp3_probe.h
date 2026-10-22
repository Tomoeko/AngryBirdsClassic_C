#ifndef AB_ORACLE_MP3_H
#define AB_ORACLE_MP3_H

static uint8_t mp3_reference_input[512 * 1024];
static uint8_t mp3_reference_output[8192];

static void probe_mp3(void) {
    int (*initialize)(void) = (int (*)(void))find_function("mpg123_init");
    void *(*create)(const char *, int *) =
        (void *(*)(const char *, int *))find_function("mpg123_new");
    int (*open_feed)(void *) = (int (*)(void *))find_function("mpg123_open_feed");
    int (*decode)(void *, const void *, size_t, void *, size_t, size_t *) =
        (int (*)(void *, const void *, size_t, void *, size_t, size_t *))find_function(
            "mpg123_decode");
    int (*get_format)(void *, int32_t *, int *, int *) =
        (int (*)(void *, int32_t *, int *, int *))find_function("mpg123_getformat");
    void (*destroy)(void *) = (void (*)(void *))find_function("mpg123_delete");
    void (*deinitialize)(void) = (void (*)(void))find_function("mpg123_exit");
    int file =
        system_call(5, (int)(uintptr_t)"/data/local/tmp/ab520/mp3-input.mp3", NULL, 0);
    if (file < 0) {
        fail("Cannot open private MP3 reference input\n");
    }
    size_t size = 0;
    while (size < sizeof(mp3_reference_input)) {
        int count = system_call(3, file, mp3_reference_input + size,
                                sizeof(mp3_reference_input) - size);
        if (count < 0) {
            fail("Cannot read private MP3 reference input\n");
        }
        if (!count) {
            break;
        }
        size += (size_t)count;
    }
    if (!size || size == sizeof(mp3_reference_input) ||
        system_call(6, file, NULL, 0) != 0 || initialize() != 0) {
        fail("Cannot initialize bounded original MP3 decoder\n");
    }
    int status = 0;
    void *decoder = create(NULL, &status);
    if (!decoder || status || open_feed(decoder)) {
        fail("Cannot create original MP3 feed decoder\n");
    }
    size_t offset = 0;
    do {
        size_t count = size - offset;
        if (count > 2048) {
            count = 2048;
        }
        size_t produced = 0;
        status =
            decode(decoder, mp3_reference_input + offset, count, NULL, 0, &produced);
        offset += count;
        if (produced || (status != -10 && status != -11) ||
            (status == -10 && offset == size)) {
            fail("Original MP3 header feed failed\n");
        }
    } while (status != -11);
    int32_t rate = 0;
    int channels = 0, encoding = 0;
    if (get_format(decoder, &rate, &channels, &encoding) || rate <= 0 || channels < 1 ||
        channels > 2) {
        fail("Cannot query original MP3 output format\n");
    }
    file = system_call(5, (int)(uintptr_t)"/data/local/tmp/ab520/mp3-output.pcm",
                       (const void *)(uintptr_t)(1 | 64 | 512), 0600);
    if (file < 0) {
        fail("Cannot open private MP3 PCM output\n");
    }
    size_t total = 0;
    size_t pending = 0;
    for (size_t iteration = 0; iteration < 65536; ++iteration) {
        size_t produced = 0;
        status = decode(decoder, mp3_reference_input + offset, pending,
                        mp3_reference_output, sizeof(mp3_reference_output), &produced);
        offset += pending;
        pending = 0;
        if (produced > sizeof(mp3_reference_output) ||
            total > 64 * 1024 * 1024 - produced ||
            (status != 0 && status != -10 && status != -12)) {
            fail("Unexpected original MP3 decode result\n");
        }
        size_t written = 0;
        while (written < produced) {
            int count = system_call(4, file, mp3_reference_output + written,
                                    produced - written);
            if (count <= 0 || (size_t)count > produced - written) {
                fail("Cannot write private original PCM samples\n");
            }
            written += (size_t)count;
        }
        total += produced;
        if (status == -12 || (status == -10 && offset == size)) {
            break;
        }
        if (status == -10) {
            pending = size - offset;
            if (pending > 2048) {
                pending = 2048;
            }
        }
        if (iteration == 65535) {
            fail("Original MP3 decode exceeded iteration bound\n");
        }
    }
    if (system_call(6, file, NULL, 0)) {
        fail("Cannot close private original PCM samples\n");
    }
    destroy(decoder);
    deinitialize();
    semihost(4, "AB_ANDROID_MP3_DECODE_V1 1\n");
    uint32_t values[] = {(uint32_t)size, (uint32_t)rate, (uint32_t)channels,
                         (uint32_t)encoding, (uint32_t)total};
    print_words(values, 5);
    semihost(4, "AB MP3 decode reference complete\n");
}

#endif
