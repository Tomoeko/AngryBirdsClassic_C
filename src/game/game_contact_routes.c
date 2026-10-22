#include "ab/game_contact_routes.h"

#include <string.h>

static void pair(AbGameContactRoutes *routes, AbGameContactStage stage, uint32_t first,
                 uint32_t second, uint32_t handler) {
    routes->handlers[stage][first][second] = handler;
    routes->handlers[stage][second][first] = handler;
}

static void basic_pair(AbGameContactRoutes *routes, uint32_t first, uint32_t second) {
    pair(routes, AB_GAME_CONTACT_BEGIN, first, second, AB_GAME_CONTACT_BASIC_BEGIN);
    pair(routes, AB_GAME_CONTACT_END, first, second, AB_GAME_CONTACT_BASIC_END);
}

bool ab_game_contact_routes_setup(AbGameContactRoutes *routes) {
    if (!routes) {
        return false;
    }
    for (uint32_t first = 0; first <= AB_COLLIDER_PIG; ++first) {
        for (uint32_t second = 0; second <= AB_COLLIDER_PIG; ++second) {
            basic_pair(routes, first, second);
        }
        pair(routes, AB_GAME_CONTACT_BEGIN, first, AB_COLLIDER_INVISIBLE,
             AB_GAME_CONTACT_BASIC_BEGIN);
    }
    for (uint32_t first = 0; first < AB_GAME_COLLIDER_TYPES; ++first) {
        basic_pair(routes, first, AB_COLLIDER_GHOST);
        basic_pair(routes, first, AB_COLLIDER_EGG);
    }
    for (uint32_t first = 0; first < AB_COLLIDER_PORTAL; ++first) {
        pair(routes, AB_GAME_CONTACT_BEGIN, first, AB_COLLIDER_PORTAL,
             AB_GAME_CONTACT_PORTAL_BEGIN);
        pair(routes, AB_GAME_CONTACT_END, first, AB_COLLIDER_PORTAL,
             AB_GAME_CONTACT_PORTAL_END);
    }
    for (uint32_t first = 0; first < AB_GAME_COLLIDER_TYPES; ++first) {
        basic_pair(routes, first, AB_COLLIDER_GLUE);
    }
    for (uint32_t first = 0; first < AB_GAME_COLLIDER_TYPES; ++first) {
        if (first == AB_COLLIDER_BIRD) {
            pair(routes, AB_GAME_CONTACT_BEGIN, first, AB_COLLIDER_BUBBLE,
                 AB_GAME_CONTACT_BUBBLE_BEGIN);
        } else {
            basic_pair(routes, first, AB_COLLIDER_BUBBLE);
        }
    }
    return true;
}

bool ab_game_contact_routes_initialize(AbGameContactRoutes *routes) {
    if (!routes) {
        return false;
    }
    memset(routes, 0, sizeof(*routes));
    return ab_game_contact_routes_setup(routes);
}

bool ab_game_contact_routes_set(AbGameContactRoutes *routes, AbGameContactStage stage,
                                uint32_t first, uint32_t second, uint32_t handler) {
    if (!routes || (unsigned)stage >= AB_GAME_CONTACT_STAGES ||
        first >= AB_GAME_COLLIDER_TYPES || second >= AB_GAME_COLLIDER_TYPES) {
        return false;
    }
    pair(routes, stage, first, second, handler);
    return true;
}

bool ab_game_contact_dispatch(const AbGameContactRoutes *routes,
                              const AbGameContactDispatchInput *input,
                              AbGameContactDispatchHost host, void *context,
                              AbError *error) {
    if (!input || (unsigned)input->stage >= AB_GAME_CONTACT_STAGES) {
        return ab_fail(error, "Invalid contact dispatch stage");
    }
    if ((input->stage == AB_GAME_CONTACT_BEGIN && input->begin_disabled) ||
        !input->first || !input->second) {
        return true;
    }
    if (!routes || input->first->collider >= AB_GAME_COLLIDER_TYPES ||
        input->second->collider >= AB_GAME_COLLIDER_TYPES) {
        return ab_fail(error, "Invalid contact dispatch routes or collider");
    }
    uint32_t handler =
        routes->handlers[input->stage][input->first->collider][input->second->collider];
    if (handler == AB_GAME_CONTACT_NO_HANDLER) {
        return true;
    }
    if (!host) {
        return ab_fail(error, "Missing contact dispatch host");
    }
    AbGameContactDispatchCall call = {
        .stage = input->stage,
        .handler = handler,
        .contact = input->contact,
        .previous = input->stage == AB_GAME_CONTACT_PRE_SOLVE ? input->previous : NULL};
    return host(context, &call, error);
}
