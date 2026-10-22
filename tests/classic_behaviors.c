#include "ab/classic_behaviors.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(value)                                                                   \
    do {                                                                               \
        if (!(value)) {                                                                \
            fprintf(stderr, "Classic behavior check at %d: %s\n", __LINE__, #value);   \
            return false;                                                              \
        }                                                                              \
    } while (0)

#include "classic_camera_level.h"

typedef struct {
    ClassicCameraLevelInput input;
    AbClassicLevel *level;
    AbClassicBehaviors *behaviors;
    AbClassicLevelView view;
    AbDataEntry configurations[2];
    AbDataTable configuration;
    AbDataTableLayout layout;
    AbLuaData catalog;
    AbDataValue flying;
    AbMuseumBirdOperation calls[16];
    size_t count;
    bool fail_camera, replace_flying;
} BehaviorFixture;

static bool service(void *context, const AbMuseumBirdCall *call, AbError *error) {
    (void)error;
    BehaviorFixture *fixture = context;
    CHECK(fixture->count < 16);
    fixture->calls[fixture->count++] = call->operation;
    if (call->operation == AB_MUSEUM_CAMERA_SEQUENCE) {
        CHECK(call->sequence &&
              !strcmp(call->sequence, "FollowShotBirdCameraSequence"));
        CHECK(!ab_data_get(&fixture->view.properties->objects[0].physical,
                           classic_camera_text("launchVelocity")));
        return !fixture->fail_camera;
    }
    CHECK(call->object.kind == AB_DATA_TABLE);
    if (fixture->replace_flying && call->operation == AB_MUSEUM_TRAJECTORY_REMOVE)
        fixture->flying = (AbDataValue){0};
    return true;
}

static bool open_fixture(BehaviorFixture *fixture) {
    *fixture = (BehaviorFixture){0};
    CHECK(classic_camera_open_level(&fixture->input, &fixture->level));
    fixture->view = ab_classic_level_view(fixture->level);
    const char *names[] = {"MuseumBirdTrajectoryRenderer",
                           "MuseumBirdCameraFollowBehavior"};
    for (size_t index = 0; index < 2; ++index)
        fixture->configurations[index] =
            (AbDataEntry){classic_camera_text(names[index]),
                          classic_camera_table(&fixture->input.empty)};
    fixture->configuration = (AbDataTable){fixture->configurations, 2, 2, NULL};
    /* Resource mutation occurs after body construction. The registry must read
     * the live definition and constructor metadata, not cache a body shortcut. */
    fixture->input.definition_entries[5] =
        (AbDataEntry){classic_camera_text("behaviors"),
                      classic_camera_table(&fixture->configuration)};
    fixture->layout = (AbDataTableLayout){.table = &fixture->configuration,
                                          .hash_capacity = 2,
                                          .no_nil_assignments = true};
    fixture->catalog =
        (AbLuaData){.globals = &fixture->input.catalog, .layouts = &fixture->layout};
    fixture->flying =
        classic_camera_table(&fixture->view.properties->objects[0].physical);
    return true;
}

static AbClassicBehaviorConfig configuration(BehaviorFixture *fixture, size_t bytes) {
    return (AbClassicBehaviorConfig){.catalog_data = &fixture->catalog,
                                     .flying_bird = &fixture->flying,
                                     .context = fixture,
                                     .call = service,
                                     .storage_bytes = bytes};
}

static bool clean_tables(BehaviorFixture *fixture) {
    for (size_t index = 0; index < fixture->view.bodies->count; ++index) {
        AbDataTable *table = &fixture->view.properties->objects[index].behaviors;
        CHECK(!table->entries && !table->count && !table->capacity);
    }
    return true;
}

static bool projected(AbClassicLevelView view, size_t expected_instances) {
    size_t instances = 0;
    for (size_t index = 0; index < view.bodies->count; ++index) {
        AbLevelPropertyObject *object = view.properties->objects + index;
        const AbDataValue *groups =
            ab_data_get(&object->script, classic_camera_text("behaviors"));
        CHECK(groups && groups->kind == AB_DATA_TABLE &&
              groups->table == &object->behaviors);
        for (size_t slot = 0; slot < groups->table->count; ++slot) {
            AbDataEntry *group = groups->table->entries + slot;
            CHECK(group->key.kind == AB_DATA_STRING &&
                  group->value.kind == AB_DATA_TABLE && group->value.table->count == 1);
            AbDataValue instance = group->value.table->entries[0].value;
            CHECK(instance.kind == AB_DATA_TABLE);
            const AbDataValue *name =
                ab_data_get(instance.table, classic_camera_text("name"));
            const AbDataValue *script =
                ab_data_get(instance.table, classic_camera_text("gameObject"));
            const AbDataValue *physical =
                ab_data_get(instance.table, classic_camera_text("physicsObject"));
            CHECK(
                name && name->kind == AB_DATA_STRING &&
                name->string.size == group->key.string.size &&
                !memcmp(name->string.data, group->key.string.data, name->string.size));
            CHECK(script && script->kind == AB_DATA_TABLE &&
                  script->table == &object->script && physical &&
                  physical->kind == AB_DATA_TABLE &&
                  physical->table == &object->physical);
            ++instances;
        }
    }
    CHECK(instances == expected_instances);
    return true;
}

static bool position_matches(AbClassicLevelView view, size_t index) {
    AbDataTable *physical = &view.properties->objects[index].physical;
    const AbDataValue *x = ab_data_get(physical, classic_camera_text("x"));
    const AbDataValue *y = ab_data_get(physical, classic_camera_text("y"));
    AbVec2 position = view.character_objects[index].jump.position;
    CHECK(x && x->kind == AB_DATA_NUMBER &&
          x->bits == classic_camera_number(position.x).bits);
    CHECK(y && y->kind == AB_DATA_NUMBER &&
          y->bits == classic_camera_number(position.y).bits);
    return true;
}

static bool edge_checks(void) {
    BehaviorFixture fixture;
    CHECK(open_fixture(&fixture));
    AbClassicBehaviorConfig config = configuration(&fixture, 1024 * 1024);
    CHECK(ab_classic_behaviors_open(fixture.level, config, &fixture.behaviors, NULL));
    CHECK(projected(fixture.view, 4));
    size_t bytes = ab_classic_behaviors_bytes(fixture.behaviors);
    CHECK(bytes && bytes <= config.storage_bytes);
    AbClassicBehaviors *duplicate = NULL;
    CHECK(!ab_classic_behaviors_open(fixture.level, config, &duplicate, NULL) &&
          !duplicate);
    CHECK(ab_classic_behaviors_launch(fixture.behaviors, 0, (AbVec2){1, 2}, NULL));
    CHECK(fixture.count == 2 && fixture.calls[0] == AB_MUSEUM_TRAJECTORY_ADD &&
          fixture.calls[1] == AB_MUSEUM_CAMERA_SEQUENCE);
    CHECK(ab_level_property_set(
        fixture.view.properties, 1, classic_camera_text("controllable").string,
        (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1}, NULL));
    AbBlockCollisionNotice notice = {.normal_present = true, .force = 5};
    CHECK(ab_classic_behaviors_collision(fixture.behaviors, 0, 1, &notice, NULL));
    CHECK(fixture.count == 2);
    CHECK(ab_classic_behaviors_collision(fixture.behaviors, 0, 2, &notice, NULL));
    CHECK(fixture.count == 4 && fixture.calls[2] == AB_MUSEUM_TRAJECTORY_REMOVE &&
          fixture.calls[3] == AB_MUSEUM_TRAJECTORY_KEEP);
    CHECK(ab_classic_behaviors_destroy(fixture.behaviors, 0, NULL) &&
          fixture.count == 4);
    CHECK(ab_classic_behaviors_destroy(fixture.behaviors, 2, NULL) &&
          fixture.count == 4);
    CHECK(!ab_classic_behaviors_launch(fixture.behaviors, 3, (AbVec2){0}, NULL));
    CHECK(!ab_classic_behaviors_collision(fixture.behaviors, 0, 3, &notice, NULL));
    CHECK(ab_classic_behaviors_launch(fixture.behaviors, 0, (AbVec2){0}, NULL));
    fixture.replace_flying = true;
    CHECK(ab_classic_behaviors_destroy(fixture.behaviors, 0, NULL));
    CHECK(fixture.count == 7 && fixture.calls[6] == AB_MUSEUM_TRAJECTORY_REMOVE);
    ab_classic_behaviors_close(fixture.behaviors);
    fixture.behaviors = NULL;
    CHECK(clean_tables(&fixture));
    CHECK(!ab_classic_behaviors_open(fixture.level, configuration(&fixture, bytes - 1),
                                     &fixture.behaviors, NULL));
    CHECK(!fixture.behaviors && clean_tables(&fixture));
    CHECK(ab_classic_behaviors_open(fixture.level, configuration(&fixture, bytes),
                                    &fixture.behaviors, NULL));
    fixture.count = 0;
    fixture.fail_camera = true;
    CHECK(!ab_classic_behaviors_launch(fixture.behaviors, 0, (AbVec2){0}, NULL));
    CHECK(fixture.count == 2);
    fixture.fail_camera = false;
    CHECK(ab_classic_behaviors_destroy(fixture.behaviors, 0, NULL));
    CHECK(fixture.count == 3 && fixture.calls[2] == AB_MUSEUM_TRAJECTORY_REMOVE);
    ab_classic_behaviors_close(fixture.behaviors);
    fixture.behaviors = NULL;
    CHECK(clean_tables(&fixture));
    AbDataEntry first = fixture.configurations[0];
    fixture.configurations[0] = fixture.configurations[1];
    fixture.configurations[1] = first;
    CHECK(ab_classic_behaviors_open(fixture.level, config, &fixture.behaviors, NULL));
    fixture.count = 0;
    fixture.fail_camera = true;
    CHECK(!ab_classic_behaviors_launch(fixture.behaviors, 0, (AbVec2){0}, NULL));
    CHECK(fixture.count == 1 && fixture.calls[0] == AB_MUSEUM_CAMERA_SEQUENCE);
    CHECK(ab_classic_behaviors_destroy(fixture.behaviors, 0, NULL) &&
          fixture.count == 1);
    ab_classic_behaviors_close(fixture.behaviors);
    fixture.behaviors = NULL;
    CHECK(clean_tables(&fixture));
    fixture.layout.hash_capacity = 0;
    CHECK(!ab_classic_behaviors_open(fixture.level, config, &fixture.behaviors, NULL));
    CHECK(!fixture.behaviors && clean_tables(&fixture));
    fixture.layout.hash_capacity = 2;
    fixture.configurations[0].key = classic_camera_text("UnknownBehavior");
    CHECK(!ab_classic_behaviors_open(fixture.level, config, &fixture.behaviors, NULL));
    CHECK(!fixture.behaviors && clean_tables(&fixture));
    CHECK(ab_classic_level_set_position(fixture.level, 0, (AbVec2){8, 9}, NULL));
    CHECK(position_matches(fixture.view, 0));
    AbObjectProperties properties = {.pose.position = {-0.0f, 11}};
    CHECK(ab_classic_level_publish_properties(fixture.level, 0, &properties, NULL));
    CHECK(position_matches(fixture.view, 0));
    CHECK(ab_world_destroy_body(fixture.view.world,
                                fixture.view.bodies->objects[0].physics.body, NULL));
    CHECK(ab_classic_level_set_position(fixture.level, 0, (AbVec2){99, 100}, NULL));
    CHECK(position_matches(fixture.view, 0) &&
          fixture.view.character_objects[0].jump.position.y == 11);
    ab_classic_level_close(fixture.level);
    return true;
}

static bool native_registry(FILE *file, AbClassicLevelView view) {
    char header[96];
    CHECK(fgets(header, sizeof(header), file) &&
          !strcmp(header, "AB_LEVEL_NATIVE_BEHAVIORS_V1 23\n"));
    for (size_t cursor = 0; cursor < view.bodies->count; ++cursor) {
        char name[64];
        size_t count;
        CHECK(fscanf(file, "%63s %zu", name, &count) == 2);
        size_t index = view.object_order[cursor];
        AbSpan actual_name = view.bodies->objects[index].name;
        CHECK(actual_name.size == strlen(name) &&
              !memcmp(actual_name.data, name, actual_name.size));
        AbDataTable *groups = &view.properties->objects[index].behaviors;
        CHECK(count == groups->count);
        for (size_t slot = 0; slot < count; ++slot) {
            size_t instances, fields;
            CHECK(fscanf(file, "%63s %zu %zu", name, &instances, &fields) == 3);
            AbDataEntry *group = groups->entries + slot;
            CHECK(group->key.kind == AB_DATA_STRING &&
                  group->key.string.size == strlen(name) &&
                  !memcmp(group->key.string.data, name, group->key.string.size));
            CHECK(group->value.kind == AB_DATA_TABLE &&
                  group->value.table->count == instances && instances == 1);
            AbDataEntry *instance = group->value.table->entries;
            CHECK(instance->key.kind == AB_DATA_NUMBER &&
                  instance->key.bits == UINT32_C(0x3f800000) &&
                  instance->value.kind == AB_DATA_TABLE &&
                  instance->value.table->count == fields && fields == 3);
        }
    }
    CHECK(fgetc(file) == '\n' && fgets(header, sizeof(header), file) &&
          !strcmp(header, "AB level behavior reference complete\n") &&
          fgetc(file) == EOF && !ferror(file));
    puts("All 23 native script tables, group traversal and six instance field/receiver "
         "identities match");
    return true;
}

static bool native_constructor_fields(FILE *file, AbClassicLevelView view) {
    static const char *const fields[] = {
        "controllable", "strength",    "levelGoal",   "startNumber",
        "damageSprite", "blinkSprite", "smileSprite",
    };
    bool seen[23] = {0};
    for (size_t cursor = 0; cursor < view.bodies->count; ++cursor) {
        char name[64];
        CHECK(fscanf(file, "%63s", name) == 1);
        size_t object = 0;
        for (; object < view.bodies->count; ++object) {
            AbSpan actual = view.bodies->objects[object].name;
            if (actual.size == strlen(name) && !memcmp(actual.data, name, actual.size))
                break;
        }
        CHECK(object < 23 && !seen[object]);
        seen[object] = true;
        const AbDataTable *physical = &view.properties->objects[object].physical;
        for (size_t field = 0; field < sizeof(fields) / sizeof(*fields); ++field) {
            char key[64];
            uint32_t tag;
            CHECK(fscanf(file, "%63s %u", key, &tag) == 2 &&
                  !strcmp(key, fields[field]));
            const AbDataValue *actual = ab_data_get(physical, classic_camera_text(key));
            uint32_t actual_tag = !actual || actual->kind == AB_DATA_NIL ? 0
                                  : actual->kind == AB_DATA_BOOLEAN      ? 1
                                  : actual->kind == AB_DATA_NUMBER       ? 3
                                  : actual->kind == AB_DATA_STRING       ? 4
                                                                         : UINT32_MAX;
            if (tag != actual_tag) {
                fprintf(stderr, "Constructor field %s.%s: native tag %u, C tag %u\n",
                        name, key, tag, actual_tag);
                return false;
            }
            if (tag == 4) {
                size_t length;
                CHECK(fscanf(file, "%zu", &length) == 1 &&
                      actual->string.size == length);
                for (size_t byte = 0; byte < length; ++byte) {
                    unsigned expected;
                    CHECK(fscanf(file, "%2x", &expected) == 1 &&
                          actual->string.data[byte] == expected);
                }
            } else {
                uint32_t bits;
                CHECK(fscanf(file, "%" SCNx32, &bits) == 1 &&
                      bits == (actual ? actual->bits : 0));
            }
        }
        CHECK(position_matches(view, object));
    }
    char footer[96];
    CHECK(fgetc(file) == '\n' && fgets(footer, sizeof(footer), file) &&
          !strcmp(footer, "AB level physical fields reference complete\n") &&
          fgetc(file) == EOF && !ferror(file));
    puts("All 161 native raw constructor fields match across 23 objects; owned x/y "
         "mirror canonical poses");
    return true;
}

static bool native_fields(FILE *file, AbClassicLevelView view) {
    char header[96];
    bool seen[23] = {0};
    CHECK(fgets(header, sizeof(header), file));
    if (!strcmp(header, "AB_LEVEL_NATIVE_PHYSICAL_FIELDS_V2 23 7\n"))
        return native_constructor_fields(file, view);
    CHECK(!strcmp(header, "AB_LEVEL_NATIVE_PHYSICAL_FIELDS_V1 23\n"));
    for (size_t cursor = 0; cursor < view.bodies->count; ++cursor) {
        char name[64];
        uint32_t tag, bits;
        CHECK(fscanf(file, "%63s %u %" SCNx32, name, &tag, &bits) == 3);
        size_t index = 0;
        for (; index < view.bodies->count; ++index) {
            AbSpan actual_name = view.bodies->objects[index].name;
            if (actual_name.size == strlen(name) &&
                !memcmp(actual_name.data, name, actual_name.size))
                break;
        }
        CHECK(index < 23 && !seen[index]);
        seen[index] = true;
        const AbDataValue *control =
            ab_data_get(&view.properties->objects[index].physical,
                        classic_camera_text("controllable"));
        CHECK(tag == (control ? (uint32_t)control->kind : 0) &&
              bits == (control ? control->bits : 0));
        CHECK(position_matches(view, index));
    }
    CHECK(fgetc(file) == '\n' && fgets(header, sizeof(header), file) &&
          !strcmp(header, "AB level physical fields reference complete\n") &&
          fgetc(file) == EOF && !ferror(file));
    puts("All 23 native raw controllable values match; owned cached x/y mirror "
         "canonical poses");
    return true;
}

static bool apk_checks(const char *path, const char *trace, bool fields) {
    AbBuffer apk = {0};
    AbZip archive = {0};
    AbClassicLevel *level = NULL;
    AbClassicBehaviors *behaviors = NULL;
    AbDataValue flying = {0};
    AbError error = {0};
    FILE *reference = NULL;
    bool okay = false;
    const char *sprites[] = {"assets/data/images/base/INGAME_BLOCKS_1.dat",
                             "assets/data/images/base/INGAME_PIGS_NEW_SHEET_1.dat",
                             "assets/data/images/base/INGAME_BLOCKS_3.dat",
                             "assets/data/images/base/INGAME_BLOCKS_2.dat"};
    AbClassicLevelAssets assets = {.level_path = "assets/data/levels/pack1/Level1.lua",
                                   .catalog_path = "assets/data/scripts/blocks.lua",
                                   .sprite_paths = sprites,
                                   .sprite_sheet_count = 4,
                                   .resource_bytes = 16 * 1024 * 1024,
                                   .placement_scale = 1,
                                   .physics_scale = 0.05f,
                                   .derive_ground_position = true,
                                   .derive_world_gravity = true};
    AbWorldDefinition world = {.broadphase_limits = {128, 1024, 1024},
                               .maximum_translation = 2,
                               .maximum_translation_squared = 4,
                               .clear_forces = true,
                               .continuous = true};
    AbClassicLevelLimits limits = {
        .objects = 32, .contacts = 128, .storage_bytes = 1024 * 1024};
    AbClassicBehaviorConfig config = {.flying_bird = &flying,
                                      .storage_bytes = 1024 * 1024};
    if (!ab_file_read(path, 64 * 1024 * 1024, &apk, &error) ||
        !ab_zip_open((AbSpan){apk.data, apk.size}, &archive, &error) ||
        !ab_classic_level_load(&archive, &assets, world, limits, true, &level,
                               &error) ||
        !ab_classic_behaviors_open(level, config, &behaviors, &error))
        goto release_level;
    AbClassicLevelView view = ab_classic_level_view(level);
    if (view.bodies->count != 23 || !view.catalog_data || !projected(view, 6))
        goto release_level;
    if (trace) {
        reference = fopen(trace, "rb");
        if (!reference || !(fields ? native_fields(reference, view)
                                   : native_registry(reference, view)))
            goto release_level;
    }
    printf(
        "Pinned APK level owns all 23 script behavior tables and six Museum instances "
        "in %zu bytes\n",
        ab_classic_behaviors_bytes(behaviors));
    okay = true;
release_level:
    if (reference)
        fclose(reference);
    ab_classic_behaviors_close(behaviors);
    ab_classic_level_close(level);
    ab_zip_close(&archive);
    ab_buffer_free(&apk);
    if (!okay)
        fprintf(stderr, "%s\n", error.message);
    return okay;
}

int main(int argc, char **argv) {
    if (argc == 2)
        return apk_checks(argv[1], NULL, false) ? 0 : 1;
    if (argc == 4 && !strcmp(argv[1], "--apk-trace"))
        return apk_checks(argv[2], argv[3], false) ? 0 : 1;
    if (argc == 4 && !strcmp(argv[1], "--apk-fields"))
        return apk_checks(argv[2], argv[3], true) ? 0 : 1;
    if (argc != 1)
        return 2;
    return edge_checks() ? 0 : 1;
}
