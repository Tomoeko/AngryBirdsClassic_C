#ifndef AB_GAME_SHOCKWAVE_ELECTRICITY_H
#define AB_GAME_SHOCKWAVE_ELECTRICITY_H

#include "ab/script_math.h"

/* Only the recovered Short Fuse constructor is handled here. Other bird-added
 * families retain their separate recovery status. This boundary consumes its
 * draw among the actual initial bird timer pairs, before later birds/goals. */
static bool game_shock_bird_prepare(void *context, AbClassicLevel *source,
                                    size_t object, AbRandom48 *random, AbError *error) {
    (void)context;
    AbClassicLevelView level = ab_classic_level_view(source);
    AbDataValue definition;
    if (!ab_object_definition_lookup(&level.properties->lookup,
                                     level.bodies->objects[object].name, &definition,
                                     error) ||
        definition.kind != AB_DATA_TABLE)
        return false;
    if (!menu_route_equal(menu_route_field(definition.table, "onBirdAdded"),
                          game_text("shockwaveBlackBirdAdded")))
        return true;
    AbDataValue ability = menu_route_field(definition.table, "shockwaveAbility");
    AbDataValue electricity = ability.kind == AB_DATA_TABLE
                                  ? menu_route_field(ability.table, "electricity")
                                  : (AbDataValue){0};
    float minimum, maximum, speed, unit;
    if (electricity.kind != AB_DATA_TABLE ||
        !game_shock_read(electricity.table, "minDelay", &minimum, error) ||
        !game_shock_read(electricity.table, "maxDelay", &maximum, error) ||
        !game_shock_optional(electricity.table, "speed", 1, &speed, error) ||
        ab_script_random(random, 0, 0, 0, &unit) != AB_SCRIPT_RANDOM_OK)
        return ab_fail(error, "Missing actual initial bird electricity definition");
    float timer = minimum + unit * (maximum - minimum);
    AbDataValue values[] = {menu_route_number(0),
                            menu_route_number(speed),
                            menu_route_number(timer),
                            {.kind = AB_DATA_BOOLEAN},
                            menu_route_field(electricity.table, "sprites")};
    const char *const names[] = {"flameTimer", "flameTimerSpeed", "appearTimer",
                                 "showFlame", "electricitySpriteSet"};
    AbObjectPropertyAccess access = {
        .physics_object = {.kind = AB_DATA_TABLE,
                           .table = &level.properties->objects[object].physical}};
    for (size_t index = 0; index < 5; ++index)
        if (!ab_object_property_set(&access, menu_span(names[index]), values[index],
                                    error))
            return false;
    return true;
}

