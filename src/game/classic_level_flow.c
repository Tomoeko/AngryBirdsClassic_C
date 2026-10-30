#include "ab/classic_level_flow.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    AbDataTable table;
    AbDataEntry entry;
} FlowNotice;

struct AbClassicLevelFlow {
    AbClassicLevelFlowConfig config;
    AbClassicLevelFlowRequest *requests;
    AbClassicLevelFlowQueue *queues;
    AbClassicLevelFlowRequest **queue_items;
    FlowNotice *notices;
    AbClassicLevelFlowQueue *staging, *initialization, *ingame;
    AbClassicLevelFlowRequest *showing;
    AbDataValue done;
    AbEventListener listener;
    size_t request_count, queue_count, notice_count, depth, work, bytes;
    bool initialized, updating, failed, subscribed;
};

static bool same(AbSpan first, AbSpan second) {
    return first.size == second.size &&
           (!first.size || (first.data && second.data &&
                            !memcmp(first.data, second.data, first.size)));
}

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL && (value.kind != AB_DATA_BOOLEAN || value.bits);
}

static bool same_id(AbDataValue first, AbDataValue second) {
    return first.kind == AB_DATA_STRING && second.kind == AB_DATA_STRING &&
           same(first.string, second.string);
}

static bool enter(AbClassicLevelFlow *flow, bool initializing, AbError *error) {
    if (!flow || flow->failed || (!initializing && !flow->initialized) ||
        flow->depth == flow->config.depth_limit)
        return ab_fail(error, "Invalid or exhausted level-start flow invocation");
    if (!flow->depth)
        flow->work = 0;
    ++flow->depth;
    return true;
}

static bool leave(AbClassicLevelFlow *flow, bool okay) {
    --flow->depth;
    flow->failed |= !okay;
    return okay;
}

static bool work(AbClassicLevelFlow *flow, AbError *error) {
    if (flow->work == flow->config.work_limit)
        return ab_fail(error, "Level-start flow work limit exhausted");
    ++flow->work;
    return true;
}

static bool fresh_queue(AbClassicLevelFlow *flow, AbClassicLevelFlowQueue **slot,
                        AbError *error) {
    if (flow->queue_count == flow->config.queue_generations)
        return ab_fail(error, "Level-start flow queue generations exhausted");
    AbClassicLevelFlowQueue *queue = flow->queues + flow->queue_count;
    queue->items = flow->queue_items + flow->queue_count * flow->config.requests;
    ++flow->queue_count;
    *slot = queue;
    return true;
}

static bool append(AbClassicLevelFlow *flow, AbClassicLevelFlowQueue *queue,
                   AbClassicLevelFlowRequest *request, AbError *error) {
    if (!queue || queue->count == flow->config.requests)
        return ab_fail(error, "Level-start flow queue capacity exhausted");
    queue->items[queue->count++] = request;
    return true;
}

static bool diagnostic(AbClassicLevelFlow *flow, AbClassicLevelFlowDiagnostic kind,
                       AbSpan name, AbError *error) {
    if (name.size && !name.data)
        return ab_fail(error, "Invalid level-start request name");
    return !flow->config.diagnostic ||
           flow->config.diagnostic(flow->config.context, kind, name, error);
}

static bool ingame(AbClassicLevelFlow *flow, bool *output, AbError *error) {
    AbDataValue result;
    if (!ab_classic_level_start_ingame(flow->config.menus, flow->config.notifications,
                                       &result, error))
        return false;
    *output = truthy(result);
    return true;
}

static bool show(AbClassicLevelFlow *flow, AbClassicLevelFlowRequest *request,
                 AbError *error) {
    flow->showing = request;
    AbClassicLevelFlowListener *listener = request ? request->listener : NULL;
    return listener && listener->on_show
               ? listener->on_show(listener->context, error)
               : ab_fail(error, "Missing reached level-start onShow method");
}

