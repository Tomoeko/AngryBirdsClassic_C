#ifndef AB_GAME_SHOCKWAVE_PIGS_H
#define AB_GAME_SHOCKWAVE_PIGS_H

static bool game_shock_new_electro(PreviewShockObject *row, AbError *error) {
    PreviewShockwaves *owner = row->owner;
    AbClassicLevelView level = ab_classic_level_view(owner->game->playfield.level);
    for (size_t slot = 0; slot < owner->capacity; ++slot) {
        PreviewElectrocute *candidate = owner->electros + slot;
        bool referenced = false;
        for (size_t object = 0; object < level.bodies->count; ++object) {
            AbDataValue value = menu_route_field(
                &level.properties->objects[object].physical, "electrocute");
            if (value.kind == AB_DATA_TABLE && value.table == &candidate->table) {
                referenced = true;
                break;
            }
        }
        if (referenced)
            continue;
        const char *const names[] = {"offsetX", "offsetY",    "offsetScale",
                                     "angle",   "angleTimer", "offsetAngle"};
        for (size_t index = 0; index < 6; ++index)
            candidate->fields[index] =
                (AbDataEntry){game_text(names[index]), menu_route_number(0)};
        candidate->table = (AbDataTable){candidate->fields, 6, 6, NULL};
        row->electro = &candidate->table;
        row->electro_present = true;
        return game_shock_write(
                   row, "electrocute",
                   (AbDataValue){.kind = AB_DATA_TABLE, .table = row->electro},
                   error) &&
               game_shock_write(row, "disableSmiling", owner->game->enabled, error);
    }
    return ab_fail(error, "Electrocution alias storage is exhausted");
}

static bool game_shock_electro_number(PreviewShockObject *row, const char *key,
                                      float value, AbError *error) {
    if (!row->electro)
        return ab_fail(error, "Missing live electrocution table");
    return game_eagle_number(row->electro, key, value, error);
}

static bool game_shock_restore_sprite(PreviewShockObject *row, AbError *error) {
    PreviewGame *game = row->owner->game;
    AbDataTable *physical = game_shock_physical(row);
    AbDataValue affected = menu_route_field(physical, "spriteAffectedByVial");
    AbDataValue sprite = {0};
    if (menu_route_equal(affected, game_text("orange"))) {
        AbDataValue puffer = menu_route_field(physical, "pufferDefinition");
        if (puffer.kind != AB_DATA_TABLE)
            return ab_fail(error, "Electrocuted puffer lost its actual definition");
        sprite = menu_route_field(puffer.table, "spriteSpecial");
    } else if (menu_route_equal(affected, game_text("pink"))) {
        sprite = menu_route_field(physical, "pinkVialSprite");
    } else if (menu_route_truthy(affected)) {
        return true;
    } else {
        AbDamageSpriteNames names;
        if (!game_shock_write(row, "dontUpdateDamageSprite", game->disabled, error) ||
            !ab_classic_visuals_update_damage(game->playfield.visuals, row->object,
                                              &game->workspace, &names, error) ||
            !game_shock_write(row, "damageSprite", names.sprite, error) ||
            !game_shock_write(row, "blinkSprite", names.blink, error) ||
            !game_shock_write(row, "smileSprite", names.smile, error))
            return false;
        return game_shock_write(row, "dontUpdateDamageSprite", game->enabled, error);
    }
    return sprite.kind == AB_DATA_STRING
               ? ab_classic_visuals_set_sprite(game->playfield.visuals, row->object,
                                               sprite.string, error)
               : ab_fail(error, "Electrocuted vial sprite is absent");
}

