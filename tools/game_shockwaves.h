#ifndef AB_GAME_SHOCKWAVES_H
#define AB_GAME_SHOCKWAVES_H

#include "game_shockwave_common.h"
#include "game_shockwave_pigs.h"
#include "game_shockwave_combat.h"
#include "game_shockwave_electricity.h"

static bool game_shock_trigger(PreviewShockObject *row, AbError *error);

static bool game_shock_delayed_update(void *context, void *receiver, float seconds,
                                      AbError *error) {
    PreviewShockObject *row = context;
    AbDataTable *physical = game_shock_physical(row);
    float timer;
    if (receiver != physical ||
        !game_shock_read(physical, "doAfterDelayTimeLeft", &timer, error))
        return false;
    timer -= seconds;
    if (!game_shock_number(row, "doAfterDelayTimeLeft", timer, error))
        return false;
    if (timer < 1) {
        AbDataValue definition;
        if (!game_shock_definition(row, &definition, error))
            return false;
        AbDataValue sprite = menu_route_field(
            definition.table, timer < .5f ? "spriteSpecial3" : "spriteSpecial2");
        if (sprite.kind != AB_DATA_STRING ||
            !game_shock_write(row, "damageSprite", sprite, error) ||
            !ab_classic_visuals_set_sprite(row->owner->game->playfield.visuals,
                                           row->object, sprite.string, error))
            return ab_fail(error, "Missing actual delayed shockwave fuse sprite");
    }
    if (timer > 0)
        return true;
    /* The source delayed closure calls the named hook directly. Its common
     * manual trigger flags/events are not inserted into this collision route. */
    return game_shock_trigger(row, error) &&
           game_shock_remove_update(row, &row->delayed, error) &&
           game_shock_write(row, "doAfterDelayTimeLeft", (AbDataValue){0}, error) &&
           game_shock_write(row, "doAfterDelayCallback", (AbDataValue){0}, error);
}

static bool game_shock_collision(void *context, const size_t *flying,
                                 const AbBirdCollisionNamedInput *contact,
                                 AbError *error) {
    (void)contact;
    PreviewGame *game = context;
    PreviewShockwaves *owner = game->shockwaves;
    if (!owner || !flying || *flying >= owner->capacity)
        return ab_fail(error, "Missing actual flying shockwave bird");
    PreviewShockObject *row = owner->objects + *flying;
    AbDataTable *physical = game_shock_physical(row);
    if (menu_route_truthy(menu_route_field(physical, "specialtyTriggered")))
        return true;
    AbDataValue definition;
    float timer;
    if (!game_shock_definition(row, &definition, error) ||
        !menu_route_equal(menu_route_field(definition.table, "onTriggerSpecialty"),
                          game_text("triggerBlackBirdSpecialtyShockwave")) ||
        !game_shock_optional(definition.table, "explosionTimer", 1.5f, &timer, error))
        return ab_fail(error, "Unsupported delayed specialty definition");
    return game_shock_number(row, "doAfterDelayTimeLeft", timer, error) &&
           game_shock_write(
               row, "doAfterDelayCallback",
               (AbDataValue){.kind = AB_DATA_OBJECT, .object = &row->delayed}, error) &&
           game_shock_add_update(row, &row->delayed, error);
}

static bool game_shock_draw(void *context, void *parameter, AbExtraDrawInput input,
                            AbError *error) {
    PreviewShockwaves *owner = context;
    PreviewShockObject *row = parameter;
    AbElectricSpriteDraw draws[11];
    size_t count;
    if (!ab_electric_shockwave_draw(
            &row->electric, &row->wave, row->origin, row->visual_scale, row->bird_scale,
            row->bird_angle, &owner->game->menu->random, draws, 11, &count, error))
        return false;
    for (size_t index = 0; index < count; ++index) {
        AbElectricSpriteDraw *draw = draws + index;
        if (!game_shock_sprite(owner, draw->sprite,
                               (AbGameSpritePose){draw->position,
                                                  {draw->scale, draw->scale},
                                                  draw->angle,
                                                  draw->alpha,
                                                  .05f},
                               input, error))
            return false;
    }
    return true;
}