static bool update_showing(AbClassicLevelFlow *flow, AbError *error) {
    if (!flow->showing)
        return true;
    AbClassicLevelFlowListener *listener = flow->showing->listener;
    AbDataValue done;
    if (!listener || !listener->is_done)
        return ab_fail(error, "Missing reached level-start isDone method");
    if (!listener->is_done(listener->context, &done, error))
        return false;
    bool paused, in_game;
    if (truthy(done)) {
        if (!ingame(flow, &in_game, error))
            return false;
        paused = !in_game;
        flow->showing = NULL;
    } else {
        if (!flow->showing || !isfinite(flow->showing->appearing))
            return ab_fail(error, "Invalid reread of active level-start request");
        if (flow->showing->appearing < 40)
            paused = true;
        else {
            if (!ingame(flow, &in_game, error))
                return false;
            paused = !in_game;
        }
    }
    return flow->config.set_paused
               ? flow->config.set_paused(flow->config.context, paused, error)
               : ab_fail(error, "Missing reached level-start pause owner");
}

static bool remove_request(AbClassicLevelFlowQueue *queue, size_t index,
                           AbError *error) {
    if (!queue || index >= queue->count)
        return ab_fail(error, "Invalid reached level-start queue index");
    memmove(queue->items + index, queue->items + index + 1,
            (queue->count - index - 1) * sizeof(*queue->items));
    --queue->count;
    queue->items[queue->count] = NULL;
    return true;
}

static bool process_init(AbClassicLevelFlow *flow, float phase, AbError *error) {
    if (!enter(flow, false, error))
        return false;
    bool okay = true;
    if (!flow->showing) {
        AbClassicLevelFlowQueue *captured = flow->initialization;
        for (size_t index = 0; okay && index < captured->count; ++index) {
            AbClassicLevelFlowRequest *request = captured->items[index];
            if (!work(flow, error)) {
                okay = false;
                break;
            }
            if (!request || !isfinite(request->appearing)) {
                okay = ab_fail(error, "Invalid initialization request iteration");
                break;
            }
            if (phase < request->appearing)
                break;
            if (request->appearing != phase)
                continue;
            AbClassicLevelFlowQueue *current = flow->initialization;
            if (index >= current->count || !current->items[index]) {
                okay = ab_fail(error, "Missing live initialization request");
                break;
            }
            request = current->items[index];
            okay = diagnostic(flow, AB_LEVEL_FLOW_CONSUME_INIT, request->name, error) &&
                   remove_request(flow->initialization, index, error) &&
                   show(flow, request, error) && update_showing(flow, error);
            if (okay && !flow->showing)
                okay = process_init(flow, phase, error);
        }
    }
    return leave(flow, okay);
}

static void swap(AbClassicLevelFlowRequest **first,
                 AbClassicLevelFlowRequest **second) {
    AbClassicLevelFlowRequest *value = *first;
    *first = *second;
    *second = value;
}

/* Native comparison preserves equal-key exchanges as well as numeric ordering;
 * a stable sort would change which listener is shown next. */
static bool sort_requests(AbClassicLevelFlow *flow, AbClassicLevelFlowQueue *queue,
                          size_t left, size_t right, AbError *error) {
    if (left >= right)
        return true;
    if (!work(flow, error))
        return false;
    AbClassicLevelFlowRequest **items = queue->items;
    if (items[right]->appearing < items[left]->appearing)
        swap(items + left, items + right);
    if (right - left == 1)
        return true;
    size_t middle = left + (right - left) / 2;
    if (items[middle]->appearing < items[left]->appearing)
        swap(items + middle, items + left);
    else if (items[right]->appearing < items[middle]->appearing)
        swap(items + middle, items + right);
    if (right - left == 2)
        return true;
    swap(items + middle, items + right - 1);
    AbClassicLevelFlowRequest *pivot = items[right - 1];
    size_t first = left, last = right - 1;
    for (;;) {
        do {
            ++first;
        } while (items[first]->appearing < pivot->appearing);
        do {
            --last;
        } while (pivot->appearing < items[last]->appearing);
        if (last < first)
            break;
        swap(items + first, items + last);
    }
    swap(items + first, items + right - 1);
    return (!first || sort_requests(flow, queue, left, first - 1, error)) &&
           sort_requests(flow, queue, first + 1, right, error);
}

