#ifndef CONFIG_H
#define CONFIG_H

#include "ui/notification.h"
#include <raylib.h>

#define BLACKBOARD_STRING_SIZE 64

typedef enum {
    BB_INT,
    BB_FLOAT,
    BB_BOOL,
    BB_POINTER,
    BB_CAMERA,
    BB_SIZE,
    BB_LEVEL,
    BB_STRING
} BlackBoardType;

typedef struct {
    char str[BLACKBOARD_STRING_SIZE];
} BlackBoardKey;

typedef struct {
    BlackBoardType type;
    union {
        int _int;
        float _float;
        BOOL _bool;
        void* _ptr;
        size_t _size;
        Camera2D _camera;
        MessageLevel _level;
        char _string[BLACKBOARD_STRING_SIZE];
    } value;
} BlackBoardValue;

DECLARE_HASHMAP(BlackBoardKey, BlackBoardValue, BlackBoard);

typedef struct {
    HASHMAP_BlackBoard blackboard;
} AbstractConfig;

BOOL ConfigHas(const char* key);

void ConfigSet(const char* key, BlackBoardValue val);

void ConfigSetInt(const char* key, int v);

void ConfigSetFloat(const char* key, float v);

void ConfigSetBool(const char* key, BOOL v);

void ConfigSetPointer(const char* key, void* v);

void ConfigSetCamera(const char* key, Camera2D v);

void ConfigSetSize(const char* key, size_t v);

void ConfigSetMessageLevel(const char* key, MessageLevel v);

void ConfigSetString(const char* key, const char* v);

BlackBoardValue ConfigGet(const char* key);

int ConfigGetInt(const char* key);

float ConfigGetFloat(const char* key);

BOOL ConfigGetBool(const char* key);

void* ConfigGetPointer(const char* key);

Camera2D ConfigGetCamera(const char* key);

size_t ConfigGetSize(const char* key);

MessageLevel ConfigGetMessageLevel(const char* key);

const char* ConfigGetString(const char* key);

void InitConfig();

void CleanConfig();

#endif
