#include "ab/classic_forces.h"
#include "ab/object_force_data.h"
#include "ab/target_data.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Force data failed at %d: %s\n", __LINE__, #condition);    \
            return false;                                                              \
        }                                                                              \
    } while (0)

static AbSpan span(const char *text) {
    return (AbSpan){(const uint8_t *)text, strlen(text)};
}

static AbDataValue string(const char *text) {
    return (AbDataValue){.kind = AB_DATA_STRING, .string = span(text)};
}

static AbDataValue number(float value) {
    AbDataValue result = {.kind = AB_DATA_NUMBER};
    memcpy(&result.bits, &value, sizeof(value));
    return result;
}

static bool same_span(AbSpan first, AbSpan second) {
    return first.size == second.size &&
           (!first.size || memcmp(first.data, second.data, first.size) == 0);
}

static bool data_edges(void) {
    AbDataTable source = {0};
    AbObjectDefinition definition = {.source = &source};
    AbObjectForceData data;
    CHECK(ab_object_force_data(&definition, 3, true, (AbDataValue){0}, &data, NULL));
    CHECK(!data.prepare_forces && !data.interval_present &&
          data.forces.object_mass == 3 && data.forces.gravity_enabled &&
          data.sound.kind == AB_FORCE_SOUND_NONE);
    const char *const gates[] = {"forceX",
                                 "forceY",
                                 "forceRelative",
                                 "airResistance",
                                 "frictionXRelative",
                                 "frictionYRelative"};
    AbDataEntry entry = {0};
    source = (AbDataTable){&entry, 1, 1, NULL};
    for (size_t index = 0; index < 6; ++index) {
        entry.key = string(gates[index]);
        entry.value = number(-0.0f);
        CHECK(
            ab_object_force_data(&definition, 3, false, (AbDataValue){0}, &data, NULL));
        CHECK(data.prepare_forces && !data.forces.gravity_enabled);
        entry.value = (AbDataValue){.kind = AB_DATA_BOOLEAN};
        CHECK(
            ab_object_force_data(&definition, 3, true, (AbDataValue){0}, &data, NULL) &&
            !data.prepare_forces);
        entry.value.bits = 1;
        CHECK(
            ab_object_force_data(&definition, 3, true, (AbDataValue){0}, &data, NULL) &&
            data.prepare_forces);
        AbObjectForceData saved = data;
        entry.value = string("0");
        CHECK(!ab_object_force_data(&definition, 3, true, (AbDataValue){0}, &data,
                                    NULL) &&
              memcmp(&data, &saved, sizeof(data)) == 0);
        entry.value = number(NAN);
        CHECK(
            !ab_object_force_data(&definition, 3, true, (AbDataValue){0}, &data, NULL));
    }
    AbDataEntry material_entry = {string("forceX"), number(2)};
    AbDataTable material = {&material_entry, 1, 1, NULL};
    source.count = 0;
    definition.material = &material;
    CHECK(ab_object_force_data(&definition, 3, true, (AbDataValue){0}, &data, NULL) &&
          data.forces.force_x_present && data.forces.force.x == 2);
    source.count = 1;
    entry = (AbDataEntry){string("forceX"), {.kind = AB_DATA_BOOLEAN}};
    CHECK(ab_object_force_data(&definition, 3, true, (AbDataValue){0}, &data, NULL) &&
          !data.forces.force_x_present && !data.prepare_forces);
    entry = (AbDataEntry){string("disableForce"), number(1)};
    CHECK(ab_object_force_data(&definition, 3, true, (AbDataValue){0}, &data, NULL) &&
          !data.forces.disabled);
    entry.value = (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1};
    CHECK(ab_object_force_data(&definition, 3, true, (AbDataValue){0}, &data, NULL) &&
          data.forces.disabled);
    AbDataEntry intervals[] = {{string("applyForceInterval"), number(0)},
                               {string("spriteWhenForceApplied"), string("active")}};
    source = (AbDataTable){intervals, 2, 2, NULL};
    CHECK(!ab_object_force_data(&definition, 3, true, (AbDataValue){0}, &data, NULL));
    CHECK(ab_object_force_data(&definition, 3, true, string("rest"), &data, NULL) &&
          data.interval_present && !data.interval.timer_present &&
          !data.interval.original_present &&
          same_span(data.interval.sprite, span("rest")) &&
          same_span(data.interval.applied_sprite, span("active")));
    CHECK(!ab_object_force_data(NULL, 3, true, string("rest"), &data, NULL));
    CHECK(!ab_object_force_data(&definition, -1, true, string("rest"), &data, NULL));
    CHECK(!ab_object_force_data(&definition, 3, true, string("rest"), NULL, NULL));
    AbDataEntry sounds[] = {{number(3), string("third")},
                            {number(1), string("")},
                            {number(2), string("second")}};
    AbDataTable choices = {sounds, 3, 3, NULL};
    AbSpan ordered[3];
    CHECK(ab_object_force_choices(&choices, ordered, 3, NULL) && ordered[0].size == 0 &&
          same_span(ordered[1], span("second")) &&
          same_span(ordered[2], span("third")));
    CHECK(!ab_object_force_choices(&choices, ordered, 2, NULL));
    sounds[0].key = number(2);
    CHECK(!ab_object_force_choices(&choices, ordered, 3, NULL));
    sounds[0].key = number(3.5f);
    CHECK(!ab_object_force_choices(&choices, ordered, 3, NULL));
    sounds[0].key = number(NAN);
    CHECK(!ab_object_force_choices(&choices, ordered, 3, NULL));
    sounds[0].key = number(0);
    CHECK(!ab_object_force_choices(&choices, ordered, 3, NULL));
    sounds[0].key = string("3");
    CHECK(!ab_object_force_choices(&choices, ordered, 3, NULL));
    sounds[0].key = number(3);
    sounds[0].value.string = (AbSpan){NULL, 1};
    CHECK(!ab_object_force_choices(&choices, ordered, 3, NULL));
    CHECK(!ab_object_force_choices(NULL, ordered, 3, NULL));
    CHECK(!ab_object_force_choices(&choices, NULL, 3, NULL));
    return true;
}

