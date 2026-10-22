#ifndef AB_ORACLE_NATIVE_EVENTS_H
#define AB_ORACLE_NATIVE_EVENTS_H

/* Authored ABI callbacks exercise the original queue/processor, not a copied
 * implementation of std::function or its containers. Only two inline payload
 * words are owned by these callbacks. Native manager/invoke slots are opaque
 * outside this isolated ARM research tool. */
static void *ne_processor;
static unsigned ne_case, ne_count;
static uint32_t ne_trace[32][6];
static uint8_t ne_wrapper[96];
static void (*ne_post_native)(void *, unsigned, float, const void *);

static uint32_t *ne_queue(void) {
    uint32_t **queues = *(uint32_t ***)((uint8_t *)ne_processor + 0x10);
    if (!queues || !queues[0])
        fail("Missing original native main queue\n");
    return queues[0];
}

static uint32_t ne_vector(const uint32_t *queue, unsigned offset, unsigned stride) {
    uint32_t begin = queue[offset / 4], end = queue[offset / 4 + 1];
    if (begin > end || (end - begin) % stride || (end - begin) / stride > 32)
        fail("Invalid original native event vector\n");
    return (end - begin) / stride;
}

static void ne_post(unsigned id, float seconds);

static void ne_invoke(const uint32_t *data) {
    uint32_t id = data[0];
    uint32_t *queue = ne_queue();
    if (id > 6 || ne_count == 32)
        fail("Invalid original native event invocation\n");
    uint32_t *row = ne_trace[ne_count++];
    row[0] = id;
    row[1] = ne_vector(queue, 0x24, 16);
    row[2] = ne_vector(queue, 0x18, 20);
    row[3] = ne_vector(queue, 0x30, 16);
    row[4] = queue[0x14 / 4];
    row[5] = ne_wrapper[0x44];
    if (!id && (ne_case & 1)) {
        ne_post(4, -1);
        ne_post(5, 0);
    }
    if (id == 1 && (ne_case & 2))
        ne_post(6, 0.25f);
}

static unsigned ne_manage(uint32_t *destination, const uint32_t *source,
                          unsigned operation) {
    if (operation == 2) {
        destination[0] = source[0];
        destination[1] = source[1];
    } else if (operation != 3) {
        fail("Unexpected original native callable manager operation\n");
    }
    return 0;
}

static void ne_post(unsigned id, float seconds) {
    uint32_t callable[] = {id, 0, (uint32_t)(uintptr_t)ne_manage,
                           (uint32_t)(uintptr_t)ne_invoke};
    ne_post_native(ne_processor, 0, seconds, callable);
}

static void ne_snapshot(unsigned step, uint32_t result) {
    uint32_t *queue = ne_queue();
    uint32_t delayed = ne_vector(queue, 0x18, 20);
    uint32_t state[] = {ne_vector(queue, 0x24, 16),
                        delayed,
                        ne_vector(queue, 0x30, 16),
                        queue[0x14 / 4],
                        result,
                        ne_count,
                        ne_wrapper[0x44]};
    uint32_t header[] = {ne_case, step};
    print_words(header, 2);
    print_words(state, 7);
    const uint32_t *list = (const uint32_t *)(uintptr_t)queue[0x18 / 4];
    for (uint32_t index = 0; index < delayed; ++index) {
        uint32_t row[] = {list[index * 5], list[index * 5 + 1]};
        print_words(row, 2);
    }
    for (unsigned index = 0; index < ne_count; ++index)
        print_words(ne_trace[index], 6);
}

static void probe_native_events(void) {
    typedef void (*Construct)(void *, const char *, const char *);
    typedef void (*Destroy)(void *);
    typedef unsigned (*Process)(void *, float);
    Construct construct =
        (Construct)find_function("_ZN4lang5event14EventProcessorC1EPKcS3_");
    Destroy destroy = (Destroy)find_function("_ZN4lang5event14EventProcessorD1Ev");
    Process process =
        (Process)find_function("_ZN4lang5event14EventProcessor7processEf");
    void *(*global)(void) =
        (void *(*)(void))find_function("_ZN4lang5event23getGlobalEventProcessorEv");
    uint32_t *(*systems)(void) =
        (uint32_t *(*)(void))find_function("_ZN4host10getSystemsEv");
    void (*update)(void *, float) =
        (void (*)(void *, float))find_function("_ZN16AnimationWrapper6updateEf");
    ne_post_native = (void (*)(void *, unsigned, float, const void *))find_function(
        "_ZN4lang5event14EventProcessor8addQueueEjfRKSt8functionIFvvEE");
    if (systems()[5])
        fail("Expected actual empty original host registry\n");
    uint32_t storage[32];
    semihost(4, "AB_ANDROID_NATIVE_EVENTS_V1 16 8\n");
    for (ne_case = 0; ne_case < 16; ++ne_case) {
        ne_count = 0;
        for (size_t index = 0; index < sizeof(ne_wrapper); ++index)
            ne_wrapper[index] = 0;
        if (ne_case < 8) {
            ne_processor = storage;
            construct(ne_processor, "ABNativeEvents", "GlobalEvents");
        } else {
            ne_processor = global();
            destroy(ne_processor);
            construct(ne_processor, "DefaultEventProcessor", "GlobalEvents");
        }
        ne_post(0, ne_case & 4 ? -0.0f : -1.0f);
        ne_post(1, ne_case & 4 ? -1.0f : 0.0f);
        ne_post(2, ne_case & 2 ? 0.125f : 0.25f);
        ne_post(3, ne_case & 2 ? 0.25f : 0.5f);
        ne_snapshot(0, 0);
        static const float steps[] = {0, 0.25f, 0.25f, 0.25f, 0, 1, 0};
        for (unsigned step = 1; step < 8; ++step) {
            float seconds = steps[step - 1];
            if (step == 2 && (ne_case & 2))
                seconds = -0.25f;
            uint32_t result;
            if (ne_case < 8)
                result = process(ne_processor, seconds);
            else {
                update(ne_wrapper, seconds);
                result = ne_vector(ne_queue(), 0x24, 16);
            }
            ne_snapshot(step, result);
        }
        if (ne_case < 8)
            destroy(ne_processor);
    }
    semihost(4, "AB native events reference complete\n");
}

#endif
