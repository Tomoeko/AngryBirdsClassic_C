#ifndef AB_ORACLE_ISLAND_CONTACTS_H
#define AB_ORACLE_ISLAND_CONTACTS_H

typedef struct {
    uint32_t native[40];
    uint32_t fixtures[2][10];
    uint32_t shapes[2][3];
    uint32_t data[23];
} OracleIslandContact;

static void prepare_island_contact(OracleIslandContact *contact,
                                   const uint32_t *body_pointers, size_t body_count,
                                   size_t case_index, size_t contact_index,
                                   uint32_t *state) {
    const uint32_t normals[][2] = {
        {0x3f800000, 0},          {0, 0x3f800000},          {0xbf800000, 0x80000000},
        {0x3f19999a, 0x3f4ccccd}, {0xbf3504f3, 0x3f3504f3},
    };
    uint32_t *data = contact->data;
    data[0] = (uint32_t)((case_index / 5 + contact_index) % body_count);
    data[1] = (uint32_t)((case_index / 17 + contact_index * 3) % body_count);
    data[2] = random_float(state, 120, 7, 0);
    data[3] = random_float(state, 120, 7, 0);
    data[4] = random_float(state, 118, 10, 0);
    data[5] = random_float(state, 116, 10, 0);
    data[6] = random_float(state, 115, 12, 1);
    uint32_t *manifold = data + 7;
    for (size_t field = 0; field < 14; ++field) {
        manifold[field] = random_float(state, 115, 14, 1);
    }
    for (size_t point = 0; point < 2; ++point) {
        manifold[point * 5 + 2] = random_float(state, 115, 13, 0);
        manifold[point * 5 + 4] = random_word(state);
    }
    manifold[10] = normals[(case_index + contact_index) % 5][0];
    manifold[11] = normals[(case_index + contact_index) % 5][1];
    manifold[14] = (uint32_t)((case_index + contact_index) % 3);
    manifold[15] =
        manifold[14] ? (uint32_t)(1 + (case_index / 3 + contact_index) % 2) : 1;
    for (size_t endpoint = 0; endpoint < 2; ++endpoint) {
        contact->shapes[endpoint][2] = data[2 + endpoint];
        contact->fixtures[endpoint][2] = body_pointers[data[endpoint]];
        contact->fixtures[endpoint][3] = (uint32_t)(uintptr_t)contact->shapes[endpoint];
        contact->native[12 + endpoint] =
            (uint32_t)(uintptr_t)contact->fixtures[endpoint];
    }
    for (size_t field = 0; field < 16; ++field) {
        contact->native[16 + field] = manifold[field];
    }
    contact->native[34] = data[4];
    contact->native[35] = data[5];
    contact->native[36] = data[6];
}

#endif