static AbDataValue game_shock_pig_sprite(PreviewShockObject *row) {
    AbDataTable *physical = game_shock_physical(row);
    AbDataValue definition = menu_route_field(physical, "definition");
    AbDataValue affected = menu_route_field(physical, "spriteAffectedByVial");
    if (menu_route_equal(affected, game_text("pink")))
        definition = menu_route_field(physical, "preBubbleDefinition");
    bool helmet = menu_route_equal(definition, game_text("HelmetPiglette"));
    bool king = menu_route_equal(definition, game_text("KingPiglette"));
    if (menu_route_equal(affected, game_text("orange")))
        return game_text(helmet ? "ELECTROCUTED_INFLATED_HELMET"
                         : king ? "ELECTROCUTED_INFLATED_KING"
                                : "PIG_SHOCK_INFLATED");
    if (menu_route_equal(affected, game_text("pink")))
        return game_text(helmet ? "ELECTROCUTED_BUBBLE_HELMET"
                         : king ? "ELECTROCUTED_BUBBLE_KING"
                                : "PIG_SHOCK_BUBBLE");
    const char *const definitions[] = {
        "SmallPiglette",   "MediumPiglette", "LargePiglette",         "HelmetPiglette",
        "GrandpaPiglette", "KingPiglette",   "BLACKBIRD_FLOATING_PIG"};
    const char *const sprites[] = {"PIG_SHOCK_SMALL",  "PIG_SHOCK_MED",
                                   "PIG_SHOCK_STACHE", "ELECTGROCUTED_HELMET_PIG",
                                   "PIG_SHOCK_STACHE", "ELECTROCUTED_KING",
                                   "PIG_SHOCK_BUBBLE"};
    for (size_t index = 0; index < 7; ++index)
        if (menu_route_equal(definition, game_text(definitions[index])))
            return game_text(sprites[index]);
    return (AbDataValue){0};
}

static bool game_shock_invulnerability_update(void *context, void *receiver,
                                              float seconds, AbError *error) {
    PreviewShockObject *row = context;
    PreviewGame *game = row->owner->game;
    float timer;
    if (receiver != game_shock_physical(row) ||
        !game_shock_read(receiver, "invulnerabilityTimer", &timer, error))
        return false;
    timer -= seconds * game->speed;
    if (!game_shock_number(row, "invulnerabilityTimer", timer, error))
        return false;
    return timer > 0 ||
           (game_shock_write(row, "ignoreAllDamage", game->disabled, error) &&
            game_shock_remove_update(row, &row->invulnerability, error) &&
            game_shock_write(row, "invulnerabilityTimer", (AbDataValue){0}, error));
}

static bool game_shock_pigs_draw(void *context, void *parameter, AbExtraDrawInput input,
                                 AbError *error) {
    (void)parameter;
    PreviewShockwaves *owner = context;
    PreviewGame *game = owner->game;
    AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
    size_t cursor = 0;
    AbStringMapEntry entry;
    while (ab_string_map_next(owner->electrocuted, &cursor, &entry, error)) {
        if (!entry.present)
            return true;
        PreviewShockObject *row = (PreviewShockObject *)entry.value;
        const AbDataValue *found =
            ab_data_get(&level.properties->world,
                        (AbDataValue){.kind = AB_DATA_STRING, .string = entry.key});
        AbDataValue member = found ? *found : (AbDataValue){0};
        if (!menu_route_truthy(member)) {
            if (!game_shock_write(row, "electrocute", (AbDataValue){0}, error) ||
                !ab_string_map_set(owner->electrocuted, entry.key, 0, false, error) ||
                !ab_string_map_resume(owner->electrocuted, entry.key, &cursor, error))
                return false;
            row->electro_present = false;
            continue;
        }
        float x, y, offset_x, offset_y, scale, angle, angle_offset;
        if (!game_shock_read(game_shock_physical(row), "x", &x, error) ||
            !game_shock_read(game_shock_physical(row), "y", &y, error) ||
            !game_shock_read(row->electro, "offsetX", &offset_x, error) ||
            !game_shock_read(row->electro, "offsetY", &offset_y, error) ||
            !game_shock_read(row->electro, "offsetScale", &scale, error) ||
            !game_shock_read(row->electro, "angle", &angle, error) ||
            !game_shock_read(row->electro, "offsetAngle", &angle_offset, error))
            return false;
        if (!game_shock_sprite(
                owner, AB_ELECTRIC_SPARK,
                (AbGameSpritePose){{x + offset_x, y + offset_y},
                                   {2 * (.5f + scale), 2 * (.5f + scale)},
                                   angle + angle_offset,
                                   1,
                                   .05f},
                input, error) ||
            !game_shock_sprite(
                owner, AB_ELECTRIC_SPARK,
                (AbGameSpritePose){{x + offset_x * 5, y + offset_y * 5},
                                   {2 * (.2f + scale * .2f), 2 * (.2f + scale * .2f)},
                                   angle_offset * 50,
                                   1,
                                   .05f},
                input, error))
            return false;
    }
    return false;
}

