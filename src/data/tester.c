#include "tester.h"
#include <easylogger.h>
#include <easyobjects.h>
#include <easyfile.h>
#include <raylib.h>
#include <easyparse.h>

#define MAX_SCRIPT_LINE_SIZE 1024

typedef enum {
    TEST_MOUSE,
    TEST_MOVE,
    TEST_KEY,
    TEST_WAIT,
    TEST_TIMESTEP,
} TestInputType;

typedef enum {
    TEST_NO_ACTION = 0,
    TEST_PRESS,
    TEST_DOWN,
    TEST_UP
} TestInputAction;

typedef struct {
    BOOL scheduled;
    float timestamp;
    TestInputType type;
    TestInputAction action;
    union {
        Vector2 _movement;
        KeyboardKey _keybtn;
        MouseButton _mousebtn;
        float _float;
    } tvalue;
    BOOL complete;
} TestEvent;
DECLARE_ARRLIST(TestEvent);
IMPL_ARRLIST(TestEvent);

static ARRLIST_TestEvent g_scheduled_events = { 0 };
static ARRLIST_TestEvent g_free_events = { 0 };

static MouseButton ParseMouseButton(const char* word) {
    if (strcmp(word, "left") == 0) {
        return MOUSE_BUTTON_LEFT;
    } else if (strcmp(word, "right") == 0) {
        return MOUSE_BUTTON_RIGHT;
    } else {
        EZ_FATAL("Invalid mouse button detected - \"%s\"", word);
    }
    return 0;
}

static KeyboardKey ParseKeyButton(const char* word) {
    #define PARSEKEY(x) if (strcmp(word, #x) == 0) { return KEY_##x; }
    PARSEKEY(APOSTROPHE);
    PARSEKEY(COMMA);
    PARSEKEY(MINUS);
    PARSEKEY(PERIOD);
    PARSEKEY(SLASH);
    PARSEKEY(ZERO);
    PARSEKEY(ONE);
    PARSEKEY(TWO);
    PARSEKEY(THREE);
    PARSEKEY(FOUR);
    PARSEKEY(FIVE);
    PARSEKEY(SIX);
    PARSEKEY(SEVEN);
    PARSEKEY(EIGHT);
    PARSEKEY(NINE);
    PARSEKEY(SEMICOLON);
    PARSEKEY(EQUAL);
    PARSEKEY(A);
    PARSEKEY(B);
    PARSEKEY(C);
    PARSEKEY(D);
    PARSEKEY(E);
    PARSEKEY(F);
    PARSEKEY(G);
    PARSEKEY(H);
    PARSEKEY(I);
    PARSEKEY(J);
    PARSEKEY(K);
    PARSEKEY(L);
    PARSEKEY(M);
    PARSEKEY(N);
    PARSEKEY(O);
    PARSEKEY(P);
    PARSEKEY(Q);
    PARSEKEY(R);
    PARSEKEY(S);
    PARSEKEY(T);
    PARSEKEY(U);
    PARSEKEY(V);
    PARSEKEY(W);
    PARSEKEY(X);
    PARSEKEY(Y);
    PARSEKEY(Z);
    PARSEKEY(LEFT_BRACKET);
    PARSEKEY(BACKSLASH);
    PARSEKEY(RIGHT_BRACKET);
    PARSEKEY(GRAVE);
    PARSEKEY(SPACE);
    PARSEKEY(ESCAPE);
    PARSEKEY(ENTER);
    PARSEKEY(TAB);
    PARSEKEY(BACKSPACE);
    PARSEKEY(INSERT);
    PARSEKEY(DELETE);
    PARSEKEY(RIGHT);
    PARSEKEY(LEFT);
    PARSEKEY(DOWN);
    PARSEKEY(UP);
    PARSEKEY(PAGE_UP);
    PARSEKEY(PAGE_DOWN);
    PARSEKEY(HOME);
    PARSEKEY(END);
    PARSEKEY(CAPS_LOCK);
    PARSEKEY(SCROLL_LOCK);
    PARSEKEY(NUM_LOCK);
    PARSEKEY(PRINT_SCREEN);
    PARSEKEY(PAUSE);
    PARSEKEY(F1);
    PARSEKEY(F2);
    PARSEKEY(F3);
    PARSEKEY(F4);
    PARSEKEY(F5);
    PARSEKEY(F6);
    PARSEKEY(F7);
    PARSEKEY(F8);
    PARSEKEY(F9);
    PARSEKEY(F10);
    PARSEKEY(F11);
    PARSEKEY(F12);
    PARSEKEY(LEFT_SHIFT);
    PARSEKEY(LEFT_CONTROL);
    PARSEKEY(LEFT_ALT);
    PARSEKEY(LEFT_SUPER);
    PARSEKEY(RIGHT_SHIFT);
    PARSEKEY(RIGHT_CONTROL);
    PARSEKEY(RIGHT_ALT);
    PARSEKEY(RIGHT_SUPER);
    PARSEKEY(KB_MENU);
    PARSEKEY(KP_0);
    PARSEKEY(KP_1);
    PARSEKEY(KP_2);
    PARSEKEY(KP_3);
    PARSEKEY(KP_4);
    PARSEKEY(KP_5);
    PARSEKEY(KP_6);
    PARSEKEY(KP_7);
    PARSEKEY(KP_8);
    PARSEKEY(KP_9);
    PARSEKEY(KP_DECIMAL);
    PARSEKEY(KP_DIVIDE);
    PARSEKEY(KP_MULTIPLY);
    PARSEKEY(KP_SUBTRACT);
    PARSEKEY(KP_ADD);
    PARSEKEY(KP_ENTER);
    PARSEKEY(KP_EQUAL);
    PARSEKEY(BACK);
    PARSEKEY(MENU);
    PARSEKEY(VOLUME_UP);
    PARSEKEY(VOLUME_DOWN);
    return 0;
}

