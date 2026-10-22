#include "ab/block_removal_pass.h"
#include "ab/game_object_removal.h"
#include "block_removal_pass_cases.h"
#include "world_storage.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Coupled removal check failed at %d: %s\n", __LINE__,      \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

typedef struct {
    uint32_t input[5], output[48];
    WorldStorage storage;
    AbPhysicsWorld world;
    AbGameObjectCore physical[2];
    AbGameObjectCoreRegistry registry;
    AbClassicJointRecord joint, glue;
    AbClassicJoints joints;
    AbGameObjectRemoval removal;
    AbDataEntry body_fields[9], raw_field, particle_field, world_field, dynamic_field;
    AbDataEntry object_fields[2], catalog_field, blocks_field;
    AbDataTable body, raw, particle_table, world_table, dynamic, objects, catalog,
        block_table;
    AbDataValue body_value;
    AbStringMap *dead, *goals;
    AbBlockRemovalState state;
    float individual;
    AbBlockRemovalScores scores;
    AbBlockRemovalScoreHost scoring;
    AbBlockRemovalEffectsHost effects;
    AbBlockRemovalParticlesHost particles;
    AbBlockRemovalDetachHost detach;
    AbBlockRemovalCleanupState cleanup_state;
    AbBlockRemovalCleanupHost cleanup;
    AbBlockRemovalPassHost pass;
} Fixture;

static AbSpan span(const char *value) {
    return (AbSpan){(const uint8_t *)value, strlen(value)};
}

static AbDataValue text(const char *value) {
    return (AbDataValue){.kind = AB_DATA_STRING, .string = span(value)};
}

static AbDataValue table(AbDataTable *value) {
    return (AbDataValue){.kind = AB_DATA_TABLE, .table = value};
}

static AbDataValue boolean(bool value) {
    return (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = value};
}

