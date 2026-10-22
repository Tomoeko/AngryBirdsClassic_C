#ifndef AB_TEST_CLASSIC_APK_H
#define AB_TEST_CLASSIC_APK_H

#include "character_state.h"
#include "ab/classic_forces.h"

typedef struct {
    AbRandom48 random;
    uint64_t final_random;
    uint32_t objects[23][10];
    size_t bird_count, bird_capacity;
    char bird_names[23][128];
    uint32_t bird_start_numbers[23];
    bool character_state;
    uint32_t characters[23][20];
    size_t goal_count, goal_capacity;
    char goal_names[23][128];
} ClassicStartupReference;

static bool read_classic_startup(FILE *file, const NativeObjectBody records[23],
                                 ClassicStartupReference *output) {
    char header[80];
    uint32_t words[4];
    CHECK(fgets(header, sizeof(header), file));
    output->character_state = strcmp(header, "AB_CLASSIC_STARTUP_V3 23 8\n") == 0;
    CHECK(output->character_state ||
          strcmp(header, "AB_CLASSIC_STARTUP_V2 23 8\n") == 0);
    CHECK(read_body_words(file, words, 4) && words[1] <= 65535 && words[3] <= 65535);
    output->random.value = words[0] | (uint64_t)words[1] << 32;
    output->final_random = words[2] | (uint64_t)words[3] << 32;
    for (size_t index = 0; index < 23; ++index) {
        char name[128];
        CHECK(fscanf(file, "%127s", name) == 1 && fgetc(file) == '\n');
        CHECK(strcmp(name, records[index].name) == 0);
        CHECK(read_body_words(file, output->objects[index], 10));
        if (output->character_state) {
            CHECK(read_body_words(file, output->characters[index], 20));
        }
    }
    CHECK(fscanf(file, "AB_CLASSIC_BIRDS_V1 %zu %zu", &output->bird_count,
                 &output->bird_capacity) == 2 &&
          fgetc(file) == '\n' && output->bird_count <= 23);
    for (size_t index = 0; index < output->bird_count; ++index) {
        uint32_t start[2];
        CHECK(fscanf(file, "%127s", output->bird_names[index]) == 1 &&
              fgetc(file) == '\n' && read_body_words(file, start, 2) && start[0] == 3);
        output->bird_start_numbers[index] = start[1];
    }
    if (output->character_state) {
        CHECK(fscanf(file, "AB_CLASSIC_GOALS_V1 %zu %zu", &output->goal_count,
                     &output->goal_capacity) == 2 &&
              fgetc(file) == '\n' && output->goal_count <= 23);
        for (size_t index = 0; index < output->goal_count; ++index) {
            CHECK(fscanf(file, "%127s", output->goal_names[index]) == 1 &&
                  fgetc(file) == '\n');
        }
    }
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB classic startup reference complete\n") == 0);
    CHECK(fgetc(file) == EOF && !ferror(file));
    return true;
}

static bool compare_classic_startup(AbClassicLevelView view,
                                    const ClassicStartupReference *reference) {
    CHECK(view.timers_initialized && view.timers && view.bodies->count == 23);
    for (size_t index = 0; index < 23; ++index) {
        const AbClassicObjectTimers *timers = view.timers + index;
        const AbObjectDimensions *size =
            &view.bodies->objects[index].physics.dimensions;
        AbDataValue start = view.bodies->objects[index].start_number;
        CHECK(view.properties && view.properties->count == view.bodies->count);
        const char *const dimensions[] = {"width", "height", "radius"};
        for (size_t component = 0; component < 3; ++component) {
            AbDataValue property;
            const char *name = dimensions[component];
            CHECK(ab_level_property_get(view.properties, index,
                                        (AbSpan){(const uint8_t *)name, strlen(name)},
                                        &property, NULL) &&
                  property.kind == AB_DATA_NUMBER &&
                  property.bits == reference->objects[index][5 + component]);
        }
        const uint32_t actual[] = {timers->present,
                                   number(timers->animation).bits,
                                   number(timers->jump).bits,
                                   number(timers->blink).bits,
                                   number(timers->oink).bits,
                                   number(size->width).bits,
                                   number(size->height).bits,
                                   number(size->radius).bits,
                                   start.kind == AB_DATA_NUMBER ? 3u : 0u,
                                   start.bits};
        CHECK(compare_level_words(actual, reference->objects[index], 10, 0, "startup",
                                  "timers/dimensions/start number", index));
        if (reference->character_state) {
            CHECK(view.character_objects);
            uint32_t state[20];
            character_state_words(view.character_objects + index, state);
            CHECK(compare_level_words(state, reference->characters[index], 20, 0,
                                      "post-timer", "character state", index));
        }
    }
    CHECK(view.bird_count == reference->bird_count &&
          view.bird_hash_capacity == reference->bird_capacity);
    for (size_t index = 0; index < view.bird_count; ++index) {
        CHECK(view.bird_order[index] < view.bodies->count);
        const AbLevelObject *bird = view.bodies->objects + view.bird_order[index];
        const char *name = reference->bird_names[index];
        CHECK(bird->name.size == strlen(name) &&
              memcmp(bird->name.data, name, bird->name.size) == 0 &&
              bird->start_number.kind == AB_DATA_NUMBER &&
              bird->start_number.bits == reference->bird_start_numbers[index]);
    }
    if (reference->character_state) {
        CHECK(view.goal_count == reference->goal_count &&
              view.goal_hash_capacity == reference->goal_capacity);
        for (size_t index = 0; index < view.goal_count; ++index) {
            CHECK(view.goal_order[index] < view.bodies->count);
            const AbLevelObject *goal = view.bodies->objects + view.goal_order[index];
            const char *name = reference->goal_names[index];
            CHECK(goal->level_goal && goal->name.size == strlen(name) &&
                  memcmp(goal->name.data, name, goal->name.size) == 0);
        }
        puts("All 23 selected post-timer character states and goal-table membership "
             "match");
    }
    puts("All 23 native startup timers, dimensions and launch numbers match; "
         "eight random draws and the separate birds-table order match");
    return true;
}