static bool game_shock_goals(PreviewShockObject *row, AbError *error) {
    PreviewShockwaves *owner = row->owner;
    PreviewGame *game = owner->game;
    AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
    AbStringMap *goals = ab_classic_goals_map(
        ab_classic_dynamics_view(ab_classic_world_runtime_view(game->world).dynamics)
            .goals);
    size_t cursor = 0;
    AbStringMapEntry entry;
    while (ab_string_map_next(goals, &cursor, &entry, error)) {
        if (!entry.present)
            return true;
        const AbDataValue *value = (const AbDataValue *)entry.value;
        if (!value || value->kind != AB_DATA_TABLE)
            return ab_fail(error, "Electrical goal has no physical table");
        size_t object = 0;
        while (object < level.bodies->count &&
               value->table != &level.properties->objects[object].physical)
            ++object;
        if (object == level.bodies->count || object >= owner->capacity)
            return ab_fail(error, "Electrical goal lost its canonical object");
        float x, y;
        if (!game_shock_read(value->table, "x", &x, error) ||
            !game_shock_read(value->table, "y", &y, error))
            return false;
        x -= row->origin.x;
        y -= row->origin.y;
        float radius = row->wave.definition.radius;
        float current = row->wave.current_radius;
        float limit = fminf(radius * radius, (current * current) * 1.2f);
        if (menu_route_truthy(menu_route_field(value->table, "electrocute")) ||
            !(x * x + y * y < limit))
            continue;
        PreviewShockObject *pig = owner->objects + object;
        if (!game_shock_audio(owner, game_text("pigs_electrocute"), error) ||
            !game_shock_make_invulnerable(pig, 2, error) ||
            !game_shock_add_update(pig, &pig->electrocute, error))
            return false;
    }
    return false;
}

static bool game_shock_sync_dynamic(PreviewShockwaves *owner, AbError *error) {
    PreviewGame *game = owner->game;
    AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
    AbDataTable *dynamic = &level.properties->dynamic;
    for (size_t index = 0; index < dynamic->count; ++index) {
        AbDataEntry entry = dynamic->entries[index];
        if (entry.key.kind != AB_DATA_STRING)
            return ab_fail(error, "Electrical dynamic key is not a name");
        if (!menu_route_truthy(entry.value)) {
            if (!ab_string_map_set(owner->dynamic, entry.key.string, 0, false, error))
                return false;
            continue;
        }
        size_t object = 0;
        while (object < level.bodies->count &&
               (entry.value.kind != AB_DATA_TABLE ||
                entry.value.table != &level.properties->objects[object].physical))
            ++object;
        if (object >= owner->capacity || object == level.bodies->count)
            return ab_fail(error, "Electrical dynamic identity is unknown");
        if (!ab_string_map_set(owner->dynamic, entry.key.string,
                               (uintptr_t)(owner->objects + object), true, error))
            return false;
    }
    return true;
}

static bool game_shock_dynamic(PreviewShockObject *wave, AbError *error) {
    PreviewShockwaves *owner = wave->owner;
    if (!game_shock_sync_dynamic(owner, error))
        return false;
    size_t cursor = 0;
    AbStringMapEntry entry;
    while (ab_string_map_next(owner->dynamic, &cursor, &entry, error)) {
        if (!entry.present)
            return true;
        if (!game_shock_impact(wave, (PreviewShockObject *)entry.value, error))
            return false;
    }
    return false;
}

static bool game_shock_update_one(PreviewShockObject *row, float seconds,
                                  bool *finished, AbError *error) {
    PreviewShockwaves *owner = row->owner;
    PreviewGame *game = owner->game;
    game->draw.background_overlay = (AbDataValue){0};
    bool puff;
    if (!ab_electric_shockwave_update(&row->electric, &row->wave, row->visual_scale,
                                      seconds, &game->menu->random, &puff, error))
        return false;
    if (puff) {
        const char *const names[] = {"smokeBuff", "blackBuff"};
        for (size_t index = 0; index < 2; ++index) {
            AbParticleEmission emission = {
                .definition = menu_span(names[index]),
                .amount = index ? 10.0f : 1.0f,
                .position = {row->origin.x * 20, row->origin.y * 20},
                .ignore_limits = index == 0};
            if (!ab_particles_emit(ab_classic_particles_engine(game->particles),
                                   &emission, error))
                return false;
        }
    }
    AbBirdShockwavePhase phase;
    if (!ab_bird_shockwave_advance(&row->wave, seconds, &phase, error))
        return false;
    owner->overlay_fields[3].value = menu_route_number(phase.overlay_alpha);
    game->draw.background_overlay =
        (AbDataValue){.kind = AB_DATA_TABLE, .table = &owner->overlay};
    *finished = false;
    if (phase.delaying)
        return true;
    game->speed = .6f;
    game->shake.amplitude = phase.camera_shake;
    if (!game_shock_goals(row, error) || !game_shock_dynamic(row, error))
        return false;
    if (phase.finished) {
        if (!ab_extra_draws_remove(game->extra_draws, menu_span(row->draw_name), error))
            return false;
        game->speed = 1;
        game->shake.amplitude = 0;
        game->prevent_ending = game->disabled;
        *finished = true;
    } else
        game->prevent_ending = game->enabled;
    return true;
}

