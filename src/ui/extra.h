#ifndef EXTRA_H
#define EXTRA_H

#include "ui/ui.h"

typedef enum {
    UI_FLOATS,
    UI_SIZES,
} UIMultiType;

typedef union {
    float _float;
    size_t _size;
} UIMultiValue;

BOOL UIDoublet(const char* precursors, UIMultiType type, void* first, void* second, UIMultiValue min, UIMultiValue max, UIMultiValue speed, UIMultiValue reset, size_t w);

BOOL UITriplet(const char* precursors, UIMultiType type, void* first, void* second, void* third, UIMultiValue min, UIMultiValue max, UIMultiValue speed, UIMultiValue reset, size_t w);

BOOL UIColoredDoublet(const Color* colors, const char* precursors, UIMultiType type, void* first, void* second, UIMultiValue min, UIMultiValue max, UIMultiValue speed, UIMultiValue reset, size_t w);

BOOL UIColoredTriplet(const Color* colors, const char* precursors, UIMultiType type, void* first, void* second, void* third, UIMultiValue min, UIMultiValue max, UIMultiValue speed, UIMultiValue reset, size_t w);

#endif