static bool process_ingame(AbClassicLevelFlow *flow, AbError *error) {
    AbClassicLevelFlowQueue *queue = flow->ingame;
    for (size_t index = 0; index < queue->count; ++index)
        if (!queue->items[index] || !isfinite(queue->items[index]->appearing))
            return ab_fail(error, "Invalid reached ingame request");
    if (queue->count > 1 && !sort_requests(flow, queue, 0, queue->count - 1, error))
        return false;
    if (flow->ingame->count) {
        AbClassicLevelFlowRequest *request = flow->ingame->items[0];
        return diagnostic(flow, AB_LEVEL_FLOW_CONSUME_INGAME, request->name, error) &&
               remove_request(flow->ingame, 0, error) && show(flow, request, error);
    }
    if (truthy(flow->done) || flow->staging->count)
        return true;
    flow->done = (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1};
    if (flow->notice_count == flow->config.notifications_count)
        return ab_fail(error, "Level-start flow notification storage exhausted");
    FlowNotice *notice = flow->notices + flow->notice_count++;
    notice->entry =
        (AbDataEntry){{.kind = AB_DATA_STRING, .string = {(const uint8_t *)"id", 2}},
                      flow->config.flow_done};
    notice->table = (AbDataTable){&notice->entry, 1, 1, NULL};
    return ab_event_manager_notify(flow->config.events, &notice->table, error);
}

static bool move_staging(AbClassicLevelFlow *flow, AbError *error) {
    AbClassicLevelFlowQueue *captured = flow->staging;
    for (size_t index = 0; index < captured->count; ++index) {
        AbClassicLevelFlowRequest *request = captured->items[index];
        if (!work(flow, error))
            return false;
        if (!request || !isfinite(request->appearing))
            return ab_fail(error, "Invalid reached staged request");
        AbClassicLevelFlowQueue *target =
            request->appearing <= 20 ? flow->initialization : flow->ingame;
        bool exists = false;
        for (size_t item = 0; item < target->count && !exists; ++item) {
            if (!work(flow, error))
                return false;
            exists = same(target->items[item]->name, request->name);
        }
        if (!exists && !append(flow, target, request, error))
            return false;
    }
    return fresh_queue(flow, &flow->staging, error);
}

static bool event_triggered(void *context, AbDataTable *event, AbError *error) {
    AbClassicLevelFlow *flow = context;
    if (!enter(flow, false, error))
        return false;
    const AbDataValue *id =
        ab_data_get(event, (AbDataValue){.kind = AB_DATA_STRING,
                                         .string = {(const uint8_t *)"id", 2}});
    bool okay = true;
    if (id && same_id(*id, flow->config.loading_init)) {
        okay = fresh_queue(flow, &flow->ingame, error);
        if (okay) {
            flow->showing = NULL;
            flow->done = (AbDataValue){.kind = AB_DATA_BOOLEAN};
            okay = process_init(flow, 10, error);
        }
    } else if (id && same_id(*id, flow->config.loading_done))
        okay = process_init(flow, 20, error);
    return leave(flow, okay);
}

bool ab_classic_level_flow_open(AbClassicLevelFlowConfig config,
                                AbClassicLevelFlow **output, AbError *error) {
    if (!output)
        return ab_fail(error, "Missing level-start flow output");
    *output = NULL;
    if (!config.events || config.loading_init.kind != AB_DATA_STRING ||
        config.loading_done.kind != AB_DATA_STRING ||
        config.flow_done.kind != AB_DATA_STRING || config.depth_limit < 2 ||
        config.depth_limit > 64 || !config.work_limit || !config.requests ||
        config.requests > 1024 || config.queue_generations < 3 ||
        config.queue_generations > 65536 || config.notifications_count > 65536)
        return ab_fail(error, "Invalid level-start flow bindings or limits");
    size_t bytes = sizeof(AbClassicLevelFlow) +
                   config.requests * sizeof(AbClassicLevelFlowRequest) +
                   config.queue_generations * sizeof(AbClassicLevelFlowQueue) +
                   config.queue_generations * config.requests *
                       sizeof(AbClassicLevelFlowRequest *) +
                   config.notifications_count * sizeof(FlowNotice);
    if (bytes > config.storage_bytes)
        return ab_fail(error, "Level-start flow exceeds storage limit");
    AbClassicLevelFlow *flow = calloc(1, sizeof(*flow));
    if (!flow)
        return ab_fail(error, "Cannot allocate level-start flow owner");
    flow->config = config;
    flow->requests = calloc(config.requests, sizeof(*flow->requests));
    flow->queues = calloc(config.queue_generations, sizeof(*flow->queues));
    flow->queue_items =
        calloc(config.queue_generations * config.requests, sizeof(*flow->queue_items));
    if (config.notifications_count)
        flow->notices = calloc(config.notifications_count, sizeof(*flow->notices));
    if (!flow->requests || !flow->queues || !flow->queue_items ||
        (config.notifications_count && !flow->notices)) {
        ab_classic_level_flow_close(flow);
        return ab_fail(error, "Cannot allocate bounded level-start flow storage");
    }
    flow->listener =
        (AbEventListener){.name = {(const uint8_t *)"LevelStartScreenFlow", 20},
                          .context = flow,
                          .event_triggered = event_triggered};
    flow->bytes = bytes;
    *output = flow;
    return true;
}

