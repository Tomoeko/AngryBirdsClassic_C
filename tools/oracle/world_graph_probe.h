#ifndef AB_ORACLE_WORLD_GRAPH_PROBE_H
#define AB_ORACLE_WORLD_GRAPH_PROBE_H

static void print_world_body(const uint32_t body[64]) {
    uint32_t output[64];
    for (size_t field = 0; field < 64; ++field) {
        output[field] = body[field];
    }
    output[22] = UINT32_C(0xdead0000);
    output[23] = output[24] = output[27] = output[28] = 0;
    print_words(output, 64);
}

static void connect_world_contact(OracleIslandContact *contact,
                                  const uint32_t body_pointers[7]) {
    uint32_t *native = contact->native;
    for (size_t endpoint = 0; endpoint < 2; ++endpoint) {
        uint32_t *body = (uint32_t *)(uintptr_t)body_pointers[contact->data[endpoint]];
        uint32_t *edge = native + 4 + endpoint * 4;
        edge[0] = body_pointers[contact->data[1 - endpoint]];
        edge[1] = (uint32_t)(uintptr_t)native;
        edge[2] = 0;
        edge[3] = body[28];
        if (edge[3]) {
            ((uint32_t *)(uintptr_t)edge[3])[2] = (uint32_t)(uintptr_t)edge;
        }
        body[28] = (uint32_t)(uintptr_t)edge;
    }
}

#endif
