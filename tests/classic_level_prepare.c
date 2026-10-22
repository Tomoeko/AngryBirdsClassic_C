#include "ab/classic_level_prepare.h"
#include "ab/episode_catalog.h"
#include "ab/target_data.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    AbClassicLevelPrepare *prepare;
    AbClassicLevelPrepareState state;
    AbSettingsStatistics *settings, *local;
    AbEpisodePreferences preferences;
    AbEpisodeStatistics statistics;
    AbEventManager *events;
    AbClassicLoader *loader;
    AbClassicLoaderState loader_state;
    AbClassicLoaderRequest request;
    AbClassicLoaderCall start, before;
    AbDataValue time, skip, do_loading, show, hide, gift;
    AbGiftBoxSession gift_session;
    AbDataValue remote_config;
    AbEventListener listener;
    const AbDataTable *ids;
    AbDataTable *started[4], *loading[4];
    size_t starts, loads, releases, before_calls, observed;
    unsigned phase;
    bool editing, physics;
} PrepareFixture;

static AbSpan span(const char *name) {
    return (AbSpan){(const uint8_t *)name, strlen(name)};
}

static AbDataValue text(const char *name) {
    return (AbDataValue){.kind = AB_DATA_STRING, .string = span(name)};
}

static AbDataValue number(float value) {
    AbDataValue result = {.kind = AB_DATA_NUMBER};
    memcpy(&result.bits, &value, sizeof(value));
    return result;
}

static AbDataValue get(const AbDataTable *table, AbDataValue key) {
    const AbDataValue *value = ab_data_get(table, key);
    return value ? *value : (AbDataValue){0};
}

static AbDataValue field(const AbDataTable *table, const char *name) {
    return get(table, text(name));
}

static bool equal(AbDataValue left, AbDataValue right) {
    AbDataEntry entry = {left, {.kind = AB_DATA_BOOLEAN}};
    AbDataTable table = {&entry, 1, 1, NULL};
    return ab_data_get(&table, right) != NULL;
}

static bool release_assets(void *context, const AbDataTable *groups, AbError *error) {
    PrepareFixture *test = context;
    if (test->phase || !equal(get(groups, number(1)), text("LEVELSELECTION")) ||
        !equal(get(groups, number(2)), text("BACKGROUNDS")))
        return ab_fail(error, "Prepared release order/groups differ");
    ++test->releases;
    test->phase = 1;
    return true;
}

static bool before_call(void *context, AbError *error) {
    PrepareFixture *test = context;
    if (test->phase != 1)
        return ab_fail(error, "Before-load callback order differs");
    test->state.level_name = text("Level3");
    ++test->before_calls;
    test->phase = 2;
    return true;
}

static bool before_load(void *context, AbDataValue method, AbError *error) {
    PrepareFixture *test = context;
    return method.kind == AB_DATA_OBJECT && method.object == &test->before
               ? test->before.call(test->before.context, error)
               : ab_fail(error, "Before-load method identity differs");
}

static bool load_internal(void *context, AbDataValue path, AbError *error) {
    PrepareFixture *test = context;
    const char *expected =
        test->loads == 1 ? "/levels/pack1/Level3" : "/levels/pack1/Level1";
    if ((test->phase != 1 && test->phase != 2) || !equal(path, text(expected)))
        return ab_fail(error, "Prepared load did not reread its live path");
    ++test->loads;
    test->phase = 3;
    return true;
}

static bool start_level(void *context, AbError *error) {
    PrepareFixture *test = context;
    if (test->phase != 3)
        return ab_fail(error, "Loader completed before level load returned");
    ++test->starts;
    test->phase = 0;
    return true;
}

static bool observe(void *context, AbDataTable *event, AbError *error) {
    PrepareFixture *test = context;
    const char *const sequence[] = {"EID_LEVEL_STARTED", "EID_LEVEL_LOADING_PREINIT",
                                    "EID_LEVEL_LOADING_INIT", "EID_DO_LOADING"};
    size_t step = test->observed % 4;
    if (!equal(field(event, "id"), field(test->ids, sequence[step])))
        return ab_fail(error, "Level preparation notification order differs");
    if (!step) {
        size_t index = test->observed / 4;
        if (index >= 4 || test->editing || test->physics ||
            !equal(field(event, "attempt"), number(index == 2 ? 2 : 1)) ||
            !equal(field(event, "firstTime"),
                   (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = index != 2}) ||
            !equal(field(event, "previousStars"), number(index == 1 ? 0 : 3)) ||
            (index != 1 && field(event, "from").kind != AB_DATA_NIL) ||
            (index == 1 && !equal(field(event, "from"), text("fixture"))))
            return ab_fail(error, "Started-event attempts/stars/source fields differ");
        test->started[index] = event;
    }
    if (step == 3) {
        size_t index = test->observed / 4;
        test->loading[index] = event;
        if (!ab_classic_level_prepare_loader(test->prepare, event, &test->request,
                                             error) ||
            test->request.completion_call != &test->start ||
            !ab_classic_loader_event(test->loader, &test->request, error))
            return false;
    }
    ++test->observed;
    return true;
}

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Prepare check failed at %d: %s (%s)\n", __LINE__,         \
                    #condition, error.message);                                        \
            goto release;                                                              \
        }                                                                              \
    } while (0)

