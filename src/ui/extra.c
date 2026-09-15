#include "extra.h"
#include "data/input.h"
#include <raymath.h>

BOOL UIDoublet_(PersistantUIData* d1, PersistantUIData* d2, const char* precursors, UIMultiType type, void* first, void* second, UIMultiValue min, UIMultiValue max, UIMultiValue speed, UIMultiValue reset, size_t w) {
    BOOL edited = FALSE;
    char buf[2] = { 0 };
    size_t csize = type == UI_FLOATS ? sizeof(float) : sizeof(size_t);
    float component_width = (w - 20 - (2 * 16) - (1 * 10)) / 2.0f;
    UIMoveCursor(5, 5);
    DrawRectangle(UIGetCursor().x - 5, UIGetCursor().y + 1, 20, 18, RED);
    if (CheckCollisionPointRec(Vector2Subtract(GetMousePosition(), UIGetPosition()), (Rectangle){UIGetCursor().x - 5, UIGetCursor().y + 1, 20, 18}) &&
        InputButtonPressed(IK_MOUSELEFT)) {
        memcpy(first, &reset, csize);
    }
    buf[0] = precursors[0];
    UIDrawText(buf);
    UIMoveCursor(17, -20);
    if (type == UI_FLOATS) edited |= UIDragFloat_(d1, (float*)first, min._float, max._float, speed._float, component_width);
    else edited |= UIDragSize_(d1, (size_t*)first, min._size, max._size, speed._size, component_width);
    UIMoveCursor(component_width + 31, -20);
    DrawRectangle(UIGetCursor().x - 5, UIGetCursor().y + 1, 20, 18, GREEN);
    if (CheckCollisionPointRec(Vector2Subtract(GetMousePosition(), UIGetPosition()), (Rectangle){UIGetCursor().x - 5, UIGetCursor().y + 1, 20, 18}) &&
        InputButtonPressed(IK_MOUSELEFT)) {
        memcpy(second, &reset, csize);
    }
    buf[0] = precursors[1];
    UIDrawText(buf);
    UIMoveCursor(component_width + 42, -20);
    if (type == UI_FLOATS) edited |= UIDragFloat_(d2, (float*)second, min._float, max._float, speed._float, component_width);
    else edited |= UIDragSize_(d2, (size_t*)second, min._size, max._size, speed._size, component_width);
    return edited;
}

BOOL UITriplet_(PersistantUIData* d1, PersistantUIData* d2, PersistantUIData* d3, const char* precursors, UIMultiType type, void* first, void* second, void* third, UIMultiValue min, UIMultiValue max, UIMultiValue speed, UIMultiValue reset, size_t w) {
    BOOL edited = FALSE;
    char buf[2] = { 0 };
    size_t csize = type == UI_FLOATS ? sizeof(float) : sizeof(size_t);
    float component_width = (w - 20 - (3 * 16) - (2 * 10)) / 3.0f;
    UIMoveCursor(5, 5);
    DrawRectangle(UIGetCursor().x - 5, UIGetCursor().y + 1, 20, 18, RED);
    if (CheckCollisionPointRec(Vector2Subtract(GetMousePosition(), UIGetPosition()), (Rectangle){UIGetCursor().x - 5, UIGetCursor().y + 1, 20, 18}) &&
        InputButtonPressed(IK_MOUSELEFT)) {
        memcpy(first, &reset, csize);
    }
    buf[0] = precursors[0];
    UIDrawText(buf);
    UIMoveCursor(17, -20);
    if (type == UI_FLOATS) edited |= UIDragFloat_(d1, (float*)first, min._float, max._float, speed._float, component_width);
    else edited |= UIDragSize_(d1, (size_t*)first, min._size, max._size, speed._size, component_width);
    UIMoveCursor(component_width + 31, -20);
    DrawRectangle(UIGetCursor().x - 5, UIGetCursor().y + 1, 20, 18, GREEN);
    if (CheckCollisionPointRec(Vector2Subtract(GetMousePosition(), UIGetPosition()), (Rectangle){UIGetCursor().x - 5, UIGetCursor().y + 1, 20, 18}) &&
        InputButtonPressed(IK_MOUSELEFT)) {
        memcpy(second, &reset, csize);
    }
    buf[0] = precursors[1];
    UIDrawText(buf);
    UIMoveCursor(component_width + 42, -20);
    if (type == UI_FLOATS) edited |= UIDragFloat_(d2, (float*)second, min._float, max._float, speed._float, component_width);
    else edited |= UIDragSize_(d2, (size_t*)second, min._size, max._size, speed._size, component_width);
    UIMoveCursor((2*component_width) + 56, -20);
    DrawRectangle(UIGetCursor().x - 5, UIGetCursor().y + 1, 20, 18, BLUE);
    if (CheckCollisionPointRec(Vector2Subtract(GetMousePosition(), UIGetPosition()), (Rectangle){UIGetCursor().x - 5, UIGetCursor().y + 1, 20, 18}) &&
        InputButtonPressed(IK_MOUSELEFT)) {
        memcpy(third, &reset, csize);
    }
    buf[0] = precursors[2];
    UIDrawText(buf);
    UIMoveCursor((2*component_width) + 67, -20);
    if (type == UI_FLOATS) edited |= UIDragFloat_(d3, (float*)third, min._float, max._float, speed._float, component_width);
    else edited |= UIDragSize_(d3, (size_t*)third, min._size, max._size, speed._size, component_width);
    return edited;
}