static bool catalog_checks(const AbZip *archive) {
    AbTargetData catalog = {0};
    AbError error = {0};
    uint8_t key[32] = {0};
    bool okay = ab_target_resource_key(archive, 16 * 1024 * 1024, key, &error) &&
                ab_target_data_open(archive, "assets/data/scripts/blocks.lua",
                                    (AbSpan){key, sizeof(key)}, 32 * 1024 * 1024, NULL,
                                    &catalog, &error);
    size_t count = 0, gated = 0, intervals = 0, choices = 0;
    if (!okay) {
        goto release_catalog;
    }
    const AbDataValue *blocks = ab_data_get(catalog.data.globals, string("blocks"));
    if (!blocks || blocks->kind != AB_DATA_TABLE || !blocks->table) {
        okay = false;
        goto release_catalog;
    }
    for (size_t index = 0; index < blocks->table->count; ++index) {
        const AbDataEntry *entry = blocks->table->entries + index;
        AbObjectDefinition definition;
        AbObjectForceData data;
        AbSpan sounds[8];
        if (entry->key.kind != AB_DATA_STRING ||
            !ab_object_definition_open(catalog.data.globals, entry->key.string, NULL,
                                       &definition, &error) ||
            !ab_object_force_data(&definition, 1, true, string("initial"), &data,
                                  &error) ||
            (data.sound_choices &&
             !ab_object_force_choices(data.sound_choices, sounds, 8, &error))) {
            okay = false;
            goto release_catalog;
        }
        ++count;
        gated += data.prepare_forces;
        intervals += data.interval_present;
        choices += data.sound.count;
    }
    okay = count == 618 && gated == 25 && intervals == 1 && choices == 3;
    if (okay) {
        puts("All 618 APK definitions initialize: 25 force gates, one interval, three "
             "sound choices");
    }

release_catalog:
    memset(key, 0, sizeof(key));
    ab_target_data_close(&catalog);
    if (!okay) {
        fprintf(stderr, "Force catalog: %s\n", error.message);
    }
    return okay;
}

