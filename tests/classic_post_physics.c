#include "ab/classic_post_physics.h"
#include "ab/classic_audio.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Classic post-physics failed at %d: %s\n", __LINE__,       \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

static AbDataValue string(const char *text) {
    return (AbDataValue){.kind = AB_DATA_STRING,
                         .string = {(const uint8_t *)text, strlen(text)}};
}

static AbDataValue number(float value) {
    AbDataValue result = {.kind = AB_DATA_NUMBER};
    memcpy(&result.bits, &value, sizeof(value));
    return result;
}

static AbDataValue table(AbDataTable *value) {
    return (AbDataValue){.kind = AB_DATA_TABLE, .table = value};
}

typedef struct {
    AbRollingAudioCall calls[16];
    size_t count;
    bool playing, fail;
} AudioTest;

static bool audio_host(void *context, AbRollingAudioCall *call, AbError *error) {
    AudioTest *test = context;
    CHECK(test->count < 16);
    test->calls[test->count++] = *call;
    if (test->fail) {
        return ab_fail(error, "Requested rolling host failure");
    }
    if (call->operation == AB_ROLLING_AUDIO_QUERY) {
        call->playing = test->playing;
    }
    return true;
}

static bool audio_is(AudioTest *test, size_t index, AbRollingAudioOperation operation,
                     const char *name, float gain) {
    CHECK(index < test->count);
    const AbRollingAudioCall *call = test->calls + index;
    CHECK(call->operation == operation && call->name.size == strlen(name) &&
          memcmp(call->name.data, name, call->name.size) == 0 && call->gain == gain);
    return true;
}

#include "classic_simulation.h"

static bool rolling_resource_checks(AbClassicLevel *level, AbAudioMixer **mixer_output,
                                    AbClassicPostPhysics **post_output) {
    const int16_t pcm[] = {1000, -1000};
    AbAudioClipBinding clips[] = {{string("alpha").string, 0, {pcm, 2, 1}},
                                  {string("zeta").string, 1, {pcm, 2, 1}}};
    AbAudioResources resources;
    CHECK(
        ab_audio_mixer_open((AbAudioMixerLimits){4, 2, 1, 65536}, mixer_output, NULL) &&
        ab_audio_mixer_start(*mixer_output, true, NULL) &&
        ab_audio_resources_bind(*mixer_output, clips, 2, &resources, NULL));
    AbClassicPostConfig config = {.minimum_x = -100,
                                  .maximum_x = 100,
                                  .audio = ab_classic_audio_rolling,
                                  .context = &resources,
                                  .storage_bytes = 65536};
    CHECK(ab_classic_post_physics_open(level, config, post_output, NULL));
    AbClassicPostView post = ab_classic_post_physics_view(*post_output);
    AbClassicLevelView loaded = ab_classic_level_view(level);
    for (size_t ordered = 0; ordered < post.count; ++ordered) {
        size_t index = post.source_indices[ordered];
        AbBodyMotion *body =
            &loaded.world->storage.bodies[loaded.frame->objects[index].body].motion;
        body->awake = ordered < 3;
        body->mass.mass = ordered == 0 ? 0.25f : ordered == 1 ? 0.5f : 0.75f;
        body->mass.angular_velocity = 400;
    }
    CHECK(ab_classic_post_physics_update(*post_output, true, NULL));
    AbAudioMixerView mixed = ab_audio_mixer_view(*mixer_output);
    CHECK(mixed.pending_count == 2 && mixed.pending[0].clip_id == 0 &&
          mixed.pending[1].clip_id == 1 && mixed.pending[0].gain == 0.25f &&
          mixed.pending[1].gain == 0.75f && mixed.pending[0].track == 2 &&
          mixed.pending[1].track == 2 &&
          mixed.pending[0].flags == AB_AUDIO_VOICE_LOOP &&
          mixed.pending[1].flags == AB_AUDIO_VOICE_LOOP &&
          ab_audio_mixer_mix(*mixer_output, 0, NULL, 0, NULL));
    for (size_t index = 0; index < post.count; ++index) {
        loaded.world->storage.bodies[loaded.frame->objects[index].body]
            .motion.mass.mass = 0.125f;
    }
    CHECK(ab_classic_post_physics_update(*post_output, true, NULL));
    mixed = ab_audio_mixer_view(*mixer_output);
    CHECK(mixed.active_count == 2 && !mixed.pending_count &&
          mixed.active[0].gain == 0.125f && mixed.active[1].gain == 0.125f);
    for (size_t index = 0; index < post.count; ++index) {
        AbBodyMotion *body =
            &loaded.world->storage.bodies[loaded.frame->objects[index].body].motion;
        body->mass.angular_velocity = 0;
        body->mass.mass = loaded.bodies->objects[index].initial_mass;
    }
    CHECK(ab_classic_post_physics_update(*post_output, true, NULL) &&
          !ab_audio_mixer_clip_is_playing(*mixer_output, 0) &&
          !ab_audio_mixer_clip_is_playing(*mixer_output, 1) &&
          ab_audio_mixer_mix(*mixer_output, 0, NULL, 0, NULL));
    ab_classic_post_physics_close(*post_output);
    *post_output = NULL;
    ab_audio_mixer_close(*mixer_output);
    *mixer_output = NULL;
    return true;
}