static bool compare_classic_sprites(FILE *file, AbClassicLevelView view) {
    char header[80];
    CHECK(view.sprites_initialized && view.initial_sprites &&
          view.bodies->count == 23 && fgets(header, sizeof(header), file) &&
          strcmp(header, "AB_INITIAL_SPRITES_V1 23\n") == 0);
    for (size_t index = 0; index < 23; ++index) {
        char name[128];
        CHECK(fscanf(file, "%127s", name) == 1 && fgetc(file) == '\n');
        AbSpan object = view.bodies->objects[index].name;
        CHECK(strlen(name) == object.size &&
              memcmp(name, object.data, object.size) == 0);
        const AbDamageSpriteNames *selected = view.initial_sprites + index;
        AbDataValue values[] = {selected->sprite, selected->blink, selected->smile};
        for (size_t field = 0; field < 3; ++field) {
            unsigned tag;
            size_t length;
            char bytes[256];
            CHECK(fscanf(file, "%u %zu", &tag, &length) == 2 && fgetc(file) == '\n' &&
                  (tag == 0 || tag == 4) && length <= sizeof(bytes) &&
                  fread(bytes, 1, length, file) == length && fgetc(file) == '\n');
            AbDataValue value = values[field];
            if (tag == 0) {
                CHECK(!length && value.kind == AB_DATA_NIL);
            } else {
                CHECK(value.kind == AB_DATA_STRING && value.string.size == length &&
                      (!length || memcmp(value.string.data, bytes, length) == 0));
                if (length) {
                    AbSpriteBinding binding;
                    CHECK(ab_sprite_set_find(view.sprites, value.string, &binding));
                }
            }
        }
    }
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB initial sprite reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    puts("All 69 native initial damage/blink/smile names and nonempty atlas bindings "
         "match");
    return true;
}

static bool classic_replay_phase(void *context, const AbClassicSimulationCall *call,
                                 AbError *error) {
    (void)error;
    AbClassicLevelView view = ab_classic_level_view(context);
    /* This retained frame fixture has no water, removals or deferred velocity
     * requests. Full body/contact/history comparisons below verify the resulting
     * unchanged external effects; this host is not their implementation. */
    CHECK(view.bodies->count == 23 && view.world->body_count == 23 &&
          !view.world->locked && call->stage >= AB_CLASSIC_SIMULATION_WATER &&
          call->stage <= AB_CLASSIC_SIMULATION_AFTER_STEP);
    return true;
}

