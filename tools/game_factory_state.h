#ifndef AB_GAME_FACTORY_STATE_H
#define AB_GAME_FACTORY_STATE_H

enum { GAME_FACTORY_NAME_BYTES = 256 };

typedef struct {
    AbDataTable counts;
    AbDataEntry *entries;
    uint8_t (*names)[GAME_FACTORY_NAME_BYTES];
    AbDataTable *events;
    AbDataEntry (*event_fields)[2];
    size_t capacity, used, events_used;
    float initial_goals, giftable, gift_selected, gift_accumulator;
} PreviewFactory;

#endif
