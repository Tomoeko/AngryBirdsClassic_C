#include "ab/ui_episode_services.h"

static bool identifier(AbUiPointerValue input, AbDataValue *output, AbError *error) {
    if (input.kind == AB_UI_POINTER_NUMBER) {
        *output = (AbDataValue){.kind = AB_DATA_NUMBER, .bits = input.number_bits};
        return true;
    }
    if (input.kind == AB_UI_POINTER_TEXT) {
        *output = (AbDataValue){.kind = AB_DATA_STRING, .string = input.text};
        return true;
    }
    if (input.kind == AB_UI_POINTER_NONE) {
        *output = (AbDataValue){0};
        return true;
    }
    if (input.kind == AB_UI_POINTER_BOOLEAN) {
        *output = (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = input.boolean};
        return true;
    }
    return ab_fail(error, "Episode UI services require a scalar identifier");
}

static bool stars(void *context, AbUiPointerValue episode, float *earned,
                  float *available, AbError *error) {
    AbUiEpisodeServices *services = context;
    AbDataValue key;
    return identifier(episode, &key, error) &&
           ab_episode_statistics_stars(services->statistics, key, false, earned,
                                       available, error);
}

static bool score(void *context, AbUiPointerValue episode, float *output,
                  AbError *error) {
    AbUiEpisodeServices *services = context;
    AbDataValue key;
    return identifier(episode, &key, error) &&
           ab_episode_statistics_score(services->statistics, key, output, error);
}

static bool feathers(void *context, AbUiPointerValue episode, float *earned,
                     float *available, AbError *error) {
    AbUiEpisodeServices *services = context;
    AbDataValue key;
    return identifier(episode, &key, error) &&
           ab_episode_statistics_feathers(services->statistics, key, earned, available,
                                          error);
}

static bool complete(void *context, AbUiPointerValue episode, bool *output,
                     AbError *error) {
    AbUiEpisodeServices *services = context;
    AbDataValue key;
    return identifier(episode, &key, error) &&
           ab_episode_statistics_complete(services->statistics, key, output, error);
}

static bool eagle(void *context, AbUiPointerValue episode, bool *output,
                  AbError *error) {
    (void)episode; /* The target default_conf helper does not read its argument. */
    AbUiEpisodeServices *services = context;
    return ab_episode_progress_show_eagle(services->progress, output, error);
}

static bool change_seen(void *context, AbUiPointerValue episode, bool *output,
                        AbError *error) {
    AbUiEpisodeServices *services = context;
    AbDataValue key;
    return identifier(episode, &key, error) &&
           ab_episode_progress_change_seen(services->progress, key, output, error);
}

static bool has_new_levels(void *context, AbUiPointerValue episode, bool *output,
                           AbError *error) {
    AbUiEpisodeServices *services = context;
    AbDataValue key;
    return identifier(episode, &key, error) &&
           ab_episode_progress_has_new_levels(services->progress, key, output, error);
}

static bool new_page(void *context, AbUiPointerValue episode, AbUiPointerValue *output,
                     AbError *error) {
    AbUiEpisodeServices *services = context;
    AbDataValue key, result;
    if (!identifier(episode, &key, error) ||
        !ab_episode_progress_new_page(services->progress, key, &result, error))
        return false;
    if (result.kind == AB_DATA_NIL)
        *output = (AbUiPointerValue){0};
    else if (result.kind == AB_DATA_NUMBER)
        *output = (AbUiPointerValue){.kind = AB_UI_POINTER_NUMBER,
                                     .number_bits = result.bits};
    else
        return ab_fail(error, "Episode new-page result is not numeric or nil");
    return true;
}

static bool visited(void *context, AbUiPointerValue episode, AbUiPointerValue page,
                    bool *output, AbError *error) {
    AbUiEpisodeServices *services = context;
    AbDataValue key, page_key;
    return identifier(episode, &key, error) && identifier(page, &page_key, error) &&
           ab_episode_progress_page_visited(services->progress, key, page_key, output,
                                            error);
}

AbUiEpisodeButtonHost ab_ui_episode_services_host(AbUiEpisodeServices *services) {
    if (!services)
        return (AbUiEpisodeButtonHost){0};
    return (AbUiEpisodeButtonHost){.episodes = services && services->statistics
                                                   ? services->statistics->episodes
                                                   : NULL,
                                   .context = services,
                                   .stars = stars,
                                   .score = score,
                                   .feathers = feathers,
                                   .show_eagle = eagle,
                                   .complete = complete,
                                   .newest_change_seen = change_seen,
                                   .has_new_levels = has_new_levels,
                                   .new_levels_page = new_page,
                                   .page_visited = visited};
}