static bool game_shock_update(void *context, float seconds, float time,
                              AbError *error) {
    (void)time;
    PreviewShockwaves *owner = context;
    size_t cursor = 0;
    AbStringMapEntry entry;
    bool continuing = false;
    for (;;) {
        if (!ab_string_map_next(owner->waves, &cursor, &entry, error))
            return false;
        if (!entry.present)
            break;
        bool finished;
        if (!game_shock_update_one((PreviewShockObject *)entry.value, seconds,
                                   &finished, error))
            return false;
        if (finished) {
            if (!ab_string_map_set(owner->waves, entry.key, 0, false, error) ||
                !ab_string_map_resume(owner->waves, entry.key, &cursor, error))
                return false;
        } else
            continuing = true;
    }
    return continuing ||
           ab_classic_game_updates_remove(
               owner->game->updates, menu_span("updateBlackBirdShockwaves"), error);
}

static bool game_shock_trigger(PreviewShockObject *row, AbError *error) {
    PreviewShockwaves *owner = row->owner;
    PreviewGame *game = owner->game;
    AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
    AbDataValue definition;
    if (row->used ||
        !ab_classic_game_updates_add(game->updates,
                                     menu_span("updateBlackBirdShockwaves"),
                                     &owner->update, error) ||
        !game_shock_definition(row, &definition, error))
        return ab_fail(error, "Invalid repeated shockwave bird activation");
    AbDataValue ability = menu_route_field(definition.table, "shockwaveAbility");
    AbClassicSceneView scene =
        ab_classic_scene_view(ab_classic_render_scene_objects(game->playfield.scene));
    if (ability.kind != AB_DATA_TABLE || row->object >= scene.count ||
        !ab_bird_shockwave_begin(ability.table, &row->wave, error) ||
        !game_shock_read(game_shock_physical(row), "x", &row->origin.x, error) ||
        !game_shock_read(game_shock_physical(row), "y", &row->origin.y, error) ||
        !game_shock_read(game_shock_physical(row), "angle", &row->bird_angle, error) ||
        !game_shock_optional(ability.table, "visualScale", 1, &row->visual_scale,
                             error))
        return false;
    row->bird_scale = scene.items[row->object].pose.scale.x;
    row->event_id = (AbDataEntry){
        game_text("id"), menu_event(game->menu, "EID_BIRD_SPECIALTY_TRIGGERED")};
    row->event = (AbDataTable){&row->event_id, 1, 1, NULL};
    if (!ab_event_manager_notify(game->menu->events, &row->event, error) ||
        !ab_electric_shockwave_init(&row->electric, &game->menu->random, error) ||
        !ab_extra_draws_set(game->extra_draws, menu_span(row->draw_name), &row->draw,
                            error))
        return false;
    game->speed = .8f;
    AbSpan name = level.bodies->objects[row->object].name;
    if (!ab_string_map_set(owner->waves, name, (uintptr_t)row, true, error) ||
        !game_shock_audio(owner, menu_route_field(definition.table, "specialSound"),
                          error) ||
        !ab_classic_birds_remove(ab_classic_game_runtime_view(game->game).birds,
                                 row->object, (AbDataValue){0}, error))
        return false;
    game->prevent_ending = game->enabled;
    if (!ab_classic_sling_allow_camera_reset(game->playfield.sling, false, error))
        return false;
    row->used = true;
    return true;
}

static bool game_shock_hook(void *context, AbDataValue name, size_t object,
                            AbError *error) {
    PreviewGame *game = context;
    if (!menu_route_equal(name, game_text("triggerBlackBirdSpecialtyShockwave")))
        return ab_fail(error, "Unresolved ordinary specialty hook: %.*s",
                       name.kind == AB_DATA_STRING ? (int)name.string.size : 0,
                       name.kind == AB_DATA_STRING ? (const char *)name.string.data
                                                   : "");
    return game->shockwaves && object < game->shockwaves->capacity
               ? game_shock_trigger(game->shockwaves->objects + object, error)
               : ab_fail(error, "Missing actual shockwave level owner");
}

static void game_shock_close(PreviewGame *game) {
    PreviewShockwaves *owner = game->shockwaves;
    if (!owner)
        return;
    ab_string_map_close(owner->waves);
    ab_string_map_close(owner->electrocuted);
    ab_string_map_close(owner->dynamic);
    free(owner->objects);
    free(owner->electros);
    free(owner);
    game->shockwaves = NULL;
}

