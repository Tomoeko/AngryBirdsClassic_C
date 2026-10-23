#include "ab/classic_playfield.h"

#include <stdlib.h>

struct AbClassicPlayfield {
    AbClassicPlayfieldConfig config;
    AbClassicPlayfieldView view;
    AbClassicLaunchHost launch;
    AbBluesTrajectoryHost blues;
    AbCameraSystem *camera_system;
    size_t bytes;
};

static bool museum_service(void *context, const AbMuseumBirdCall *call,
                           AbError *error) {
    AbClassicPlayfield *owner = context;
    if (call->operation == AB_MUSEUM_CAMERA_CANCEL)
        return ab_camera_system_cancel_sequence(owner->camera_system, error);
    if (call->operation == AB_MUSEUM_CAMERA_SEQUENCE)
        return ab_classic_camera_start_sequence(owner->view.camera, call->sequence,
                                                error);
    return ab_classic_trajectories_museum(owner->view.trajectories, call, error);
}

static bool budget(AbClassicPlayfieldLimits limits, size_t *output, AbError *error) {
    const size_t parts[] = {sizeof(AbClassicPlayfield),
                            limits.visuals,
                            limits.scene,
                            limits.trajectories,
                            limits.camera,
                            limits.behaviors,
                            limits.shots,
                            limits.services,
                            limits.sling};
    size_t bytes = 0;
    for (size_t index = 0; index < sizeof(parts) / sizeof(*parts); ++index) {
        if (!parts[index] || bytes > limits.storage_bytes ||
            parts[index] > limits.storage_bytes - bytes)
            return ab_fail(error, "Invalid or exhausted playfield byte budget");
        bytes += parts[index];
    }
    *output = bytes;
    return true;
}

static bool open_scene(AbClassicPlayfield *owner, AbError *error) {
    AbClassicPlayfieldConfig *config = &owner->config;
    AbClassicPlayfieldView *view = &owner->view;
    AbClassicPlayfieldLimits limits = config->limits;
    if (!ab_classic_visuals_open(
            config->level,
            (AbClassicVisualResources){.provider = config->sprites,
                                       .context = config->context,
                                       .missing = config->missing_visual,
                                       .storage_bytes = limits.visuals,
                                       .name_capacity = limits.name_capacity},
            &view->visuals, error) ||
        !ab_classic_render_scene_open(
            config->level,
            (AbClassicRenderSceneConfig){
                .provider = config->sprites,
                .id_counter = config->renderer_counter,
                .extra_renderers = limits.extra_renderers,
                .storage_bytes = limits.scene,
                .object_blend = config->object_blend,
                .terrain = config->terrain ? *config->terrain : (AbMaskTexture){0}},
            &view->scene, error) ||
        !ab_classic_scene_sync_visuals(ab_classic_render_scene_objects(view->scene),
                                       view->visuals, error))
        return false;
    return ab_classic_trajectories_open(
        (AbClassicTrajectoryConfig){.level = config->level,
                                    .scene = view->scene,
                                    .tracks = limits.tracks,
                                    .points = limits.points,
                                    .storage_bytes = limits.trajectories},
        &view->trajectories, error);
}

static bool open_behaviors(AbClassicPlayfield *owner, AbError *error) {
    AbClassicPlayfieldConfig *config = &owner->config;
    AbClassicCameraConfig camera = config->camera;
    camera.level = config->level;
    camera.storage_bytes = config->limits.camera;
    if (!ab_classic_camera_open(&camera, &owner->view.camera, error))
        return false;
    owner->camera_system = ab_classic_camera_system(owner->view.camera);
    owner->blues = (AbBluesTrajectoryHost){.context = owner->view.trajectories,
                                           .call = ab_classic_trajectories_blues,
                                           .live_flying_bird = config->flying_object,
                                           .live_other_birds = config->other_birds};
    return ab_classic_behaviors_open(
        config->level,
        (AbClassicBehaviorConfig){.flying_bird = config->flying_object,
                                  .context = owner,
                                  .call = museum_service,
                                  .blues = &owner->blues,
                                  .object_capacity = config->limits.behavior_objects,
                                  .storage_bytes = config->limits.behaviors},
        &owner->view.behaviors, error);
}

