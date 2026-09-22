#ifndef EXTRA_H
#define EXTRA_H

#include "ui/ui.h"

typedef enum {
    UI_FLOATS,
    UI_INTS,
    UI_SIZES,
} UIMultiType;

typedef union {
    float _float;
    size_t _size;
    int32_t _int;
} UIMultiValue;

#define PERSISTENT_TRIPLE_UI(func, ...) \
    ({ClearJustUsedUI(); \
      static PersistantUIData s1_ui_data_##__COUNTER__ = { 0 }; \
      static PersistantUIData s2_ui_data_##__COUNTER__ = { 0 }; \
      static PersistantUIData s3_ui_data_##__COUNTER__ = { 0 }; \
      func(&s1_ui_data_##__COUNTER__, &s2_ui_data_##__COUNTER__, &s3_ui_data_##__COUNTER__, __VA_ARGS__);})

#define PERSISTENT_DOUBLE_UI(func, ...) \
    ({ClearJustUsedUI(); \
      static PersistantUIData s1_ui_data_##__COUNTER__ = { 0 }; \
      static PersistantUIData s2_ui_data_##__COUNTER__ = { 0 }; \
      func(&s1_ui_data_##__COUNTER__, &s2_ui_data_##__COUNTER__, __VA_ARGS__);})

BOOL UIDoublet_(PersistantUIData* d1, PersistantUIData* d2, const char* precursors, UIMultiType type, void* first, void* second, UIMultiValue min, UIMultiValue max, UIMultiValue speed, UIMultiValue reset, size_t w);
#define UIDoublet(precursors, type, first, second, min, max, speed, reset, w) \
    PERSISTENT_DOUBLE_UI(UIDoublet_, precursors, type, first, second, min, max, speed, reset, w)

BOOL UITriplet_(PersistantUIData* d1, PersistantUIData* d2, PersistantUIData* d3, const char* precursors, UIMultiType type, void* first, void* second, void* third, UIMultiValue min, UIMultiValue max, UIMultiValue speed, UIMultiValue reset, size_t w);
#define UITriplet(precursors, type, first, second, third, min, max, speed, reset, w) \
    PERSISTENT_TRIPLE_UI(UITriplet_, precursors, type, first, second, third, min, max, speed, reset, w)

BOOL UIColoredDoublet_(PersistantUIData* d1, PersistantUIData* d2, const Color* colors, const char* precursors, UIMultiType type, void* first, void* second, UIMultiValue min, UIMultiValue max, UIMultiValue speed, UIMultiValue reset, size_t w);
#define UIColoredDoublet(colors, precursors, type, first, second, min, max, speed, reset, w) \
    PERSISTENT_DOUBLE_UI(UIColoredDoublet_, colors, precursors, type, first, second, min, max, speed, reset, w)

BOOL UIColoredTriplet_(PersistantUIData* d1, PersistantUIData* d2, PersistantUIData* d3, const Color* colors, const char* precursors, UIMultiType type, void* first, void* second, void* third, UIMultiValue min, UIMultiValue max, UIMultiValue speed, UIMultiValue reset, size_t w);
#define UIColoredTriplet(colors, precursors, type, first, second, third, min, max, speed, reset, w) \
    PERSISTENT_TRIPLE_UI(UIColoredTriplet_, colors, precursors, type, first, second, third, min, max, speed, reset, w)

void UIColumnHeader(const char* text, size_t w);

#endif
