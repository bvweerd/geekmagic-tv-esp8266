#include "logger.h"

char logBuffer[LOG_BUFFER_SIZE][LOG_LINE_LENGTH];
int logIndex = 0;
int logCount = 0;

void loggerInit() {
    memset(logBuffer, 0, sizeof(logBuffer));
    logIndex = 0;
    logCount = 0;
}

void logPrint(const String &msg) {
    // Print to serial
    Serial.println(msg);

    // Store in circular buffer
    strncpy(logBuffer[logIndex], msg.c_str(), LOG_LINE_LENGTH - 1);
    logBuffer[logIndex][LOG_LINE_LENGTH - 1] = '\0';

    logIndex = (logIndex + 1) % LOG_BUFFER_SIZE;
    if (logCount < LOG_BUFFER_SIZE) {
        logCount++;
    }
}

void logPrintf(const char* format, ...) {
    char buffer[LOG_LINE_LENGTH];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, LOG_LINE_LENGTH, format, args);
    va_end(args);

    logPrint(String(buffer));
}

String logGetAll() {
    String result = "";
    result.reserve(LOG_BUFFER_SIZE * LOG_LINE_LENGTH);

    int start = (logCount < LOG_BUFFER_SIZE) ? 0 : logIndex;
    int entries = (logCount < LOG_BUFFER_SIZE) ? logCount : LOG_BUFFER_SIZE;

    for (int i = 0; i < entries; i++) {
        int idx = (start + i) % LOG_BUFFER_SIZE;
        result += logBuffer[idx];
        result += "\n";
    }

    return result;
}
