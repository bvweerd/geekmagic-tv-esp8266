#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>

#define LOG_BUFFER_SIZE 50
#define LOG_LINE_LENGTH 128

void loggerInit();
void logPrint(const String &msg);
void logPrintf(const char* format, ...);
String logGetAll();

#endif
