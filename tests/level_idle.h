#ifndef AB_TEST_LEVEL_IDLE_H
#define AB_TEST_LEVEL_IDLE_H

#include "ab/bird_jump.h"

typedef struct {
    AbBirdJumpState birds[2];
    size_t indices[2];
    bool allow_roll[2];
    uint32_t draws[256];
    size_t draw_count;
    size_t draw_index;
    AbBodyPose history[2][2];
    AbBodyPose rendered[2];
    uint32_t history_index;
    bool history_tracked;
} LevelIdle;

static bool idle_random(void *context, int minimum, int maximum, int *output) {
    LevelIdle *idle = context;
    CHECK(idle->draw_index < idle->draw_count);
    uint32_t value = idle->draws[idle->draw_index++];
    CHECK(value <= 36 && (int)value >= minimum && (int)value <= maximum + 1);
    *output = (int)value;
    return true;
}

static bool initialize_level_idle(FILE *file, AbPhysicsWorld *world,
                                  const NativeObjectBody records[23],
                                  const AbDataTable *catalog, bool extended,
                                  LevelIdle *idle) {
    *idle = (LevelIdle){.history_tracked = extended};
    for (size_t item = 0; item < 2; ++item) {
        uint32_t input[16];
        CHECK(read_body_words(file, input, extended ? 16 : 13) && input[0] < 23 &&
              input[11] <= 1 && input[12] <= 1);
        size_t index = input[0];
        CHECK(!records[index].leading_bird &&
              records[index].instance.collider.bits == UINT32_C(0x40400000));
        idle->indices[item] = index;
        idle->birds[item] =
            (AbBirdJumpState){.position = {from_bits(input[1]), from_bits(input[2])},
                              .rotation = from_bits(input[3]),
                              .timer = from_bits(input[4]),
                              .height = from_bits(input[5]),
                              .speed = from_bits(input[6]),
                              .phase = from_bits(input[7]),
                              .roll = from_bits(input[8]),
                              .saved_y = from_bits(input[9]),
                              .saved_angle = from_bits(input[10]),
                              .jumping = input[11] != 0,
                              .rebound = input[12] != 0};
        AbObjectDefinition definition;
        AbError error = {0};
        CHECK(ab_object_definition_open(catalog, span(records[index].definition), NULL,
                                        &definition, &error));
        AbDataValue roll = ab_object_property(&definition, span("allowRoll"));
        idle->allow_roll[item] = roll.kind == AB_DATA_BOOLEAN && roll.bits != 0;
        AbVec2 physical = extended
                              ? (AbVec2){from_bits(input[13]), from_bits(input[14])}
                              : idle->birds[item].position;
        float angle = extended ? from_bits(input[15]) : idle->birds[item].rotation;
        CHECK(ab_world_set_transform(world, records[index].instance.body, physical,
                                     angle, &error));
    }
    if (extended) {
        uint32_t count;
        CHECK(read_body_words(file, &count, 1) && count > 0 && count <= 256);
        idle->draw_count = count;
        CHECK(read_body_words(file, idle->draws, count));
        CHECK(read_body_words(file, &idle->history_index, 1) &&
              idle->history_index <= 1);
        for (size_t item = 0; item < 2; ++item) {
            uint32_t poses[9];
            CHECK(read_body_words(file, poses, 9));
            for (size_t slot = 0; slot < 3; ++slot) {
                AbBodyPose pose = {
                    {from_bits(poses[3 * slot]), from_bits(poses[3 * slot + 1])},
                    from_bits(poses[3 * slot + 2])};
                if (slot < 2) {
                    idle->history[item][slot] = pose;
                } else {
                    idle->rendered[item] = pose;
                }
            }
        }
        return true;
    }
    uint32_t draws[4];
    CHECK(read_body_words(file, draws, 4) && draws[0] == 3);
    idle->draw_count = 3;
    memcpy(idle->draws, draws + 1, 3 * sizeof(*idle->draws));
    return true;
}

static bool idle_state_matches(FILE *file, const LevelIdle *idle, size_t frame) {
    for (size_t item = 0; item < 2; ++item) {
        const AbBirdJumpState *state = idle->birds + item;
        if (idle->history_tracked) {
            uint32_t expected[13];
            CHECK(read_body_words(file, expected, 13));
            const uint32_t actual[] = {(uint32_t)idle->indices[item],
                                       number(state->position.x).bits,
                                       number(state->position.y).bits,
                                       number(state->rotation).bits,
                                       number(state->timer).bits,
                                       number(state->height).bits,
                                       number(state->speed).bits,
                                       number(state->phase).bits,
                                       number(state->roll).bits,
                                       number(state->saved_y).bits,
                                       number(state->saved_angle).bits,
                                       state->jumping,
                                       state->rebound};
            CHECK(
                compare_level_words(actual, expected, 13, frame, "idle", "bird", item));
            continue;
        }
        uint32_t expected[7];
        CHECK(read_body_words(file, expected, 7));
        uint32_t actual[] = {
            (uint32_t)idle->indices[item],  number(state->position.x).bits,
            number(state->position.y).bits, number(state->rotation).bits,
            number(state->timer).bits,      state->jumping,
            number(state->saved_y).bits};
        CHECK(compare_level_words(actual, expected, 7, frame, "idle", "bird", item));
    }
    return true;
}

