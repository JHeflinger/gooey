#include "extra.h"

BOOL UIDoublet(const char* precursors, UIMultiType type, void* first, void* second, void* min, void* max, void* speed, void* reset, size_t w) {
    
    return FALSE;
}

BOOL UITriplet(const char* precursors, UIMultiType type, void* first, void* second, void* third, void* min, void* max, void* speed, void* reset, size_t w) {
    BOOL edited = FALSE;
    char buf[2] = { 0 };
    float component_width = (w - 20 - (3 * 16) - (2 * 10)) / 3.0f;
    UIMoveCursor(5, 5);
    DrawRectangle(UIGetCursor().x - 5, UIGetCursor().y + 1, 20, 18, RED);
    if (CheckCollisionPointRec(Vector2Subtract(GetMousePosition(), UIGetPosition()), (Rectangle){UIGetCursor().x - 5, UIGetCursor().y + 1, 20, 18}) &&
        InputButtonPressed(IK_MOUSELEFT)) {
        tc->translation.x = 0.0f;
    }
    buf[0] = precursors[0];
    UIDrawText(buf);
    UIMoveCursor(17, -20);
    edited |= UIDragFloat(&(tc->translation.x), -FLT_MAX, FLT_MAX, 0.1f, component_width);
    UIMoveCursor(component_width + 31, -20);
    DrawRectangle(UIGetCursor().x - 5, UIGetCursor().y + 1, 20, 18, GREEN);
    if (CheckCollisionPointRec(Vector2Subtract(GetMousePosition(), UIGetPosition()), (Rectangle){UIGetCursor().x - 5, UIGetCursor().y + 1, 20, 18}) &&
        InputButtonPressed(IK_MOUSELEFT)) {
        tc->translation.y = 0.0f;
    }
    buf[0] = precursors[1];
    UIDrawText(buf);
    UIMoveCursor(component_width + 42, -20);
    edited |= UIDragFloat(&(tc->translation.y), -FLT_MAX, FLT_MAX, 0.1f, component_width);
    UIMoveCursor((2*component_width) + 56, -20);
    DrawRectangle(UIGetCursor().x - 5, UIGetCursor().y + 1, 20, 18, BLUE);
    if (CheckCollisionPointRec(Vector2Subtract(GetMousePosition(), UIGetPosition()), (Rectangle){UIGetCursor().x - 5, UIGetCursor().y + 1, 20, 18}) &&
        InputButtonPressed(IK_MOUSELEFT)) {
        tc->translation.z = 0.0f;
    }
    buf[0] = precursors[2];
    UIDrawText(buf);
    UIMoveCursor((2*component_width) + 67, -20);
    edited |= UIDragFloat(&(tc->translation.z), -FLT_MAX, FLT_MAX, 0.1f, component_width);
    return edited;
}

BOOL UIColoredDoublet(const Color* colors, const char* precursors, UIMultiType type, void* first, void* second, void* min, void* max, void* speed, void* reset, size_t w) {

    return FALSE;
}

BOOL UIColoredTriplet(const Color* colors, const char* precursors, UIMultiType type, void* first, void* second, void* third, void* min, void* max, void* speed, void* reset, size_t w) {

    return FALSE;
}