void ab_classic_level_flow_close(AbClassicLevelFlow *flow) {
    if (flow) {
        if (flow->subscribed) {
            (void)ab_event_manager_remove(
                flow->config.events, flow->config.loading_init, &flow->listener, NULL);
            (void)ab_event_manager_remove(
                flow->config.events, flow->config.loading_done, &flow->listener, NULL);
        }
        free(flow->requests);
        free(flow->queues);
        free(flow->queue_items);
        free(flow->notices);
        free(flow);
    }
}

size_t ab_classic_level_flow_bytes(const AbClassicLevelFlow *flow) {
    return flow ? flow->bytes : 0;
}

AbClassicLevelFlowView ab_classic_level_flow_view(AbClassicLevelFlow *flow) {
    AbClassicLevelFlowView result = {0};
    if (flow) {
        if (flow->staging)
            result.staging = *flow->staging;
        if (flow->initialization)
            result.initialization = *flow->initialization;
        if (flow->ingame)
            result.ingame = *flow->ingame;
        result.showing = flow->showing;
        result.done = flow->done;
    }
    return result;
}

bool ab_classic_level_flow_initialize(AbClassicLevelFlow *flow, AbError *error) {
    if (!enter(flow, true, error))
        return false;
    bool okay = fresh_queue(flow, &flow->staging, error) &&
                fresh_queue(flow, &flow->initialization, error) &&
                fresh_queue(flow, &flow->ingame, error);
    if (okay) {
        flow->showing = NULL;
        flow->done = (AbDataValue){.kind = AB_DATA_BOOLEAN};
        flow->initialized = true;
        /* A successful first subscription must also be removed if the second
         * subscription fails. Remove tolerates a missing identity in a live list. */
        flow->subscribed = true;
        okay = ab_event_manager_add(flow->config.events, flow->config.loading_init,
                                    &flow->listener, error) &&
               ab_event_manager_add(flow->config.events, flow->config.loading_done,
                                    &flow->listener, error);
    }
    return leave(flow, okay);
}

bool ab_classic_level_flow_push(AbClassicLevelFlow *flow, AbSpan name, float appearing,
                                AbClassicLevelFlowListener *listener, AbError *error) {
    if (!enter(flow, false, error))
        return false;
    bool valid_time = appearing == 10 || appearing == 20 || appearing == 30 ||
                      appearing == 35 || appearing == 38 || appearing == 40 ||
                      appearing == 50;
    bool okay = listener && listener->on_show && listener->is_done && valid_time;
    if (!okay)
        ab_fail(error, "Invalid level-start listener or appearing time");
    if (okay)
        okay = diagnostic(flow, AB_LEVEL_FLOW_REQUEST, name, error);
    if (okay && flow->request_count == flow->config.requests)
        okay = ab_fail(error, "Level-start request records exhausted");
    if (okay) {
        AbClassicLevelFlowRequest *request = flow->requests + flow->request_count++;
        *request = (AbClassicLevelFlowRequest){name, appearing, listener};
        okay = append(flow, flow->staging, request, error);
    }
    return leave(flow, okay);
}

bool ab_classic_level_flow_update(AbClassicLevelFlow *flow, AbError *error) {
    if (!flow || flow->updating || !enter(flow, false, error))
        return ab_fail(error, "Invalid or reentrant level-start update");
    flow->updating = true;
    bool in_game = false;
    bool okay = update_showing(flow, error) && ingame(flow, &in_game, error);
    if (okay && in_game && !flow->showing)
        okay = process_ingame(flow, error);
    if (okay && flow->staging->count)
        okay = move_staging(flow, error);
    flow->updating = false;
    return leave(flow, okay);
}

bool ab_classic_level_flow_clear_init(AbClassicLevelFlow *flow, AbError *error) {
    if (!enter(flow, false, error))
        return false;
    bool okay =
        move_staging(flow, error) && fresh_queue(flow, &flow->initialization, error);
    return leave(flow, okay);
}
