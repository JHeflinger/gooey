#include "input.h"
#include <easylogger.h>
#include <raymath.h>

static InputMap g_input_map = { 0 };

START_INPUT_KEY_DEFINITIONS()
DEFINE_INPUT_KEY(IK_DEV, KEY_D, "D");
DEFINE_INPUT_KEY(IK_ENTER, KEY_ENTER, "Enter");
DEFINE_INPUT_KEY(IK_BACKSPACE, KEY_BACKSPACE, "Backspace");
DEFINE_INPUT_KEY(IK_LEFT, KEY_LEFT, "Left");
DEFINE_INPUT_KEY(IK_RIGHT, KEY_RIGHT, "Right");
DEFINE_INPUT_KEY(IK_UP, KEY_UP, "Up");
DEFINE_INPUT_KEY(IK_DOWN, KEY_DOWN, "Down");
DEFINE_INPUT_KEY(IK_ZOOM, KEY_Z, "Z");
DEFINE_INPUT_KEY(IK_TOGGLE_HINTS, KEY_H, "H");
DEFINE_INPUT_KEY(IK_RESET_CAMERA, KEY_GRAVE, "`");
DEFINE_INPUT_KEY(IK_PAN_CAMERA, KEY_SPACE, "Spacebar");
DEFINE_INPUT_KEY(IK_FIT_CAMERA, KEY_F, "F");
DEFINE_INPUT_KEY(IK_TOGGLE_HINTS, KEY_H, "H");
DEFINE_INPUT_KEY(IK_L_OVERRIDE, KEY_L, "L");
DEFINE_INPUT_KEY(IK_C_OVERRIDE, KEY_C, "C");
DEFINE_INPUT_KEY(IK_B_OVERRIDE, KEY_B, "B");
DEFINE_INPUT_KEY(IK_O_OVERRIDE, KEY_O, "O");
DEFINE_INPUT_KEY(IK_S_OVERRIDE, KEY_S, "S");
DEFINE_INPUT_KEY(IK_ZOOM, KEY_Z, "Z");
DEFINE_INPUT_KEY(IK_DELETE, KEY_DELETE, "Delete");
DEFINE_INPUT_KEY(IK_I_OVERRIDE, KEY_I, "I");
DEFINE_INPUT_KEY(IK_P_OVERRIDE, KEY_P, "P");
DEFINE_INPUT_KEY(IK_SELECT_FACE, KEY_T, "T");
DEFINE_INPUT_KEY(IK_SELECT_VERTEX, KEY_V, "V");
DEFINE_INPUT_KEY(IK_SELECT_NONE, KEY_N, "N");
DEFINE_INPUT_KEY(IK_SELECT, KEY_LEFT_SHIFT, "L Shift";
DEFINE_INPUT_KEY(IK_PAN_SELECTED, KEY_Y, "Y");
DEFINE_INPUT_KEY(IK_A_OVERRIDE, KEY_A, "A");
END_INPUT_KEY_DEFINITIONS()

START_INPUT_BUTTON_DEFINITIONS()
DEFINE_INPUT_BUTTON(IK_MOUSELEFT, MOUSE_BUTTON_LEFT, "Mouse Left");
DEFINE_INPUT_BUTTON(IK_MOUSERIGHT, MOUSE_BUTTON_RIGHT, "Mouse Right");
END_INPUT_BUTTON_DEFINITIONS()

void InitializeInput() {
    InitializeKeys();
    InitializeButtons();
    g_input_map.initialized = TRUE;
}

void BlockInputs() {
    g_input_map.blocked = TRUE;
}

void UnblockInputs() {
    g_input_map.blocked = FALSE;
}

BOOL InputsBlocked() {
    return g_input_map.blocked;
}

const char* InputKeyRepresentation(const int key) {
    EZ_ASSERT(key < NUM_KEY_INPUTS, "Invalid key code");
    return g_input_map.keynames[key];
}

const char* InputButtonRepresentation(const int button) {
    EZ_ASSERT(button < NUM_BUTTON_INPUTS, "Invalid button code");
    return g_input_map.btnnames[button];
}

BOOL InputKeyPressed(const int key) {
    EZ_ASSERT(key < NUM_KEY_INPUTS, "Invalid key code");
    if (g_input_map.blocked) return FALSE;
    return IsKeyPressed(g_input_map.keymap[key]);
}

BOOL InputKeyReleased(const int key) {
    EZ_ASSERT(key < NUM_KEY_INPUTS, "Invalid key code");
    if (g_input_map.blocked) return FALSE;
    return IsKeyReleased(g_input_map.keymap[key]);
}

BOOL InputKeyDown(const int key) {
    EZ_ASSERT(key < NUM_KEY_INPUTS, "Invalid key code");
    if (g_input_map.blocked) return FALSE;
    return IsKeyDown(g_input_map.keymap[key]);
}

BOOL InputKeyUp(const int key) {
    EZ_ASSERT(key < NUM_KEY_INPUTS, "Invalid key code");
    if (g_input_map.blocked) return FALSE;
    return IsKeyUp(g_input_map.keymap[key]);
}

BOOL InputButtonPressed(const int btn) {
    EZ_ASSERT(btn < NUM_BUTTON_INPUTS, "Invalid button code");
    if (g_input_map.blocked) return FALSE;
    return IsMouseButtonPressed(g_input_map.btnmap[btn]);
}

BOOL InputButtonReleased(const int btn) {
    EZ_ASSERT(btn < NUM_BUTTON_INPUTS, "Invalid button code");
    if (g_input_map.blocked) return FALSE;
    return IsMouseButtonReleased(g_input_map.btnmap[btn]);
}

BOOL InputButtonDown(const int btn) {
    EZ_ASSERT(btn < NUM_BUTTON_INPUTS, "Invalid button code");
    if (g_input_map.blocked) return FALSE;
    return IsMouseButtonDown(g_input_map.btnmap[btn]);
}

BOOL InputButtonUp(const int btn) {
    EZ_ASSERT(btn < NUM_BUTTON_INPUTS, "Invalid button code");
    if (g_input_map.blocked) return FALSE;
    return IsMouseButtonUp(g_input_map.btnmap[btn]);
}