static bool rolling_checks(AbClassicLevel **level_output,
                           AbClassicPostPhysics **post_output,
                           AbClassicSimulation **simulation_output,
                           AbAudioMixer **mixer_output) {
    AbDataEntry properties[] = {
        {string("type"), string("circle")},
        {string("radius"), number(1)},
        {string("density"), number(1)},
        {string("friction"), number(0.3f)},
        {string("restitution"), number(0)},
        {string("collider"), number(0)},
        {string("materialName"), string("Alpha")},
        {string("rollingSound"), string("definition_override")},
    };
    AbDataEntry zeta_properties[8];
    memcpy(zeta_properties, properties, sizeof(properties));
    zeta_properties[6].value = string("Zeta");
    AbDataTable alpha = {properties, 8, 8, NULL};
    AbDataTable zeta = {zeta_properties, 8, 8, NULL};
    AbDataTable empty = {0};
    AbDataEntry alpha_sound = {string("rollingSound"), string("alpha")};
    AbDataEntry zeta_sound = {string("rollingSound"), string("zeta")};
    AbDataTable alpha_material = {&alpha_sound, 1, 1, NULL};
    AbDataTable zeta_material = {&zeta_sound, 1, 1, NULL};
    AbDataEntry material_entries[] = {{string("Alpha"), table(&alpha_material)},
                                      {string("Zeta"), table(&zeta_material)}};
    AbDataTable materials = {material_entries, 2, 2, NULL};
    AbDataEntry block_entries[] = {{string("AlphaCircle"), table(&alpha)},
                                   {string("ZetaCircle"), table(&zeta)},
                                   {string("Ground"), table(&empty)}};
    AbDataTable blocks = {block_entries, 3, 3, NULL};
    AbDataEntry catalog_entries[] = {{string("blocks"), table(&blocks)},
                                     {string("materials"), table(&materials)},
                                     {string("collider_types"), table(&empty)}};
    AbDataTable catalog = {catalog_entries, 3, 3, NULL};
    const char *names[] = {"Z_first", "A_middle", "Z_last"};
    AbDataEntry placements[3][5], world_entries[3];
    AbDataTable placement_tables[3];
    for (size_t index = 0; index < 3; ++index) {
        placements[index][0] = (AbDataEntry){string("name"), string(names[index])};
        placements[index][1] = (AbDataEntry){
            string("definition"), string(index == 1 ? "AlphaCircle" : "ZetaCircle")};
        placements[index][2] = (AbDataEntry){string("x"), number((float)index * 4)};
        placements[index][3] = (AbDataEntry){string("y"), number(0)};
        placements[index][4] = (AbDataEntry){string("angle"), number(0)};
        placement_tables[index] = (AbDataTable){placements[index], 5, 5, NULL};
        world_entries[index] =
            (AbDataEntry){string(names[index]), table(placement_tables + index)};
    }
    AbDataTable world_table = {world_entries, 3, 3, NULL};
    AbDataEntry globals_entries[] = {{string("world"), table(&world_table)},
                                     {string("joints"), table(&empty)}};
    AbDataTable globals = {globals_entries, 2, 2, NULL};
    AbDataTableLayout layout = {
        .table = &world_table, .hash_capacity = 4, .no_nil_assignments = true};
    AbLuaData data = {.globals = &globals, .layouts = &layout};
    AbLevelBodySource source = {.level = &data,
                                .catalog = &catalog,
                                .placement_scale = 1,
                                .physics_scale = 1,
                                .ground_position = {0, 10}};
    AbWorldDefinition physics = {.broadphase_limits = {64, 128, 128},
                                 .maximum_translation = 2,
                                 .maximum_translation_squared = 4};
    CHECK(ab_classic_level_open(
        &source, physics,
        (AbClassicLevelLimits){
            .objects = 4, .contacts = 16, .storage_bytes = 65536, .joints = 4},
        false, level_output, NULL));
    AbClassicLevel *level = *level_output;
    AbClassicLevelView loaded = ab_classic_level_view(level);
    AudioTest test = {0};
    AbClassicPostConfig config = {.minimum_x = -100,
                                  .maximum_x = 100,
                                  .audio = audio_host,
                                  .context = &test,
                                  .storage_bytes = 65536};
    CHECK(ab_classic_post_physics_open(level, config, post_output, NULL));
    AbClassicPostPhysics *post = *post_output;
    AbClassicPostView view = ab_classic_post_physics_view(post);
    CHECK(view.count == 4 && !test.count);
    for (size_t ordered = 0; ordered < view.count; ++ordered) {
        size_t index = view.source_indices[ordered];
        const AbLevelObject *object = loaded.bodies->objects + index;
        AbBodyMotion *body = &loaded.world->storage.bodies[object->physics.body].motion;
        body->awake = ordered < 3;
        body->mass.mass = ordered == 0 ? 0.25f : ordered == 1 ? 0.5f : 0.375f;
        body->mass.angular_velocity = ordered == 2 ? 800 : 400;
    }
    CHECK(ab_classic_post_physics_update(post, true, NULL) && test.count == 4 &&
          audio_is(&test, 0, AB_ROLLING_AUDIO_QUERY, "alpha", 0) &&
          audio_is(&test, 1, AB_ROLLING_AUDIO_PLAY, "alpha", 0.25f) &&
          audio_is(&test, 2, AB_ROLLING_AUDIO_QUERY, "zeta", 0) &&
          audio_is(&test, 3, AB_ROLLING_AUDIO_PLAY, "zeta", 0.75f));
    test.count = 0;
    test.playing = true;
    size_t first = view.source_indices[0];
    loaded.world->storage.bodies[loaded.frame->objects[first].body]
        .motion.mass.angular_velocity = 0;
    for (size_t ordered = 1; ordered < 3; ++ordered) {
        size_t index = view.source_indices[ordered];
        AbBodyMotion *body =
            &loaded.world->storage.bodies[loaded.frame->objects[index].body].motion;
        body->mass.mass = ordered == 1 ? 0.375f : 0.5f;
        body->mass.angular_velocity = ordered == 1 ? 800 : 400;
    }
    CHECK(ab_classic_post_physics_update(post, true, NULL) && test.count == 3 &&
          audio_is(&test, 0, AB_ROLLING_AUDIO_STOP, "alpha", 0) &&
          audio_is(&test, 1, AB_ROLLING_AUDIO_QUERY, "zeta", 0) &&
          audio_is(&test, 2, AB_ROLLING_AUDIO_GAIN, "zeta", 0.75f));
    test.count = 0;
    for (size_t index = 0; index < view.count; ++index) {
        AbBodyMotion *body =
            &loaded.world->storage.bodies[loaded.frame->objects[index].body].motion;
        body->awake = false;
        body->mass.angular_velocity = 0;
    }
    CHECK(ab_classic_post_physics_update(post, true, NULL) && test.count == 2 &&
          audio_is(&test, 0, AB_ROLLING_AUDIO_STOP, "alpha", 0) &&
          audio_is(&test, 1, AB_ROLLING_AUDIO_STOP, "zeta", 0));
    test.count = 0;
    CHECK(ab_classic_post_physics_update(post, true, NULL) && !test.count &&
          !view.activity->has_awake && !view.activity->moving);
    AbBodyMotion *body =
        &loaded.world->storage.bodies[loaded.frame->objects[first].body].motion;
    body->awake = true;
    test.fail = true;
    AbError error = {0};
    CHECK(!ab_classic_post_physics_update(post, true, &error) && test.count == 1 &&
          strstr(error.message, "Requested rolling host failure") &&
          loaded.motion_properties[first].previous_awake && view.activity->has_awake);
    ab_classic_post_physics_close(post);
    *post_output = NULL;
    config.audio = NULL;
    CHECK(ab_classic_post_physics_open(level, config, post_output, NULL) &&
          !ab_classic_post_physics_update(*post_output, true, &error) &&
          strstr(error.message, "rolling-audio resource host"));
    ab_classic_post_physics_close(*post_output);
    *post_output = NULL;
    CHECK(rolling_resource_checks(level, mixer_output, post_output));
    CHECK(simulation_checks(level, simulation_output));
    CHECK(simulation_removal_checks(level, simulation_output));
    ab_classic_level_close(level);
    *level_output = NULL;
    CHECK(ab_classic_level_open(
        &source, physics,
        (AbClassicLevelLimits){
            .objects = 8, .contacts = 16, .storage_bytes = 256 * 1024, .joints = 4},
        false, level_output, NULL));
    CHECK(simulation_dynamic_checks(*level_output, simulation_output));
    ab_classic_level_close(*level_output);
    *level_output = NULL;
    return true;
}

int main(void) {
    AbClassicLevel *level = NULL;
    AbClassicPostPhysics *post = NULL;
    AbClassicSimulation *simulation = NULL;
    AbAudioMixer *mixer = NULL;
    bool okay = rolling_checks(&level, &post, &simulation, &mixer);
    ab_classic_simulation_close(simulation);
    ab_classic_post_physics_close(post);
    ab_audio_mixer_close(mixer);
    ab_classic_level_close(level);
    return okay ? 0 : 1;
}
