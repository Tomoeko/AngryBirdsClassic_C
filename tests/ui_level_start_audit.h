#ifndef AB_UI_LEVEL_START_AUDIT_H
#define AB_UI_LEVEL_START_AUDIT_H

#include "ui_object_animation_apk.h"

static bool level_start_recovery_regressions(PreviewMenu *menu, AbMetal *renderer,
                                             AbError *error) {
    const char *const names[] = {"LevelP3_216",
                                 "Level745",
                                 "EggDefender_classic_1",
                                 "EggDefender_classic_2",
                                 "Level5",
                                 "Level77",
                                 "Level45",
                                 "EggDefender_classic_7",
                                 "EggDefender_classic_9",
                                 "BlackBird_6",
                                 "BlackBird_8",
                                 "BlackBird_10"};
    const size_t episodes[] = {3, 8, 9, 9, 1, 1, 1, 9, 9, 10, 10, 10};
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
        size_t frames = scene >= 5 ? 180 : 3;
        int16_t pcm[512];
        for (size_t frame = 0; frame < frames; ++frame)
            if (!menu_preview_draw(menu, 1.0f / 30, error) ||
                !ab_audio_mixer_mix(menu->mixer, 267, pcm, 512, error) ||
                !ab_audio_mixer_mix(menu->mixer, frame % 3 == 2 ? 267 : 266, pcm, 512,
                                    error))
                return false;
        if ((scene == 5 || scene == 6) && !game->shot.bird_ready)
            return ab_fail(error, "Coincident-snap scene did not finish sling reload");
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
         "start and draw on Metal; Level77/Level45 finish coincident-snap reload "
         "and run 180 physics frames.");
    return true;
}

/* Optional private recovery inventory. Exercise the actual application owners
 * for the selected ordinary frame count at every catalog request, without granting
 * progression or substituting missing hooks. A successful start is not a
 * playthrough/fidelity claim. Each failed borrowing game is discarded before
 * the next request; the prepared APK resource owner remains shared. */
static bool level_audit_bound(const char *name, size_t minimum, size_t maximum,
                              size_t *output, AbError *error) {
    const char *value = getenv(name);
    if (!value)
        return true;
    char *end = NULL;
    unsigned long selected = strtoul(value, &end, 10);
    if (!value[0] || !end || *end || selected < minimum || selected > maximum)
        return ab_fail(error, "Invalid %s audit bound", name);
    *output = (size_t)selected;
    return true;
}

static bool level_start_audit(PreviewMenu *menu, AbError *error) {
    const char *path = getenv("AB_TEST_AUDIT_LEVELS");
    const char *filter = getenv("AB_TEST_AUDIT_LEVEL_NAME");
    bool replay_enabled = getenv("AB_TEST_AUDIT_ABILITIES") != NULL;
    if (!path)
        return true;
    size_t frames = 3;
    const AbDataTable *ids = ab_episode_catalog_ids(menu->catalog);
    size_t first_episode = 1, last_episode = ids->count;
    if (!level_audit_bound("AB_TEST_AUDIT_FRAMES", 1, 10000, &frames, error) ||
        !level_audit_bound("AB_TEST_AUDIT_FIRST_EPISODE", 1, ids->count, &first_episode,
                           error) ||
        !level_audit_bound("AB_TEST_AUDIT_LAST_EPISODE", first_episode, ids->count,
                           &last_episode, error))
        return false;
    FILE *file = fopen(path, "wx");
    if (!file)
        return ab_fail(error, "Cannot create private level-start audit");
    const AbDataTable *episodes = ab_episode_catalog_episodes(menu->catalog);
    size_t count = 0, opened = 0, started = 0;
    bool okay =
        fputs(replay_enabled ? "episode\tpage\tlevel\tname\tphase\tmessage\tframes\tsee"
                               "d\tshots\tability_inputs\tended\tscene\n"
                             : "episode\tpage\tlevel\tname\tphase\tmessage\tframes\n",
              file) >= 0;
    int16_t pcm[512];
    game_session_free(menu);
    for (size_t id_index = first_episode; okay && id_index <= last_episode;
         ++id_index) {
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
                size_t advanced = 0;
                PhysicsReplayInput replay = {0};
                uint32_t seed = UINT32_C(0x52000000) ^ ((uint32_t)id_index << 16) ^
                                ((uint32_t)page << 8) ^ (uint32_t)index;
                if (replay_enabled &&
                    (!ab_random48_seed(&menu->random, seed) ||
                     !ab_engine_random_seed(&menu->engine_random, seed))) {
                    okay = ab_fail(error, "Cannot seed the private physics audit");
                    break;
                }
                /* The audit drives audio explicitly; publish/remove ended
                 * voices even after a rejected constructor with no game frame. */
                if (!ab_audio_mixer_mix(menu->mixer, 0, NULL, 0, error)) {
                    okay = false;
                    break;
                }
                bool ready = ab_classic_level_request_resolve(
                    episodes, &metadata, &menu->level_request, &reached);
                menu->level_request_ready = ready;
                bool gui = menu_minigame_name(name);
                bool route_enabled = menu->route_enabled;
                if (gui)
                    menu->route_enabled = true;
                if (ready) {
                    phase = "open";
                    ready = gui ? menu_minigame_open(menu, &reached)
                                : game_session_open(menu, &reached);
                }
                if (ready) {
                    ++opened;
                    phase = "frame";
                    for (size_t frame = 0; ready && frame < frames; ++frame) {
                        ready =
                            (!replay_enabled || !menu->game ||
                             physics_replay_input(menu, &replay, &reached)) &&
                            (replay_enabled || gui
                                 ? menu_preview_draw(menu, 1.0f / 30, &reached)
                                 : game_session_draw(menu, 1.0f / 30, &reached)) &&
                            ab_audio_mixer_mix(menu->mixer, 267, pcm, 512, &reached) &&
                            ab_audio_mixer_mix(menu->mixer, frame % 3 == 2 ? 267 : 266,
                                               pcm, 512, &reached);
                        if (ready)
                            ++advanced;
                    }
                }
                if (ready) {
                    ++started;
                    phase = "started";
                }
                ++count;
                okay = fprintf(file, "%zu\t%zu\t%zu\t%.*s\t%s\t%s\t%zu", id_index, page,
                               index, (int)name.string.size, name.string.data, phase,
                               reached.message, advanced) >= 0;
                if (okay && replay_enabled)
                    okay = fprintf(file, "\t%08x\t%.0f\t%zu\t%u\t%s", seed,
                                   menu->game ? (double)menu->game->shot.birds_shot : 0,
                                   replay.ability_attempts,
                                   menu->game && menu->game->ended,
                                   gui ? "gui" : "physics") >= 0;
                okay = okay && fputc('\n', file) != EOF && fflush(file) == 0;
                game_session_free(menu);
                if (menu->minigame && menu->loader) {
                    ab_classic_loader_close(menu->loader);
                    menu->loader = NULL;
                }
                if (!menu_minigame_close(menu, error))
                    okay = false;
                menu->route_enabled = route_enabled;
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
    printf("Catalog physics inventory: %zu requests, %zu constructed, %zu passed "
           "%zu ordinary frames. This does not establish full level playthroughs.\n",
           count, opened, started, frames);
    return true;
}

#endif