static bool game_shock_electrocute_update(void *context, void *receiver, float seconds,
                                          AbError *error) {
    PreviewShockObject *row = context;
    PreviewShockwaves *owner = row->owner;
    PreviewGame *game = owner->game;
    AbDataTable *physical = game_shock_physical(row);
    AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
    float timer, sprite_timer;
    if (receiver != physical ||
        !game_shock_optional(physical, "electrocuteTimer", 2, &timer, error))
        return false;
    timer -= seconds;
    if (!game_shock_number(row, "electrocuteTimer", timer, error))
        return false;
    if (!menu_route_truthy(menu_route_field(physical, "electrocuteSpriteTimer"))) {
        if (!game_shock_random(owner, 0, 0, 0, &sprite_timer, error))
            return false;
        sprite_timer = (sprite_timer + .5f) * .07f;
    } else if (!game_shock_read(physical, "electrocuteSpriteTimer", &sprite_timer,
                                error))
        return false;
    sprite_timer -= seconds;
    if (!game_shock_number(row, "electrocuteSpriteTimer", sprite_timer, error) ||
        !game_shock_write(row, "dontUpdateDamageSprite", game->enabled, error) ||
        !ab_string_map_set(owner->electrocuted, level.bodies->objects[row->object].name,
                           (uintptr_t)row, true, error))
        return false;
    if (!row->electro_present && !game_shock_new_electro(row, error))
        return false;
    const char *const offsets[] = {"offsetX", "offsetY", "offsetScale", "offsetAngle"};
    for (size_t index = 0; index < 4; ++index) {
        float value;
        if (!game_shock_random(owner, 0, 0, 0, &value, error))
            return false;
        if (!game_shock_electro_number(row, offsets[index],
                                       (value - .5f) * (index < 2 ? .5f : .1f), error))
            return false;
    }
    float angle_timer;
    if (!game_shock_read(row->electro, "angleTimer", &angle_timer, error))
        return false;
    angle_timer -= seconds;
    if (!game_shock_electro_number(row, "angleTimer", angle_timer, error))
        return false;
    if (angle_timer <= 0) {
        float value;
        if (!game_shock_random(owner, 0, 0, 0, &value, error))
            return false;
        if (!game_shock_electro_number(row, "angleTimer", value * .1f + .05f, error))
            return false;
        if (!game_shock_random(owner, 2, 1, 36, &value, error))
            return false;
        float degrees = value * 10;
        if (!game_shock_electro_number(
                row, "angle", (float)((double)degrees * 0x1.1df46a2529d39p-6), error))
            return false;
    }
    if (!ab_extra_draws_set(game->extra_draws, menu_span("electrocutePigs"),
                            &owner->pig_draw, error))
        return false;
    if (sprite_timer <= 0) {
        float index;
        if (!game_shock_write(row, "electrocuteSpriteTimer", (AbDataValue){0}, error) ||
            !game_shock_optional(physical, "electrocuteSpriteIndex", 0, &index, error))
            return false;
        index = index - floorf(index / 5) * 5 + 1;
        if (!game_shock_number(row, "electrocuteSpriteIndex", index, error))
            return false;
        if (index == 1 || index == 3) {
            if (!game_shock_restore_sprite(row, error))
                return false;
        } else {
            AbDataValue sprite = game_shock_pig_sprite(row);
            if (sprite.kind != AB_DATA_STRING ||
                !ab_classic_visuals_set_sprite(game->playfield.visuals, row->object,
                                               sprite.string, error))
                return ab_fail(error, "Missing actual electrocuted pig sprite");
        }
    }
    if (timer > 0)
        return true;
    if (!game_shock_write(row, "electrocuteTimer", (AbDataValue){0}, error))
        return false;
    if (!menu_route_truthy(menu_route_field(physical, "isPuffing")) &&
        !menu_route_equal(menu_route_field(physical, "spriteAffectedByVial"),
                          game_text("pink"))) {
        if (!menu_route_truthy(menu_route_field(physical, "vialEffectQueue"))) {
            if (!game_shock_write(row, "dontUpdateDamageSprite", game->disabled,
                                  error) ||
                !game_shock_dead(row, error))
                return false;
        } else if (!game_shock_write(row, "shouldDieAfterVialEffect", game->enabled,
                                     error))
            return false;
    } else if (!game_shock_restore_sprite(row, error))
        return false;
    row->electro_present = false;
    return ab_string_map_set(owner->electrocuted,
                             level.bodies->objects[row->object].name, 0, false,
                             error) &&
           game_shock_write(row, "electrocute", (AbDataValue){0}, error) &&
           game_shock_remove_update(row, &row->electrocute, error);
}

