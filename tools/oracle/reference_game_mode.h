#ifndef AB_REFERENCE_GAME_MODE_H
#define AB_REFERENCE_GAME_MODE_H

typedef struct {
    unsigned root, show, museum, other_result, accelerometer;
} ReferenceGameMode;

static ReferenceGameMode reference_game_mode(unsigned profile) {
    return (ReferenceGameMode){profile & 3, (profile >> 2) & 1, (profile >> 3) & 1,
                               (profile >> 4) & 3, (profile >> 6) & 1};
}

enum {
    REFERENCE_MODE_SNAPSHOT,
    REFERENCE_MODE_RESUME,
    REFERENCE_MODE_CALL,
    REFERENCE_MODE_PAUSE,
    REFERENCE_MODE_PAUSE_ZERO,
    REFERENCE_MODE_SET_OTHER,
    REFERENCE_MODE_TOGGLE_PLAY,
    REFERENCE_MODE_FAIL_CURSOR,
};

static unsigned reference_game_mode_operation(unsigned step) {
    const unsigned operations[] = {
        REFERENCE_MODE_SNAPSHOT, REFERENCE_MODE_RESUME,      REFERENCE_MODE_CALL,
        REFERENCE_MODE_PAUSE,    REFERENCE_MODE_CALL,        REFERENCE_MODE_PAUSE_ZERO,
        REFERENCE_MODE_CALL,     REFERENCE_MODE_SET_OTHER,   REFERENCE_MODE_CALL,
        REFERENCE_MODE_RESUME,   REFERENCE_MODE_TOGGLE_PLAY, REFERENCE_MODE_PAUSE,
        REFERENCE_MODE_CALL,     REFERENCE_MODE_PAUSE_ZERO,  REFERENCE_MODE_RESUME,
        REFERENCE_MODE_CALL,     REFERENCE_MODE_FAIL_CURSOR, REFERENCE_MODE_PAUSE};
    return operations[step];
}

#endif
