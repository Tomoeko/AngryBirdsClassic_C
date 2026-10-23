#ifndef AB_UI_LEVEL_START_AUDIT_H
#define AB_UI_LEVEL_START_AUDIT_H

#include "ui_object_animation_apk.h"

static bool level_start_recovery_regressions(PreviewMenu *menu, AbMetal *renderer,
                                             AbError *error) {
    const char *const names[] = {"LevelP3_216", "Level745", "EggDefender_classic_1",
                                 "EggDefender_classic_2", "Level5"};
    const size_t episodes[] = {3, 8, 9, 9, 1};
    for (size_t scene = 0; scene < sizeof(episodes) / sizeof(*episodes); ++scene) {
        if (!ability_test_episode_level(menu, episodes[scene], names[scene], error))
            return false;
        PreviewGame *game = menu->game;
        if (!scene) {
            AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
            size_t object = 0;
            while (object < level.bodies->count &&
                   !menu_route_equal(
                       menu_route_field(&level.properties->objects[object].physical,
                                        "definition"),
                       menu_data("StaticFragileBlock06")))
                ++object;
            if (object == level.bodies->count)
                return ab_fail(error, "Actual empty-particle material is absent");
            uint8_t engine_before[sizeof(menu->engine_random)];
            memcpy(engine_before, &menu->engine_random, sizeof(engine_before));
            uint64_t script_before = menu->random.value;
            size_t particles =
                ab_particles_count(ab_classic_particles_engine(game->particles), false);
            if (!ab_classic_particles_legacy(
                    game->particles, level.bodies->objects[object].name, menu_data(""),
                    (AbDataValue){0}, false, false, error))
                return false;
            if (memcmp(engine_before, &menu->engine_random, sizeof(engine_before)) ||
                script_before != menu->random.value ||
                particles != ab_particles_count(
                                 ab_classic_particles_engine(game->particles), false))
                return ab_fail(error, "Empty particle material reached the engine/RNG");
        }
        for (size_t frame = 0; frame < 3; ++frame)
            if (!menu_preview_draw(menu, 1.0f / 30, error))
                return false;
        AbImage image = {0};
        float clear[4];
        menu_background_clear(menu, clear);
        bool okay = ab_metal_capture(renderer, menu->context.draw_list,
                                     (uint32_t)menu->screen.x, (uint32_t)menu->screen.y,
                                     clear, 16 * 1024 * 1024, &image, error);
        ab_image_free(&image);
        if (okay && scene == 3)
            okay = object_animation_reference(game, error);
        game_session_free(menu);
        menu->level_request_ready = false;
        if (!okay)
            return false;
    }
    puts("Recovered level startup: empty-particle material suppresses engine/RNG; "
         "large Level745, retained composite theme parts and animated objects "
         "start and draw on Metal.");
    return true;
}

/* Optional private recovery inventory. Exercise the actual application owners
 * and three ordinary frames for every catalog request, without granting
 * progression or substituting missing hooks. A successful start is not a
 * playthrough/fidelity claim. Each failed borrowing game is discarded before
 * the next request; the prepared APK resource owner remains shared. */
static bool level_start_audit(PreviewMenu *menu, AbError *error) {
    const char *path = getenv("AB_TEST_AUDIT_LEVELS");
    const char *filter = getenv("AB_TEST_AUDIT_LEVEL_NAME");
    if (!path)
        return true;
    FILE *file = fopen(path, "wx");
    if (!file)
        return ab_fail(error, "Cannot create private level-start audit");
    const AbDataTable *episodes = ab_episode_catalog_episodes(menu->catalog);
    const AbDataTable *ids = ab_episode_catalog_ids(menu->catalog);
    size_t count = 0, opened = 0, started = 0;
    bool okay = fputs("episode\tpage\tlevel\tname\tphase\tmessage\n", file) >= 0;
    int16_t pcm[512];
    game_session_free(menu);
    for (size_t id_index = 1; okay && id_index <= ids->count; ++id_index) {
        AbDataValue id = result_at(ids, id_index);
        const AbDataValue *episode = ab_data_get(episodes, id);
        if (!episode || episode->kind != AB_DATA_TABLE) {
            okay = ab_fail(error, "Audit catalog has no actual episode record");
            break;
        }
        AbDataValue pages = menu_route_field(episode->table, "pages");
        if (pages.kind != AB_DATA_TABLE) {
            okay = ab_fail(error, "Audit episode has no actual page list");
            break;
        }
        for (size_t page = 1; okay && page <= pages.table->count; ++page) {
            AbDataValue record = result_at(pages.table, page);
            if (record.kind == AB_DATA_NIL)
                break;
            AbDataValue levels = record.kind == AB_DATA_TABLE
                                     ? menu_route_field(record.table, "levels")
                                     : (AbDataValue){0};
            if (levels.kind != AB_DATA_TABLE) {
                okay = ab_fail(error, "Audit page has no actual level list");
                break;
            }
            for (size_t index = 1; okay && index <= levels.table->count; ++index) {
                AbDataValue level = result_at(levels.table, index);
                if (level.kind == AB_DATA_NIL)
                    break;
                AbDataValue name = level.kind == AB_DATA_TABLE
                                       ? menu_route_field(level.table, "name")
                                       : (AbDataValue){0};
                if (name.kind != AB_DATA_STRING || name.string.size > 128) {
                    okay = ab_fail(error, "Audit level has no actual bounded name");
                    break;
                }
                if (filter && (strlen(filter) != name.string.size ||
                               memcmp(filter, name.string.data, name.string.size)))
                    continue;
                AbDataEntry fields[] = {
                    {menu_data("episode"), id},
                    {menu_data("page"), menu_route_number((float)page)},
                    {menu_data("level"), menu_route_number((float)index)},
                    {menu_data("levelName"), name}};
                AbDataTable metadata = {fields, 4, 4, NULL};
                AbError reached = {0};
                const char *phase = "request";
                /* The audit drives audio explicitly; publish/remove ended
                 * voices even after a rejected constructor with no game frame. */
                if (!ab_audio_mixer_mix(menu->mixer, 0, NULL, 0, error)) {
                    okay = false;
                    break;
                }
                bool ready = ab_classic_level_request_resolve(
                    episodes, &metadata, &menu->level_request, &reached);
                menu->level_request_ready = ready;
                if (ready) {
                    phase = "open";
                    ready = game_session_open(menu, &reached);
                }
                if (ready) {
                    ++opened;
                    phase = "frame";
                    for (size_t frame = 0; ready && frame < 3; ++frame)
                        ready =
                            game_session_draw(menu, 1.0f / 30, &reached) &&
                            ab_audio_mixer_mix(menu->mixer, 267, pcm, 512, &reached) &&
                            ab_audio_mixer_mix(menu->mixer, frame == 2 ? 267 : 266, pcm,
                                               512, &reached);
                }
                if (ready) {
                    ++started;
                    phase = "started";
                }
                ++count;
                okay = fprintf(file, "%zu\t%zu\t%zu\t%.*s\t%s\t%s\n", id_index, page,
                               index, (int)name.string.size, name.string.data, phase,
                               reached.message) >= 0 &&
                       fflush(file) == 0;
                game_session_free(menu);
                menu->level_request_ready = false;
            }
        }
    }
    if (fclose(file))
        okay = false;
    if (!okay)
        return error && error->message[0]
                   ? false
                   : ab_fail(error, "Cannot complete private level-start audit");
    printf("Catalog startup inventory: %zu requests, %zu constructed, %zu passed "
           "three frames. This does not establish full level playthroughs.\n",
           count, opened, started);
    return true;
}

#endif
