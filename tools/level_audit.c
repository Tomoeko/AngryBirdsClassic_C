#include "ab/android_assets.h"
#include "ab/asset_sprite_provider.h"
#include "ab/classic_render_scene.h"
#include "ab/classic_behaviors.h"
#include "ab/classic_characters.h"
#include "ab/classic_simulation.h"
#include "ab/classic_shots.h"
#include "ab/target_data.h"
#include "ab/theme_assets.h"
#include "ab/hash.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    bool scene, behaviors, simulation, characters, shots, damage_query;
    AbError scene_error, behavior_error, simulation_error, character_error, shot_error;
    AbError damage_query_error;
} AuditStages;

typedef struct {
    AbAndroidAssets *planning;
    AbAssetResources *resources;
    size_t missing;
} Host;

static AbDataValue text(const char *value) {
    return (AbDataValue){.kind = AB_DATA_STRING,
                         .string = {(const uint8_t *)value, strlen(value)}};
}

static bool manage(void *context, AbSpan group, bool load, AbError *error) {
    Host *host = context;
    AbDataEntry entry = {{.kind = AB_DATA_NUMBER, .bits = 0x3f800000},
                         {.kind = AB_DATA_STRING, .string = group}};
    AbDataTable requested = {&entry, 1, 1, NULL};
    return ab_android_assets_manage(host->planning, &requested, load,
                                    ab_asset_resources_host, host->resources, error);
}

static bool upload(void *context, uint32_t id, AbSpriteAtlasImage image,
                   AbError *error) {
    (void)context;
    return (id && id <= 64 && image.image && image.image->rgba.data) ||
           ab_fail(error, "Invalid logical texture upload");
}

static bool remove_texture(void *context, uint32_t id, AbError *error) {
    (void)context;
    return (id && id <= 64) || ab_fail(error, "Invalid logical texture release");
}

static void missing(void *context, size_t object, AbSpan name) {
    Host *host = context;
    if (host->missing < 5) {
        fprintf(stderr, "Missing visual at object %zu: %.*s\n", object, (int)name.size,
                (const char *)name.data);
    }
    ++host->missing;
}

static bool check_level(const AbZip *archive, const char *path, size_t objects,
                        AbAssetSpriteProvider *adapter, Host *host, size_t *bodies,
                        size_t *ordinary, size_t *composites, AuditStages *stages,
                        AbError *error) {
    AbClassicLevel *level = NULL;
    AbClassicVisuals *visuals = NULL;
    AbClassicRenderScene *scene = NULL;
    AbClassicBehaviors *behaviors = NULL;
    AbClassicSimulation *simulation = NULL;
    AbClassicCharacters *characters = NULL;
    AbClassicShots *shots = NULL;
    AbDataValue flying = {0};
    AbRandom48 random;
    uint32_t counter = 0;
    bool okay = false;
    if (!ab_random48_seed(&random, 0))
        return ab_fail(error, "Cannot initialize explicit audit seed");
    AbClassicLevelAssets assets = {.level_path = path,
                                   .catalog_path = "assets/data/scripts/blocks.lua",
                                   .resource_bytes = 32 * 1024 * 1024,
                                   .placement_scale = 1,
                                   .physics_scale = 0.05f,
                                   .derive_ground_position = true,
                                   .derive_world_gravity = true,
                                   .sprite_context = adapter,
                                   .sprite_size = ab_asset_sprite_provider_size};
    assets.random = &random;
    size_t contacts = objects * 32;
    size_t nodes = objects * 4 < 16 ? 16 : objects * 4;
    AbWorldDefinition definition = {.broadphase_limits = {nodes, contacts, contacts},
                                    .maximum_translation = 2,
                                    .maximum_translation_squared = 4,
                                    .warm_start = true,
                                    .clear_forces = true,
                                    .continuous = true};
    AbClassicLevelLimits limits = {objects, contacts, 32 * 1024 * 1024, 0, 0};
    if (!ab_classic_level_load(archive, &assets, definition, limits, true, &level,
                               error)) {
        goto release;
    }
    AbClassicLevelView view = ab_classic_level_view(level);
    *bodies = view.bodies->count;
    if (!ab_classic_visuals_open(
            level,
            (AbClassicVisualResources){.provider = &adapter->provider,
                                       .context = host,
                                       .missing = missing,
                                       .storage_bytes = 4 * 1024 * 1024,
                                       .name_capacity = 256},
            &visuals, error)) {
        goto release;
    }
    size_t range_indices[64];
    AbStringOrderNode range_nodes[64];
    AbStringOrderWorkspace ranges = {range_indices, 64, range_nodes, 64};
    stages->damage_query = true;
    for (size_t index = 0; index < view.bodies->count; ++index) {
        AbDamageSpriteNames names;
        if (stages->damage_query &&
            !ab_classic_visuals_damage_names(visuals, index, &ranges, &names,
                                             &stages->damage_query_error))
            stages->damage_query = false;
        const AbObjectVisual *object = ab_classic_visuals_object(visuals, index);
        if (object->attached) {
            *ordinary += object->attached->sprite != NULL;
            *composites += object->attached->composite != NULL;
        }
    }
    stages->scene = ab_classic_render_scene_open(
        level,
        (AbClassicRenderSceneConfig){.provider = &adapter->provider,
                                     .id_counter = &counter,
                                     .extra_renderers = 128,
                                     .storage_bytes = 4 * 1024 * 1024,
                                     .object_blend = AB_BLEND_PREMULTIPLIED},
        &scene, &stages->scene_error);
    stages->scene = stages->scene && ab_classic_scene_sync_visuals(
                                         ab_classic_render_scene_objects(scene),
                                         visuals, &stages->scene_error);
    stages->behaviors = ab_classic_behaviors_open(
        level,
        (AbClassicBehaviorConfig){.flying_bird = &flying,
                                  .storage_bytes = 16 * 1024 * 1024},
        &behaviors, &stages->behavior_error);
    /* Construction checks only. No fabricated phase, audio or behavior services
     * are supplied; advancing a session requires their real implementations. */
    stages->simulation = ab_classic_simulation_open(
        level,
        (AbClassicSimulationConfig){.minimum_x = INT32_MIN,
                                    .maximum_x = INT32_MAX,
                                    .velocity_iterations = 8,
                                    .position_iterations = 3,
                                    .maximum_steps = 64,
                                    .random = &random,
                                    .limits = {.storage_bytes = 16 * 1024 * 1024,
                                               .queued_forces = 128,
                                               .force_sound_choices = 128,
                                               .glue_pairs = 128}},
        &simulation, &stages->simulation_error);
    stages->characters = ab_classic_characters_open(
        level, visuals, &random, (AbClassicCharacterHost){0}, 16 * 1024 * 1024,
        &characters, &stages->character_error);
    stages->shots =
        ab_classic_shots_open((AbClassicShotConfig){.level = level,
                                                    .visuals = visuals,
                                                    .behaviors = behaviors,
                                                    .flying_object = &flying,
                                                    .storage_bytes = 1024 * 1024},
                              &shots, &stages->shot_error);
    okay = true;

release:
    ab_classic_shots_close(shots);
    ab_classic_characters_close(characters);
    ab_classic_simulation_close(simulation);
    ab_classic_behaviors_close(behaviors);
    ab_classic_render_scene_close(scene);
    ab_classic_visuals_close(visuals);
    ab_classic_level_close(level);
    return okay;
}

