#include "tester.h"
#include <easylogger.h>
#include <easyobjects.h>
#include <easyfile.h>
#include <raylib.h>

#define MAX_SCRIPT_LINE_SIZE 1024

typedef enum {
    TEST_MOUSE,
    TEST_MOVE,
    TEST_KEY,
    TEST_WAIT
} TestInputType;

typedef enum {
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
    } tvalue;
    BOOL complete;
} TestEvent;
DECLARE_ARRLIST(TestEvent);
IMPL_ARRLIST(TestEvent);

static ARRLIST_TestEvent g_scheduled_events = { 0 };
static ARRLIST_TestEvent g_free_events = { 0 };

void LoadTester(const char* path) {
    EZ_INFO("Loading test file \"%s\"", path);
    ez_File* ezf = ez_load_file(path);
    ez_FileParser parser = ez_parser(ezf);
    char buffer[MAX_SCRIPT_LINE_SIZE] = { 0 };
    int lines = 0;
    while (ez_next_line(&parser, buffer, sizeof(buffer))) {
        lines++;
        TestEvent event = { 0 };
        printf("%s\n", buffer);
        char word[MAX_SCRIPT_LINE_SIZE] = { 0 };
        int wptr = 0;
        int stages = 0;
        for (int i = 0; buffer[i] != '\0'; i++) {
            if (buffer[i] == ' ') {
                word[wptr] = '\0';
                wptr = 0;
                stages++;
                // process word here
                // <schedule/nil> <float/nil> <type> <action/nil> <value> <value/nil>
            } else {
                word[wptr] = buffer[i];
                wptr++;
            }
        }
        if (!event.complete) {
            EZ_FATAL("Broken test command detected on line %d: \"%s\"", lines, buffer);
        }
        if (event.scheduled) {
            ARRLIST_TestEvent_add(&g_scheduled_events, event);
        } else {
            ARRLIST_TestEvent_add(&g_free_events, event);
        }
    }
    ez_free_file(ezf);
}

void CleanTester() {
    ARRLIST_TestEvent_clear(&g_scheduled_events);
    ARRLIST_TestEvent_clear(&g_free_events);
}
