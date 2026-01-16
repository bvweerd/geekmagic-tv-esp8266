#ifndef SETTINGS_H
#define SETTINGS_H

#include <Arduino.h>
#include <EEPROM.h>

// Firmware version - increment when Settings structure changes
#define FIRMWARE_VERSION 2

// Semantic version string (replaced by GitHub Action during release builds)
#ifndef FIRMWARE_VERSION_STRING
#define FIRMWARE_VERSION_STRING "dev"
#endif

struct Settings {
    uint16_t version;          // Firmware version for compatibility check
    int brightness;
    int theme;
    char lastImage[64];
    long gmtOffset;            // GMT offset in seconds
    bool valid;
    uint32_t crc;              // CRC32 checksum for data integrity
};

// Boot failure tracking structure (separate from settings)
struct BootCounter {
    uint16_t magic;            // Magic number to validate boot counter
    uint8_t failCount;         // Number of consecutive boot failures
    uint32_t lastBootTime;     // Timestamp of last successful boot
};

// Power cycle reset structure (user-initiated factory reset)
struct PowerCycleCounter {
    uint16_t magic;            // Magic number to validate power cycle counter (0x5C01)
    uint8_t cycleCount;        // Number of quick power cycles
};

void settingsInit();
void settingsLoad(Settings &settings);
void settingsSave(const Settings &settings);
void settingsReset(Settings &settings);
uint32_t settingsCalculateCRC(const Settings &settings);
bool settingsValidate(const Settings &settings);

// Boot counter functions
void bootCounterInit();
uint8_t bootCounterGet();
void bootCounterIncrement();
void bootCounterReset();
bool bootCounterCheckFailsafe();

// Power cycle counter functions (user-initiated factory reset)
void powerCycleCounterInit();
uint8_t powerCycleCounterGet();
void powerCycleCounterIncrement();
void powerCycleCounterReset();
bool powerCycleCounterCheckReset();

#endif