static bool game_shock_electricity_update(void *context, void *receiver, float seconds,
                                          AbError *error) {
    PreviewShockObject *row = context;
    PreviewShockwaves *owner = row->owner;
    PreviewGame *game = owner->game;
    AbDataTable *physical = game_shock_physical(row);
    AbDataValue definition;
    if (receiver != physical || !game_shock_definition(row, &definition, error))
        return false;
    AbDataValue ability = menu_route_field(definition.table, "shockwaveAbility");
    AbDataValue electricity = ability.kind == AB_DATA_TABLE
                                  ? menu_route_field(ability.table, "electricity")
                                  : (AbDataValue){0};
    float timer;
    if (electricity.kind != AB_DATA_TABLE ||
        !game_shock_read(physical, "appearTimer", &timer, error))
        return false;
    timer -= seconds;
    if (!game_shock_number(row, "appearTimer", timer, error))
        return false;
    if (timer <= 0) {
        float minimum, maximum, value;
        if (!game_shock_read(electricity.table, "minDelay", &minimum, error) ||
            !game_shock_read(electricity.table, "maxDelay", &maximum, error) ||
            !game_shock_random(owner, 0, 0, 0, &value, error) ||
            !game_shock_number(row, "appearTimer",
                               minimum + value * (maximum - minimum), error) ||
            !game_shock_write(row, "showFlame", game->enabled, error) ||
            !game_shock_random(owner, 2, 1, 36, &value, error))
            return false;
        float degrees = value * 10;
        if (!game_shock_number(row, "flameAngleOffset",
                               (float)((double)degrees * 0x1.1df46a2529d39p-6),
                               error) ||
            !game_shock_random(owner, 2, 1, 2, &value, error))
            return false;
        AbDataValue selected = row->electricity_sets[value == 1 ? 0 : 1];
        float scale;
        if (!game_shock_write(row, "electricitySpriteSet", selected, error) ||
            !game_shock_optional(selected.table, "scale", 1, &scale, error) ||
            !game_shock_number(row, "flameSpriteScale", scale, error) ||
            !game_shock_write(row, "flameSpriteOffsetDisabled",
                              menu_route_field(selected.table, "offsetDisabled"),
                              error))
            return false;
    }
    float flame;
    if (!game_shock_read(physical, "flameTimer", &flame, error))
        return false;
    if (menu_route_truthy(menu_route_field(physical, "showFlame"))) {
        float speed;
        if (!game_shock_read(physical, "flameTimerSpeed", &speed, error))
            return false;
        flame += seconds * speed;
        if (!game_shock_number(row, "flameTimer", flame, error))
            return false;
        if (flame >= 1) {
            flame = 0;
            if (!game_shock_number(row, "flameTimer", flame, error) ||
                !game_shock_write(row, "showFlame", game->disabled, error))
                return false;
        }
    }
    AbDataValue selected = menu_route_field(physical, "electricitySpriteSet");
    size_t set = selected.kind == AB_DATA_TABLE &&
                         selected.table == row->electricity_sets[0].table
                     ? 0
                     : 1;
    if (selected.kind != AB_DATA_TABLE ||
        selected.table != row->electricity_sets[set].table)
        return ab_fail(error, "Electricity sprite set lost its authored identity");
    AbDataValue offsets = menu_route_field(selected.table, "offset");
    AbDataValue offset = {0};
    if (offsets.kind == AB_DATA_TABLE) {
        const AbDataValue *found =
            ab_data_get(offsets.table, menu_route_field(physical, "sprite"));
        if (found)
            offset = *found;
        if (!menu_route_truthy(offset))
            offset = menu_route_field(offsets.table, "default");
    }
    if (!menu_route_truthy(offset))
        offset = (AbDataValue){.kind = AB_DATA_TABLE, .table = &row->zero_offset};
    float index = floorf(flame * (float)row->electricity_counts[set]) + 1;
    if (!isfinite(index) || index < 1 || index > (float)row->electricity_counts[set])
        return ab_fail(error, "Electricity animation index is outside its source set");
    return game_shock_write(row, "flameOffset", offset, error) &&
           game_shock_write(row, "flameSprite",
                            game_script_index(selected.table, (size_t)index), error);
}

static bool game_shock_electricity_draw(void *context, void *parameter,
                                        AbExtraDrawInput input, AbError *error) {
    PreviewShockwaves *owner = context;
    PreviewShockObject *row = parameter;
    PreviewGame *game = owner->game;
    AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
    if (level.removed_objects[row->object])
        return ab_extra_draws_remove(game->extra_draws,
                                     menu_span(row->electricity_draw_name), error);
    AbDataTable *physical = game_shock_physical(row);
    AbDataValue sprite = menu_route_field(physical, "flameSprite");
    if (!menu_route_truthy(sprite) ||
        !menu_route_truthy(menu_route_field(physical, "showFlame")))
        return true;
    AbDataValue selected = menu_route_field(physical, "electricitySpriteSet");
    if (selected.kind != AB_DATA_TABLE)
        return ab_fail(error, "Electricity draw lost its authored sprite set");
    size_t set = selected.table == row->electricity_sets[0].table ? 0 : 1;
    size_t frame = 0;
    while (frame < row->electricity_counts[set] &&
           !menu_route_equal(game_script_index(selected.table, frame + 1), sprite))
        ++frame;
    if (frame == row->electricity_counts[set])
        return ab_fail(error, "Electricity sprite is not in its prepared set");
    AbClassicSceneView scene =
        ab_classic_scene_view(ab_classic_render_scene_objects(game->playfield.scene));
    float scale, angle, rotation, cosine, sine;
    AbVec2 offset, position;
    AbDataValue point = menu_route_field(physical, "flameOffset");
    if (point.kind != AB_DATA_TABLE ||
        !game_shock_read(physical, "flameSpriteScale", &scale, error) ||
        !game_shock_read(point.table, "x", &offset.x, error) ||
        !game_shock_read(point.table, "y", &offset.y, error) ||
        !game_shock_read(physical, "angle", &angle, error) ||
        !game_shock_read(physical, "flameAngleOffset", &rotation, error) ||
        !game_shock_read(physical, "x", &position.x, error) ||
        !game_shock_read(physical, "y", &position.y, error) ||
        !ab_script_cosine(angle, &cosine) || !ab_script_sine(angle, &sine))
        return false;
    scale = scene.items[row->object].pose.scale.x * scale;
    offset.x *= scale;
    offset.y *= scale;
    position.x += offset.x * cosine - offset.y * sine;
    position.y += offset.x * sine + offset.y * cosine;
    AbDrawCommand command;
    bool visible;
    return ab_game_sprite_command(
               row->electricity_sprites[set] + frame,
               (AbGameSpritePose){position, {scale, scale}, rotation, 1, .05f},
               input.camera, input.width, input.height, &command, &visible, error) &&
           (!visible || ab_draw_list_push(input.list, &command, error));
}

