#ifndef AB_TEST_LEVEL_RELOAD_H
#define AB_TEST_LEVEL_RELOAD_H

typedef struct {
    AbSlingReloadState state;
    AbSlingTransferInput transfer;
    AbVec2 target;
    size_t object_index;
    bool started;
    bool ready;
} LevelReload;

static bool reload_state_matches(const LevelReload *reload, const uint32_t input[5],
                                 size_t frame) {
    uint32_t actual[] = {
        number(reload->state.timer).bits, number(reload->state.current_bird_index).bits,
        reload->state.bird_fired, reload->state.fill_in_next_bird, reload->ready};
    return compare_level_words(actual, input, 5, frame, "reload", "state", 0);
}

static bool initialize_level_reload(FILE *file, const AbLevelObject objects[23],
                                    AbVec2 target, LevelReload *reload) {
    uint32_t input[5];
    CHECK(read_body_words(file, input, 5) && input[2] <= 1 && input[3] <= 1 &&
          input[4] <= 1);
    *reload = (LevelReload){.state = {.timer = from_bits(input[0]),
                                      .current_bird_index = from_bits(input[1]),
                                      .bird_fired = input[2] != 0,
                                      .fill_in_next_bird = input[3] != 0,
                                      .allow_reset_to_bird_camera = true},
                            .target = target,
                            .object_index = SIZE_MAX,
                            .ready = input[4] != 0};
    for (size_t index = 0; index < 23; ++index) {
        const AbDataValue *order = data_field(objects[index].placement, "startNumber");
        if (objects[index].physics.controllable && order &&
            order->kind == AB_DATA_NUMBER &&
            from_bits(order->bits) == reload->state.current_bird_index + 1.0f) {
            CHECK(reload->object_index == SIZE_MAX);
            reload->object_index = index;
        }
    }
    CHECK(reload->object_index < 23 && !reload->ready &&
          !reload->state.fill_in_next_bird);
    return true;
}

static bool advance_level_reload(LevelReload *reload, AbPhysicsWorld *world,
                                 const NativeObjectBody records[23], float seconds,
                                 bool completed) {
    const AbObjectBody *bird = &records[reload->object_index].instance;
    AbError error = {0};
    if (reload->ready) {
        AbVec2 sitting =
            completed ? reload->target
                      : (AbVec2){reload->target.x - 0.1f, reload->target.y - 0.1f};
        CHECK(ab_slingshot_hold_body(world, bird, sitting, &error));
    }
    AbSlingReloadGate gate;
    CHECK(ab_slingshot_reload_gate(&reload->state, seconds, true, &gate));
    reload->state = gate.next;
    if (reload->state.fill_in_next_bird) {
        const AbBodyMassState *body = &world->storage.bodies[bird->body].motion.mass;
        float rotation;
        CHECK(ab_angle_from_components(body->transform.sine, body->transform.cosine,
                                       &rotation));
        if (!reload->started) {
            AbSlingTransferStart start = {.position = body->transform.position,
                                          .target = reload->target,
                                          .rotation = rotation};
            CHECK(ab_slingshot_transfer_begin(&start, &reload->transfer));
            reload->started = true;
        }
        reload->transfer.rotation = rotation;
        reload->transfer.seconds = seconds;
        AbSlingTransferFrame frame;
        CHECK(
            ab_slingshot_transfer_body(world, bird, &reload->transfer, &frame, &error));
        reload->transfer.time = frame.next_time;
        if (frame.complete) {
            reload->ready = true;
            reload->state.fill_in_next_bird = false;
            reload->state.current_bird_index = reload->state.current_bird_index + 1.0f;
        }
    }
    return true;
}

#endif