static bool classic_apk_checks(const char *apk_path, const char *bodies_path,
                               const char *frames_path, const char *startup_path,
                               const char *sprites_path, const char *flight_path,
                               const char *point_gpu_path) {
    AbBuffer apk = {0};
    AbZip archive = {0};
    AbClassicLevel *level = NULL;
    AbClassicSimulation *simulation = NULL;
    NativeObjectBody records[23] = {0};
    FILE *bodies = NULL, *frames = NULL, *startup_file = NULL;
    FILE *sprites_file = NULL, *flight_file = NULL;
    LevelCamera camera = {0};
    ClassicStartupReference startup = {0};
    AbError error = {0};
    bool okay = false;
    if (!ab_file_read(apk_path, 64 * 1024 * 1024, &apk, &error) ||
        !ab_zip_open((AbSpan){apk.data, apk.size}, &archive, &error)) {
        goto release_level;
    }
    bodies = fopen(bodies_path, "rb");
    if (!bodies || !read_body_records(bodies, records)) {
        goto release_level;
    }
    if (startup_path) {
        startup_file = fopen(startup_path, "rb");
        if (!startup_file || !read_classic_startup(startup_file, records, &startup)) {
            goto release_level;
        }
    }
    if (sprites_path && !(sprites_file = fopen(sprites_path, "rb"))) {
        goto release_level;
    }
    if (flight_path && (!(flight_file = fopen(flight_path, "rb")) ||
                        !level_camera_assets(&camera, &archive, &error))) {
        goto release_level;
    }
    uint64_t initial_random = startup.random.value;
    uint32_t controls[8] = {0, 0, UINT32_C(0x3d088889), 8, 3, 0, 1, 1};
    if (frames_path) {
        frames = fopen(frames_path, "rb");
        char header[80];
        if (!frames || !fgets(header, sizeof(header), frames) ||
            strcmp(header, "AB_LEVEL_NATIVE_FRAMES_V2 12 23\n") != 0 ||
            !read_body_words(frames, controls, 8)) {
            goto release_level;
        }
    }
    const char *paths[] = {"assets/data/images/base/INGAME_BLOCKS_1.dat",
                           "assets/data/images/base/INGAME_PIGS_NEW_SHEET_1.dat",
                           "assets/data/images/base/INGAME_BLOCKS_3.dat"};
    /* World settings and sling timing remain explicit captured caller inputs.
     * Placements, definitions, materials, box sizes and ground derive from APK. */
    AbClassicLevelAssets assets = {
        .level_path = "assets/data/levels/pack1/Level1.lua",
        .catalog_path = "assets/data/scripts/blocks.lua",
        .sprite_paths = paths,
        .sprite_sheet_count = 3,
        .resource_bytes = 16 * 1024 * 1024,
        .placement_scale = 1,
        .physics_scale = 0.05f,
        .derive_ground_position = true,
        .derive_world_gravity = true,
        .random = startup_path ? &startup.random : NULL,
    };
    AbClassicLevelLimits limits = {
        .objects = 32, .contacts = 128, .storage_bytes = 1024 * 1024};
    AbWorldDefinition definition = level_world_definition(controls);
    AbClassicLevelAssets invalid = assets;
    invalid.level_path = "assets/data/levels/missing.lua";
    if (ab_classic_level_load(&archive, &invalid, definition, limits, true, &level,
                              &error) ||
        level || startup.random.value != initial_random) {
        goto release_level;
    }
    invalid = assets;
    invalid.resource_bytes = 1024;
    if (ab_classic_level_load(&archive, &invalid, definition, limits, true, &level,
                              &error) ||
        level || startup.random.value != initial_random) {
        goto release_level;
    }
    error = (AbError){0};
    if (!ab_classic_level_load(&archive, &assets, definition, limits, true, &level,
                               &error)) {
        goto release_level;
    }
    AbClassicLevelView view = ab_classic_level_view(level);
    AbClassicSimulationConfig simulation_config = {
        .minimum_x = -49,
        .maximum_x = 89,
        .velocity_iterations = controls[3],
        .position_iterations = controls[4],
        .maximum_steps = 32,
        .phase = classic_replay_phase,
        .context = level,
        .limits = {
            .storage_bytes = 65536, .queued_forces = 32, .force_sound_choices = 32}};
    if (!ab_classic_simulation_open(level, simulation_config, &simulation, &error)) {
        goto release_level;
    }
    if ((frames && (number(view.world->definition.gravity.x).bits != controls[0] ||
                    number(view.world->definition.gravity.y).bits != controls[1])) ||
        !view.data || !view.catalog || !view.sprites ||
        !bind_level_body_records(view.world, view.bodies, records) ||
        !compare_initial_bodies(view.world, records) ||
        (sprites_file && !compare_classic_sprites(sprites_file, view)) ||
        (startup_path && (startup.random.value != startup.final_random ||
                          !compare_classic_startup(view, &startup))) ||
        (frames && !replay_level_frames(frames, view.world, records, controls, true,
                                        view.frame, NULL, simulation)) ||
        (flight_file && !replay_level_flight_camera(
                            flight_file, view.world, records, controls, view.catalog,
                            view.bodies->objects, level, &camera))) {
        goto release_level;
    }
    puts("Loaded original level/catalog/box bounds directly from APK into owned C "
         "world; native comparison passes without extracted inputs or size fixtures");
    if (point_gpu_path && !level_camera_write_gpu(&camera, point_gpu_path, &error))
        goto release_level;
    okay = true;

release_level:
    if (!okay && error.message[0]) {
        fprintf(stderr, "Classic APK level: %s\n", error.message);
    }
    if (bodies && fclose(bodies) != 0) {
        okay = false;
    }
    if (frames && fclose(frames) != 0) {
        okay = false;
    }
    if (startup_file && fclose(startup_file) != 0) {
        okay = false;
    }
    if (sprites_file && fclose(sprites_file) != 0) {
        okay = false;
    }
    if (flight_file && fclose(flight_file) != 0) {
        okay = false;
    }
    level_camera_close(&camera);
    ab_classic_simulation_close(simulation);
    ab_classic_level_close(level);
    ab_zip_close(&archive);
    ab_buffer_free(&apk);
    return okay;
}

#endif
