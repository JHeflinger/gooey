#ifndef EXTRA_H
#define EXTRA_H

#include "ui/ui.h"

typedef enum {
    UI_FLOATS,
    UI_SIZES,
    UI_INTS
} UIMultiType;

BOOL UIDoublet(const char* precursors, UIMultiType type, void* first, void* second);

BOOL UITriplet(const char* precursors, UIMultiType type, void* first, void* second);

BOOL UIColoredDoublet(const Color* colors, const char* precursors, UIMultiType type, void* first, void* second);

BOOL UIColoredTriplet(const Color* colors, const char* precursors, UIMultiType type, void* first, void* second);

#endif