static bool game_shock_make_invulnerable(PreviewShockObject *row, float duration,
                                         AbError *error) {
    PreviewGame *game = row->owner->game;
    AbDataTable *physical = game_shock_physical(row);
    bool active = menu_route_truthy(menu_route_field(physical, "invulnerabilityTimer"));
    if (!active && (!game_shock_write(row, "ignoreAllDamage", game->enabled, error) ||
                    !game_shock_add_update(row, &row->invulnerability, error)))
        return false;
    float timer;
    return game_shock_optional(physical, "invulnerabilityTimer", 0, &timer, error) &&
           game_shock_number(row, "invulnerabilityTimer", fmaxf(duration, timer),
                             error);
}

/* BlackBird3915 shares the electrocuted table, rather than making a deep copy.
 * The table pool retains aliases when an earlier source starts a fresh effect. */
static bool game_shock_transfer(PreviewShockObject *source, PreviewShockObject *target,
                                AbError *error) {
    if (!source || !target || source->owner != target->owner)
        return ab_fail(error, "Foreign electrocution transfer identity");
    AbObjectUpdates *updates = NULL;
    bool active = false;
    if (!ab_classic_object_hooks_updates(game_shock_hooks(source->owner),
                                         source->object, &updates, error) ||
        (updates &&
         !ab_object_update_has(updates, &source->electrocute, &active, error)))
        return false;
    if (!active)
        return true;
    AbDataTable *physical = game_shock_physical(source);
    const char *const fields[] = {"electrocute", "electrocuteTimer",
                                  "electrocuteSpriteTimer"};
    for (size_t index = 0; index < 3; ++index)
        if (!game_shock_write(target, fields[index],
                              menu_route_field(physical, fields[index]), error))
            return false;
    AbDataValue shared = menu_route_field(physical, "electrocute");
    if (shared.kind != AB_DATA_NIL && shared.kind != AB_DATA_TABLE)
        return ab_fail(error, "Invalid source electrocution table alias");
    target->electro = shared.kind == AB_DATA_TABLE ? shared.table : NULL;
    target->electro_present = target->electro != NULL;
    float duration;
    AbClassicLevelView level =
        ab_classic_level_view(source->owner->game->playfield.level);
    return game_shock_read(game_shock_physical(target), "electrocuteTimer", &duration,
                           error) &&
           game_shock_write(target, "dontUpdateDamageSprite",
                            source->owner->game->enabled, error) &&
           game_shock_make_invulnerable(target, duration, error) &&
           ab_string_map_set(source->owner->electrocuted,
                             level.bodies->objects[target->object].name,
                             (uintptr_t)target, true, error) &&
           game_shock_add_update(target, &target->electrocute, error);
}

#endif