static bool check(const char *path) {
    PrepareFixture test = {0};
    AbBuffer bytes = {0};
    AbZip archive = {0};
    AbEpisodeCatalog *catalog = NULL;
    AbTargetData events = {0}, stars = {0};
    AbError error = {0};
    uint8_t key[32] = {0};
    bool okay = false;
    CHECK(ab_file_read(path, 64 * 1024 * 1024, &bytes, &error) &&
          ab_zip_open((AbSpan){bytes.data, bytes.size}, &archive, &error) &&
          ab_target_resource_key(&archive, 16 * 1024 * 1024, key, &error) &&
          ab_episode_catalog_open(
              &archive, (AbSpan){key, sizeof(key)},
              (AbEpisodeCatalogConfig){.device_model = span("android"),
                                       .disable_external_links = true},
              2 * 1024 * 1024, &catalog, &error) &&
          ab_episode_catalog_annotate_levels(catalog, &error) &&
          ab_target_data_open(&archive, "assets/data/scripts_common/events.lua",
                              (AbSpan){key, sizeof(key)}, 1024 * 1024, NULL, &events,
                              &error) &&
          ab_target_data_open(&archive, "assets/data/scripts/starLimits.lua",
                              (AbSpan){key, sizeof(key)}, 3 * 1024 * 1024, NULL, &stars,
                              &error));
    test.ids = events.data.globals;
    test.do_loading = field(test.ids, "EID_DO_LOADING");
    test.show = field(test.ids, "EID_SHOW_LOADING_PAGE");
    test.hide = field(test.ids, "EID_HIDE_LOADING_PAGE");
    test.start = (AbClassicLoaderCall){&test, start_level};
    test.before = (AbClassicLoaderCall){&test, before_call};
    test.time = number(0);
    const AbDataTable *episodes = ab_episode_catalog_episodes(catalog);
    AbDataValue thresholds = field(stars.data.globals, "Level1");
    CHECK(thresholds.kind == AB_DATA_TABLE);
    AbDataEntry score_fields[] = {
        {text("score"), field(thresholds.table, "goldScore")},
        {text("completed"), {.kind = AB_DATA_BOOLEAN, .bits = 1}}};
    AbDataTable score = {score_fields, 2, 2, NULL};
    AbDataEntry highscore = {text("Level1"), {.kind = AB_DATA_TABLE, .table = &score}};
    AbDataTable highscores = {&highscore, 1, 1, NULL};
    test.statistics = (AbEpisodeStatistics){episodes, ab_episode_catalog_ids(catalog),
                                            &highscores, stars.data.globals};
    AbSettingsStatisticsConfig storage = {span("fixture"), span("fixture"), 16, 32,
                                          1024 * 1024};
    CHECK(
        ab_settings_statistics_open(storage, &test.settings, &error) &&
        ab_settings_statistics_open(storage, &test.local, &error) &&
        ab_episode_preferences_init(&test.preferences, test.local, episodes, &error) &&
        ab_giftbox_session_initialize(&test.gift_session, test.settings,
                                      &test.remote_config, (AbSessionClock){0},
                                      &error) &&
        ab_event_manager_open((AbEventManagerLimits){8, 4, 4, 4, 8, 65536},
                              (AbEventManagerHost){0}, &test.events, &error));
    AbClassicLevelPrepareConfig config = {.episodes = episodes,
                                          .event_ids = test.ids,
                                          .statistics = &test.statistics,
                                          .settings = test.settings,
                                          .preferences = &test.preferences,
                                          .events = test.events,
                                          .state = &test.state,
                                          .editing = &test.editing,
                                          .physics_enabled = &test.physics,
                                          .giftbox_enabled = &test.gift,
                                          .giftbox = &test.gift_session,
                                          .start_level = &test.start,
                                          .requests = 4,
                                          .storage_bytes = 128 * 1024,
                                          .context = &test,
                                          .release_assets = release_assets,
                                          .before_load = before_load,
                                          .load_level_internal = load_internal};
    CHECK(ab_classic_level_prepare_open(config, &test.prepare, &error) &&
          ab_classic_loader_open((AbClassicLoaderConfig){.state = &test.loader_state,
                                                         .script_time = &test.time,
                                                         .do_loading = &test.do_loading,
                                                         .show_loading = &test.show,
                                                         .hide_loading = &test.hide,
                                                         .skip_input = &test.skip,
                                                         .events = test.events,
                                                         .notifications = 8,
                                                         .storage_bytes = 8192},
                                 &test.loader, &error));
    test.loader_state.frame = number(0);
    test.listener = (AbEventListener){.context = &test, .event_triggered = observe};
    const char *const names[] = {"EID_LEVEL_STARTED", "EID_LEVEL_LOADING_PREINIT",
                                 "EID_LEVEL_LOADING_INIT", "EID_DO_LOADING"};
    for (size_t index = 0; index < 4; ++index)
        CHECK(ab_event_manager_add(test.events, field(test.ids, names[index]),
                                   &test.listener, &error));
    AbDataEntry fields[3][6];
    AbDataTable metadata[3];
    AbDataValue episode = get(episodes, number(1));
    AbDataValue page = get(field(episode.table, "pages").table, number(1));
    const AbDataTable *levels = field(page.table, "levels").table;
    for (size_t index = 0; index < 3; ++index) {
        fields[index][0] = (AbDataEntry){text("episode"), number(1)};
        fields[index][1] = (AbDataEntry){text("page"), number(1)};
        fields[index][2] = (AbDataEntry){text("level"), number(index == 1 ? 2 : 1)};
        AbDataValue level = get(levels, number(index == 1 ? 2 : 1));
        fields[index][3] = (AbDataEntry){text("levelName"), field(level.table, "name")};
        metadata[index] = (AbDataTable){fields[index], 4, 6, NULL};
    }
    fields[1][4] = (AbDataEntry){text("doNotResetAttemptCounter"), number(0)};
    fields[1][5] = (AbDataEntry){text("onBeforeLoadLevelInternal"),
                                 {.kind = AB_DATA_OBJECT, .object = &test.before}};
    metadata[1].count = 6;
    for (size_t index = 0; index < 3; ++index) {
        test.editing = test.physics = true;
        if (index == 1) {
            test.gift = (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1};
            test.state.number_of_attempts = number(99);
        }
        CHECK(
            ab_classic_level_prepare(test.prepare, metadata + index,
                                     index == 1 ? text("fixture") : (AbDataValue){0},
                                     index == 1 ? number(0) : (AbDataValue){0},
                                     &error) &&
            !test.editing && !test.physics && test.observed == (index + 1) * 4 &&
            !test.phase &&
            equal(test.state.number_of_attempts, number(index == 1 ? 99 : 1)) &&
            equal(field(ab_settings_statistics_root(test.settings), "lastPlayedLevel"),
                  field(metadata + index, "levelName")));
        for (size_t frame = 0; frame < 8 && test.starts == index; ++frame) {
            test.time = number((float)(frame + 1));
            CHECK(ab_classic_loader_update(test.loader, &error));
        }
        CHECK(test.starts == index + 1 && test.loads == index + 1 && !test.phase);
    }
    CHECK(test.releases == 3 && test.before_calls == 1 &&
          equal(field(ab_settings_statistics_root(test.settings),
                      "levelTryAmountForCurrentGiftBox"),
                number(2)) &&
          test.started[0] != test.started[1] && test.started[0] != test.started[2] &&
          test.loading[0] != test.loading[1] &&
          field(test.loading[0], "min_display_time").kind == AB_DATA_NIL &&
          equal(field(test.loading[1], "min_display_time"), number(0)));
    AbClassicLoaderRequest untouched = {.id = number(99)};
    AbDataTable foreign = {0};
    CHECK(!ab_classic_level_prepare_loader(test.prepare, &foreign, &untouched, NULL) &&
          untouched.id.bits == number(99).bits);
    ab_classic_loader_close(test.loader);
    test.loader = NULL;
    ab_classic_level_prepare_close(test.prepare);
    test.prepare = NULL;
    config.giftbox = NULL;
    CHECK(ab_classic_level_prepare_open(config, &test.prepare, &error) &&
          !ab_classic_level_prepare(test.prepare, metadata, (AbDataValue){0},
                                    (AbDataValue){0}, NULL) &&
          !ab_classic_level_prepare(test.prepare, metadata, (AbDataValue){0},
                                    (AbDataValue){0}, NULL) &&
          test.observed == 12);
    float count = 0;
    CHECK(ab_settings_statistics_level_started_count(test.settings, span("Level1"),
                                                     &count, &error) &&
          count == 3);
    puts("Actual catalog preparation: canonical globals, attempt/page settings, "
         "real event/Loader composition, prior stars, false/nil/zero fields, "
         "live before-load path and missing-service prefix retention. "
         "Services are controlled fixtures; complete native prepare parity remains "
         "open.");
    okay = true;
release:
    if (!okay && !error.message[0])
        fputs("Level preparation fixture differs\n", stderr);
    ab_classic_loader_close(test.loader);
    ab_classic_level_prepare_close(test.prepare);
    ab_event_manager_close(test.events);
    ab_settings_statistics_close(test.local);
    ab_settings_statistics_close(test.settings);
    ab_target_data_close(&stars);
    ab_target_data_close(&events);
    ab_episode_catalog_close(catalog);
    ab_zip_close(&archive);
    ab_buffer_free(&bytes);
    memset(key, 0, sizeof(key));
    return okay;
}

int main(int argc, char **argv) {
    return argc == 2 ? (check(argv[1]) ? 0 : 1) : 2;
}
