#include "ab/classic_controller.h"
#include "ab/classic_game_input.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

struct AbClassicController {
    AbClassicControllerConfig config;
    AbClassicLevelView level;
    AbGameTailObject *objects;
    AbClassicWorldCursor cursor;
    AbCameraSystemInput camera;
    size_t count, capacity, bytes;
    bool updating, failed;
};

static AbSpan span(const char *name) {
    return (AbSpan){(const uint8_t *)name, strlen(name)};
}

static AbDataValue field(const AbDataTable *table, const char *name) {
    const AbDataValue *found =
        ab_data_get(table, (AbDataValue){.kind = AB_DATA_STRING, .string = span(name)});
    return found ? *found : (AbDataValue){0};
}

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL && (value.kind != AB_DATA_BOOLEAN || value.bits);
}

static bool get(AbClassicController *owner, size_t object, const char *key,
                AbDataValue *output, AbError *error) {
    return ab_level_property_get(owner->level.properties, object, span(key), output,
                                 error);
}

static bool live(AbClassicController *owner, size_t object, bool *output,
                 AbError *error) {
    AbDataValue name = {.kind = AB_DATA_STRING,
                        .string = owner->level.bodies->objects[object].name};
    AbDataValue value;
    if (!ab_object_world_lookup(&owner->level.properties->lookup, name, &value, error))
        return false;
    if (value.kind != AB_DATA_NIL && value.kind != AB_DATA_TABLE)
        return ab_fail(error, "Unsupported controller world value");
    *output = value.kind != AB_DATA_NIL;
    if (*output &&
        (owner->level.removed_objects[object] || value.kind != AB_DATA_TABLE ||
         value.table != &owner->level.properties->objects[object].physical))
        return ab_fail(error, "Rebound controller world object");
    return true;
}

static bool external(AbClassicController *owner, AbGameTailCall *call, AbError *error) {
    return owner->config.service
               ? owner->config.service(owner->config.context, call, error)
               : ab_fail(error, "Missing classic controller service %u",
                         (unsigned)call->operation);
}

static bool update_effects(AbClassicControllerEffects *effects, float seconds,
                           float time, AbError *error) {
    if (!effects || effects->capacity > UINT32_C(16777216) ||
        (effects->capacity && !effects->items))
        return ab_fail(error, "Invalid current level-effects registry");
    for (size_t index = 0; index < effects->capacity; ++index) {
        AbClassicControllerEffect *effect = effects->items[index];
        if (!effect)
            break;
        if (!effect->update)
            return ab_fail(error, "Missing current level-effect update method");
        if (!effect->update(effect->context, seconds, time, error))
            return false;
    }
    return true;
}

static bool boundaries(AbClassicController *owner, size_t object, float seconds,
                       bool *alive, AbError *error) {
    AbDataValue outside, controllable, flag;
    if (!get(owner, object, "outsideBoundaries", &outside, error))
        return false;
    *alive = true;
    if (!truthy(outside))
        return true;
    AbSpan name = owner->level.bodies->objects[object].name;
    if (ab_classic_sling_view(owner->config.sling).current_bird_index == 0) {
        if (!owner->config.boundary_warning)
            return ab_fail(error, "Missing initial outside-boundary warning service");
        if (!owner->config.boundary_warning(owner->config.context, name, error))
            return false;
    }
    if (!get(owner, object, "controllable", &controllable, error))
        return false;
    if (truthy(controllable)) {
        if (!get(owner, object, "isMightyBait", &flag, error))
            return false;
        bool mighty = truthy(flag);
        if (!mighty) {
            if (!get(owner, object, "isMightyEagle", &flag, error))
                return false;
            mighty = truthy(flag);
        }
        if (mighty) {
            if (!owner->config.mighty_exit)
                return ab_fail(error, "Missing mighty bird boundary exit service");
            if (!owner->config.mighty_exit(owner->config.context, object, seconds,
                                           error))
                return false;
        }
    } else {
        if (!get(owner, object, "levelGoal", &flag, error))
            return false;
        if (truthy(flag) &&
            !ab_classic_removals_remove_pig(owner->config.removals, name, error))
            return false;
    }
    *alive = false;
    return true;
}