static bool compare_native(FILE *file, AbClassicLevel *level, AbClassicForces *forces) {
    char header[80];
    AbClassicLevelView source = ab_classic_level_view(level);
    AbClassicForceView view = ab_classic_forces_view(forces);
    CHECK(view.count == 23 && fgets(header, sizeof(header), file) &&
          strcmp(header, "AB_CLASSIC_FORCE_PROPERTIES_V1 23\n") == 0);
    for (size_t index = 0; index < view.count; ++index) {
        char name[128];
        uint32_t words[26];
        CHECK(fscanf(file, "%127s", name) == 1 && fgetc(file) == '\n');
        for (size_t field = 0; field < 26; ++field) {
            CHECK(fscanf(file, "%" SCNx32, words + field) == 1);
        }
        CHECK(fgetc(file) == '\n');
        const AbGamePhysicsObject *row = view.objects + index;
        const AbLevelObject *object =
            source.bodies->objects + view.source_indices[index];
        CHECK(same_span(object->name, span(name)) &&
              row->prepare_forces == (words[0] != 0) && row->gravity_enabled &&
              *row->gravity_enabled == (words[1] != 0) &&
              number(row->forces.object_mass).bits == words[2]);
        float actual[] = {row->forces.force.x,          row->forces.force.y,
                          row->forces.relative_force,   row->forces.target_velocity,
                          row->friction.air_resistance, row->friction.relative.x,
                          row->friction.relative.y};
        bool present[] = {row->forces.force_x_present, row->forces.force_y_present,
                          row->forces.relative_present, row->forces.target_present};
        for (size_t field = 0; field < 7; ++field) {
            uint32_t tag = words[3 + field * 2], bits = words[4 + field * 2];
            CHECK(tag == 0 || tag == 1 || tag == 3);
            CHECK(number(actual[field]).bits == (tag == 3 ? bits : 0));
            CHECK(field >= 4 || present[field] == (tag == 3));
        }
        CHECK(row->forces.disabled == (words[17] == 1 && words[18] != 0) &&
              (row->interval != NULL) == (words[19] == 3) &&
              row->sound->gain_present == (words[21] == 3) &&
              number(row->sound->gain).bits == (words[21] == 3 ? words[22] : 0));
        /* This retained idle snapshot has no interval/sound state. */
        CHECK(words[23] == 0 && words[24] == 0 && words[25] == 0 && !row->interval &&
              row->sound->kind == AB_FORCE_SOUND_NONE);
    }
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB classic force properties complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    puts("All 23 native object-map entries match cached mass, gates, flags and initial "
         "numeric force fields");
    return true;
}

typedef struct {
    size_t object;
    AbObjectForceCall calls[2];
    size_t count;
} ForceHostRecord;

static bool record_resource(void *context, size_t object, const AbObjectForceCall *call,
                            AbError *error) {
    ForceHostRecord *record = context;
    if (object != record->object || record->count >= 2) {
        return ab_fail(error, "Unexpected force owner resource request");
    }
    record->calls[record->count++] = *call;
    return true;
}

/* Exercise the owner boundary using the APK's interval definition on an
 * existing body. This is a C binding check, not another native capture. */
