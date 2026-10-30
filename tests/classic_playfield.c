#include "ab/classic_playfield.h"
#include "ab/classic_level_runtime.h"
#include "ab/classic_block_scripts.h"
#include "ab/camera_bootstrap.h"
#include "ab/target_data.h"
#include "ab/android_assets.h"
#include "ab/asset_sprite_provider.h"
#include "ab/theme_assets.h"
#include "ab/classic_object_removal.h"
#include "ab/classic_post_physics.h"
#include "ab/classic_birds.h"
#include "ab/level_effect.h"
#include "../tools/oracle/reference_blues_split.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Playfield check failed at %d: %s\n", __LINE__,            \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

typedef struct {
    AbBuffer bytes;
    AbZip archive;
    AbClassicLevel *level;
    AbSpriteAtlas *atlas;
    AbSpriteProvider sprites;
    AbAndroidAssets *planning;
    AbAssetResources *resources;
    AbAssetTextures *textures;
    AbMaskTexture terrain;
    AbAssetSpriteProvider grouped_sprites;
    AbTargetData themes;
    AbThemeAssets theme_assets;
    AbDataTable empty_theme;
    AbSpan theme_scratch[32];
    AbClassicPlayfield *owner;
    AbClassicPostPhysics *blue_post;
    AbClassicObjectRemoval *physical;
    AbStringMap *teleporters;
    AbClassicBirds *birds;
    AbClassicParticles *blue_particles;
    AbStringMap *blue_hats;
    AbEngineRandom engine_random;
    AbSpan particle_names[16];
    size_t particle_indices[32];
    AbStringOrderNode particle_nodes[32];
    AbStringOrderWorkspace particle_workspace;
    AbDataTable base_effect;
    AbDataValue level_effect;
    float remove_time, remove_speed_squared;
    FILE *child_reference;
    FILE *split_reference;
    uint8_t blue_names[8192];
    uint8_t split_names[2048];
    AbDataTable blue_damage_factors;
    AbCameraSnap snaps[2];
    AbTargetData source[3];
    AbCameraSequence sequences[3];
    AbAudioBank *audio;
    AbAudioMixer *mixer;
    AbSettingsStatistics *statistics;
    AbEventManager *events;
    AbPowerupUses *powerups;
    AbRandom48 random;
    AbClassicGameGlobals globals;
    AbShotState shot_state;
    AbDataValue flying, enabled, red_special, next_sound;
    uint32_t counter;
    float gravity;
    size_t logs, notifications;
    AbEventListener listener;
    const AbDataTable *retained[4];
} PlayfieldTest;

static AbSpan span(const char *value) {
    return (AbSpan){(const uint8_t *)value, strlen(value)};
}