static bool boomerangs(AbClassicController *owner, AbGameTailCall *call,
                       AbError *error) {
    if (owner->config.boomerang)
        return ab_classic_boomerang_update(owner->config.boomerang, call->seconds,
                                           error);
    for (size_t order = 0; order < owner->level.bird_count; ++order) {
        size_t object = owner->level.bird_order[order];
        bool present;
        if (!live(owner, object, &present, error))
            return false;
        if (!present)
            continue;
        AbDataValue definition, shot;
        AbDataTable *physical = &owner->level.properties->objects[object].physical;
        AbDataValue name = field(physical, "name");
        if (name.kind != AB_DATA_STRING ||
            !ab_object_definition_lookup(&owner->level.properties->lookup, name.string,
                                         &definition, error) ||
            definition.kind != AB_DATA_TABLE ||
            !get(owner, object, "shot", &shot, error))
            return ab_fail(error, "Unsupported controller boomerang object");
        AbDataValue specialty = field(definition.table, "specialty");
        if (shot.kind == AB_DATA_BOOLEAN && shot.bits &&
            specialty.kind == AB_DATA_STRING && specialty.string.size == 9 &&
            !memcmp(specialty.string.data, "BOOMERANG", 9))
            return external(owner, call, error);
    }
    return true;
}

static bool update_script(AbClassicController *owner, size_t object, float seconds,
                          float time, AbError *error) {
    AbDataValue script;
    if (!get(owner, object, "gameObjectScript", &script, error))
        return false;
    if (!truthy(script))
        return true;
    if (script.kind != AB_DATA_TABLE ||
        script.table != &owner->level.properties->objects[object].script)
        return ab_fail(error, "Rebound controller GameObject script");
    const AbStringMap *groups =
        ab_classic_behaviors_groups(owner->config.behaviors, object);
    uint32_t seconds_bits, time_bits;
    memcpy(&seconds_bits, &seconds, sizeof(seconds_bits));
    memcpy(&time_bits, &time, sizeof(time_bits));
    AbUiPointerValue arguments[] = {
        {.kind = AB_UI_POINTER_NUMBER, .number_bits = seconds_bits},
        {.kind = AB_UI_POINTER_NUMBER, .number_bits = time_bits}};
    return groups ? ab_game_object_delegate(groups, span("update"), arguments, 2, error)
                  : ab_fail(error, "Missing controller GameObject behavior map");
}

static bool object_call(AbClassicController *owner, AbGameTailCall *call,
                        AbError *error) {
    size_t order = 0;
    while (order < owner->count && call->object != owner->objects + order)
        ++order;
    if (order == owner->count)
        return ab_fail(error, "Foreign controller traversal object");
    size_t object = owner->objects[order].identity;
    if (object != order)
        return ab_fail(error, "Rebound controller object identity");
    if (call->operation == AB_GAME_TAIL_HOOK_EXISTS)
        return live(owner, object, &call->result, error);
    /* pairs captured this canonical value before any phase callbacks. A nil
     * assignment to world[name] does not replace that visit's local value. */
    AbDataValue value, finished, disabled;
    switch (call->operation) {
        case AB_GAME_TAIL_ANIMATION:
            if (!get(owner, object, "animation", &value, error))
                return false;
            if (!truthy(value))
                return true;
            if (!get(owner, object, "animationFinished", &finished, error))
                return false;
            if (truthy(finished))
                return true;
            if (!get(owner, object, "animationDisabled", &disabled, error))
                return false;
            return truthy(disabled) || external(owner, call, error);
        case AB_GAME_TAIL_ROCKET:
            return get(owner, object, "rocket", &value, error) &&
                   (!truthy(value) || external(owner, call, error));
        case AB_GAME_TAIL_BIRD:
            return ab_classic_birds_update(owner->config.birds, object, call->seconds,
                                           &call->result, error);
        case AB_GAME_TAIL_BOUNDARIES:
            return boundaries(owner, object, call->seconds, &call->result, error);
        case AB_GAME_TAIL_OBJECT_EFFECT:
            return owner->config.effect_kind == AB_CLASSIC_CONTROLLER_EFFECT_BASE ||
                   external(owner, call, error);
        case AB_GAME_TAIL_OBJECT_SCRIPT:
            return update_script(owner, object, call->seconds, call->time, error);
        default:
            return ab_fail(error, "Unexpected controller object operation");
    }
}