static bool resource_binding_checks(AbClassicLevel *level) {
    AbClassicLevelView source = ab_classic_level_view(level);
    const AbDataValue *blocks = ab_data_get(source.catalog, string("blocks"));
    CHECK(blocks && blocks->kind == AB_DATA_TABLE && blocks->table);
    AbSpan interval_name = {0};
    for (size_t index = 0; index < blocks->table->count; ++index) {
        const AbDataEntry *entry = blocks->table->entries + index;
        AbObjectDefinition definition;
        CHECK(entry->key.kind == AB_DATA_STRING &&
              ab_object_definition_open(source.catalog, entry->key.string, NULL,
                                        &definition, NULL));
        if (ab_object_property(&definition, span("applyForceInterval")).kind ==
            AB_DATA_NUMBER) {
            interval_name = entry->key.string;
        }
    }
    CHECK(interval_name.size);
    AbLevelObject *object = source.bodies->objects;
    AbSpan definition = object->definition_name, name = object->name;
    object->definition_name = interval_name;
    object->name = span("z_binding_test");
    AbClassicForces *forces = NULL;
    AbRandom48 random = {.value = 17}, expected = random;
    ForceHostRecord host = {0};
    CHECK(!ab_classic_forces_open(level, (AbClassicForceLimits){.storage_bytes = 65536},
                                  &random, record_resource, &host, &forces, NULL) &&
          !forces && random.value == 17);
    CHECK(ab_classic_forces_open(
        level,
        (AbClassicForceLimits){.storage_bytes = 65536,
                               .sound_choices = 3,
                               .object_capacity = source.bodies->capacity},
        &random, record_resource, &host, &forces, NULL));
    AbClassicForceView view = ab_classic_forces_view(forces);
    AbGamePhysicsObject *row = view.objects + view.count - 1;
    CHECK(view.source_indices[view.count - 1] == 0 && row->prepare_forces &&
          row->interval && !row->interval->timer_present &&
          row->sound->kind == AB_FORCE_SOUND_ARRAY && row->sound->count == 3 &&
          random.value == 17);
    uint32_t draw;
    CHECK(ab_random48_next(&expected, &draw));
    AbGamePhysicsPrepare prepare = {.objects = view.objects, .count = view.count};
    CHECK(ab_game_physics_before_step(&prepare, source.world,
                                      row->interval->interval + 1, NULL));
    CHECK(host.count == 2 && host.calls[0].effect == AB_FORCE_EFFECT_SPRITE &&
          same_span(host.calls[0].name, row->interval->applied_sprite) &&
          host.calls[1].effect == AB_FORCE_EFFECT_AUDIO &&
          same_span(host.calls[1].name, row->sound->choices[draw % 3]) &&
          host.calls[1].gain == row->sound->gain && random.value == expected.value &&
          row->interval->timer_present && row->interval->original_present &&
          same_span(row->interval->original_sprite,
                    source.initial_sprites[0].sprite.string));
    AbObjectForceInterval *stable_interval = row->interval;
    const AbObjectForceSound *stable_sound = row->sound;
    void *stable_context = row->resource_context;
    float retained_timer = stable_interval->timer;
    size_t initial_count = view.count;
    object->definition_name = definition;
    object->name = name;
    const char *const names[] = {"0_dynamic_circle", "zz_dynamic_circle",
                                 "m_dynamic_circle"};
    size_t created[3];
    size_t bytes = ab_classic_forces_bytes(forces);
    for (size_t index = 0; index < 3; ++index) {
        AbGameCircleInput circle = {.name = span(names[index]),
                                    .position = {100 + (float)index * 4, -10},
                                    .radius = 1,
                                    .density = 1};
        CHECK(ab_classic_level_create_circle(level, circle, created + index, NULL) &&
              created[index] == initial_count + index);
        /* The source may copy a force-bearing definition after native circle
         * creation. That does not rerun ObjectCreation's native setters. */
        source.bodies->objects[created[index]].definition_name = interval_name;
        AbObjectPropertyAccess access = {
            .physics_object = {
                .kind = AB_DATA_TABLE,
                .table = &source.properties->objects[created[index]].physical}};
        CHECK(ab_object_property_set(
                  &access, span("definition"),
                  (AbDataValue){.kind = AB_DATA_STRING, .string = interval_name},
                  NULL) &&
              !ab_classic_forces_register_circle(forces, created[index] + 1, NULL));
        source.world->locked = true;
        CHECK(!ab_classic_forces_register_circle(forces, created[index], NULL));
        source.world->locked = false;
        CHECK(ab_classic_forces_register_circle(forces, created[index], NULL) &&
              !ab_classic_forces_register_circle(forces, created[index], NULL));
        view = ab_classic_forces_view(forces);
        CHECK(view.count == initial_count + index + 1 &&
              ab_classic_forces_bytes(forces) == bytes &&
              random.value == expected.value &&
              stable_interval->timer == retained_timer);
        size_t rank = 0;
        while (rank < view.count && view.source_indices[rank] != created[index])
            ++rank;
        CHECK(rank < view.count && !view.objects[rank].prepare_forces &&
              !view.objects[rank].interval &&
              view.objects[rank].sound->kind == AB_FORCE_SOUND_NONE &&
              !view.objects[rank].forces.force_x_present &&
              !view.objects[rank].forces.force_y_present &&
              view.objects[rank].gravity_enabled ==
                  &source.bodies->objects[created[index]].physics.gravity_enabled);
        CHECK(index == 0   ? rank == 0
              : index == 1 ? rank + 1 == view.count
                           : rank > 0 && rank + 1 < view.count);
    }
    size_t rank = 0;
    while (rank < view.count && view.source_indices[rank] != 0)
        ++rank;
    CHECK(rank < view.count && view.objects[rank].interval == stable_interval &&
          view.objects[rank].sound == stable_sound &&
          view.objects[rank].resource_context == stable_context);
    CHECK(ab_world_clear_forces(source.world, NULL));
    source.bodies->objects[created[0]].physics.gravity_enabled = false;
    host.count = 0;
    prepare.objects = view.objects;
    prepare.count = view.count;
    CHECK(ab_random48_next(&expected, &draw) &&
          ab_game_physics_before_step(&prepare, source.world,
                                      stable_interval->interval + 1, NULL) &&
          host.count == 2 && random.value == expected.value &&
          same_span(host.calls[1].name, stable_sound->choices[draw % 3]));
    AbBodyMotion *new_body =
        &source.world->storage.bodies[source.bodies->objects[created[0]].physics.body]
             .motion;
    CHECK(new_body->force.x == 0 &&
          new_body->force.y ==
              -(new_body->mass.mass * source.world->definition.gravity.y));
    source.bodies->objects[created[0]].physics.gravity_enabled = true;
    CHECK(ab_world_clear_forces(source.world, NULL));
    AbQueuedForce request = {span(names[0]), {2, 3}, {100, -10}};
    AbForceQueue queue = {&request, 1, 1};
    CHECK(ab_classic_forces_apply_queue(forces, &queue, NULL) &&
          new_body->force.x == 2 && new_body->force.y == 3 && new_body->torque == 0);
    CHECK(ab_classic_level_forget_object(level, created[0], NULL) &&
          ab_world_clear_forces(source.world, NULL) &&
          ab_classic_forces_apply_queue(forces, &queue, NULL) &&
          new_body->force.x == 0 && new_body->force.y == 0);
    ab_classic_forces_close(forces);
    object->definition_name = definition;
    object->name = name;
    puts("APK interval resources retain source indices, numeric sound order and shared "
         "RNG");
    return true;
}

