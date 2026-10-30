#ifndef AB_UI_BIRTHDAY_APK_H
#define AB_UI_BIRTHDAY_APK_H

static bool birthday_widgets(PreviewMenu *menu, AbMetal *renderer, AbError *error) {
    const char *const names[] = {"Level978",  "Level979", "Level925",  "Level926",
                                 "Level931",  "Level932", "Level928",  "Level927",
                                 "Level1045", "Level977", "Level1048", "Level976",
                                 "Level930",  "Level974", "Level980"};
    bool launch = menu->launch_enabled;
    menu->launch_enabled = true;
    for (size_t scene = 0; scene < sizeof(names) / sizeof(*names); ++scene) {
        if (!ability_test_episode_level(menu, 7, names[scene], error))
            return false;
        PreviewGame *game = menu->game;
        AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
        if (!game->hats.count || game->hats.count != level.bird_count)
            return ab_fail(error,
                           "Birthday construction did not collect its actual birds");
        size_t created = 0;
        for (size_t index = 0; index < game->hats.count; ++index) {
            PreviewHat *row = game->hats.records + index;
            if (!row->source.collision.object_name.size)
                continue;
            ++created;
            const AbClassicJointRecord *joint =
                ab_classic_joints_metadata(level.joints, row->source.joint);
            AbBirdHat *registered = NULL;
            if (!joint || joint->world_joint == SIZE_MAX ||
                !game_hat_lookup(game, level.bodies->objects[row->bird].name,
                                 &registered, error) ||
                registered != &row->source ||
                !level.character_objects[row->bird].has_hat)
                return ab_fail(error,
                               "Birthday hat lost its actual body/joint/registry");
        }
        if (!created)
            return ab_fail(error, "Birthday source offsets created no hats");
        for (size_t frame = 0; frame < 360; ++frame)
            if (!game_eagle_test_frame(menu, error))
                return false;
        if (!game->shot.bird_ready || level.world->failed)
            return ab_fail(error, "Birthday scene did not complete sling transfer");
        if (!scene) {
            if (!game_eagle_test_capture(menu, renderer, "AB_BIRTHDAY_CAPTURE", error))
                return false;
            AbBirdHat *hat = NULL;
            AbSpan current = menu_span(game->shot.current_bird_name);
            if (!game_hat_lookup(game, current, &hat, error) || !hat)
                return ab_fail(error, "Actual held birthday bird lost its hat");
            size_t pink = 0;
            while (pink < level.bodies->count &&
                   !menu_route_equal(
                       (AbDataValue){.kind = AB_DATA_STRING,
                                     .string = level.bodies->objects[pink].name},
                       (AbDataValue){.kind = AB_DATA_STRING, .string = current}))
                ++pink;
            if (pink == level.bodies->count || !game->bubbles)
                return ab_fail(error,
                               "Birthday pink bird lost its actual effect owner");
            for (size_t phase = 0; phase < 3; ++phase) {
                AbVec2 target = ab_classic_sling_view(game->playfield.sling).band.rest;
                if (phase) {
                    target.x -= 4;
                    target.y += 3;
                }
                AbCameraViewport viewport;
                AbVec2 point;
                AbUiPointerReply reply;
                if (!ab_camera_system_query_viewport(
                        ab_classic_camera_system(game->playfield.camera), &viewport) ||
                    !ab_coordinate_point(&viewport.rendering, AB_SPACE_PHYSICS,
                                         AB_SPACE_SCREEN, target, &point) ||
                    !menu_preview_pointer(menu,
                                          menu_span(phase == 0   ? "LPRESS"
                                                    : phase == 1 ? "LHOLD"
                                                                 : "LRELEASE"),
                                          point, &reply, error) ||
                    !game_eagle_test_frame(menu, error))
                    return false;
            }
            bool charging = false, bubbling = false;
            for (size_t frame = 0; frame < 240; ++frame) {
                if (frame == 15 || frame == 16) {
                    AbUiPointerReply reply;
                    if (!menu_preview_pointer(
                            menu, menu_span(frame == 15 ? "LPRESS" : "LRELEASE"),
                            (AbVec2){700, 250}, &reply, error))
                        return false;
                }
                if (!game_eagle_test_frame(menu, error))
                    return false;
                const AbDataTable *physical = &level.properties->objects[pink].physical;
                charging |= menu_route_field(physical, "pinkTimer").kind != AB_DATA_NIL;
                if (!bubbling &&
                    menu_route_truthy(menu_route_field(physical, "inBubble"))) {
                    bubbling = true;
                    const AbExtraDrawFunction *draw = NULL;
                    if (!ab_extra_draws_find(game->extra_draws,
                                             menu_span("ingame_bubbles"), &draw,
                                             error) ||
                        draw != &game->bubbles->draw ||
                        !game_eagle_test_capture(menu, renderer, "AB_BUBBLES_CAPTURE",
                                                 error))
                        return ab_fail(error,
                                       "Pink ability lost its real bubble drawing");
                }
            }
            if (!charging || !bubbling || !level.removed_objects[pink] ||
                menu_route_field(&level.properties->objects[pink].physical,
                                 "bubbleAntiGravityTimer")
                        .kind != AB_DATA_NIL)
                return ab_fail(error, "Pink ability did not charge, lift and retire");
            if (!menu_route_truthy(hat->joint_destroyed) ||
                ab_classic_joints_metadata(level.joints, hat->joint))
                return ab_fail(error,
                               "Released birthday weld survived its deferred phase");
        } else if (scene == 1) {
            if (!game_eagle_launch(game, error) || game->eagle->bait == SIZE_MAX)
                return ab_fail(error,
                               "Birthday Eagle activation did not construct bait");
            for (size_t index = 0; index < game->hats.count; ++index) {
                AbBirdHat *hat = &game->hats.records[index].source;
                if (hat->collision.object_name.size &&
                    !menu_route_truthy(hat->joint_destroyed))
                    return ab_fail(error,
                                   "Eagle activation left an attached birthday hat");
            }
            for (size_t frame = 0; frame < 120; ++frame)
                if (!game_eagle_test_frame(menu, error))
                    return false;
        }
        game_session_free(menu);
        menu->level_request_ready = false;
    }
    menu->launch_enabled = launch;
    puts("Birthday page: all 15 actual levels, hat selection/bodies/welds, idle "
         "jump/sling transfer, mouse ability release, Eagle queue replacement "
         "and Metal drawing.");
    return true;
}

#endif