static bool host_operation(void *context, AbGameTailCall *call, AbError *error) {
    AbClassicController *owner = context;
    if (call->object)
        return object_call(owner, call, error);
    AbClassicControllerConfig *config = &owner->config;
    AbShotState *shot = ab_classic_shots_state(config->shots);
    switch (call->operation) {
        case AB_GAME_TAIL_SLING: {
            bool okay = ab_classic_sling_update(config->sling, call->seconds, error);
            config->state->bird_fired = shot->bird_fired;
            return okay;
        }
        case AB_GAME_TAIL_WOBBLE:
            return ab_classic_sling_wobble(config->sling, call->seconds,
                                           config->wobble_steps, error);
        case AB_GAME_TAIL_GRENADES:
            return ab_classic_eggs_update(config->eggs, error);
        case AB_GAME_TAIL_CAMERA:
            owner->camera.specialty_available = shot->specialty_available;
            return ab_classic_camera_update(config->camera, call->seconds,
                                            &owner->camera, error);
        case AB_GAME_TAIL_BOOMERANG:
            return boomerangs(owner, call, error);
        case AB_GAME_TAIL_CHARACTER_ANIMATIONS: {
            AbClassicCharacterView characters =
                ab_classic_characters_view(config->characters);
            characters.animations->current_bird = SIZE_MAX;
            for (size_t index = 0; shot->current_bird_name && index < owner->count;
                 ++index) {
                AbSpan name = owner->level.bodies->objects[index].name;
                size_t length = strlen(shot->current_bird_name);
                if (name.size == length &&
                    !memcmp(name.data, shot->current_bird_name, length)) {
                    characters.animations->current_bird = index;
                    break;
                }
            }
            characters.animations->level_completed = *config->level_completed;
            return ab_classic_characters_update(config->characters, call->seconds,
                                                error);
        }
        case AB_GAME_TAIL_FLOATING_SCORES:
            return ab_floating_scores_update(config->floating, call->seconds, error);
        case AB_GAME_TAIL_SCORE: {
            bool okay = ab_classic_scores_update(config->scores, config->score, error);
            shot->score = *config->score;
            AbClassicGameGlobals *globals = ab_classic_shots_globals(config->shots);
            return (!globals || ab_classic_game_globals_publish_score(
                                    globals, *config->score, error)) &&
                   okay;
        }
        case AB_GAME_TAIL_PHYSICS_ENABLED:
            call->result = *config->physics_enabled;
            return true;
        case AB_GAME_TAIL_LEVEL_EFFECTS:
            return update_effects(config->level_effects, call->seconds, call->time,
                                  error);
        case AB_GAME_TAIL_BLOCK_SCRIPTS:
            return ab_block_scripts_update(config->blocks, config->block_update,
                                           call->seconds, call->time, error);
        case AB_GAME_TAIL_GFX_EFFECTS:
            return (!config->input ||
                    ab_classic_game_input_finish_cursor(config->input, error)) &&
                   ab_classic_render_scene_update_gfx(config->scene, call->seconds,
                                                      error);
        case AB_GAME_TAIL_DRAW:
            if (!external(owner, call, error))
                return false;
            if (config->input)
                ab_classic_game_input_finish_draw(config->input);
            return true;
        default:
            return external(owner, call, error);
    }
}

static bool read_darkness(AbClassicController *owner, AbError *error) {
    AbClassicGameGlobals *globals = ab_classic_shots_globals(owner->config.shots);
    if (!globals)
        return true;
    AbDataValue value = globals->eagle_darkness;
    owner->config.state->darkness_present = value.kind != AB_DATA_NIL;
    if (value.kind == AB_DATA_NIL)
        return true;
    float darkness;
    memcpy(&darkness, &value.bits, sizeof(darkness));
    if (value.kind != AB_DATA_NUMBER || !isfinite(darkness))
        return ab_fail(error, "Unsupported canonical eagle darkness");
    owner->config.state->eagle_darkness = darkness;
    return true;
}

static void publish_darkness(AbClassicController *owner) {
    AbClassicGameGlobals *globals = ab_classic_shots_globals(owner->config.shots);
    if (!globals)
        return;
    AbDataValue value = {0};
    if (owner->config.state->darkness_present) {
        value.kind = AB_DATA_NUMBER;
        memcpy(&value.bits, &owner->config.state->eagle_darkness, sizeof(value.bits));
    }
    globals->eagle_darkness = value;
}

static bool host(void *context, AbGameTailCall *call, AbError *error) {
    AbClassicController *owner = context;
    if (!ab_classic_shots_refresh_globals(owner->config.shots, error))
        return false;
    /* The tail subtracts/clamps before its first subsequent callback. Publish
     * at every source service boundary; callbacks can replace or clear the same
     * canonical global before later object/draw phases. */
    publish_darkness(owner);
    bool okay = host_operation(context, call, error);
    AbError ignored = {0};
    bool refreshed = ab_classic_shots_refresh_globals(owner->config.shots,
                                                      okay ? error : &ignored) &&
                     read_darkness(owner, okay ? error : &ignored);
    return okay && refreshed;
}