static bool idle_history_matches(FILE *file, const LevelIdle *idle, size_t frame) {
    for (size_t item = 0; item < 2; ++item) {
        uint32_t expected[9], actual[9];
        CHECK(read_body_words(file, expected, 9));
        AbBodyPose poses[] = {idle->history[item][0], idle->history[item][1],
                              idle->rendered[item]};
        for (size_t slot = 0; slot < 3; ++slot) {
            actual[3 * slot] = number(poses[slot].position.x).bits;
            actual[3 * slot + 1] = number(poses[slot].position.y).bits;
            actual[3 * slot + 2] = number(poses[slot].angle).bits;
        }
        CHECK(compare_level_words(actual, expected, 9, frame, "idle history", "bird",
                                  item));
    }
    return true;
}

static void reset_idle_history(LevelIdle *idle, size_t item,
                               const AbBodyMassState *body, float angle) {
    AbBodyPose pose = {body->transform.position, angle};
    idle->history[item][0] = idle->history[item][1] = idle->rendered[item] = pose;
}

static bool advance_level_idle(LevelIdle *idle, AbPhysicsWorld *world,
                               const NativeObjectBody records[23],
                               const LevelReload *reload, bool reload_started,
                               float seconds, float interpolation, bool completed,
                               bool controller_enabled) {
    AbError error = {0};
    AbBirdJumpRandom random = {idle, idle_random};
    for (size_t item = 0; item < 2; ++item) {
        size_t index = idle->indices[item];
        const AbObjectBody *object = &records[index].instance;
        const AbBodyMassState *body = &world->storage.bodies[object->body].motion.mass;
        AbBirdJumpState *state = idle->birds + item;
        bool transferring = index == reload->object_index && reload->started;
        if (index == reload->object_index && reload_started) {
            state->timer = 10000;
            state->jumping = false;
        }
        state->position = body->transform.position;
        state->rotation = body->angle;
        if (idle->history_tracked) {
            if (transferring && controller_enabled) {
                reset_idle_history(idle, item, body, body->angle);
            } else if (world->storage.bodies[object->body].motion.awake) {
                CHECK(ab_body_pose_interpolate(idle->history[item], idle->history_index,
                                               interpolation, &idle->rendered[item]));
                CHECK(ab_angle_from_components(
                    body->transform.sine, body->transform.cosine, &state->rotation));
            }
            state->position = idle->rendered[item].position;
        }
        if (transferring && reload->ready) {
            continue;
        }
        if (!controller_enabled) {
            continue;
        }
        if (!transferring && !idle->history_tracked) {
            AbBodyPose pose = {.position = body->transform.position};
            CHECK(ab_angle_from_components(body->transform.sine, body->transform.cosine,
                                           &pose.angle));
            AbBodyPose history[] = {pose, pose};
            CHECK(ab_body_pose_interpolate(history, 0, interpolation, &pose));
            state->position = pose.position;
            /* The captured Lua angle retains the body's assigned angle even
             * when the rendered/history interpolation differs by one ULP. */
            state->rotation = body->angle;
        }
        AbBirdJumpFrame output;
        CHECK(ab_bird_jump_frame(state, seconds, completed, idle->allow_roll[item],
                                 &random, &output));
        *state = output.next;
        for (size_t event = 0; event < output.event_count; ++event) {
            const AbBirdJumpEvent *call = output.events + event;
            if (call->kind == AB_BIRD_JUMP_POSITION) {
                CHECK(ab_world_set_transform(world, object->body, call->value,
                                             body->angle, &error));
            } else {
                float angle = 0.0f;
                CHECK(call->kind == AB_BIRD_JUMP_ROTATION &&
                      ab_game_angle_wrap(call->value.x, &angle));
                CHECK(ab_world_set_transform(world, object->body,
                                             body->transform.position, angle, &error));
                CHECK(ab_body_angular_velocity_set(
                    &world->storage.bodies[object->body].motion, 0));
                state->rotation = angle;
            }
            if (idle->history_tracked) {
                /* Position-only writes preserve the object's rendered angle,
                 * which can now be signed; rotation writes use the assigned
                 * wrapped angle. Native retry frames 86..88 distinguish them. */
                float angle = call->kind == AB_BIRD_JUMP_POSITION
                                  ? idle->rendered[item].angle
                                  : state->rotation;
                reset_idle_history(idle, item, body, angle);
            }
        }
    }
    return true;
}

#endif
