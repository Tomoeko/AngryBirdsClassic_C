#ifndef AB_REFERENCE_MUSEUM_PAUSE_H
#define AB_REFERENCE_MUSEUM_PAUSE_H

/* Input flags only; no expected results. */
typedef struct {
    unsigned root, child, show, visible;
} ReferenceMuseumPause;

static ReferenceMuseumPause reference_museum_pause(unsigned profile) {
    return (ReferenceMuseumPause){profile & 3, (profile >> 2) & 1, (profile >> 3) & 7,
                                  (profile >> 6) & 3};
}

#endif