static bool open_launch(AbClassicPlayfield *owner, AbError *error) {
    AbClassicPlayfieldConfig *config = &owner->config;
    AbClassicPlayfieldView *view = &owner->view;
    AbClassicPlayfieldStart start = config->start;
    if (!ab_classic_launch_host_initialize(
            (AbClassicLaunchHostConfig){.level = config->level,
                                        .shots = &view->shots,
                                        .sling = &view->sling,
                                        .audio = config->audio,
                                        .camera = &owner->camera_system,
                                        .next_bird_sound = config->next_bird_sound,
                                        .powerups = config->powerups,
                                        .powerups_enabled = config->powerups_enabled,
                                        .red_special_available =
                                            config->red_special_available,
                                        .context = config->context,
                                        .level_hook = config->level_hook,
                                        .shot_trails = config->shot_trails,
                                        .definition_hook = config->definition_hook},
            &owner->launch, error) ||
        !ab_shot_services_open(
            (AbShotServicesConfig){.statistics = config->statistics,
                                   .events = config->events,
                                   .remaining_shot = ab_classic_launch_host_remaining,
                                   .context = &owner->launch,
                                   .notifications = config->limits.notifications,
                                   .storage_bytes = config->limits.services},
            &view->services, error) ||
        !ab_classic_shots_open(
            (AbClassicShotConfig){.level = config->level,
                                  .visuals = view->visuals,
                                  .behaviors = view->behaviors,
                                  .camera = view->camera,
                                  .flying_object = config->flying_object,
                                  .globals = config->globals,
                                  .state = config->shot_state,
                                  .service = ab_classic_launch_host_dispatch,
                                  .context = &owner->launch,
                                  .object_capacity = config->limits.shot_objects,
                                  .storage_bytes = config->limits.shots},
            &view->shots, error))
        return false;
    return ab_classic_sling_open(
        (AbClassicSlingConfig){
            .level = config->level,
            .shots = view->shots,
            .behaviors = view->behaviors,
            .services = view->services,
            .events = config->events,
            .audio = config->audio,
            .camera = owner->camera_system,
            .start = start.pose,
            .physics_scale = start.physics_scale,
            .default_force = start.default_force,
            .current_bird_index = start.current_bird_index,
            .fill_in_next_bird = start.fill_in_next_bird,
            .allow_reset_to_bird_camera = start.allow_reset_to_bird_camera,
            .disabled_for_current_level = start.disabled_for_current_level,
            .context = &owner->launch,
            .camera_distances = ab_classic_launch_host_camera_distances,
            .reload_sound = ab_classic_launch_host_reload_audio,
            .storage_bytes = config->limits.sling},
        &view->sling, error);
}

bool ab_classic_playfield_open(AbClassicPlayfieldConfig config,
                               AbClassicPlayfield **output, AbError *error) {
    if (!output)
        return ab_fail(error, "Missing playfield output");
    *output = NULL;
    size_t bytes = 0;
    if (!config.level || !ab_classic_level_view(config.level).properties ||
        !config.sprites || !config.audio || !config.statistics || !config.events ||
        !config.globals || !config.flying_object || !config.powerups_enabled ||
        !config.red_special_available || !config.next_bird_sound ||
        !config.renderer_counter || !budget(config.limits, &bytes, error))
        return ab_fail(error, "Invalid playfield owners or capacity");
    AbClassicPlayfield *owner = calloc(1, sizeof(*owner));
    if (!owner)
        return ab_fail(error, "Cannot allocate playfield");
    owner->config = config;
    owner->view.level = config.level;
    owner->bytes = bytes;
    if (!open_scene(owner, error) || !open_behaviors(owner, error) ||
        !open_launch(owner, error)) {
        ab_classic_playfield_close(owner);
        return false;
    }
    *output = owner;
    return true;
}

void ab_classic_playfield_close(AbClassicPlayfield *owner) {
    if (!owner)
        return;
    ab_classic_sling_close(owner->view.sling);
    ab_classic_shots_close(owner->view.shots);
    ab_shot_services_close(owner->view.services);
    ab_classic_behaviors_close(owner->view.behaviors);
    ab_classic_camera_close(owner->view.camera);
    ab_classic_trajectories_close(owner->view.trajectories);
    ab_classic_render_scene_close(owner->view.scene);
    ab_classic_visuals_close(owner->view.visuals);
    free(owner);
}

size_t ab_classic_playfield_bytes(const AbClassicPlayfield *owner) {
    return owner ? owner->bytes : 0;
}

AbClassicPlayfieldView ab_classic_playfield_view(const AbClassicPlayfield *owner) {
    return owner ? owner->view : (AbClassicPlayfieldView){0};
}

bool ab_classic_playfield_bind_other_birds(AbClassicPlayfield *owner,
                                           const AbDataValue *field, AbError *error) {
    if (!owner || !owner->view.behaviors)
        return ab_fail(error, "Invalid playfield other-birds binding");
    owner->config.other_birds = field;
    owner->blues.live_other_birds = field;
    return true;
}
