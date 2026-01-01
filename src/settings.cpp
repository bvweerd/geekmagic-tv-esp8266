#include "settings.h"

#define EEPROM_SIZE 512
#define SETTINGS_MAGIC 0xCAFE
#define SETTINGS_ADDR 0

void settingsInit() {
    EEPROM.begin(EEPROM_SIZE);
}

void settingsLoad(Settings &settings) {
    uint16_t magic;
    EEPROM.get(SETTINGS_ADDR, magic);

    if (magic == SETTINGS_MAGIC) {
        EEPROM.get(SETTINGS_ADDR + 2, settings);
    } else {
        settings.brightness = 70;
        settings.theme = 0;
        settings.lastImage[0] = '\0';
        settings.gmtOffset = 3600;      // Default to +1 hour (CET)
        settings.valid = true;
    }
}

void settingsSave(const Settings &settings) {
    uint16_t magic = SETTINGS_MAGIC;
    EEPROM.put(SETTINGS_ADDR, magic);
    EEPROM.put(SETTINGS_ADDR + 2, settings);
    EEPROM.commit();
}