static uint32_t bits(float value) {
    uint32_t result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

static bool named(AbSpan value, const char *name) {
    return value.size == strlen(name) && !memcmp(value.data, name, value.size);
}

static bool event(Fixture *fixture, uint32_t kind) {
    CHECK(fixture->output[1] < 24);
    fixture->output[2 + fixture->output[1]++] = kind;
    return true;
}

static bool destroy(void *context, AbDataValue object, AbError *error) {
    Fixture *fixture = context;
    (void)error;
    CHECK(object.table == &fixture->body && event(fixture, 1));
    if (fixture->input[1] & 32u) {
        fixture->body_fields[5].value.bits ^= 1u;
    }
    return true;
}

static bool definition(void *context, AbSpan key, AbDataValue *output, AbError *error) {
    Fixture *fixture = context;
    (void)error;
    CHECK(named(key, "queued") && event(fixture, 2));
    *output = table(&fixture->raw);
    return true;
}

static bool score(void *context, AbDataValue object, AbScoreNumber *output,
                  AbError *error) {
    Fixture *fixture = context;
    (void)error;
    CHECK(object.table == &fixture->body && event(fixture, 3));
    return ab_break_score(
        &(AbBreakScoreInput){.block_increment = 500,
                             .goal_increment = 5000,
                             .level_goal_truthy =
                                 fixture->body_fields[5].value.bits != 0},
        output);
}

static bool floating(void *context, const AbBlockRemovalFloatingScore *request,
                     AbError *error) {
    Fixture *fixture = context;
    (void)error;
    CHECK(event(fixture, 4) && !request->text_present && request->score.present &&
          request->sprite.kind == AB_DATA_BOOLEAN && !request->sprite.bits &&
          request->x.kind == AB_DATA_NUMBER && request->x.bits == 0x3f800000u &&
          request->y.kind == AB_DATA_NUMBER && request->y.bits == 0xc0000000u);
    fixture->output[44] = bits(request->score.value);
    fixture->output[45] = bits(request->lifetime);
    fixture->output[46] = bits(request->max_scale);
    if (fixture->input[1] & 16u) {
        fixture->body_fields[0].value = text("renamed");
        fixture->scores.total.value = 100;
    }
    return true;
}

static bool property(void *context, AbDataValue object, AbSpan key, AbDataValue *output,
                     AbError *error) {
    Fixture *fixture = context;
    (void)error;
    CHECK(object.table == &fixture->body);
    if (named(key, "particleAmount")) {
        CHECK(event(fixture, 7));
        *output = (AbDataValue){.kind = AB_DATA_NUMBER};
    } else if (named(key, "useNewParticleTable")) {
        CHECK(event(fixture, 11));
        *output = boolean((fixture->input[1] & 2u) != 0);
    } else {
        CHECK(named(key, "particles") &&
              event(fixture, fixture->output[1] == 4 ? 5u : 12u));
        *output = table(&fixture->particle_table);
    }
    return true;
}

static bool sound(void *context, AbDataValue object, AbDataValue raw,
                  AbDataValue *output, AbError *error) {
    Fixture *fixture = context;
    (void)error;
    CHECK(object.table == &fixture->body && raw.table == &fixture->raw &&
          event(fixture, 6));
    *output = (AbDataValue){0};
    return true;
}

static bool random_value(void *context, float lower, float upper, AbDataValue *output,
                         AbError *error) {
    Fixture *fixture = context;
    (void)error;
    CHECK(lower == 12 && upper == 20 && event(fixture, 8));
    *output = (AbDataValue){.kind = AB_DATA_NUMBER, .bits = 0x41880000u};
    return true;
}

static bool update(void *context, AbDataValue object, AbError *error) {
    Fixture *fixture = context;
    (void)error;
    return object.table == &fixture->body && event(fixture, 9);
}

static bool notify(void *context, AbDataValue object, AbError *error) {
    Fixture *fixture = context;
    (void)error;
    CHECK(object.table == &fixture->body && event(fixture, 10));
    if (fixture->input[1] & 64u) {
        fixture->body_fields[7].value.bits ^= 1u;
        fixture->body_fields[6].value.bits ^= 1u;
    }
    return true;
}

static bool functions(void *context, AbDataValue object, AbUiPointerValue *output,
                      AbError *error) {
    Fixture *fixture = context;
    (void)error;
    CHECK(object.table == &fixture->body);
    *output = (AbUiPointerValue){0};
    return true;
}

static bool new_particles(void *context, AbSpan key, AbDataValue descriptor, bool first,
                          bool second, AbError *error) {
    Fixture *fixture = context;
    (void)error;
    return named(key, "queued") && descriptor.kind == AB_DATA_BOOLEAN &&
           !descriptor.bits && !first && !second && event(fixture, 13);
}

static bool old_particles(void *context, AbSpan key, AbDataValue particles,
                          AbDataValue amount, bool first, bool second, AbError *error) {
    Fixture *fixture = context;
    (void)error;
    return named(key, "queued") && particles.table == &fixture->particle_table &&
           amount.kind == AB_DATA_NUMBER && !amount.bits && !first && !second &&
           event(fixture, 14);
}

static bool remove_object(void *context, AbSpan key, AbError *error) {
    Fixture *fixture = context;
    CHECK(named(key, "queued") && event(fixture, 15));
    bool removed = false;
    return ab_game_object_remove(&fixture->removal, key, &removed, error) && removed;
}

static bool pig(void *context, AbSpan key, AbError *error) {
    Fixture *fixture = context;
    (void)error;
    CHECK(named(key, "queued") && event(fixture, 16));
    if (fixture->input[1] & 128u) {
        fixture->body_fields[6].value = boolean(true);
        fixture->cleanup_state.level_name = text("renamed");
        fixture->body_fields[3].value =
            (AbDataValue){.kind = AB_DATA_NUMBER, .bits = 0x40400000u};
        fixture->body_fields[4].value =
            (AbDataValue){.kind = AB_DATA_NUMBER, .bits = 0x40800000u};
    }
    return true;
}

static bool gift(void *context, AbDataValue level, AbDataValue x, AbDataValue y,
                 AbError *error) {
    Fixture *fixture = context;
    (void)error;
    bool changed = (fixture->input[1] & 128u) && fixture->body_fields[5].value.bits;
    return level.kind == AB_DATA_STRING &&
           named(level.string, changed ? "renamed" : "level") &&
           x.kind == AB_DATA_NUMBER &&
           x.bits == (changed ? 0x40400000u : 0x3f800000u) &&
           y.kind == AB_DATA_NUMBER &&
           y.bits == (changed ? 0x40800000u : 0xc0000000u) && event(fixture, 17);
}

static bool prepare(Fixture *fixture, const uint32_t input[5]) {
    memset(fixture, 0, sizeof(*fixture));
    memcpy(fixture->input, input, sizeof(fixture->input));
    CHECK(ab_world_open(&fixture->world, default_definition(),
                        prepare_storage(&fixture->storage, 0, 0), NULL));
    fixture->registry.world = &fixture->world;
    CHECK(ab_string_map_open(2, 2, 4096, &fixture->registry.objects, NULL) &&
          ab_string_map_open(0, 0, 4096, &fixture->removal.teleporters, NULL) &&
          ab_string_map_open(1, 1, 4096, &fixture->dead, NULL) &&
          ab_string_map_open(1, 1, 4096, &fixture->goals, NULL));
    const char *const names[] = {"queued", "second"};
    for (size_t index = 0; index < 2; ++index) {
        fixture->physical[index].name = span(names[index]);
        fixture->physical[index].destroy_timer = 0.125f;
        AbBodyDefinition body = {
            .type = AB_BODY_DYNAMIC, .awake = true, .active = true, .gravity_scale = 1};
        CHECK(ab_world_create_body(&fixture->world, &body,
                                   &fixture->physical[index].body, NULL) &&
              ab_string_map_set(fixture->registry.objects, span(names[index]),
                                (uintptr_t)(fixture->physical + index), true, NULL));
    }
    CHECK(ab_classic_joints_open(
        &fixture->joints, &fixture->world,
        (AbClassicJointStorage){&fixture->joint, 1, &fixture->glue, 1}, NULL));
    AbClassicJoint joint = {
        .type = (AbClassicJointType)input[3],
        .name = span("joint"),
        .endpoints = {span(names[input[4]]), span(names[1u - input[4]])},
        .destroy_timer = 0.75f,
        .joint = {.first_body = input[4],
                  .second_body = 1u - input[4],
                  .constraint.kind = (AbJointKind)(input[3] - 1u)}};
    size_t slot;
    CHECK(ab_classic_joints_create(&fixture->joints, &joint, &slot, NULL));
    fixture->removal.objects = &fixture->registry;
    fixture->removal.joints = &fixture->joints;
    fixture->world.locked = input[2] != 0;
    const char *const fields[] = {"name",     "strength",  "definition", "x",
                                  "y",        "levelGoal", "isGiftBox",  "killSilently",
                                  "specialty"};
    for (size_t index = 0; index < 9; ++index) {
        fixture->body_fields[index].key = text(fields[index]);
    }
    fixture->body_fields[0].value = text("body");
    fixture->body_fields[1].value.kind = AB_DATA_NUMBER;
    fixture->body_fields[2].value = text("raw");
    fixture->body_fields[3].value =
        (AbDataValue){.kind = AB_DATA_NUMBER, .bits = 0x3f800000u};
    fixture->body_fields[4].value =
        (AbDataValue){.kind = AB_DATA_NUMBER, .bits = 0xc0000000u};
    fixture->body_fields[5].value = boolean((input[1] & 4u) != 0);
    fixture->body_fields[6].value = boolean((input[1] & 8u) != 0);
    fixture->body_fields[7].value = boolean((input[1] & 1u) != 0);
    fixture->body = (AbDataTable){fixture->body_fields, 9, 9, NULL};
    fixture->body_value = table(&fixture->body);
    fixture->raw_field = (AbDataEntry){text("spriteScore"), boolean(false)};
    fixture->raw = (AbDataTable){&fixture->raw_field, 1, 1, NULL};
    fixture->particle_field = (AbDataEntry){text("removal"), boolean(false)};
    fixture->particle_table = (AbDataTable){&fixture->particle_field, 1, 1, NULL};
    fixture->world_field = fixture->dynamic_field =
        (AbDataEntry){text("queued"), fixture->body_value};
    fixture->world_table = (AbDataTable){&fixture->world_field, 1, 1, NULL};
    fixture->dynamic = (AbDataTable){&fixture->dynamic_field, 1, 1, NULL};
    fixture->object_fields[0] =
        (AbDataEntry){text("world"), table(&fixture->world_table)};
    fixture->object_fields[1] =
        (AbDataEntry){text("dynamic"), table(&fixture->dynamic)};
    fixture->objects = (AbDataTable){fixture->object_fields, 2, 2, NULL};
    fixture->catalog_field = (AbDataEntry){text("raw"), table(&fixture->raw)};
    fixture->catalog = (AbDataTable){&fixture->catalog_field, 1, 1, NULL};
    fixture->blocks_field = (AbDataEntry){text("blocks"), table(&fixture->catalog)};
    fixture->block_table = (AbDataTable){&fixture->blocks_field, 1, 1, NULL};
    CHECK(ab_string_map_set(fixture->dead, span("queued"),
                            (uintptr_t)&fixture->body_value, true, NULL) &&
          ab_string_map_set(fixture->goals, span("queued"), 1, true, NULL));
    fixture->state = (AbBlockRemovalState){table(&fixture->objects), fixture->dead};
    fixture->scores = (AbBlockRemovalScores){.total = {.value = 0, .present = true},
                                             .entries = &fixture->individual,
                                             .capacity = 1};
    fixture->scoring = (AbBlockRemovalScoreHost){.context = fixture,
                                                 .destroy = destroy,
                                                 .definition = definition,
                                                 .score = score,
                                                 .floating = floating,
                                                 .floating_scaling = {true, 1},
                                                 .scores = &fixture->scores};
    fixture->effects = (AbBlockRemovalEffectsHost){.context = fixture,
                                                   .state = &fixture->state,
                                                   .property = property,
                                                   .destroyed_sound = sound,
                                                   .random = random_value,
                                                   .update_destroyed = update};
    fixture->particles = (AbBlockRemovalParticlesHost){.context = fixture,
                                                       .notify = notify,
                                                       .functions = functions,
                                                       .property = property,
                                                       .new_particles = new_particles,
                                                       .old_particles = old_particles};
    fixture->detach = (AbBlockRemovalDetachHost){fixture, table(&fixture->block_table),
                                                 remove_object};
    fixture->cleanup_state =
        (AbBlockRemovalCleanupState){&fixture->state, fixture->goals, text("level")};
    fixture->cleanup = (AbBlockRemovalCleanupHost){fixture, pig, gift};
    fixture->pass = (AbBlockRemovalPassHost){.scoring = &fixture->scoring,
                                             .effects = &fixture->effects,
                                             .particles = &fixture->particles,
                                             .detach = &fixture->detach,
                                             .cleanup_state = &fixture->cleanup_state,
                                             .cleanup = &fixture->cleanup};
    return true;
}

static void close_fixture(Fixture *fixture) {
    fixture->world.locked = false;
    ab_string_map_close(fixture->registry.objects);
    ab_string_map_close(fixture->removal.teleporters);
    ab_string_map_close(fixture->dead);
    ab_string_map_close(fixture->goals);
    ab_world_close(&fixture->world);
}

static bool snapshot(Fixture *fixture) {
    uint32_t *output = fixture->output;
    output[26] = bits(fixture->scores.total.value);
    output[27] = (uint32_t)fixture->scores.count;
    output[28] = bits(fixture->individual);
    for (size_t index = 0; index < 3; ++index) {
        output[29 + index] = fixture->body_fields[5 + index].value.bits;
    }
    output[32] = named(fixture->body_fields[0].value.string, "renamed");
    uintptr_t handle = 0;
    bool present = false;
    CHECK(ab_string_map_get(fixture->dead, span("queued"), &handle, &present, NULL));
    output[33] = present;
    output[34] = fixture->world_field.value.kind != AB_DATA_NIL;
    output[35] = fixture->dynamic_field.value.kind != AB_DATA_NIL;
    CHECK(ab_string_map_get(fixture->goals, span("queued"), &handle, &present, NULL));
    output[36] = present;
    AbStringMapLayout layout;
    CHECK(ab_string_map_layout(fixture->registry.objects, &layout));
    output[37] = (uint32_t)layout.count;
    output[38] = (uint32_t)fixture->world.body_count;
    output[39] = (uint32_t)fixture->world.joint_count;
    output[40] = (uint32_t)fixture->joints.joint_count;
    output[41] = (uint32_t)fixture->joints.glue_count;
    output[42] = fixture->physical[1].delayed_destroy;
    output[43] = bits(fixture->physical[1].destroy_timer);
    return true;
}

static bool read_words(FILE *file, uint32_t *output, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        CHECK(fscanf(file, "%8" SCNx32, output + index) == 1);
    }
    return fgetc(file) == '\n';
}

