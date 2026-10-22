#ifndef AB_REFERENCE_ROOT_QUERIES_H
#define AB_REFERENCE_ROOT_QUERIES_H

typedef struct {
    unsigned root, children, require, episode, power, main_menu, unlock;
} ReferenceRootQuery;

static inline ReferenceRootQuery reference_root_query(unsigned profile) {
    return (ReferenceRootQuery){
        profile & 7,        (profile >> 5) & 3, (profile >> 3) & 3, (profile >> 3) & 3,
        (profile >> 5) & 3, (profile >> 2) & 1, (profile >> 7) & 3};
}

#endif
