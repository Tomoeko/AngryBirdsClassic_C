#ifndef AB_ORACLE_CONTACT_STATE_PROBE_H
#define AB_ORACLE_CONTACT_STATE_PROBE_H

static uint32_t contact_evaluation[16];
static uint32_t contact_events[2][18];
static size_t contact_event_count;
static uint32_t contact_listener_mode;

static void evaluate_contact(void *contact, void *manifold, const void *first,
                             const void *second) {
    (void)contact;
    (void)first;
    (void)second;
    for (size_t field = 0; field < 16; ++field) {
        ((uint32_t *)manifold)[field] = contact_evaluation[field];
    }
}

static void contact_event(uint32_t kind, void *contact, const void *previous) {
    if (contact_event_count >= 2) {
        fail("Unexpected contact callback count\n");
    }
    uint32_t *event = contact_events[contact_event_count++];
    event[0] = kind;
    event[1] = ((uint32_t *)contact)[1];
    for (size_t field = 0; field < 16; ++field) {
        event[2 + field] = previous ? ((const uint32_t *)previous)[field] : 0;
    }
    if (kind == 1 && contact_listener_mode == 2) {
        ((uint32_t *)contact)[1] &= ~UINT32_C(4);
    }
}

static void contact_begin(void *listener, void *contact) {
    (void)listener;
    contact_event(1, contact, NULL);
}

static void contact_end(void *listener, void *contact) {
    (void)listener;
    contact_event(2, contact, NULL);
}

static void contact_pre_solve(void *listener, void *contact, const void *previous) {
    (void)listener;
    contact_event(3, contact, previous);
}

static void print_contact(const uint32_t contact[37]) {
    uint32_t output[20];
    output[0] = contact[1];
    for (size_t field = 0; field < 16; ++field) {
        output[1 + field] = contact[16 + field];
    }
    for (size_t field = 0; field < 3; ++field) {
        output[17 + field] = contact[34 + field];
    }
    print_words(output, 20);
}

static void probe_contact_state(void) {
    typedef void (*ConstructContact)(void *, void *, int, void *, int);
    typedef void (*UpdateContact)(void *, void *);
    ConstructContact construct =
        (ConstructContact)find_function("_ZN9b2ContactC1EP9b2FixtureiS1_i");
    UpdateContact update =
        (UpdateContact)find_function("_ZN9b2Contact6UpdateEP17b2ContactListener");
    uint32_t evaluator_vtable[] = {(uint32_t)(uintptr_t)evaluate_contact};
    uint32_t listener_vtable[] = {0, 0, (uint32_t)(uintptr_t)contact_begin,
                                  (uint32_t)(uintptr_t)contact_end,
                                  (uint32_t)(uintptr_t)contact_pre_solve};
    uint32_t listener[] = {(uint32_t)(uintptr_t)listener_vtable};
    uint32_t state = UINT32_C(0x520c01fe);
    semihost(4, "AB_ANDROID_CONTACT_STATE_V1 4096\n");
    for (size_t index = 0; index < 4096; ++index) {
        uint32_t contact[37], fixtures[2][10] = {0}, bodies[2][40] = {0};
        uint32_t materials[4];
        for (size_t endpoint = 0; endpoint < 2; ++endpoint) {
            materials[endpoint] = random_float(&state, 100, 40, 0);
            materials[2 + endpoint] = random_float(&state, 115, 15, 1);
            fixtures[endpoint][2] = (uint32_t)(uintptr_t)bodies[endpoint];
            fixtures[endpoint][4] = materials[endpoint];
            fixtures[endpoint][5] = materials[2 + endpoint];
        }
        if (index % 16 < 2) {
            fixtures[0][4] = materials[0] = index % 2 ? 0x80000000 : 0;
            fixtures[1][4] = materials[1] = 0x3f800000;
            fixtures[0][5] = materials[2] = index % 2 ? 0x80000000 : 0;
            fixtures[1][5] = materials[3] = index % 2 ? 0 : 0x80000000;
        }
        for (size_t field = 0; field < 37; ++field) {
            contact[field] = random_word(&state);
        }
        contact[31] = (uint32_t)(index % 3);
        print_words(materials, 4);
        print_contact(contact);
        construct(contact, fixtures[0], 0, fixtures[1], 0);
        print_contact(contact);
        uint32_t control[8] = {index % 4 == 0,
                               (uint32_t)(index % 3),
                               random_word(&state),
                               random_word(&state),
                               random_word(&state),
                               random_float(&state, 115, 20, 1),
                               random_float(&state, 115, 20, 1),
                               (uint32_t)((index / 4) % 2)};
        uint32_t circles[2][5] = {{0, 0, 0x3f800000, 0, 0}, {0, 0, 0x3f800000, 0, 0}};
        if (control[0]) {
            ((uint8_t *)fixtures[(index / 4) % 2])[0x26] = 1;
            fixtures[0][3] = (uint32_t)(uintptr_t)circles[0];
            fixtures[1][3] = (uint32_t)(uintptr_t)circles[1];
            bodies[0][6] = bodies[1][6] = 0x3f800000;
            bodies[1][3] = control[7] ? 0 : 0x41200000;
        }
        contact[0] = (uint32_t)(uintptr_t)evaluator_vtable;
        contact[1] = control[2];
        for (size_t field = 0; field < 16; ++field) {
            contact[16 + field] = random_word(&state);
            contact_evaluation[field] = random_word(&state);
        }
        contact[20] %= 4;
        contact[25] %= 4;
        contact_evaluation[4] %= 4;
        contact_evaluation[9] %= 4;
        contact[31] = (uint32_t)(index % 3);
        contact_evaluation[15] = (uint32_t)((index / 3) % 3);
        for (size_t endpoint = 0; endpoint < 2; ++endpoint) {
            bodies[endpoint][1] = control[3 + endpoint];
            bodies[endpoint][36] = control[5 + endpoint];
        }
        print_words(control, 8);
        print_words(contact + 16, 16);
        print_words(contact_evaluation, 16);
        contact_event_count = 0;
        contact_listener_mode = control[1];
        update(contact, control[1] ? listener : NULL);
        print_contact(contact);
        uint32_t body_output[] = {bodies[0][1], bodies[1][1], bodies[0][36],
                                  bodies[1][36]};
        print_words(body_output, 4);
        uint32_t event_count = (uint32_t)contact_event_count;
        print_words(&event_count, 1);
        for (size_t event = 0; event < contact_event_count; ++event) {
            print_words(contact_events[event], 18);
        }
    }
    semihost(4, "AB contact state reference complete\n");
}

#endif