static uint32_t bits(float value) {
    uint32_t result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

static float from_bits(uint32_t value) {
    float result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

static AbDataValue field(const AbDataTable *table, const char *name) {
    const AbDataValue *value =
        ab_data_get(table, (AbDataValue){.kind = AB_DATA_STRING, .string = span(name)});
    return value ? *value : (AbDataValue){0};
}

static bool sprite(void *context, AbSpan name, AbSpriteAtlasBinding *output) {
    return ab_sprite_atlas_find(context, name, output);
}

static bool texture(void *context, const AbImage *image, uint32_t *output) {
    AbSpriteAtlas *atlas = context;
    for (size_t index = 0; index < ab_sprite_atlas_count(atlas); ++index) {
        AbSpriteAtlasImage view;
        if (ab_sprite_atlas_image(atlas, index, &view) && view.image == image) {
            *output = (uint32_t)index + 1;
            return true;
        }
    }
    return false;
}

static const char *const sequence_names[] = {"FollowShotBirdEndCameraSequence",
                                             "FollowShotBirdCameraSequence",
                                             "LevelEndSequence"};

static bool load_sequence(void *context, const char *name,
                          const AbCameraSequence **output, AbError *error) {
    PlayfieldTest *test = context;
    for (size_t index = 0; index < 3; ++index) {
        if (!strcmp(name, sequence_names[index])) {
            *output = test->sequences + index;
            return true;
        }
    }
    return ab_fail(error, "Unprepared playfield test camera resource");
}

static bool log_sequence(void *context, AbSpan message, AbError *error) {
    (void)error;
    PlayfieldTest *test = context;
    CHECK(message.size && message.data);
    ++test->logs;
    return true;
}

static bool observe(void *context, AbDataTable *event, AbError *error) {
    (void)error;
    PlayfieldTest *test = context;
    CHECK(test->notifications < 4);
    for (size_t index = 0; index < test->notifications; ++index)
        CHECK(test->retained[index] != event);
    test->retained[test->notifications++] = event;
    return true;
}

static bool manage_group(void *context, AbSpan name, bool loading, AbError *error) {
    PlayfieldTest *test = context;
    AbDataEntry entry = {{.kind = AB_DATA_NUMBER, .bits = UINT32_C(0x3f800000)},
                         {.kind = AB_DATA_STRING, .string = name}};
    AbDataTable requested = {&entry, 1, 1, NULL};
    return ab_android_assets_manage(test->planning, &requested, loading,
                                    ab_asset_resources_host, test->resources, error);
}

/* Explicit headless texture boundary. Decode/cache/selection are real; these
 * callbacks validate logical image bindings without claiming GPU upload. */
static bool logical_upload(void *context, uint32_t id, AbSpriteAtlasImage image,
                           AbError *error) {
    (void)context;
    return (id && id <= 64 && image.image && image.image->rgba.data) ||
           ab_fail(error, "Invalid headless playfield texture binding");
}

static bool logical_remove(void *context, uint32_t id, AbError *error) {
    (void)context;
    return (id && id <= 64) || ab_fail(error, "Invalid headless texture release");
}

static bool open_game_resources(PlayfieldTest *test, float width, float height,
                                AbError *error) {
    uint8_t key[32] = {0};
    bool okay =
        ab_target_resource_key(&test->archive, 16 * 1024 * 1024, key, error) &&
        ab_target_data_open(&test->archive, "assets/data/scripts/themes.lua",
                            (AbSpan){key, sizeof(key)}, 4 * 1024 * 1024, NULL,
                            &test->themes, error) &&
        ab_android_assets_open(&test->archive, (AbSpan){key, sizeof(key)},
                               (AbAndroidAssetLimits){width, height, false, false,
                                                      false, 64, 8, 1024 * 1024},
                               &test->planning, error) &&
        ab_asset_resources_open(
            &test->archive,
            (AbAssetResourceLimits){128, 32768, 128 * 1024 * 1024, 16 * 1024 * 1024},
            &test->resources, error) &&
        ab_asset_textures_open(
            (AbAssetTextureHost){NULL, logical_upload, logical_remove}, 64,
            128 * 1024 * 1024, &test->textures, error) &&
        ab_asset_sprite_provider_init(test->resources, test->textures,
                                      &test->grouped_sprites, error);
    memset(key, 0, sizeof(key));
    CHECK(okay &&
          ab_theme_assets_init(&test->empty_theme, test->theme_scratch, 16,
                               &test->theme_assets, error) &&
          manage_group(test, span("INGAME"), true, error) &&
          manage_group(test, span("INGAME_PIGS_NEW"), true, error));
    AbDataValue theme = field(test->themes.data.globals, "theme1");
    CHECK(theme.kind == AB_DATA_TABLE &&
          ab_theme_assets_transition(&test->theme_assets,
                                     field(theme.table, "graphicSetName"), false,
                                     manage_group, test, error) &&
          ab_asset_textures_sync(test->textures,
                                 ab_asset_resources_owners(test->resources), error));
    AbDataValue terrain = field(theme.table, "texture");
    AbSpriteAtlasImage image;
    CHECK(terrain.kind == AB_DATA_STRING &&
          ab_asset_resources_find_image(test->resources, terrain.string, &image) &&
          ab_asset_textures_find(test->textures, image.image, &test->terrain.texture) &&
          ab_asset_textures_set_wrap(test->textures, image.image, true, true, error));
    test->terrain.width = image.image->width;
    test->terrain.height = image.image->height;
    return true;
}

static bool open_level(PlayfieldTest *test, const char *path, AbError *error) {
    const char *paths[] = {"assets/data/images/base/INGAME_BLOCKS_1.dat",
                           "assets/data/images/base/INGAME_PIGS_NEW_SHEET_1.dat",
                           "assets/data/images/base/INGAME_BLOCKS_3.dat",
                           "assets/data/images/base/INGAME_BLOCKS_2.dat"};
    AbClassicLevelAssets assets = {.level_path = path,
                                   .catalog_path = "assets/data/scripts/blocks.lua",
                                   .sprite_paths = paths,
                                   .sprite_sheet_count = 4,
                                   .resource_bytes = 16 * 1024 * 1024,
                                   .placement_scale = 1,
                                   .physics_scale = 0.05f,
                                   .derive_ground_position = true,
                                   .derive_world_gravity = true,
                                   .derive_placement_scale = true,
                                   .physics_to_world = 20};
    if (test->resources) {
        assets.sprite_paths = NULL;
        assets.sprite_sheet_count = 0;
        assets.sprite_context = &test->grouped_sprites;
        assets.sprite_size = ab_asset_sprite_provider_size;
    }
    AbWorldDefinition definition = {.broadphase_limits = {512, 4096, 4096},
                                    .maximum_translation = 2,
                                    .maximum_translation_squared = 4,
                                    .warm_start = true,
                                    .clear_forces = true,
                                    .continuous = true};
    CHECK(ab_classic_level_load(
        &test->archive, &assets, definition,
        (AbClassicLevelLimits){
            .objects = 128, .contacts = 1024, .storage_bytes = 4 * 1024 * 1024},
        true, &test->level, error));
    AbClassicLevelView level = ab_classic_level_view(test->level);
    test->gravity = level.world->definition.gravity.y;
    CHECK(ab_camera_museum_snaps(level.data->globals, (AbVec2){480, 320}, test->snaps,
                                 error));
    return true;
}

static bool open_resources(PlayfieldTest *test, AbError *error) {
    AbClassicLevelView level = ab_classic_level_view(test->level);
    CHECK(ab_sprite_atlas_open(&test->archive, level.sprites, 16 * 1024 * 1024,
                               8 * 1024 * 1024, &test->atlas, error) &&
          ab_random48_seed(&test->random, 7) &&
          ab_audio_mixer_open((AbAudioMixerLimits){32, 512, 1, 256 * 1024},
                              &test->mixer, error));
    test->sprites = (AbSpriteProvider){.context = test->atlas,
                                       .sprite = sprite,
                                       .composite = NULL,
                                       .texture = texture};
    uint8_t key[32] = {0};
    bool okay = ab_target_resource_key(&test->archive, 16 * 1024 * 1024, key, error) &&
                ab_audio_bank_open(
                    &test->archive, (AbSpan){key, sizeof(key)},
                    (AbAudioBankConfig){test->mixer, &test->random, 64 * 1024 * 1024,
                                        4 * 1024 * 1024, 4 * 1024 * 1024},
                    &test->audio, error);
    for (size_t index = 0; okay && index < 3; ++index) {
        char path[160];
        int written =
            snprintf(path, sizeof(path), "assets/data/scripts/cameraSequences/%s.lua",
                     sequence_names[index]);
        okay = written > 0 && (size_t)written < sizeof(path) &&
               ab_target_data_open(&test->archive, path, (AbSpan){key, sizeof(key)},
                                   1024 * 1024, NULL, test->source + index, error);
        AbDataValue sequence = okay
                                   ? field(test->source[index].data.globals, "sequence")
                                   : (AbDataValue){0};
        okay = okay && sequence.kind == AB_DATA_TABLE &&
               ab_camera_sequence_read(sequence.table, 65536, test->sequences + index,
                                       error);
    }
    memset(key, 0, sizeof(key));
    CHECK(okay && ab_audio_mixer_start(test->mixer, true, error) &&
          ab_powerup_uses_open(16, 32, 4096, &test->powerups, error) &&
          ab_settings_statistics_open(
              (AbSettingsStatisticsConfig){span("test"), span("test"), 8, 8, 65536},
              &test->statistics, error) &&
          ab_event_manager_open((AbEventManagerLimits){8, 4, 8, 0, 8, 32768},
                                (AbEventManagerHost){0}, &test->events, error));
    test->listener = (AbEventListener){span("PlayfieldObserver"), test, observe};
    CHECK(ab_event_manager_add(
        test->events,
        (AbDataValue){.kind = AB_DATA_STRING, .string = span("EID_BIRD_SHOT")},
        &test->listener, error));
    return true;
}

static AbClassicPlayfieldConfig configuration(PlayfieldTest *test,
                                              const uint32_t controls[8],
                                              const uint32_t data[17]) {
    return (AbClassicPlayfieldConfig){
        .level = test->level,
        .sprites = &test->sprites,
        .terrain = &test->terrain,
        .audio = test->audio,
        .statistics = test->statistics,
        .events = test->events,
        .powerups = test->powerups,
        .globals = &test->globals,
        .shot_state = &test->shot_state,
        .flying_object = &test->flying,
        .powerups_enabled = &test->enabled,
        .red_special_available = &test->red_special,
        .next_bird_sound = &test->next_sound,
        .renderer_counter = &test->counter,
        .object_blend = AB_BLEND_PREMULTIPLIED,
        .camera = {.snaps = test->snaps,
                   .snap_count = 2,
                   .pixel_size = {480, 320},
                   .absolute_bottom = 200,
                   .horizontal_leeway = 200,
                   .gravity = &test->gravity,
                   .shot_sequence = test->sequences + 1,
                   .end_sequence = test->sequences,
                   .resource_context = test,
                   .load_sequence = load_sequence,
                   .sequence_log = log_sequence,
                   .sequence_capacity = test->sequences[2].count},
        .start = {.pose = {.level_start = {from_bits(controls[0]),
                                           from_bits(controls[1])},
                           .band = {from_bits(data[11]), from_bits(data[12])}},
                  .physics_scale = from_bits(controls[4]),
                  .default_force = from_bits(controls[3]),
                  .current_bird_index = 1,
                  .allow_reset_to_bird_camera = true},
        .limits = {.visuals = 65536,
                   .scene = 131072,
                   .trajectories = 65536,
                   .camera = 131072,
                   .behaviors = 65536,
                   .shots = 65536,
                   .services = 16384,
                   .sling = 16384,
                   .storage_bytes = 1024 * 1024,
                   .name_capacity = 128,
                   .extra_renderers = 64,
                   .tracks = 16,
                   .points = 64,
                   .notifications = 8}};
}

static bool runtime_camera_log(void *context, AbSpan message, AbError *error) {
    if (!message.data || !message.size)
        return ab_fail(error, "Empty fresh-runtime camera diagnostic");
    ++*(size_t *)context;
    return true;
}

static bool runtime_prediction_log(void *context, size_t raycasts, AbError *error) {
    (void)context;
    return fprintf(stderr, "Impact prediction raycasts: %zu\n", raycasts) >= 0 ||
           ab_fail(error, "Cannot write impact prediction diagnostic");
}

#include "classic_world_runtime.h"

static bool fresh_runtime(PlayfieldTest *test, bool exercise_physics, AbError *error) {
    if (exercise_physics)
        CHECK(open_game_resources(test, 1024, 768, error));
    uint32_t controls[8] = {0}, data[17] = {0}, renderer_counter = 0;
    AbClassicPlayfieldConfig playfield = configuration(test, controls, data);
    playfield.limits.behaviors = 256 * 1024;
    AbShotState shot = {0};
    AbClassicGameGlobals globals = {.time = {.kind = AB_DATA_NUMBER},
                                    .score = {.kind = AB_DATA_NUMBER},
                                    .last_score_time = {.kind = AB_DATA_NUMBER},
                                    .old_score = {.kind = AB_DATA_NUMBER},
                                    .extra_tutorial_timer = {.kind = AB_DATA_NUMBER},
                                    .specialty_available = {.kind = AB_DATA_BOOLEAN}};
    AbDataValue flying = {0};
    AbDataValue next_sound = {.kind = AB_DATA_STRING,
                              .string = span("bird_next_military")};
    size_t logs = 0;
    playfield.level = NULL;
    if (exercise_physics)
        playfield.sprites = &test->grouped_sprites.provider;
    playfield.shot_state = &shot;
    playfield.globals = &globals;
    playfield.flying_object = &flying;
    playfield.next_bird_sound = &next_sound;
    playfield.renderer_counter = &renderer_counter;
    playfield.start.allow_reset_to_bird_camera = false;
    playfield.camera.pixel_size = (AbVec2){1920, 1080};
    playfield.camera.resource_context = &logs;
    playfield.camera.sequence_log = runtime_camera_log;
    playfield.camera.prediction = (AbClassicImpactServices){
        .maximum_raycasts = 4096, .print = runtime_prediction_log};
    AbClassicLevelRuntime *runtime = NULL;
    AbClassicLevelRuntimeConfig config = {
        .archive = &test->archive,
        .level_path = "assets/data/levels/pack1/Level1.lua",
        .random = &test->random,
        .world = {.broadphase_limits = {512, 4096, 4096},
                  .maximum_translation = 2,
                  .maximum_translation_squared = 4,
                  .warm_start = true,
                  .clear_forces = true,
                  .continuous = true},
        .level_limits = {.objects = 128,
                         .contacts = 1024,
                         .storage_bytes = 4 * 1024 * 1024},
        .resource_bytes = 16 * 1024 * 1024,
        .dynamic_objects_per_bird = 3,
        .playfield = playfield};
    CHECK(ab_classic_level_runtime_open(config, &runtime, error));
    AbClassicLevelRuntimeView view = ab_classic_level_runtime_view(runtime);
    AbClassicPlayfieldView live = ab_classic_playfield_view(view.playfield);
    AbClassicLevelView level = ab_classic_level_view(view.level);
    CHECK(view.level && view.first_bird < level.bodies->count &&
          view.placement_scale == 1 && renderer_counter && level.timers_initialized &&
          level.sprites_initialized && ab_classic_shots_state(live.shots) == &shot &&
          !shot.dragged && !shot.current_bird_name && !shot.bird_ready &&
          !shot.bird_fired && shot.next_bird_timer == 0.5f &&
          shot.bird_reload_delay == 1 && !logs);
    const AbObjectBody *first = &level.bodies->objects[view.first_bird].physics;
    AbVec2 loaded =
        level.world->storage.bodies[first->body].motion.mass.transform.position;
    AbSlingshotStartPose pose = ab_classic_sling_start_pose(live.sling);
    CHECK(pose.base.x == loaded.x && pose.base.y == loaded.y &&
          level.world->storage.bodies[first->body].motion.mass.type == AB_BODY_STATIC &&
          ab_classic_camera_start_sequence(live.camera,
                                           "DefaultLevelStartCameraSequence", error) &&
          logs == 2);
    if (!exercise_physics) {
        CHECK(ab_classic_sling_start_queue(live.sling, error));
        for (size_t frame = 0; frame < 12 && !shot.bird_ready; ++frame)
            CHECK(ab_classic_sling_update(live.sling, 0.25f, error));
        CHECK(shot.bird_ready && shot.current_bird_name && !shot.dragged &&
              shot.birds_shot == 0 && shot.bird_count == (float)level.bird_count &&
              ab_classic_sling_view(live.sling).current_bird_index == 1);
        bool physics = true;
        AbClassicBlockScripts *scripts = NULL;
        CHECK(ab_classic_block_scripts_open(
            (AbClassicBlockScriptConfig){.level = view.level,
                                         .physics_enabled = &physics,
                                         .storage_bytes = 65536},
            &scripts, error));
        AbClassicBlockScriptView blocks = ab_classic_block_scripts_view(scripts);
        CHECK(blocks.manager->registered.count == 3 &&
              !blocks.manager->initialized.count && !blocks.manager->count &&
              blocks.count == level.bodies->count &&
              ab_physics_script_step(blocks.physics, 1.0f / 60, error) &&
              ab_block_scripts_update(blocks.manager, blocks.update, 1.0f / 60, 0,
                                      error));
        for (size_t index = 0; index < blocks.count; ++index) {
            bool ground = level.bodies->objects[index].name.size == 6 &&
                          !memcmp(level.bodies->objects[index].name.data, "ground", 6);
            CHECK(blocks.objects[index].scripts_present == !ground &&
                  !blocks.objects[index].count && !blocks.objects[index].tracked);
        }
        ab_classic_block_scripts_close(scripts);
    }
    if (exercise_physics)
        CHECK(fresh_world_runtime(test, view.playfield, &globals, &flying, error));
    shot = (AbShotState){0};
    flying = (AbDataValue){0};
    ab_classic_level_runtime_close(runtime);
    CHECK(ab_audio_mixer_all_action(test->mixer, AB_AUDIO_STOP, error) &&
          ab_audio_mixer_mix(test->mixer, 0, NULL, 0, error));
    return true;
}

static bool read_words(FILE *file, uint32_t *output, size_t count) {
    for (size_t index = 0; index < count; ++index)
        CHECK(fscanf(file, "%8" SCNx32, output + index) == 1);
    return true;
}

static bool blue_particle_sprite(void *context, AbSpan name, uintptr_t *output,
                                 AbError *error) {
    PlayfieldTest *test = context;
    AbSpriteProvider *provider = &test->grouped_sprites.provider;
    AbSpriteAtlasBinding binding;
    if (!provider->sprite(provider->context, name, &binding) || !binding.image)
        return ab_fail(error, "Missing required Blue removal particle sprite");
    *output = (uintptr_t)binding.image;
    return true;
}

static bool blue_removal_volume(void *context, AbDataValue receiver, size_t object,
                                AbDataValue *output, AbError *error) {
    PlayfieldTest *test = context;
    AbClassicLevelView level = ab_classic_level_view(test->level);
    if (receiver.kind != AB_DATA_TABLE || receiver.table != &test->base_effect ||
        object >= level.bodies->count)
        return ab_fail(error, "Foreign base bird-volume receiver");
    *output = (AbDataValue){.kind = AB_DATA_NUMBER,
                            .bits = bits(ab_level_effect_base_bird_volume())};
    return true;
}

static bool open_blue_birds(PlayfieldTest *test, AbClassicPlayfieldView view,
                            size_t source, AbError *error) {
    AbClassicLevelView level = ab_classic_level_view(test->level);
    AbDataValue particles = {0};
    CHECK(ab_level_property_get(level.properties, source, span("particles"), &particles,
                                error));
    CHECK(particles.kind == AB_DATA_NIL ||
          (particles.kind == AB_DATA_BOOLEAN && !particles.bits) ||
          (particles.kind == AB_DATA_TABLE && particles.table));
    AbDataValue removal =
        field(particles.kind == AB_DATA_TABLE ? particles.table : NULL, "removal");
    size_t count = 0;
    if (removal.kind != AB_DATA_NIL &&
        (removal.kind != AB_DATA_BOOLEAN || removal.bits)) {
        CHECK(removal.kind == AB_DATA_TABLE && removal.table &&
              removal.table->count <= 16);
        for (size_t index = 0; index < removal.table->count; ++index) {
            AbDataValue descriptor = removal.table->entries[index].value;
            CHECK(descriptor.kind == AB_DATA_TABLE && descriptor.table);
            AbDataValue name = field(descriptor.table, "definition");
            CHECK(name.kind == AB_DATA_STRING);
            test->particle_names[count++] = name.string;
        }
    }
    test->particle_workspace =
        (AbStringOrderWorkspace){test->particle_indices, 32, test->particle_nodes, 32};
    test->level_effect =
        (AbDataValue){.kind = AB_DATA_TABLE, .table = &test->base_effect};
    /* Explicit prepared BaseLevelEffect and RNG state. Removal parameters are
     * the actual Museum source literals, not fresh-session bootstrap proof. */
    test->remove_time = 1;
    test->remove_speed_squared = from_bits(UINT32_C(0x3b23d70a));
    CHECK(ab_engine_random_seed(&test->engine_random, 7) &&
          ab_classic_particles_open(
              &test->archive,
              (AbClassicParticleConfig){.level = test->level,
                                        .definitions = test->particle_names,
                                        .definition_count = count,
                                        .resource_bytes = 16 * 1024 * 1024,
                                        .storage_bytes = 256 * 1024,
                                        .capacity = test->split_reference ? 512 : 128,
                                        .level_capacity =
                                            test->split_reference ? 512 : 128,
                                        .engine_random = &test->engine_random,
                                        .script_random = &test->random,
                                        .admission = {40, 125, 0.5f},
                                        .workspace = &test->particle_workspace,
                                        .context = test,
                                        .sprite = blue_particle_sprite},
              &test->blue_particles, error) &&
          ab_string_map_open(0, 128, 16384, &test->blue_hats, error) &&
          ab_classic_birds_open(
              (AbClassicBirdConfig){
                  .level = test->level,
                  .shots = view.shots,
                  .behaviors = view.behaviors,
                  .camera = view.camera,
                  .physical = test->physical,
                  .particles = test->blue_particles,
                  .audio = test->audio,
                  .hats = test->blue_hats,
                  .flying_object = &test->flying,
                  .specialty_available = &test->globals.specialty_available,
                  .level_effect = &test->level_effect,
                  .remove_speed_squared = &test->remove_speed_squared,
                  .remove_time = &test->remove_time,
                  .context = test,
                  .removal_volume = blue_removal_volume,
                  .storage_bytes = 256 * 1024,
                  .object_capacity = level.bodies->capacity,
                  .other_generations = test->split_reference ? BS_PROFILES + 2
                                                             : 2 * level.bird_count + 2,
                  .other_entries = level.bodies->capacity + 2},
              &test->birds, error));
    return ab_classic_playfield_bind_other_birds(
        test->owner, ab_classic_birds_view(test->birds).other_birds_field, error);
}

#include "classic_blues_child.h"
#include "classic_blues_split.h"
#include "classic_blues_split_native.h"

typedef struct {
    PlayfieldTest *test;
    AbClassicPlayfieldView view;
    size_t object, logs, ends;
    AbDataTable *trajectory;
} DynamicCameraCheck;

static bool dynamic_camera_done(void *context, AbCameraSystem *system, AbError *error) {
    (void)error;
    DynamicCameraCheck *check = context;
    AbClassicLevelView level = ab_classic_level_view(check->view.level);
    AbDataTable *groups = &level.properties->objects[check->object].behaviors;
    AbDataValue camera = field(groups, "MuseumBluesCameraFollowBehavior");
    CHECK(ab_camera_system_view(system).sequence->current &&
          check->test->logs == check->logs && camera.kind == AB_DATA_TABLE &&
          !camera.table->count &&
          field(groups, "MuseumBluesTrajectoryRenderer").table == check->trajectory &&
          !ab_classic_behaviors_add(check->view.behaviors, check->object,
                                    span("MuseumBluesCameraFollowBehavior"),
                                    (AbDataValue){0}, NULL));
    ++check->ends;
    return true;
}

static bool check_dynamic_renderers(PlayfieldTest *test, AbClassicPlayfieldView view,
                                    size_t source, AbError *error) {
    AbClassicLevelView level = ab_classic_level_view(test->level);
    AbDataTable *physical = &level.properties->objects[source].physical;
    const char *const fields[] = {"radius", "density", "friction", "restitution"};
    float values[4];
    for (size_t index = 0; index < 4; ++index) {
        AbDataValue value = field(physical, fields[index]);
        CHECK(value.kind == AB_DATA_NUMBER);
        values[index] = from_bits(value.bits);
    }
    AbDataValue sprite = field(physical, "sprite");
    CHECK(sprite.kind == AB_DATA_STRING && sprite.string.size);
    AbClassicRenderSceneView before = ab_classic_render_scene_view(view.scene);
    AbClassicScene *objects = ab_classic_render_scene_objects(view.scene);
    AbClassicSceneView object_view = ab_classic_scene_view(objects);
    const AbSceneSprite *stable_items = object_view.items;
    const AbObjectVisual *stable_visual =
        ab_classic_visuals_object(view.visuals, source);
    AbSpan old_sprite = stable_visual->sprite;
    size_t count = level.bodies->count;
    AbGameObjectCoreRegistry *physical_registry =
        ab_classic_object_removal_registry(test->physical);
    size_t initial_native_birds = physical_registry->bird_count;
    AbGameObjectCore *new_core[3] = {0};
    size_t initial_tracks = ab_classic_trajectories_view(view.trajectories).tracks;
    size_t ground = 0;
    while (ground < count &&
           !(level.bodies->objects[ground].name.size == 6 &&
             !memcmp(level.bodies->objects[ground].name.data, "ground", 6)))
        ++ground;
    CHECK(ground < count);
    uint32_t counter = test->counter;
    AbDrawCommand commands[256];
    AbDrawList list = {commands, 0, 256};
    AbCoordinateCamera camera = {.top_left = {-40, -200}, .zoom = {1, 1}};
    CHECK(ab_classic_render_scene_draw(view.scene, camera, 480, 320, &list, error));
    size_t initial_commands = list.count;
    const char *const names[] = {"test_blue_circle_a", "test_blue_circle_b",
                                 "test_blue_circle_c"};
    AbDataTable empty_properties = {0};
    AbDataEntry descriptor_entry = {
        {.kind = AB_DATA_STRING, .string = span("MuseumBluesTrajectoryRenderer")},
        {.kind = AB_DATA_TABLE, .table = &empty_properties}};
    AbDataTable descriptor = {&descriptor_entry, 1, 1, NULL};
    AbDataTableLayout layout = {
        .table = &descriptor, .hash_capacity = 1, .no_nil_assignments = true};
    AbLuaData descriptor_data = {.layouts = &layout};
    AbDataEntry target_entry = {
        .key = {.kind = AB_DATA_STRING, .string = span("target")}};
    AbDataTable target_properties = {&target_entry, 1, 1, NULL};
    AbCameraSequenceStage follow_stage = {.action = span("followTarget"),
                                          .properties = &target_properties};
    AbCameraSequence follow_sequence = {&follow_stage, 1};
    const AbCameraFollowBird *stable_target = NULL;
    for (size_t index = 0; index < 3; ++index) {
        /* Controlled constructor inputs exercise dynamic registration. These
         * positions/layers do not claim source split-body arithmetic. */
        AbGameCircleInput input = {.name = span(names[index]),
                                   .position = {(float)index * 2, -5},
                                   .radius = values[0],
                                   .density = values[1],
                                   .friction = values[2],
                                   .restitution = values[3],
                                   .controllable = true,
                                   .z_order = -0.25f};
        size_t created = SIZE_MAX;
        CHECK(ab_classic_level_create_circle(test->level, input, &created, error) &&
              created == count + index &&
              ab_classic_render_scene_create_object(view.scene, view.visuals, created,
                                                    sprite.string, -0.25f, error));
        CHECK(!ab_classic_camera_set_flying(view.camera, created, NULL) &&
              !ab_classic_camera_register_object(view.camera, created + 1, NULL) &&
              ab_classic_camera_register_object(view.camera, created, error) &&
              !ab_classic_camera_register_object(view.camera, created, NULL));
        if (!index) {
            target_entry.value =
                (AbDataValue){.kind = AB_DATA_TABLE,
                              .table = &level.properties->objects[created].physical};
            CHECK(ab_camera_system_start(ab_classic_camera_system(view.camera),
                                         &follow_sequence, NULL, NULL, error));
            stable_target =
                ab_camera_system_target_bird(ab_classic_camera_system(view.camera));
            CHECK(stable_target && stable_target->position.x == input.position.x &&
                  stable_target->position.y == input.position.y);
        }
        CHECK(ab_camera_system_target_bird(ab_classic_camera_system(view.camera)) ==
              stable_target);
        AbClassicRenderSceneView current = ab_classic_render_scene_view(view.scene);
        object_view = ab_classic_scene_view(objects);
        CHECK(current.object_count == created + 1 &&
              current.initial_count == before.initial_count &&
              current.count == before.count + index + 1 &&
              test->counter == counter + (uint32_t)index + 1 &&
              object_view.items == stable_items && object_view.count == created + 1 &&
              object_view.items[created].layer == 0 &&
              !object_view.items[created].unbound &&
              object_view.items[created].region && object_view.items[created].texture &&
              object_view.items[created].pose.scale.x == 1 &&
              ab_classic_visuals_object(view.visuals, source) == stable_visual &&
              stable_visual->sprite.data == old_sprite.data);
        CHECK(ab_classic_behaviors_create_object(
            view.behaviors, created,
            (AbDataValue){.kind = AB_DATA_TABLE, .table = &descriptor},
            &descriptor_data, error));
        level = ab_classic_level_view(test->level);
        /* Prepare the native collider setter used by ordinary split birds;
         * fixture filtering retains the direct circle's initial values. */
        level.frame->objects[created].collider = AB_COLLIDER_BIRD;
        level.bodies->objects[created].physics.collider =
            (AbDataValue){.kind = AB_DATA_NUMBER, .bits = bits(3)};
        CHECK(
            !ab_classic_shots_register_circle(view.shots, created + 1, NULL) &&
            ab_classic_shots_register_circle(view.shots, created, error) &&
            !ab_classic_shots_register_circle(view.shots, created, NULL) &&
            ab_classic_post_physics_register_circle(test->blue_post, created, error) &&
            !ab_classic_object_removal_register_circle(test->physical, created + 1,
                                                       NULL) &&
            ab_classic_object_removal_register_circle(test->physical, created, error) &&
            !ab_classic_object_removal_register_circle(test->physical, created, NULL) &&
            ab_classic_birds_register_object(test->birds, created, error));
        uintptr_t core = 0;
        bool present = false;
        CHECK(ab_string_map_get(physical_registry->objects, input.name, &core, &present,
                                error) &&
              present && core);
        new_core[index] = (AbGameObjectCore *)core;
        CHECK(new_core[index]->body == level.bodies->objects[created].physics.body &&
              new_core[index]->renderable && !new_core[index]->rolling_sound.size &&
              !new_core[index]->camera_animation &&
              physical_registry->bird_count == initial_native_birds + index + 1);
        AbLevelPropertyObject *binding = level.properties->objects + created;
        CHECK(field(&binding->physical, "gameObjectScript").table == &binding->script &&
              field(&binding->script, "physicsObject").table == &binding->physical &&
              field(&binding->script, "behaviors").table == &binding->behaviors &&
              binding->behaviors.count == 1 &&
              ab_classic_trajectories_view(view.trajectories).tracks ==
                  initial_tracks + index + 1);
        AbDataValue children =
            field(&binding->behaviors, "MuseumBluesTrajectoryRenderer");
        CHECK(children.kind == AB_DATA_TABLE && children.table->count == 1);
        AbDataValue behavior = children.table->entries[0].value;
        CHECK(behavior.kind == AB_DATA_TABLE &&
              field(behavior.table, "isCluster").kind == AB_DATA_BOOLEAN &&
              !field(behavior.table, "isCluster").bits &&
              field(behavior.table, "flying").bits &&
              field(behavior.table, "gameObject").table == &binding->script &&
              field(behavior.table, "physicsObject").table == &binding->physical &&
              !ab_classic_behaviors_create_object(view.behaviors, created,
                                                  (AbDataValue){0}, NULL, NULL));
    }
    AbDataTable *child_groups = &level.properties->objects[count + 2].behaviors;
    AbDataTable *trajectory =
        field(child_groups, "MuseumBluesTrajectoryRenderer").table;
    DynamicCameraCheck camera_check = {.test = test,
                                       .view = view,
                                       .object = count + 2,
                                       .logs = test->logs,
                                       .trajectory = trajectory};
    AbCameraSequenceStage wait_stage = {
        .action = span("wait"), .duration = 5, .duration_present = true};
    AbCameraSequence pending_sequence = {&wait_stage, 1};
    AbCameraSystem *camera_system = ab_classic_camera_system(view.camera);
    CHECK(ab_camera_system_start(camera_system, &pending_sequence, dynamic_camera_done,
                                 &camera_check, error) &&
          ab_classic_camera_set_flying(view.camera, count + 2, error) &&
          ab_classic_behaviors_add(view.behaviors, count + 2,
                                   span("MuseumBluesCameraFollowBehavior"),
                                   (AbDataValue){0}, error));
    AbDataValue camera_group = field(child_groups, "MuseumBluesCameraFollowBehavior");
    CHECK(camera_check.ends == 1 && test->logs == camera_check.logs + 2 &&
          child_groups->count == 2 && camera_group.kind == AB_DATA_TABLE &&
          camera_group.table->count == 1 &&
          field(child_groups, "MuseumBluesTrajectoryRenderer").table == trajectory &&
          ab_classic_camera_view(view.camera).loaded_sequence->count ==
              test->sequences[1].count &&
          ab_classic_camera_view(view.camera).object_count == count + 3);
    AbDataValue camera_behavior = camera_group.table->entries[0].value;
    CHECK(camera_behavior.kind == AB_DATA_TABLE &&
          field(camera_behavior.table, "gameObject").table ==
              &level.properties->objects[count + 2].script &&
          field(camera_behavior.table, "physicsObject").table ==
              &level.properties->objects[count + 2].physical &&
          ab_classic_behaviors_debug_draw(view.behaviors, count + 2, error));
    CHECK(ab_classic_render_scene_sync(view.scene, error) &&
          ab_classic_render_scene_update_gfx(view.scene, 0.125f, error));
    list.count = 0;
    CHECK(ab_classic_render_scene_draw(view.scene, camera, 480, 320, &list, error) &&
          list.count == initial_commands + 3);
    AbBlockCollisionNotice collision = {0};
    CHECK(
        ab_classic_behaviors_destroy(view.behaviors, count + 1, error) &&
        ab_classic_trajectories_view(view.trajectories).tracks == initial_tracks + 3 &&
        ab_classic_behaviors_collision(view.behaviors, count + 1, ground, &collision,
                                       error) &&
        ab_classic_trajectories_view(view.trajectories).tracks == initial_tracks + 2 &&
        ab_classic_object_removal_call(test->physical, span(names[1]), error));
    level = ab_classic_level_view(test->level);
    CHECK(level.removed_objects[count + 1] &&
          !ab_world_has_body(level.world, new_core[1]->body) &&
          !new_core[1]->renderable &&
          physical_registry->bird_count == initial_native_birds + 2 &&
          ab_level_properties_forget(level.properties, count + 1, error) &&
          ab_classic_render_scene_sync(view.scene, error) &&
          ab_classic_scene_sync_visuals(objects, view.visuals, error));
    CHECK(ab_classic_render_scene_view(view.scene).count == before.count + 2 &&
          !ab_classic_scene_view(objects).items[count + 1].visible &&
          ab_classic_visuals_object(view.visuals, count + 1)->attached == NULL);
    list.count = 0;
    CHECK(ab_classic_render_scene_draw(view.scene, camera, 480, 320, &list, error) &&
          list.count == initial_commands + 2);
    size_t replacement = SIZE_MAX;
    AbGameCircleInput replacement_input = {.name = span("test_blue_replacement"),
                                           .position = {2, -5},
                                           .radius = values[0],
                                           .density = values[1],
                                           .controllable = true};
    CHECK(ab_classic_level_create_circle(test->level, replacement_input, &replacement,
                                         error) &&
          replacement == count + 3 &&
          level.bodies->objects[replacement].physics.body == new_core[1]->body &&
          ab_classic_render_scene_create_object(view.scene, view.visuals, replacement,
                                                sprite.string, 0, error));
    level.frame->objects[replacement].collider = AB_COLLIDER_BIRD;
    level.bodies->objects[replacement].physics.collider =
        (AbDataValue){.kind = AB_DATA_NUMBER, .bits = bits(3)};
    CHECK(
        ab_classic_shots_register_circle(view.shots, replacement, error) &&
        ab_classic_post_physics_register_circle(test->blue_post, replacement, error) &&
        ab_classic_object_removal_register_circle(test->physical, replacement, error));
    CHECK(ab_classic_camera_register_object(view.camera, replacement, error) &&
          ab_classic_birds_register_object(test->birds, replacement, error));
    uintptr_t replacement_handle = 0, retired_handle = 0;
    bool replacement_present = false, retired_present = true;
    CHECK(ab_string_map_get(physical_registry->objects, replacement_input.name,
                            &replacement_handle, &replacement_present, error) &&
          ab_string_map_get(physical_registry->objects, span(names[1]), &retired_handle,
                            &retired_present, error) &&
          replacement_present && replacement_handle && !retired_present &&
          replacement_handle != (uintptr_t)new_core[1] &&
          physical_registry->bird_count == initial_native_birds + 3 &&
          !new_core[1]->renderable);
    CHECK(
        ab_classic_object_removal_call(test->physical, replacement_input.name, error) &&
        level.removed_objects[replacement] &&
        physical_registry->bird_count == initial_native_birds + 2 &&
        ab_level_properties_forget(level.properties, replacement, error) &&
        ab_classic_render_scene_sync(view.scene, error) &&
        ab_classic_scene_sync_visuals(objects, view.visuals, error));
    list.count = 0;
    CHECK(ab_classic_render_scene_draw(view.scene, camera, 480, 320, &list, error) &&
          list.count == initial_commands + 2);
    size_t failed_object = SIZE_MAX;
    AbGameCircleInput final = {.name = span("test_blue_circle_failed"),
                               .radius = values[0],
                               .density = values[1]};
    uint8_t oversized[129];
    memset(oversized, 'x', sizeof(oversized));
    CHECK(ab_classic_level_create_circle(test->level, final, &failed_object, error) &&
          failed_object == count + 4 &&
          !ab_classic_render_scene_create_object(
              view.scene, view.visuals, failed_object,
              (AbSpan){oversized, sizeof(oversized)}, 0, NULL));
    level = ab_classic_level_view(test->level);
    AbDataValue failed_sprite =
        field(&level.properties->objects[failed_object].physical, "sprite");
    CHECK(test->counter == counter + 5 &&
          ab_classic_render_scene_view(view.scene).count == before.count + 3 &&
          ab_classic_render_scene_view(view.scene).object_count == count + 5 &&
          ab_classic_visuals_object(view.visuals, failed_object)->attached &&
          failed_sprite.kind == AB_DATA_STRING && !failed_sprite.string.size);
    /* Discard after this reached storage error; prior effects intentionally
     * remain. No continued gameplay is tested with this failed construction. */
    AbDataTable *failed_groups = &level.properties->objects[count].behaviors;
    AbDataTable *retained_trajectory =
        field(failed_groups, "MuseumBluesTrajectoryRenderer").table;
    size_t logs = test->logs;
    CHECK(!ab_classic_behaviors_add(view.behaviors, count, span("unrecovered_class"),
                                    (AbDataValue){0}, NULL));
    AbDataValue empty_group = field(failed_groups, "unrecovered_class");
    CHECK(empty_group.kind == AB_DATA_TABLE && !empty_group.table->count &&
          failed_groups->count == 2 && test->logs == logs &&
          field(failed_groups, "MuseumBluesTrajectoryRenderer").table ==
              retained_trajectory);
    uint8_t short_name[1];
    AbDataTable empty_birds = {0};
    AbDataValue birds = {.kind = AB_DATA_TABLE, .table = &empty_birds};
    AbClassicBluesChild bounded;
    size_t unchanged_object = SIZE_MAX;
    AbSpan unchanged_name = span("unchanged");
    CHECK(ab_classic_blues_child_initialize(
        (AbClassicBluesChildConfig){.playfield = view,
                                    .birds = &birds,
                                    .names = {short_name, sizeof(short_name)}},
        &bounded, error));
    CHECK(!ab_classic_blues_child_create(&bounded, source, (AbSpan){NULL, 1},
                                         (AbVec2){0}, &unchanged_object,
                                         &unchanged_name, NULL) &&
          !bounded.failed && !bounded.used &&
          !ab_classic_blues_child_create(&bounded, source, span("a"), (AbVec2){0},
                                         &unchanged_object, &unchanged_name, NULL) &&
          bounded.failed && !bounded.used && unchanged_object == SIZE_MAX &&
          unchanged_name.size == 9 && !memcmp(unchanged_name.data, "unchanged", 9) &&
          level.bodies->count == count + 5);
    return true;
}

static bool check_blue(PlayfieldTest *test, AbClassicPlayfieldConfig config,
                       AbError *error) {
    /* Release retained payload views before their playfield. Camera source
     * resources and audio remain reusable across these two level instances. */
    for (size_t index = 0; index < 4; ++index)
        test->retained[index] = NULL;
    test->shot_state = (AbShotState){0};
    ab_classic_playfield_close(test->owner);
    test->owner = NULL;
    ab_sprite_atlas_close(test->atlas);
    test->atlas = NULL;
    ab_classic_level_close(test->level);
    test->level = NULL;
    test->flying = (AbDataValue){0};
    CHECK(open_game_resources(test, 480, 320, error) &&
          open_level(test, "assets/data/levels/pack1/Level8.lua", error));
    AbClassicLevelView level = ab_classic_level_view(test->level);
    AbDataValue theme = field(level.data->globals, "theme");
    CHECK(theme.kind == AB_DATA_STRING && theme.string.size == 6 &&
          !memcmp(theme.string.data, "theme1", 6));
    config.level = test->level;
    config.camera.object_capacity = level.bodies->capacity;
    config.sprites = &test->grouped_sprites.provider;
    config.limits.behaviors = 512 * 1024;
    config.limits.behavior_objects = level.bodies->capacity;
    config.limits.shot_objects = level.bodies->capacity;
    config.limits.shots = 65536;
    if (test->child_reference || test->split_reference)
        config.limits.tracks = 64;
    config.limits.storage_bytes = 2 * 1024 * 1024;
    CHECK(ab_classic_playfield_open(config, &test->owner, error));
    AbClassicPlayfieldView view = ab_classic_playfield_view(test->owner);
    CHECK(ab_classic_post_physics_open(
              test->level,
              (AbClassicPostConfig){.minimum_x = -100000,
                                    .maximum_x = 100000,
                                    .storage_bytes = 256 * 1024,
                                    .object_capacity = level.bodies->capacity},
              &test->blue_post, error) &&
          ab_string_map_open(0, 8, 4096, &test->teleporters, error) &&
          ab_classic_object_removal_open(
              (AbClassicObjectRemovalConfig){
                  .level = test->level,
                  .visuals = view.visuals,
                  .scene = view.scene,
                  .rolling_levels =
                      ab_classic_post_physics_view(test->blue_post).rolling_levels,
                  .teleporters = test->teleporters,
                  .storage_bytes = 65536,
                  .object_capacity = level.bodies->capacity},
              &test->physical, error));
    size_t object = SIZE_MAX;
    for (size_t index = 0; index < level.bodies->count; ++index) {
        AbSpan name = level.bodies->objects[index].definition_name;
        if (name.size == 13 && !memcmp(name.data, "SmallBlueBird", 13)) {
            object = index;
            break;
        }
    }
    CHECK(object != SIZE_MAX);
    CHECK(open_blue_birds(test, view, object, error));
    AbDataValue group = field(&level.properties->objects[object].behaviors,
                              "MuseumBluesTrajectoryRenderer");
    CHECK(group.kind == AB_DATA_TABLE);
    const AbDataValue *instance =
        ab_data_get(group.table, (AbDataValue){.kind = AB_DATA_NUMBER,
                                               .bits = UINT32_C(0x3f800000)});
    CHECK(instance && instance->kind == AB_DATA_TABLE &&
          field(instance->table, "isCluster").kind == AB_DATA_BOOLEAN &&
          field(instance->table, "isCluster").bits &&
          !ab_classic_trajectories_view(view.trajectories).tracks);
    if (test->split_reference)
        return check_blue_split_native(test, view, test->split_reference, error);
    /* Actual source class only: launch physics and split-body creation are
     * outside this integration check. */
    CHECK(ab_classic_behaviors_launch(view.behaviors, object, (AbVec2){0}, error) &&
          field(instance->table, "flying").bits &&
          ab_classic_trajectories_view(view.trajectories).tracks == 1);
    if (test->child_reference) {
        CHECK(ab_classic_behaviors_specialty(view.behaviors, object, error) &&
              field(instance->table, "split").bits &&
              !ab_classic_trajectories_view(view.trajectories).tracks &&
              check_blues_child(test, view, object, test->child_reference, error) &&
              check_blue_lifecycle(test, view, object, error) &&
              check_dynamic_renderers(test, view, object, error));
    } else {
        CHECK(check_blue_split(test, view, object, error));
    }
    return true;
}

static bool exercise(PlayfieldTest *test, FILE *reference, AbError *error) {
    char marker[80];
    uint32_t controls[8], data[17], expected[5];
    CHECK(fgets(marker, sizeof(marker), reference) &&
          !strcmp(marker, "AB_LIVE_BIRD_SHOT_V1 1\n") &&
          read_words(reference, controls, 8) && read_words(reference, data, 17) &&
          read_words(reference, expected, 5));
    test->globals =
        (AbClassicGameGlobals){.time = {.kind = AB_DATA_NUMBER},
                               .score = {.kind = AB_DATA_NUMBER},
                               .last_score_time = {.kind = AB_DATA_NUMBER},
                               .old_score = {.kind = AB_DATA_NUMBER},
                               .extra_tutorial_timer = {.kind = AB_DATA_NUMBER},
                               .specialty_available = {.kind = AB_DATA_BOOLEAN}};
    test->enabled = (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1};
    test->next_sound =
        (AbDataValue){.kind = AB_DATA_STRING, .string = span("bird_next_military")};
    AbClassicPlayfieldConfig config = configuration(test, controls, data);
    test->shot_state.next_bird_timer = 0.125f;
    test->shot_state.birds_shot = -7;
    AbClassicPlayfield *invalid = (AbClassicPlayfield *)(uintptr_t)1;
    AbClassicPlayfieldConfig bad = config;
    bad.limits.storage_bytes = 1;
    CHECK(!ab_classic_playfield_open(bad, &invalid, NULL) && !invalid &&
          !test->counter && !ab_classic_playfield_bytes(NULL));
    /* Fail after scene/camera/behaviors/services/shots were created. Teardown
     * must release their storage and unbind behavior groups. Discard the level
     * because its published visual fields need not survive teardown. */
    bad = config;
    bad.limits.sling = 1;
    CHECK(!ab_classic_playfield_open(bad, &invalid, NULL) && !invalid && test->counter);
    AbClassicLevelView level = ab_classic_level_view(test->level);
    for (size_t index = 0; index < level.properties->count; ++index)
        CHECK(!level.properties->objects[index].behaviors.entries);
    ab_sprite_atlas_close(test->atlas);
    test->atlas = NULL;
    ab_classic_level_close(test->level);
    test->level = NULL;
    CHECK(open_level(test, "assets/data/levels/pack1/Level1.lua", error));
    level = ab_classic_level_view(test->level);
    CHECK(ab_sprite_atlas_open(&test->archive, level.sprites, 16 * 1024 * 1024,
                               8 * 1024 * 1024, &test->atlas, error));
    test->sprites.context = test->atlas;
    config.level = test->level;
    uint32_t consumed = test->counter;
    CHECK(ab_classic_playfield_open(config, &test->owner, error) &&
          ab_classic_playfield_bytes(test->owner) <= config.limits.storage_bytes &&
          test->counter > consumed && !test->logs && !test->notifications &&
          !ab_audio_mixer_view(test->mixer).pending_count);
    AbClassicPlayfieldView view = ab_classic_playfield_view(test->owner);
    level = ab_classic_level_view(test->level);
    CHECK(view.level == test->level && view.visuals && view.scene && view.camera &&
          view.trajectories && view.behaviors && view.shots && view.services &&
          view.sling);
    size_t object = SIZE_MAX;
    for (size_t index = 0; index < level.bodies->count; ++index) {
        AbSpan name = level.bodies->objects[index].name;
        if (name.size == 9 && !memcmp(name.data, "RedBird_4", 9))
            object = index;
    }
    CHECK(object != SIZE_MAX);
    size_t body_index = level.bodies->objects[object].physics.body;
    AbShotBird *bird = ab_classic_shots_bird(view.shots, object);
    CHECK(bird && bits(bird->mass) == controls[7] &&
          ab_world_set_body_type(level.world, body_index, AB_BODY_DYNAMIC, error));
    /* Prepared captured launch, not a source fresh-session defaults claim. */
    AbShotState *shot = ab_classic_shots_state(view.shots);
    CHECK(shot == &test->shot_state && shot->next_bird_timer == 0.125f &&
          shot->birds_shot == -7);
    *shot = (AbShotState){.dragged = bird,
                          .bird_count = (float)level.bird_count,
                          .bird_reload_delay = 0.5f,
                          .bird_ready = true};
    AbVec2 position = {from_bits(data[11]), from_bits(data[12])};
    AbVec2 impulse = {from_bits(data[13]), from_bits(data[14])};
    CHECK(ab_classic_shots_launch(view.shots, &position, &impulse, error));
    AbBodyMotion *body = &level.world->storage.bodies[body_index].motion;
    uint32_t actual[] = {36 | (body->awake ? 2u : 0u),
                         bits(body->mass.linear_velocity.x),
                         bits(body->mass.linear_velocity.y),
                         bits(body->mass.angular_velocity), bits(body->sleep_time)};
    CHECK(!memcmp(actual, expected, sizeof(actual)));
    CHECK(shot->birds_shot == 1 && shot->flying == bird && !shot->dragged);
    CHECK(test->logs == 2 && test->notifications == 1 &&
          ab_classic_sling_view(view.sling).launch_cue);
    CHECK(ab_classic_trajectories_view(view.trajectories).tracks == 1 &&
          ab_audio_mixer_view(test->mixer).pending_count == 2);
    CHECK(ab_classic_trajectories_update(view.trajectories, error) &&
          !ab_classic_trajectories_view(view.trajectories).points);
    /* Explicit post-capture motion exercises point creation. Full simulation
     * scheduling is outside this constructor comparison. */
    CHECK(ab_classic_shots_set_position(
              view.shots, object, (AbVec2){bird->position.x + 2, bird->position.y},
              error) &&
          ab_classic_trajectories_update(view.trajectories, error) &&
          ab_classic_trajectories_view(view.trajectories).points == 1);
    const AbDataTable *first_position =
        field(test->retained[0], "launchPosition").table;
    CHECK(first_position);
    for (size_t queued = 0; queued < 2; ++queued) {
        for (size_t frame = 0; frame < 8 && !shot->bird_ready; ++frame)
            CHECK(ab_classic_sling_update(view.sling, 0.25f, error));
        CHECK(shot->bird_ready && shot->current_bird_name &&
              ab_classic_sling_select(view.sling, error) &&
              ab_classic_sling_stretch(view.sling, (AbVec2){4, -2}, error) &&
              ab_classic_sling_release(view.sling, error) && !shot->bird_ready &&
              shot->birds_shot == (float)queued + 2 &&
              test->notifications == queued + 2 &&
              field(test->retained[queued + 1], "launchPosition").table !=
                  first_position);
        size_t flying = SIZE_MAX;
        for (size_t index = 0; index < level.bodies->count; ++index)
            if (ab_classic_shots_bird(view.shots, index) == shot->flying)
                flying = index;
        CHECK(flying != SIZE_MAX &&
              ab_classic_shots_set_position(
                  view.shots, flying,
                  (AbVec2){shot->flying->position.x + 2, shot->flying->position.y},
                  error) &&
              ab_classic_trajectories_update(view.trajectories, error));
    }
    CHECK(test->logs == 6 && ab_shot_services_notification_count(view.services) == 4 &&
          ab_classic_trajectories_view(view.trajectories).tracks == 3 &&
          ab_classic_render_scene_view(view.scene).extra_count >= 3 &&
          field(first_position, "x").bits ==
              bits(config.start.pose.level_start.x - 0.1f));
    CHECK(fgetc(reference) == '\n' && fgets(marker, sizeof(marker), reference) &&
          !strcmp(marker, "AB live bird shot complete\n") && fgetc(reference) == EOF);
    CHECK(check_blue(test, config, error));
    puts("Production playfield owns "
         "scene/camera/behaviors/trajectories/shots/services/sling. "
         "Prepared first launch matches native body words; both remaining birds reload "
         "and launch through actual resource audio, camera and trajectory owners. "
         "Level8 source Blues behavior uses the same real trajectory owner. "
         "Fresh global initialization, continuous frames, full GUI and GPU submission "
         "remain separate.");
    return true;
}

static void close_test(PlayfieldTest *test) {
    if (test->owner)
        ab_classic_playfield_bind_other_birds(test->owner, NULL, NULL);
    ab_classic_birds_close(test->birds);
    ab_classic_particles_close(test->blue_particles);
    ab_string_map_close(test->blue_hats);
    ab_classic_object_removal_close(test->physical);
    ab_classic_post_physics_close(test->blue_post);
    ab_string_map_close(test->teleporters);
    ab_event_manager_close(test->events);
    test->shot_state = (AbShotState){0};
    ab_classic_playfield_close(test->owner);
    ab_audio_mixer_close(test->mixer);
    ab_audio_bank_close(test->audio);
    ab_powerup_uses_close(test->powerups);
    ab_settings_statistics_close(test->statistics);
    ab_sprite_atlas_close(test->atlas);
    ab_classic_level_close(test->level);
    ab_asset_textures_close(&test->textures, NULL);
    ab_asset_resources_close(test->resources);
    ab_android_assets_close(test->planning);
    ab_target_data_close(&test->themes);
    for (size_t index = 0; index < 3; ++index) {
        ab_camera_sequence_free(test->sequences + index);
        ab_target_data_close(test->source + index);
    }
    ab_zip_close(&test->archive);
    ab_buffer_free(&test->bytes);
}

int main(int argc, char **argv) {
    if (argc != 3 && argc != 4 && !(argc == 5 && !strcmp(argv[3], "--blues-split"))) {
        fputs("Usage: ab_classic_playfield_tests PINNED_APK LIVE_SHOT_CORPUS "
              "[BLUES_CHILD_CORPUS | --blues-split BLUES_SPLIT_CORPUS]\n"
              "       ab_classic_playfield_tests PINNED_APK --fresh\n",
              stderr);
        return 2;
    }
    PlayfieldTest test = {0};
    AbError error = {0};
    bool fresh_only = argc == 3 && !strcmp(argv[2], "--fresh");
    FILE *reference = fresh_only ? NULL : fopen(argv[2], "rb");
    if (argc == 4)
        test.child_reference = fopen(argv[3], "r");
    if (argc == 5)
        test.split_reference = fopen(argv[4], "r");
    bool okay = (fresh_only || reference) &&
                (argc == 3 || test.child_reference || test.split_reference) &&
                ab_file_read(argv[1], 64 * 1024 * 1024, &test.bytes, &error) &&
                ab_zip_open((AbSpan){test.bytes.data, test.bytes.size}, &test.archive,
                            &error) &&
                open_level(&test, "assets/data/levels/pack1/Level1.lua", &error) &&
                open_resources(&test, &error) &&
                fresh_runtime(&test, fresh_only, &error) &&
                (fresh_only || exercise(&test, reference, &error));
    if (!okay && error.message[0])
        fprintf(stderr, "%s\n", error.message);
    if (reference && fclose(reference))
        okay = false;
    if (test.child_reference && fclose(test.child_reference))
        okay = false;
    if (test.split_reference && fclose(test.split_reference))
        okay = false;
    close_test(&test);
    return okay ? 0 : 1;
}
