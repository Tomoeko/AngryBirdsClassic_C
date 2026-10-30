#ifndef AB_UI_EXPLOSION_JOINTS_APK_H
#define AB_UI_EXPLOSION_JOINTS_APK_H

/* Exercise the real Matilda/egg factory and explosion owner beside authored
 * breakable joints. The blast call is a controlled contact boundary, followed
 * by ordinary live frames; it is not an original-game playthrough comparison. */
static bool explosion_joint_widgets(PreviewMenu *menu, AbMetal *renderer,
                                    AbError *error) {
    for (size_t connected = 0; connected < 2; ++connected) {
        if (!ability_test_episode_level(menu, 2, "LevelP2_80", error))
            return false;
        PreviewGame *game = menu->game;
        AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
        size_t joint = 0;
        while (joint < level.joints->joint_count &&
               !(level.joints->storage.joints[joint].definition.breakable.kind ==
                     AB_DATA_BOOLEAN &&
                 level.joints->storage.joints[joint].definition.breakable.bits))
            ++joint;
        if (joint == level.joints->joint_count)
            return ab_fail(error, "Explosion fixture lost its breakable joint");
        AbDataValue target;
        if (!ab_object_world_lookup(
                &level.properties->lookup,
                (AbDataValue){
                    .kind = AB_DATA_STRING,
                    .string =
                        level.joints->storage.joints[joint].definition.endpoints[0]},
                &target, error) ||
            target.kind != AB_DATA_TABLE)
            return false;
        AbVec2 point = {menu_real(menu_route_field(target.table, "x").bits),
                        menu_real(menu_route_field(target.table, "y").bits)};
        size_t bird = SIZE_MAX;
        for (size_t index = 0; index < level.bird_count; ++index) {
            size_t candidate = level.bird_order[index];
            AbDataValue definition;
            if (!ab_object_definition_lookup(&level.properties->lookup,
                                             level.bodies->objects[candidate].name,
                                             &definition, error) ||
                definition.kind != AB_DATA_TABLE)
                return false;
            if (menu_route_equal(menu_route_field(definition.table, "specialty"),
                                 menu_data("GRENADE"))) {
                bird = candidate;
                break;
            }
        }
        if (bird == SIZE_MAX)
            return ab_fail(error, "Explosion fixture has no actual Matilda");
        if (!ab_classic_shots_set_position(game->playfield.shots, bird, point, error))
            return false;
        game->flying = (AbDataValue){
            .kind = AB_DATA_TABLE, .table = &level.properties->objects[bird].physical};
        game->globals.specialty_available = game->enabled;
        size_t egg = level.bodies->count;
        if (!ab_classic_bird_specialty_trigger_flying(
                ab_classic_game_runtime_view(game->game).specialty, 0, error) ||
            level.bodies->count != egg + 1 ||
            !ab_classic_level_set_position(game->playfield.level, egg, point, error))
            return false;
        AbDataValue definition;
        if (!ab_object_definition_lookup(&level.properties->lookup,
                                         level.bodies->objects[egg].name, &definition,
                                         error) ||
            definition.kind != AB_DATA_TABLE || definition.table->count >= 128)
            return false;
        AbDataEntry fields[128];
        memcpy(fields, definition.table->entries,
               definition.table->count * sizeof(*fields));
        AbDataTable selected = {fields, definition.table->count, 128, NULL};
        /* Cover the source's optional all-connected-joint radius separately
         * from ordinary break-force requests, without changing level data. */
        if (connected) {
            AbObjectPropertyAccess access = {
                .physics_object = {.kind = AB_DATA_TABLE, .table = &selected}};
            if (!ab_object_property_set(
                    &access, menu_span("explosionJointDestructionRadius"),
                    menu_route_field(definition.table, "explosionRadius"), error))
                return false;
        }
        size_t before = level.joints->joint_count;
        if (!ab_classic_explosions_make(
                &game->explosions, egg,
                (AbDataValue){.kind = AB_DATA_TABLE, .table = &selected},
                (AbDataValue){0}, (AbDataValue){0}, error))
            return false;
        AbGameTailJoints *tail = ab_classic_joint_updates_tail(game->joint_updates);
        if (!tail->destroy->count || level.joints->joint_count != before ||
            ab_classic_joint_updates_notifications(game->joint_updates))
            return ab_fail(error, "Explosion did not defer its real joint requests");
        size_t requests = tail->destroy->count;
        for (size_t frame = 0; frame < 120 && !game->ended; ++frame)
            if (!game_eagle_test_frame(menu, error))
                return false;
        if (tail->destroy->count ||
            ab_classic_joint_updates_notifications(game->joint_updates) < requests ||
            level.world->failed)
            return ab_fail(error, "Deferred explosion joint pass did not finish");
        AbImage image = {0};
        float clear[] = {0, 0, 0, 1};
        bool okay = ab_metal_capture(renderer, menu->context.draw_list,
                                     (uint32_t)menu->screen.x, (uint32_t)menu->screen.y,
                                     clear, 16 * 1024 * 1024, &image, error);
        ab_image_free(&image);
        if (!okay)
            return false;
        printf("Actual egg explosion: %zu deferred joint requests, connected-radius "
               "branch %zu, live physics and Metal draw.\n",
               requests, connected);
        game_session_free(menu);
        menu->level_request_ready = false;
    }
    return true;
}

#endif
