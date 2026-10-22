#ifndef AB_ORACLE_WORLD_EVENTS_H
#define AB_ORACLE_WORLD_EVENTS_H

static uint32_t world_events[128][36];
static size_t world_event_count;

static void normalize_native_manifold(uint32_t output[16], const uint32_t input[16]) {
    for (size_t field = 0; field < 16; ++field) {
        output[field] = 0;
    }
    if (!input[15]) {
        return;
    }
    if (input[15] > 2) {
        fail("Invalid native world event manifold\n");
    }
    for (size_t field = 0; field < input[15] * 5; ++field) {
        output[field] = input[field];
    }
    for (size_t field = 10; field < 16; ++field) {
        output[field] = input[field];
    }
}

static void record_world_event(uint32_t kind, const void *contact, const void *detail) {
    if (world_event_count >= 128) {
        fail("Native world event capacity exceeded\n");
    }
    uint32_t *output = world_events[world_event_count++];
    const uint32_t *native = contact;
    for (size_t field = 0; field < 36; ++field) {
        output[field] = 0;
    }
    uint32_t key = world_contact_key(native);
    output[0] = kind;
    output[1] = key >> 16;
    output[2] = key & 0xffff;
    output[3] = native[1];
    normalize_native_manifold(output + 4, native + 16);
    if (kind == 3) {
        normalize_native_manifold(output + 20, detail);
    } else if (kind == 4) {
        const uint32_t *impulse = detail;
        output[20] = impulse[4];
        if (impulse[4] > 2) {
            fail("Invalid native world event impulse count\n");
        }
        for (size_t point = 0; point < impulse[4]; ++point) {
            output[21 + point] = impulse[point];
            output[23 + point] = impulse[2 + point];
        }
    }
}

static void world_begin_event(void *listener, const void *contact) {
    (void)listener;
    record_world_event(1, contact, NULL);
}

static void world_end_event(void *listener, const void *contact) {
    (void)listener;
    record_world_event(2, contact, NULL);
}

static void world_pre_event(void *listener, const void *contact, const void *previous) {
    (void)listener;
    record_world_event(3, contact, previous);
#if defined(AB_ORACLE_WORLD_MUTATIONS)
    uint32_t *mutable_contact = (uint32_t *)(uintptr_t)contact;
    if (mutable_contact[1] & 32) {
        mutable_contact[1] &= ~UINT32_C(4);
    }
#endif
}

static void world_post_event(void *listener, const void *contact, const void *impulse) {
    (void)listener;
    record_world_event(4, contact, impulse);
}

static void print_world_events(void) {
    uint32_t count = (uint32_t)world_event_count;
    print_words(&count, 1);
    for (size_t event = 0; event < world_event_count; ++event) {
        print_words(world_events[event], 36);
    }
}

#endif
