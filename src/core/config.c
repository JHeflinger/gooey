#include "config.h"
#include "util/logger.h"
#include <easyhash.h>

static uint64_t hash_blackboard_key(BlackBoardKey key) {
    return ez_hash_string(key.str);
}

IMPL_HASHMAP(BlackBoardKey, BlackBoardValue, BlackBoard, hash_blackboard_key);

static AbstractConfig g_config = { 0 };

BOOL ConfigHas(const char* key) {
    EZ_ASSERT(strlen(key) < BLACKBOARD_STRING_SIZE, "Key is too large for blackboard");
    BlackBoardKey bk = { 0 };
    strcpy(bk.str, key);
    return HASHMAP_BlackBoard_has(&(g_config.blackboard), bk);
}

void ConfigSet(const char* key, BlackBoardValue val) {
    EZ_ASSERT(strlen(key) < BLACKBOARD_STRING_SIZE, "Key is too large for blackboard");
    BlackBoardKey bk = { 0 };
    strcpy(bk.str, key);
    HASHMAP_BlackBoard_set(&(g_config.blackboard), bk, val);
}

void ConfigSetInt(const char* key, int v) {
    BlackBoardValue val = { 0 };
    val.type = BB_INT;
    val.value._int = v;
    ConfigSet(key, val);
}

void ConfigSetFloat(const char* key, float v) {
    BlackBoardValue val = { 0 };
    val.type = BB_FLOAT;
    val.value._float = v;
    ConfigSet(key, val);
}

void ConfigSetBool(const char* key, BOOL v) {
    BlackBoardValue val = { 0 };
    val.type = BB_BOOL;
    val.value._bool = v;
    ConfigSet(key, val);
}

void ConfigSetPointer(const char* key, void* v) {
    BlackBoardValue val = { 0 };
    val.type = BB_POINTER;
    val.value._ptr = v;
    ConfigSet(key, val);
}

void ConfigSetCamera(const char* key, Camera2D v) {
    BlackBoardValue val = { 0 };
    val.type = BB_CAMERA;
    val.value._camera = v;
    ConfigSet(key, val);
}

void ConfigSetSize(const char* key, size_t v) {
    BlackBoardValue val = { 0 };
    val.type = BB_SIZE;
    val.value._size = v;
    ConfigSet(key, val);
}

void ConfigSetMessageLevel(const char* key, MessageLevel v) {
    BlackBoardValue val = { 0 };
    val.type = BB_LEVEL;
    val.value._size = v;
    ConfigSet(key, val);
}

void ConfigSetString(const char* key, const char* v) {
    BlackBoardValue val = { 0 };
    val.type = BB_STRING;
    EZ_ASSERT(strlen(v) < BLACKBOARD_STRING_SIZE, "String size too great for a blackboard value");
    strcpy(val.value._string, v);
    ConfigSet(key, val);
}

BlackBoardValue ConfigGet(const char* key) {
    EZ_ASSERT(strlen(key) < BLACKBOARD_STRING_SIZE, "Key is too large for blackboard");
    BlackBoardKey bk = { 0 };
    strcpy(bk.str, key);
    return HASHMAP_BlackBoard_get(&(g_config.blackboard), bk);
}

int ConfigGetInt(const char* key) {
    BlackBoardValue val = ConfigGet(key);
    EZ_ASSERT(val.type == BB_INT, "BlackBoard value was requested to be an int but was not of that type");
    return val.value._int;
}

float ConfigGetFloat(const char* key) {
    BlackBoardValue val = ConfigGet(key);
    EZ_ASSERT(val.type == BB_FLOAT, "BlackBoard value was requested to be a float but was not of that type");
    return val.value._float;
}

BOOL ConfigGetBool(const char* key) {
    BlackBoardValue val = ConfigGet(key);
    EZ_ASSERT(val.type == BB_BOOL, "BlackBoard value was requested to be a bool but was not of that type");
    return val.value._bool;
}

void* ConfigGetPointer(const char* key) {
    BlackBoardValue val = ConfigGet(key);
    EZ_ASSERT(val.type == BB_POINTER, "BlackBoard value was requested to be a pointer but was not of that type");
    return val.value._ptr;
}

Camera2D ConfigGetCamera(const char* key) {
    BlackBoardValue val = ConfigGet(key);
    EZ_ASSERT(val.type == BB_CAMERA, "BlackBoard value was requested to be a camera but was not of that type");
    return val.value._camera;
}

size_t ConfigGetSize(const char* key) {
    BlackBoardValue val = ConfigGet(key);
    EZ_ASSERT(val.type == BB_SIZE, "BlackBoard value was requested to be a size_t but was not of that type");
    return val.value._size;
}

MessageLevel ConfigGetMessageLevel(const char* key) {
    BlackBoardValue val = ConfigGet(key);
    EZ_ASSERT(val.type == BB_LEVEL, "BlackBoard value was requested to be a MessageLevel but was not of that type");
    return val.value._level;
}

const char* ConfigGetString(const char* key) {
    BlackBoardValue val = ConfigGet(key);
    EZ_ASSERT(val.type == BB_STRING, "BlackBoard value was requested to be a string but was not of that type");
    EZ_ASSERT(strlen(key) < BLACKBOARD_STRING_SIZE, "Key is too large for blackboard");
    BlackBoardKey bk = { 0 };
    strcpy(bk.str, key);
    return HASHMAP_BlackBoard_get(&(g_config.blackboard), bk).value._string;
}

void InitConfig() {
    FILE *file = fopen(".gooeyconf", "rb");
    if (file) {
        fseek(file, 0, SEEK_END);
        long filesize = ftell(file);
        rewind(file);
        if ((size_t)filesize < sizeof(AbstractConfig)) {
            logwarn("Existing config is invalid size");
        } else {
            AbstractConfig oldconf = g_config;
            fread(&g_config, 1, sizeof(AbstractConfig), file);
            if ((size_t)filesize == sizeof(AbstractConfig) + (g_config.blackboard.capacity * sizeof(HASHENTRY_BlackBoard))) {
                HASHMAP_BlackBoard_clear(&(oldconf.blackboard));
                g_config.blackboard.entries = EZ_ALLOC(g_config.blackboard.capacity, sizeof(HASHENTRY_BlackBoard));
                fread(g_config.blackboard.entries, g_config.blackboard.capacity, sizeof(HASHENTRY_BlackBoard), file);
            } else {
                logwarn("Existing config has invalid size");
                g_config = oldconf;
            }
        }
        fclose(file);
    } else {
        logtrace("Unable to detect existing config");
    }
}

void CleanConfig() {
    FILE *file = fopen(".gooeyconf", "wb");
    if (file) {
        fwrite(&g_config, 1, sizeof(AbstractConfig), file);
        fwrite(g_config.blackboard.entries, g_config.blackboard.capacity, sizeof(HASHENTRY_BlackBoard), file);
        fclose(file);
    }
    HASHMAP_BlackBoard_clear(&(g_config.blackboard));
}