BOOL UIColoredDoublet_(PersistantUIData* d1, PersistantUIData* d2, const Color* colors, const char* precursors, UIMultiType type, void* first, void* second, UIMultiValue min, UIMultiValue max, UIMultiValue speed, UIMultiValue reset, size_t w) {
    BOOL edited = FALSE;
    char buf[2] = { 0 };
    size_t csize = type == UI_FLOATS ? sizeof(float) : sizeof(size_t);
    float component_width = (w - 20 - (2 * 16) - (1 * 10)) / 2.0f;
    UIMoveCursor(5, 5);
    DrawRectangle(UIGetCursor().x - 5, UIGetCursor().y + 1, 20, 18, colors[0]);
    if (CheckCollisionPointRec(Vector2Subtract(GetMousePosition(), UIGetPosition()), (Rectangle){UIGetCursor().x - 5, UIGetCursor().y + 1, 20, 18}) &&
        InputButtonPressed(IK_MOUSELEFT)) {
        memcpy(first, &reset, csize);
    }
    buf[0] = precursors[0];
    UIDrawText(buf);
    UIMoveCursor(17, -20);
    if (type == UI_FLOATS) edited |= UIDragFloat_(d1, (float*)first, min._float, max._float, speed._float, component_width);
    else edited |= UIDragSize_(d1, (size_t*)first, min._size, max._size, speed._size, component_width);
    UIMoveCursor(component_width + 31, -20);
    DrawRectangle(UIGetCursor().x - 5, UIGetCursor().y + 1, 20, 18, colors[1]);
    if (CheckCollisionPointRec(Vector2Subtract(GetMousePosition(), UIGetPosition()), (Rectangle){UIGetCursor().x - 5, UIGetCursor().y + 1, 20, 18}) &&
        InputButtonPressed(IK_MOUSELEFT)) {
        memcpy(second, &reset, csize);
    }
    buf[0] = precursors[1];
    UIDrawText(buf);
    UIMoveCursor(component_width + 42, -20);
    if (type == UI_FLOATS) edited |= UIDragFloat_(d2, (float*)second, min._float, max._float, speed._float, component_width);
    else edited |= UIDragSize_(d2, (size_t*)second, min._size, max._size, speed._size, component_width);
    return edited;
}

BOOL UIColoredTriplet_(PersistantUIData* d1, PersistantUIData* d2, PersistantUIData* d3, const Color* colors, const char* precursors, UIMultiType type, void* first, void* second, void* third, UIMultiValue min, UIMultiValue max, UIMultiValue speed, UIMultiValue reset, size_t w) {
    BOOL edited = FALSE;
    char buf[2] = { 0 };
    size_t csize = type == UI_FLOATS ? sizeof(float) : sizeof(size_t);
    float component_width = (w - 20 - (3 * 16) - (2 * 10)) / 3.0f;
    UIMoveCursor(5, 5);
    DrawRectangle(UIGetCursor().x - 5, UIGetCursor().y + 1, 20, 18, colors[0]);
    if (CheckCollisionPointRec(Vector2Subtract(GetMousePosition(), UIGetPosition()), (Rectangle){UIGetCursor().x - 5, UIGetCursor().y + 1, 20, 18}) &&
        InputButtonPressed(IK_MOUSELEFT)) {
        memcpy(first, &reset, csize);
    }
    buf[0] = precursors[0];
    UIDrawText(buf);
    UIMoveCursor(17, -20);
    if (type == UI_FLOATS) edited |= UIDragFloat_(d1, (float*)first, min._float, max._float, speed._float, component_width);
    else edited |= UIDragSize_(d1, (size_t*)first, min._size, max._size, speed._size, component_width);
    UIMoveCursor(component_width + 31, -20);
    DrawRectangle(UIGetCursor().x - 5, UIGetCursor().y + 1, 20, 18, colors[1]);
    if (CheckCollisionPointRec(Vector2Subtract(GetMousePosition(), UIGetPosition()), (Rectangle){UIGetCursor().x - 5, UIGetCursor().y + 1, 20, 18}) &&
        InputButtonPressed(IK_MOUSELEFT)) {
        memcpy(second, &reset, csize);
    }
    buf[0] = precursors[1];
    UIDrawText(buf);
    UIMoveCursor(component_width + 42, -20);
    if (type == UI_FLOATS) edited |= UIDragFloat_(d2, (float*)second, min._float, max._float, speed._float, component_width);
    else edited |= UIDragSize_(d2, (size_t*)second, min._size, max._size, speed._size, component_width);
    UIMoveCursor((2*component_width) + 56, -20);
    DrawRectangle(UIGetCursor().x - 5, UIGetCursor().y + 1, 20, 18, colors[2]);
    if (CheckCollisionPointRec(Vector2Subtract(GetMousePosition(), UIGetPosition()), (Rectangle){UIGetCursor().x - 5, UIGetCursor().y + 1, 20, 18}) &&
        InputButtonPressed(IK_MOUSELEFT)) {
        memcpy(third, &reset, csize);
    }
    buf[0] = precursors[2];
    UIDrawText(buf);
    UIMoveCursor((2*component_width) + 67, -20);
    if (type == UI_FLOATS) edited |= UIDragFloat_(d3, (float*)third, min._float, max._float, speed._float, component_width);
    else edited |= UIDragSize_(d3, (size_t*)third, min._size, max._size, speed._size, component_width);
    return edited;
}