static bool game_shock_open(PreviewGame *game, AbError *error) {
    AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
    bool needed = false;
    for (size_t bird = 0; bird < level.bird_count; ++bird) {
        AbDataValue definition;
        if (!ab_object_definition_lookup(
                &level.properties->lookup,
                level.bodies->objects[level.bird_order[bird]].name, &definition, error))
            return false;
        needed |=
            definition.kind == AB_DATA_TABLE &&
            menu_route_equal(menu_route_field(definition.table, "onTriggerSpecialty"),
                             game_text("triggerBlackBirdSpecialtyShockwave"));
    }
    for (size_t index = 0; index < level.bodies->count; ++index) {
        if (!level.bodies->objects[index].definition_name.size)
            continue;
        AbDataValue definition;
        if (!ab_object_definition_lookup(&level.properties->lookup,
                                         level.bodies->objects[index].name, &definition,
                                         error))
            return false;
        needed |= definition.kind == AB_DATA_TABLE &&
                  menu_route_truthy(menu_route_field(definition.table, "vialEffect"));
    }
    if (!needed)
        return true;
    size_t capacity = ab_classic_level_runtime_view(game->level).object_capacity;
    if (capacity >
        (8 * 1024 * 1024) / (sizeof(PreviewShockObject) + sizeof(PreviewElectrocute)))
        return ab_fail(error, "Electrical effect rows exceed retained storage limit");
    PreviewShockwaves *owner = calloc(1, sizeof(*owner));
    if (!owner)
        return ab_fail(error, "Cannot allocate electrical effect owner");
    game->shockwaves = owner;
    owner->game = game;
    owner->capacity = capacity;
    owner->objects = calloc(capacity, sizeof(*owner->objects));
    owner->electros = calloc(capacity, sizeof(*owner->electros));
    size_t nodes = 1;
    while (nodes < capacity)
        nodes *= 2;
    if (!owner->objects || !owner->electros ||
        !ab_string_map_open(0, nodes, 1024 * 1024, &owner->waves, error) ||
        !ab_string_map_open(0, nodes, 1024 * 1024, &owner->electrocuted, error) ||
        !ab_string_map_open(0, nodes, 1024 * 1024, &owner->dynamic, error))
        return ab_fail(error, "Cannot allocate stable electrical effect registries");
    owner->update = (AbClassicGameUpdateFunction){owner, game_shock_update};
    owner->pig_draw =
        (AbExtraDrawFunction){.context = owner, .draw = game_shock_pigs_draw};
    const char *const colors[] = {"r", "g", "b", "a"};
    const float values[] = {0, .15f, .2f, 0};
    for (size_t index = 0; index < 4; ++index)
        owner->overlay_fields[index] =
            (AbDataEntry){game_text(colors[index]), menu_route_number(values[index])};
    owner->overlay = (AbDataTable){owner->overlay_fields, 4, 4, NULL};
    for (unsigned sprite = 0; sprite < AB_ELECTRIC_SPRITES; ++sprite)
        if (!ab_lua_asset_sprite_bind(
                &game->menu->sprites.provider,
                ab_electric_shockwave_sprite((AbElectricSprite)sprite),
                owner->sprites + sprite, error) ||
            !owner->sprites[sprite].region)
            return ab_fail(error, "Missing original electrical effect sprite %u",
                           sprite);
    for (size_t object = 0; object < capacity; ++object) {
        PreviewShockObject *row = owner->objects + object;
        row->owner = owner;
        row->object = object;
        row->invulnerability =
            (AbObjectUpdateFunction){row, game_shock_invulnerability_update};
        row->electrocute = (AbObjectUpdateFunction){row, game_shock_electrocute_update};
        row->delayed = (AbObjectUpdateFunction){row, game_shock_delayed_update};
        row->draw = (AbExtraDrawFunction){
            .context = owner, .parameter = row, .draw = game_shock_draw};
        if (object < level.bodies->count) {
            AbSpan name = level.bodies->objects[object].name;
            int size =
                snprintf(row->draw_name, sizeof(row->draw_name),
                         "drawShockwaveParticles_%.*s", (int)name.size, name.data);
            if (size < 0 || (size_t)size >= sizeof(row->draw_name))
                return ab_fail(error, "Electrical draw name exceeds stable storage");
        }
    }
    AbClassicWorldCursor cursor = {0};
    for (;;) {
        size_t object;
        bool present;
        if (!ab_classic_level_world_next(game->playfield.level, &cursor, &object,
                                         &present, error))
            return false;
        if (!present)
            break;
        if (level.bodies->objects[object].physics.controllable &&
            !game_shock_electricity_bind(owner->objects + object, error))
            return false;
    }
    return game_shock_sync_dynamic(owner, error);
}

#endif
