#ifndef AB_ORACLE_WORLD_OPERATIONS_H
#define AB_ORACLE_WORLD_OPERATIONS_H

static uint32_t step_fixture_pointers[6];
static size_t step_fixture_count;
static void (*step_set_transform)(void *, const void *, float);
static void (*step_set_type)(void *, int);
static void (*step_set_active)(void *, bool);
static void (*step_set_fixed)(void *, bool);
static void (*step_set_filter)(void *, const void *);
static void (*step_set_sensor)(void *, bool);

static void prepare_world_operations(void) {
    step_set_transform = (void (*)(void *, const void *, float))find_function(
        "_ZN6b2Body12SetTransformERK6b2Vec2f");
    step_set_type =
        (void (*)(void *, int))find_function("_ZN6b2Body7SetTypeE10b2BodyType");
    step_set_active = (void (*)(void *, bool))find_function("_ZN6b2Body9SetActiveEb");
    step_set_fixed =
        (void (*)(void *, bool))find_function("_ZN6b2Body16SetFixedRotationEb");
    step_set_filter = (void (*)(void *, const void *))find_function(
        "_ZN9b2Fixture13SetFilterDataERK8b2Filter");
    step_set_sensor =
        (void (*)(void *, bool))find_function("_ZN9b2Fixture9SetSensorEb");
}

static void prepare_world_operation(uint32_t output[5], size_t frame,
                                    size_t body_count) {
    for (size_t field = 0; field < 5; ++field) {
        output[field] = 0;
    }
    output[1] = (uint32_t)(2 + (frame / 32) % (body_count - 1));
    switch (frame % 32) {
        case 0:
        case 6:
            output[0] = 1;
            output[2] = world_step_number(frame % 32 ? -1.25f : 1.25f);
            output[3] = world_step_number(3.0f);
            output[4] = world_step_number(frame % 32 ? -0.2f : 0.2f);
            break;
        case 4:
        case 5:
        case 8:
            output[0] = 2;
            output[2] = frame % 32 == 8;
            break;
        case 12:
        case 13:
        case 16:
        case 20:
            output[0] = 3;
            output[2] = frame % 32 == 20 ? 2u : frame % 32 == 16 ? 1u : 0;
            break;
        case 21:
        case 22:
        case 29:
        case 30:
        case 31:
            output[0] = 4;
            output[2] = frame % 32 == 21 || frame % 32 == 29 || frame % 32 == 30;
            break;
        case 23:
        case 24:
        case 25:
            output[0] = 5;
            output[2] = frame % 32 == 24 ? 2u : 1u;
            output[3] = frame % 32 == 23 ? 0 : 0xffff;
            output[4] = frame % 32 == 24 ? 0xffffu : frame % 32 == 25 ? 1u : 0;
            break;
        case 26:
        case 27:
        case 28:
            output[0] = 6;
            output[2] = frame % 32 != 28;
            break;
        default:
            break;
    }
}

static void apply_world_operation(const uint32_t input[5],
                                  const uint32_t *body_pointers) {
    void *body = (void *)(uintptr_t)body_pointers[input[1] - 1];
    void *fixture = (void *)(uintptr_t)step_fixture_pointers[input[1] - 1];
    switch (input[0]) {
        case 0:
            break;
        case 1: {
            FloatBits angle = {.bits = input[4]};
            step_set_transform(body, input + 2, angle.number);
            break;
        }
        case 2:
            step_set_active(body, input[2] != 0);
            break;
        case 3:
            step_set_type(body, (int)input[2]);
            break;
        case 4:
            step_set_fixed(body, input[2] != 0);
            break;
        case 5: {
            uint32_t filter[] = {input[2] | (input[3] << 16), input[4]};
            step_set_filter(fixture, filter);
            break;
        }
        case 6:
            step_set_sensor(fixture, input[2] != 0);
            break;
        default:
            fail("Unsupported controlled world operation\n");
    }
}

static void print_step_fixtures(const uint32_t *world) {
    const uint32_t *nodes = (const uint32_t *)(uintptr_t)world[0x191dc / 4];
    for (size_t index = 0; index < step_fixture_count; ++index) {
        const uint32_t *fixture =
            (const uint32_t *)(uintptr_t)step_fixture_pointers[index];
        const uint32_t *body = (const uint32_t *)(uintptr_t)fixture[2];
        uint32_t output[19] = {fixture[10],
                               body[37],
                               fixture[0],
                               fixture[4],
                               fixture[5],
                               fixture[8] & 0xffff,
                               fixture[8] >> 16,
                               fixture[9] & 0xffff,
                               (fixture[9] >> 16) & 0xff,
                               fixture[7],
                               UINT32_C(0xffffffff)};
        if (fixture[7]) {
            const uint32_t *proxy = (const uint32_t *)(uintptr_t)fixture[6];
            output[10] = proxy[6];
            const uint32_t *node = nodes + proxy[6] * 9;
            for (size_t field = 0; field < 4; ++field) {
                output[11 + field] = proxy[field];
                output[15 + field] = node[field];
            }
        }
        print_words(output, 19);
    }
}

#endif
