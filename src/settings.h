#ifndef SETTINGS_H
#define SETTINGS_H

#include <Arduino.h>
#include <EEPROM.h>

struct Settings {
    int brightness;
    int theme;
    char lastImage[64];
    long gmtOffset;       // GMT offset in seconds
    bool valid;
};

void settingsInit();
void settingsLoad(Settings &settings);
void settingsSave(const Settings &settings);

#endif