static bool game_shock_electricity_bind(PreviewShockObject *row, AbError *error) {
    PreviewGame *game = row->owner->game;
    AbDataValue definition;
    if (!game_shock_definition(row, &definition, error))
        return false;
    if (!menu_route_equal(menu_route_field(definition.table, "onBirdAdded"),
                          game_text("shockwaveBlackBirdAdded")))
        return true;
    AbDataValue ability = menu_route_field(definition.table, "shockwaveAbility");
    AbDataValue electricity = ability.kind == AB_DATA_TABLE
                                  ? menu_route_field(ability.table, "electricity")
                                  : (AbDataValue){0};
    if (electricity.kind != AB_DATA_TABLE)
        return ab_fail(error, "Missing actual bird electricity definition");
    for (size_t set = 0; set < 2; ++set) {
        AbDataValue frames =
            menu_route_field(electricity.table, set ? "sprites2" : "sprites");
        if (frames.kind != AB_DATA_TABLE)
            return ab_fail(error, "Missing actual electricity animation frames");
        row->electricity_sets[set] = frames;
        for (size_t frame = 0; frame < 16; ++frame) {
            AbDataValue sprite = game_script_index(frames.table, frame + 1);
            if (sprite.kind == AB_DATA_NIL)
                break;
            if (sprite.kind != AB_DATA_STRING ||
                !ab_lua_asset_sprite_bind(&game->menu->sprites.provider, sprite.string,
                                          row->electricity_sprites[set] + frame,
                                          error) ||
                !row->electricity_sprites[set][frame].region)
                return ab_fail(error, "Missing actual bird electricity sprite");
            ++row->electricity_counts[set];
        }
        if (!row->electricity_counts[set] ||
            game_script_index(frames.table, 17).kind != AB_DATA_NIL)
            return ab_fail(error, "Electricity animation exceeds prepared storage");
    }
    row->zero_fields[0] = (AbDataEntry){game_text("x"), menu_route_number(0)};
    row->zero_fields[1] = (AbDataEntry){game_text("y"), menu_route_number(0)};
    row->zero_offset = (AbDataTable){row->zero_fields, 2, 2, NULL};
    AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
    AbSpan name = level.bodies->objects[row->object].name;
    int size = snprintf(row->electricity_draw_name, sizeof(row->electricity_draw_name),
                        "drawBlackBirdFlame_%.*s", (int)name.size, name.data);
    float z;
    if (size < 0 || (size_t)size >= sizeof(row->electricity_draw_name) ||
        !game_shock_read(definition.table, "z_order", &z, error))
        return ab_fail(error, "Invalid actual electricity draw name/layer");
    row->electricity_update =
        (AbObjectUpdateFunction){row, game_shock_electricity_update};
    row->electricity_draw = (AbExtraDrawFunction){.context = row->owner,
                                                  .parameter = row,
                                                  .draw = game_shock_electricity_draw,
                                                  .z = z + 1,
                                                  .z_present = true};
    return game_shock_add_update(row, &row->electricity_update, error) &&
           ab_extra_draws_set(game->extra_draws, menu_span(row->electricity_draw_name),
                              &row->electricity_draw, error);
}

#endif
