#ifndef AB_REFERENCE_LEVEL_START_FLOW_H
#define AB_REFERENCE_LEVEL_START_FLOW_H

/* Prepared inputs/callback actions only. No expected queue order or results. */
enum {
    REFERENCE_FLOW_UPDATE,
    REFERENCE_FLOW_PUSH,
    REFERENCE_FLOW_LOADING_INIT,
    REFERENCE_FLOW_LOADING_DONE,
    REFERENCE_FLOW_CLEAR,
    REFERENCE_FLOW_SNAPSHOT,
};

typedef struct {
    unsigned operation, name, listener;
    float appearing;
} ReferenceLevelFlow;

static const char *reference_flow_name(unsigned name) {
    const char *names[] = {"A", "B", "C", "D", "E", "F", "G", "H", "M", "L"};
    return name < 10 ? names[name] : "invalid";
}

static ReferenceLevelFlow reference_level_flow(unsigned profile, unsigned step) {
    ReferenceLevelFlow input = {REFERENCE_FLOW_UPDATE, 0, 0, 0};
    if (!step)
        input.operation = REFERENCE_FLOW_SNAPSHOT;
    else if (step <= 5) {
        input.operation = REFERENCE_FLOW_PUSH;
        input.name = step == 5 ? 0 : step - 1;
        input.listener = step - 1;
        input.appearing = step == 1 ? 10.0f : step == 2 ? 20.0f : 30.0f;
        if ((profile & 16) && step <= 3)
            input.appearing = 10;
    } else if (step == 7 || step == 22)
        input.operation = REFERENCE_FLOW_LOADING_INIT;
    else if (step == 10)
        input.operation = REFERENCE_FLOW_LOADING_DONE;
    else if (step >= 11 && step <= 14) {
        const float timing[] = {30, 35, 38, 40, 50};
        input.operation = REFERENCE_FLOW_PUSH;
        input.name = step - 7;
        input.listener = step - 7;
        input.appearing = timing[profile % 5];
    } else if (step == 21)
        input.operation = REFERENCE_FLOW_CLEAR;
    return input;
}

#endif