static bool native_checks(const AbZip *archive, const char *reference) {
    const char *paths[] = {"assets/data/images/base/INGAME_BLOCKS_1.dat",
                           "assets/data/images/base/INGAME_PIGS_NEW_SHEET_1.dat",
                           "assets/data/images/base/INGAME_BLOCKS_3.dat"};
    AbClassicLevelAssets assets = {.level_path = "assets/data/levels/pack1/Level1.lua",
                                   .catalog_path = "assets/data/scripts/blocks.lua",
                                   .sprite_paths = paths,
                                   .sprite_sheet_count = 3,
                                   .resource_bytes = 16 * 1024 * 1024,
                                   .placement_scale = 1,
                                   .physics_scale = 0.05f,
                                   .derive_ground_position = true,
                                   .derive_world_gravity = true};
    AbWorldDefinition definition = {.broadphase_limits = {128, 256, 256},
                                    .maximum_translation = 2,
                                    .maximum_translation_squared = 4,
                                    .clear_forces = true};
    AbClassicLevel *level = NULL;
    AbClassicForces *forces = NULL;
    AbRandom48 random = {.value = 17};
    AbError error = {0};
    FILE *file = fopen(reference, "rb");
    bool okay =
        file &&
        ab_classic_level_load(archive, &assets, definition,
                              (AbClassicLevelLimits){.objects = 32,
                                                     .contacts = 128,
                                                     .storage_bytes = 1024 * 1024},
                              true, &level, &error) &&
        ab_classic_forces_open(
            level, (AbClassicForceLimits){.storage_bytes = 65536, .sound_choices = 32},
            &random, NULL, NULL, &forces, &error) &&
        random.value == 17 && compare_native(file, level, forces) &&
        resource_binding_checks(level);
    if (file) {
        fclose(file);
    }
    ab_classic_forces_close(forces);
    ab_classic_level_close(level);
    if (!okay) {
        fprintf(stderr, "Native force properties: %s\n", error.message);
    }
    return okay;
}

int main(int argc, char **argv) {
    if (!data_edges()) {
        return 1;
    }
    if (argc == 1) {
        puts("Initial force fields, truthiness, coercion limits and sound ordering "
             "pass");
        return 0;
    }
    if ((argc != 3 && argc != 4) || strcmp(argv[1], "--apk") != 0) {
        return 2;
    }
    AbBuffer bytes = {0};
    AbZip archive = {0};
    AbError error = {0};
    bool okay = ab_file_read(argv[2], 64 * 1024 * 1024, &bytes, &error) &&
                ab_zip_open((AbSpan){bytes.data, bytes.size}, &archive, &error) &&
                catalog_checks(&archive) &&
                (argc != 4 || native_checks(&archive, argv[3]));
    ab_zip_close(&archive);
    ab_buffer_free(&bytes);
    if (!okay) {
        fprintf(stderr, "%s\n", error.message);
    }
    return okay ? 0 : 1;
}