static bool native_case(FILE *file, uint32_t sample) {
    uint32_t input[5], generated[5], expected[48];
    block_removal_pass_case(sample, generated);
    CHECK(read_words(file, input, 5) && !memcmp(input, generated, sizeof(input)) &&
          read_words(file, expected, 48));
    Fixture fixture;
    AbError error = {0};
    bool okay = prepare(&fixture, input) &&
                ab_block_removal_pass(&fixture.state, &fixture.pass, &error) &&
                snapshot(&fixture);
    if (!okay) {
        fprintf(stderr,
                "Coupled native removal %" PRIu32 " stopped after %" PRIu32
                " events: %s\n",
                sample, fixture.output[1], error.message);
    }
    for (size_t index = 0; okay && index < 48; ++index) {
        if (fixture.output[index] != expected[index]) {
            fprintf(stderr,
                    "Coupled native removal %" PRIu32 " field %zu: %08" PRIx32
                    " != %08" PRIx32 "\n",
                    sample, index, fixture.output[index], expected[index]);
            okay = false;
        }
    }
    close_fixture(&fixture);
    return okay;
}

static bool native_checks(FILE *file) {
    char line[96];
    CHECK(fgets(line, sizeof(line), file) &&
          !strcmp(line, "AB_ANDROID_BLOCK_REMOVAL_PASS_V1 512\n"));
    for (uint32_t sample = 0; sample < 512; ++sample) {
        CHECK(native_case(file, sample));
    }
    CHECK(fgets(line, sizeof(line), file) &&
          !strcmp(line, "AB block removal pass reference complete\n") &&
          fgetc(file) == EOF && !ferror(file));
    puts(
        "Matched 27,136 words from 512 complete original removal passes: callback "
        "order, "
        "live mutations, scoring, native physical/joint cleanup and final script maps");
    return true;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        return 2;
    }
    FILE *file = fopen(argv[1], "r");
    if (!file) {
        return 1;
    }
    bool okay = native_checks(file);
    if (fclose(file)) {
        okay = false;
    }
    return okay ? 0 : 1;
}
