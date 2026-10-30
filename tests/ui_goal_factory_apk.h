#ifndef AB_UI_GOAL_FACTORY_APK_H
#define AB_UI_GOAL_FACTORY_APK_H

static bool goal_factory_event(void *context, AbDataTable *event, AbError *error) {
    AbDataValue object = menu_route_field(event, "object");
    AbDataValue script = object.kind == AB_DATA_TABLE
                             ? menu_route_field(object.table, "gameObjectScript")
                             : (AbDataValue){0};
    AbDataValue physical = script.kind == AB_DATA_TABLE
                               ? menu_route_field(script.table, "physicsObject")
                               : (AbDataValue){0};
    if (object.kind != AB_DATA_TABLE || physical.kind != AB_DATA_TABLE ||
        physical.table != object.table)
        return ab_fail(error, "Object-created event preceded its canonical script");
    ++*(size_t *)context;
    return true;
}

static bool goal_factory_aliases(PreviewGame *game, size_t source, size_t first,
                                 size_t second, AbError *error) {
    PreviewShockwaves *owner = game->shockwaves;
    if (!owner || second >= owner->capacity)
        return ab_fail(error, "Goal alias fixture lost the electrical owner");
    PreviewShockObject *original = owner->objects + source;
    PreviewShockObject *clone = owner->objects + first;
    PreviewShockObject *other = owner->objects + second;
    if (!game_shock_add_update(original, &original->electrocute, error) ||
        !game_shock_electrocute_update(original, game_shock_physical(original), 0,
                                       error) ||
        !game_shock_transfer(original, clone, error) ||
        !game_shock_transfer(original, other, error))
        return false;
    AbDataTable *shared = original->electro;
    if (!shared || clone->electro != shared || other->electro != shared ||
        menu_route_field(game_shock_physical(clone), "electrocute").table != shared)
        return ab_fail(error, "Electrocution transfer copied its source table");
    if (!game_shock_write(original, "electrocute", (AbDataValue){0}, error))
        return false;
    original->electro_present = false;
    if (!game_shock_new_electro(original, error) || original->electro == shared ||
        !game_shock_electro_number(clone, "offsetX", .75f, error))
        return ab_fail(error,
                       "A fresh electrical effect reused a retained clone alias");
    float observed;
    return game_shock_read(other->electro, "offsetX", &observed, error) &&
           observed == .75f &&
           game_shock_read(original->electro, "offsetX", &observed, error) &&
           observed == 0;
}

static bool goal_factory_widgets(PreviewMenu *menu, AbMetal *renderer, AbError *error) {
    if (!ability_test_episode_level(menu, 10, "BlackBird_9", error))
        return false;
    PreviewGame *game = menu->game;
    AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
    AbClassicWorldRuntimeView world = ab_classic_world_runtime_view(game->world);
    AbClassicGoals *goals = ab_classic_dynamics_view(world.dynamics).goals;
    size_t initial = level.bodies->count, original_goals = level.goal_count;
    if (!original_goals)
        return ab_fail(error, "Goal factory fixture has no actual source goal");
    const AbDataTable *anchor =
        &level.properties->objects[level.goal_order[0]].physical;
    size_t original_pig = level.goal_order[0], notices = 0;
    AbEventListener listener = {menu_span("GoalFactoryFixture"), &notices,
                                goal_factory_event};
    if (!ab_event_manager_add(menu->events, menu_event(menu, "EID_OBJECT_CREATED"),
                              &listener, error))
        return false;
    float x, y;
    if (!game_shock_read(anchor, "x", &x, error) ||
        !game_shock_read(anchor, "y", &y, error))
        return false;
    const char *const definitions[] = {
        "SmallPiglette",   "MediumPiglette", "LargePiglette",         "HelmetPiglette",
        "GrandpaPiglette", "KingPiglette",   "BLACKBIRD_FLOATING_PIG"};
    for (size_t index = 0; index < 7; ++index) {
        AbDataValue name = menu_data(definitions[index]);
        const AbDataValue *prior = ab_data_get(&game->factory.counts, name);
        float count =
            prior && prior->kind == AB_DATA_NUMBER ? menu_real(prior->bits) : 0;
        size_t object;
        if (!game_factory_goal_circle(
                game, name, (AbVec2){x + (float)index * 8, y - 60}, &object, error) ||
            object != initial + index ||
            !ab_classic_goals_set(goals, object, true, error))
            return false;
        const AbDataValue *current = ab_data_get(&game->factory.counts, name);
        AbDataTable *physical = &level.properties->objects[object].physical;
        AbObjectDefinition definition;
        if (!current || current->kind != AB_DATA_NUMBER ||
            menu_real(current->bits) != count + 1 ||
            !menu_route_equal(menu_route_field(physical, "definition"), name) ||
            !ab_object_definition_open(level.catalog, name.string, NULL, &definition,
                                       error))
            return ab_fail(error, "Dynamic goal counter/definition is incorrect");
        AbDataValue extra = menu_route_field(physical, "extraSprites");
        AbDataValue source_extra = menu_route_field(definition.source, "extraSprites");
        if (source_extra.kind == AB_DATA_TABLE &&
            (extra.kind != AB_DATA_TABLE || extra.table == source_extra.table ||
             extra.table->count != source_extra.table->count))
            return ab_fail(error,
                           "Dynamic goal aliases the catalog's mutable face table");
        AbClassicCharacters *characters =
            ab_classic_game_runtime_view(game->game).characters;
        if (!ab_classic_characters_set_goal_property(characters, object,
                                                     menu_span("blinkTimer"),
                                                     menu_route_number(10000), error) ||
            !ab_classic_characters_set_goal_property(characters, object,
                                                     menu_span("oinkTimer"),
                                                     menu_route_number(10000), error) ||
            !ab_classic_characters_set_goal_property(
                characters, object, menu_span("disableSmiling"), game->enabled, error))
            return false;
    }
    if (notices != 7 ||
        !ab_event_manager_remove(menu->events, menu_event(menu, "EID_OBJECT_CREATED"),
                                 &listener, error) ||
        !goal_factory_aliases(game, original_pig, initial, initial + 1, error))
        return false;
    if (!ab_classic_level_sync_goal_order(game->playfield.level,
                                          ab_classic_goals_map(goals), error) ||
        ab_classic_level_view(game->playfield.level).goal_count != original_goals + 7 ||
        game->explosions.physical_count != initial + 7 ||
        !physics_replay_capture(menu, renderer, error))
        return ab_fail(error, "Goal factory lost live traversal/renderer registration");
    for (size_t frame = 0; frame < 30; ++frame)
        if (!game_eagle_test_frame(menu, error))
            return false;
    game_session_free(menu);
    menu->level_request_ready = false;
    puts("All six actual pig types and floating pig: counters, owned face tables, "
         "canonical goals and creation events, shared electrical table lifetimes, "
         "physical/renderer adapters, Metal and subsequent frames.");
    return true;
}

#endif