bool ab_classic_controller_open(AbClassicControllerConfig config,
                                AbClassicController **output, AbError *error) {
    if (!output)
        return ab_fail(error, "Missing classic controller output");
    *output = NULL;
    AbClassicLevelView level = ab_classic_level_view(config.level);
    AbClassicCharacterView characters = ab_classic_characters_view(config.characters);
    size_t count = level.bodies ? level.bodies->count : 0;
    size_t capacity = config.object_capacity ? config.object_capacity : count;
    if (!level.bodies || !level.properties || !level.object_order ||
        !level.removed_objects || !config.shots || !config.sling || !config.birds ||
        !config.behaviors || !config.camera || !characters.animations ||
        characters.count != level.bodies->count || !config.removals ||
        !config.floating || !config.scores || !config.blocks || !config.block_update ||
        ab_classic_render_scene_level(config.scene) != config.level ||
        !config.level_effects || !config.state || !config.score ||
        !config.physics_enabled || !config.level_completed ||
        ab_classic_shots_level(config.shots) != config.level ||
        config.effect_kind < AB_CLASSIC_CONTROLLER_EFFECT_BASE ||
        config.effect_kind > AB_CLASSIC_CONTROLLER_EFFECT_CUSTOM || capacity < count ||
        capacity > level.bodies->capacity || capacity > UINT32_C(16777216) ||
        config.storage_bytes < sizeof(AbClassicController) ||
        capacity > (config.storage_bytes - sizeof(AbClassicController)) /
                       sizeof(AbGameTailObject))
        return ab_fail(error, "Invalid classic controller bindings");
    if (config.input &&
        (!config.input->initialized || config.input->config.shots != config.shots ||
         config.input->config.sling != config.sling ||
         config.input->config.tail != config.state ||
         config.input->config.level_completed != config.level_completed))
        return ab_fail(error, "Controller and input do not share canonical bindings");
    AbClassicController *owner = calloc(1, sizeof(*owner));
    if (!owner)
        return ab_fail(error, "Cannot allocate classic controller");
    owner->config = config;
    owner->level = level;
    owner->count = count;
    owner->capacity = capacity;
    owner->bytes = sizeof(*owner) + capacity * sizeof(*owner->objects);
    owner->objects = calloc(capacity, sizeof(*owner->objects));
    if (capacity && !owner->objects) {
        ab_classic_controller_close(owner);
        return ab_fail(error, "Cannot allocate classic controller traversal");
    }
    for (size_t object = 0; object < count; ++object) {
        /* Always query script membership at the later source boundary. */
        owner->objects[object] = (AbGameTailObject){object, true};
    }
    *output = owner;
    return true;
}

void ab_classic_controller_close(AbClassicController *owner) {
    if (owner) {
        free(owner->objects);
        free(owner);
    }
}

size_t ab_classic_controller_bytes(const AbClassicController *owner) {
    return owner ? owner->bytes : 0;
}

AbGameTailObject *ab_classic_controller_object(AbClassicController *owner,
                                               size_t object) {
    return owner && object < owner->count ? owner->objects + object : NULL;
}

bool ab_classic_controller_register_object(AbClassicController *owner, size_t object,
                                           AbError *error) {
    if (!owner || owner->failed || object != owner->count ||
        object >= owner->capacity || object >= owner->level.bodies->count ||
        object >= owner->level.properties->count ||
        owner->level.removed_objects[object] || owner->level.world->failed ||
        owner->level.world->locked)
        return ab_fail(error, "Invalid new controller object binding");
    owner->objects[object] = (AbGameTailObject){object, true};
    ++owner->count;
    return true;
}

static bool world_begin(void *context, AbError *error) {
    (void)error;
    ((AbClassicController *)context)->cursor = (AbClassicWorldCursor){0};
    return true;
}

static bool world_next(void *context, AbGameTailObject **output, AbError *error) {
    AbClassicController *owner = context;
    size_t object = SIZE_MAX;
    bool present = false;
    if (!ab_classic_level_world_next(owner->config.level, &owner->cursor, &object,
                                     &present, error))
        return false;
    if (present && object >= owner->count)
        return ab_fail(error, "Unregistered current controller object");
    *output = present ? owner->objects + object : NULL;
    return true;
}

AbClassicGameInput *ab_classic_controller_input(const AbClassicController *owner) {
    return owner ? owner->config.input : NULL;
}

bool ab_classic_controller_update(AbClassicController *owner, float seconds, float time,
                                  const AbCameraSystemInput *camera, AbError *error) {
    if (!owner || owner->updating || owner->failed || !camera || !isfinite(seconds) ||
        !isfinite(time) || owner->level.world->failed || owner->level.world->locked)
        return ab_fail(error, "Invalid, failed or reentrant classic controller");
    if (!ab_classic_shots_refresh_globals(owner->config.shots, error) ||
        !read_darkness(owner, error))
        return false;
    owner->updating = true;
    owner->camera = *camera;
    AbShotState *shot = ab_classic_shots_state(owner->config.shots);
    if (!ab_classic_shots_globals(owner->config.shots))
        shot->time = time;
    owner->config.state->bird_fired = shot->bird_fired;
    bool okay = ab_game_update_tail_world(
        owner->config.state, (AbGameTailWorld){owner, world_begin, world_next}, seconds,
        time, host, owner, error);
    owner->failed = !okay;
    owner->updating = false;
    return okay;
}