void LoadTester(const char* path) {
    EZ_INFO("Loading test file \"%s\"", path);
    ez_File* ezf = ez_load_file(path);
    ez_FileParser parser = ez_parser(ezf);
    char buffer[MAX_SCRIPT_LINE_SIZE] = { 0 };
    int lines = 0;
    while (ez_next_line(&parser, buffer, sizeof(buffer))) {
        lines++;
        TestEvent event = { 0 };
        char word[MAX_SCRIPT_LINE_SIZE] = { 0 };
        int wptr = 0;
        int stages = 0;
        for (int i = 0; buffer[i] != '\0'; i++) {
            if (buffer[i] == ' ') {
                word[wptr] = '\0';
                wptr = 0;
                stages++;
                if (stages == 1) { // <schedule/nil>
                    if (strcmp(word, "schedule") == 0) {
                        event.scheduled = TRUE;
                        continue;
                    } else {
                        stages += 2;
                    }
                }
                if (stages == 2) { // <float/nil>
                    if (!ez_parse_float(word, &(event.timestamp))) {
                        EZ_FATAL("Unable to parse timestamp on line %d: \"%s\"", lines, word);
                    }
                    continue;
                }
                if (stages == 3) { // <type>
                    if (strcmp(word, "mouse") == 0) {
                        event.type = TEST_MOUSE;
                    } else if (strcmp(word, "move") == 0) {
                        event.type = TEST_MOVE;
                    } else if (strcmp(word, "key") == 0) {
                        event.type = TEST_KEY;
                    } else if (strcmp(word, "wait") == 0) {
                        event.type = TEST_WAIT;
                    } else if (strcmp(word, "timestep") == 0) {
                        event.type = TEST_TIMESTEP;
                    } else {
                        EZ_FATAL("Unable to parse type on line %d: \"%s\"", lines, word);
                    }
                    continue;
                }
                if (stages == 4) { // <action/nil>
                    if (event.type == TEST_MOUSE || event.type == TEST_KEY) {
                        if (strcmp(word, "press") == 0) {
                            event.action = TEST_PRESS;
                        } else if (strcmp(word, "down") == 0) {
                            event.action = TEST_DOWN;
                        } else if (strcmp(word, "up") == 0) {
                            event.action = TEST_UP;
                        } else {
                            EZ_FATAL("Unable to parse action on line %d: \"%s\"", lines, word);
                        }
                        continue;
                    } else {
                        stages += 1;
                    }
                }
                if (stages == 5) { // <value>
                    if (event.type == TEST_MOUSE) {
                        event.tvalue._mousebtn = ParseMouseButton(word);
                    } else if (event.type == TEST_KEY) {
                        event.tvalue._keybtn = ParseKeyButton(word);
                    } else if (event.type == TEST_MOVE) {
                        if (!ez_parse_float(word, &(event.tvalue._movement.x))) {
                            EZ_FATAL("Unable to parse float on line %d: \"%s\"", lines, word);
                        }
                    } else {
                        if (!ez_parse_float(word, &(event.tvalue._float))) {
                            EZ_FATAL("Unable to parse float on line %d: \"%s\"", lines, word);
                        }
                    }
                    continue;
                }
                if (stages == 6) { // <value/nil>
                    if (event.type == TEST_MOVE) {
                        if (!ez_parse_float(word, &(event.tvalue._movement.y))) {
                            EZ_FATAL("Unable to parse float on line %d: \"%s\"", lines, word);
                        }
                        continue;
                    } else {
                        EZ_FATAL("Too many arguments on line %d: \"%s\"", lines, buffer);
                    }
                }
                if (stages > 6) {
                    EZ_FATAL("Too many arguments on line %d: \"%s\"", lines, buffer);
                }
            } else {
                word[wptr] = buffer[i];
                wptr++;
            }
        }
        if (event.scheduled) {
            ARRLIST_TestEvent_add(&g_scheduled_events, event);
        } else {
            ARRLIST_TestEvent_add(&g_free_events, event);
        }
    }
    ez_free_file(ezf);
    EZ_INFO("Successfully loaded test");
}

void CleanTester() {
    ARRLIST_TestEvent_clear(&g_scheduled_events);
    ARRLIST_TestEvent_clear(&g_free_events);
}