static bool pinned_apk(AbSpan input, AbError *error) {
    uint8_t digest[32];
    char hex[65];
    if (!ab_sha256(input, digest, error))
        return false;
    ab_digest_hex(digest, hex);
    return strcmp(hex,
                  "b524b6b4b0ad999f46828546dee1bb047d6e00f724285a2e8c9f4b34a8bee7ad") ==
               0 ||
           ab_fail(error, "Level audit requires the pinned Angry Birds 5.2.0 APK");
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fputs("Usage: ab_level_audit PINNED_APK CATALOG_LEVEL_THEMES_TSV\n"
              "Rows contain an APK level path, tab, and declared theme name.\n"
              "Checks owner construction with explicit seed zero; does not run "
              "gameplay or establish native parity.\n",
              stderr);
        return 2;
    }
    AbBuffer bytes = {0};
    AbZip archive = {0};
    AbTargetData themes = {0}, catalog = {0};
    AbAssetTextures *textures = NULL;
    AbAssetSpriteProvider adapter = {0};
    Host host = {0};
    AbThemeAssets transition;
    AbDataTable empty = {0};
    AbSpan scratch[32];
    uint8_t key[32] = {0};
    AbError error = {0};
    FILE *paths = fopen(argv[2], "rb");
    bool okay = false;
    AbAndroidAssetLimits planning = {480, 320, false, false, false, 64, 8, 1024 * 1024};
    AbAssetResourceLimits resource_limits = {128, 32768, 128 * 1024 * 1024,
                                             16 * 1024 * 1024};
    if (!paths || !ab_file_read(argv[1], 64 * 1024 * 1024, &bytes, &error) ||
        !pinned_apk((AbSpan){bytes.data, bytes.size}, &error) ||
        !ab_zip_open((AbSpan){bytes.data, bytes.size}, &archive, &error) ||
        !ab_target_resource_key(&archive, 32 * 1024 * 1024, key, &error) ||
        !ab_target_data_open(&archive, "assets/data/scripts/themes.lua",
                             (AbSpan){key, sizeof(key)}, 4 * 1024 * 1024, NULL, &themes,
                             &error) ||
        !ab_target_data_open(&archive, "assets/data/scripts/blocks.lua",
                             (AbSpan){key, sizeof(key)}, 32 * 1024 * 1024, NULL,
                             &catalog, &error) ||
        !ab_android_assets_open(&archive, (AbSpan){key, sizeof(key)}, planning,
                                &host.planning, &error) ||
        !ab_asset_resources_open(&archive, resource_limits, &host.resources, &error) ||
        !ab_asset_textures_open((AbAssetTextureHost){NULL, upload, remove_texture}, 64,
                                128 * 1024 * 1024, &textures, &error) ||
        !ab_asset_sprite_provider_init(host.resources, textures, &adapter, &error) ||
        !ab_theme_assets_init(&empty, scratch, 16, &transition, &error) ||
        !manage(&host, text("INGAME").string, true, &error) ||
        !manage(&host, text("INGAME_PIGS_NEW").string, true, &error)) {
        goto release;
    }
    size_t count = 0, passed = 0, scenes = 0, missing_levels = 0, composite_levels = 0;
    puts("level\ttheme\tload\tobjects\tsprites\tcomposites\tmissing_visuals"
         "\trender_scene\tbehaviors\tsimulation\tcharacters\tshot_metadata\tload_error"
         "\trender_error\tbehavior_error\tsimulation_error\tcharacter_error\tshot_"
         "error\tdamage_query\tdamage_query_error");
    char line[640];
    while (fgets(line, sizeof(line), paths)) {
        size_t size = strlen(line);
        char *theme_name = strchr(line, '\t');
        if (!size || line[size - 1] != '\n' || !theme_name) {
            goto release;
        }
        line[size - 1] = 0;
        *theme_name++ = 0;
        const AbDataValue *theme = ab_data_get(themes.data.globals, text(theme_name));
        const AbDataValue *graphics =
            theme && theme->kind == AB_DATA_TABLE
                ? ab_data_get(theme->table, text("graphicSetName"))
                : NULL;
        error = (AbError){0};
        if (!graphics ||
            !ab_theme_assets_transition(&transition, *graphics, false, manage, &host,
                                        &error) ||
            !ab_asset_textures_sync(textures, ab_asset_resources_owners(host.resources),
                                    &error)) {
            fprintf(stderr, "Cannot select actual theme groups for %s: %s\n", line,
                    error.message);
            goto release;
        }
        AbTargetData data = {0};
        if (!ab_target_data_open(&archive, line, (AbSpan){key, sizeof(key)},
                                 32 * 1024 * 1024, catalog.data.globals, &data,
                                 &error)) {
            goto release;
        }
        const AbDataValue *world = ab_data_get(data.data.globals, text("world"));
        const AbDataValue *declared = ab_data_get(data.data.globals, text("theme"));
        bool matches = declared && declared->kind == AB_DATA_STRING &&
                       declared->string.size == strlen(theme_name) &&
                       !memcmp(declared->string.data, theme_name, strlen(theme_name));
        size_t objects =
            world && world->kind == AB_DATA_TABLE ? world->table->count + 1 : 0;
        ab_target_data_close(&data);
        if (!matches || !objects || objects > 2048) {
            ab_fail(&error, "Invalid level world or mismatched declared theme");
            goto release;
        }
        size_t bodies = 0, ordinary = 0, composites = 0;
        AuditStages stages = {0};
        host.missing = 0;
        bool level_okay = check_level(&archive, line, objects, &adapter, &host, &bodies,
                                      &ordinary, &composites, &stages, &error);
        const char *status = !level_okay    ? "failed"
                             : composites   ? "composite"
                             : host.missing ? "missing"
                             : stages.scene ? "ordinary"
                                            : "unbound";
        printf("%s\t%s\t%s\t%zu\t%zu\t%zu\t%zu\t%d\t%d\t%d\t%d\t%d"
               "\t%s\t%s\t%s\t%s\t%s\t%s\t%d\t%s\n",
               line, theme_name, status, bodies, ordinary, composites, host.missing,
               stages.scene, stages.behaviors, stages.simulation, stages.characters,
               stages.shots, error.message, stages.scene_error.message,
               stages.behavior_error.message, stages.simulation_error.message,
               stages.character_error.message, stages.shot_error.message,
               stages.damage_query, stages.damage_query_error.message);
        fflush(stdout);
        ++count;
        passed += level_okay;
        scenes += stages.scene;
        missing_levels += host.missing != 0;
        composite_levels += composites != 0;
    }
    fprintf(stderr,
            "Catalog owner construction: %zu rows, %zu loaded, %zu "
            "shared render owners, %zu missing and %zu composite levels.\n",
            count, passed, scenes, missing_levels, composite_levels);
    okay = count != 0 && !ferror(paths) && !ferror(stdout);

release:
    if (!okay && error.message[0]) {
        fprintf(stderr, "%s\n", error.message);
    }
    if (!ab_asset_textures_close(&textures, &error)) {
        okay = false;
    }
    ab_asset_resources_close(host.resources);
    ab_android_assets_close(host.planning);
    ab_target_data_close(&catalog);
    ab_target_data_close(&themes);
    ab_zip_close(&archive);
    ab_buffer_free(&bytes);
    memset(key, 0, sizeof(key));
    if (paths && fclose(paths)) {
        okay = false;
    }
    return okay ? 0 : 1;
}
