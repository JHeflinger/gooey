#ifndef NOTIFICATION_H
#define NOTIFICATION_H

#include <easyobjects.h>

#define MAX_NOTIFICATION_SIZE 2048

typedef enum {
    LEVEL_NONE  = 0, // no logs at all
    LEVEL_TRACE = 1, // verbose information
    LEVEL_INFO  = 2, // specifically requested information
    LEVEL_WARN  = 3, // information hinting towards failing behavior
    LEVEL_ERROR = 4  // information explicitly stating failure
} MessageLevel;

typedef struct {
    char message[MAX_NOTIFICATION_SIZE];
    MessageLevel level;
} Notification;
DECLARE_ARRLIST(Notification);

void DrawNotifications();

void CleanNotifications();

void Notify(MessageLevel level, char* message, ...);

BOOL ParseMessageLevel(const char* str, MessageLevel* value);

#endif
