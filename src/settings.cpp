#include "settings.h"

#define EEPROM_SIZE 512
#define SETTINGS_MAGIC 0xCAFE
#define SETTINGS_ADDR 0
#define BOOT_COUNTER_MAGIC 0xB007
#define BOOT_COUNTER_ADDR (SETTINGS_ADDR + sizeof(uint16_t) + sizeof(Settings))
#define BOOT_FAILURE_THRESHOLD 5  // Reset EEPROM after 5 consecutive boot failures
#define POWER_CYCLE_COUNTER_MAGIC 0x5C01  // 5C = "Power Cycle"
#define POWER_CYCLE_COUNTER_ADDR (BOOT_COUNTER_ADDR + sizeof(BootCounter))
#define POWER_CYCLE_THRESHOLD 5  // Factory reset after 5 quick power cycles

// CRC32 lookup table for fast calculation
static const uint32_t crc32_table[16] = {
    0x00000000, 0x1db71064, 0x3b6e20c8, 0x26d930ac,
    0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
    0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c,
    0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c
};

void settingsInit() {
    EEPROM.begin(EEPROM_SIZE);
}

// Calculate CRC32 checksum for settings (excluding the CRC field itself)
uint32_t settingsCalculateCRC(const Settings &settings) {
    uint32_t crc = 0xFFFFFFFF;
    const uint8_t *data = (const uint8_t*)&settings;
    size_t len = sizeof(Settings) - sizeof(uint32_t); // Exclude CRC field

    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        crc = crc32_table[crc & 0x0F] ^ (crc >> 4);
        crc = crc32_table[crc & 0x0F] ^ (crc >> 4);
    }

    return ~crc;
}

// Validate settings structure
bool settingsValidate(const Settings &settings) {
    // Check version compatibility
    if (settings.version != FIRMWARE_VERSION) {
        Serial.printf("Settings version mismatch: expected %d, got %d\n",
                      FIRMWARE_VERSION, settings.version);
        return false;
    }

    // Validate CRC
    uint32_t calculatedCRC = settingsCalculateCRC(settings);
    if (calculatedCRC != settings.crc) {
        Serial.printf("Settings CRC mismatch: expected 0x%08X, got 0x%08X\n",
                      calculatedCRC, settings.crc);
        return false;
    }

    // Sanity checks on values
    if (settings.brightness < 0 || settings.brightness > 100) {
        Serial.println("Settings brightness out of range");
        return false;
    }

    if (settings.theme < 0 || settings.theme > 10) {
        Serial.println("Settings theme out of range");
        return false;
    }

    return true;
}

// Reset settings to factory defaults
void settingsReset(Settings &settings) {
    Serial.println("Resetting settings to factory defaults");

    settings.version = FIRMWARE_VERSION;
    settings.brightness = 70;
    settings.theme = 0;
    settings.lastImage[0] = '\0';
    settings.gmtOffset = 3600;      // Default to +1 hour (CET)
    settings.valid = true;
    settings.crc = settingsCalculateCRC(settings);
}

void settingsLoad(Settings &settings) {
    uint16_t magic;
    EEPROM.get(SETTINGS_ADDR, magic);

    if (magic == SETTINGS_MAGIC) {
        EEPROM.get(SETTINGS_ADDR + 2, settings);

        // Validate loaded settings
        if (!settingsValidate(settings)) {
            Serial.println("Settings validation failed - resetting to defaults");
            settingsReset(settings);
            settingsSave(settings);  // Save valid defaults
        } else {
            Serial.println("Settings loaded and validated successfully");
        }
    } else {
        Serial.println("No valid settings found - initializing defaults");
        settingsReset(settings);
        settingsSave(settings);  // Save defaults on first boot
    }
}

void settingsSave(const Settings &settings) {
    Settings tempSettings = settings;
    tempSettings.version = FIRMWARE_VERSION;
    tempSettings.crc = settingsCalculateCRC(tempSettings);

    uint16_t magic = SETTINGS_MAGIC;
    EEPROM.put(SETTINGS_ADDR, magic);
    EEPROM.put(SETTINGS_ADDR + 2, tempSettings);
    EEPROM.commit();

    Serial.println("Settings saved with CRC validation");
}

// Boot counter functions for failure detection
void bootCounterInit() {
    // Boot counter is already initialized by EEPROM.begin()
    // Just increment the failure counter
    bootCounterIncrement();
}

uint8_t bootCounterGet() {
    BootCounter counter;
    uint16_t magic;

    EEPROM.get(BOOT_COUNTER_ADDR, magic);

    if (magic == BOOT_COUNTER_MAGIC) {
        EEPROM.get(BOOT_COUNTER_ADDR, counter);
        return counter.failCount;
    }

    return 0;
}

void bootCounterIncrement() {
    BootCounter counter;
    uint16_t magic;

    EEPROM.get(BOOT_COUNTER_ADDR, magic);

    if (magic == BOOT_COUNTER_MAGIC) {
        EEPROM.get(BOOT_COUNTER_ADDR, counter);
        counter.failCount++;
    } else {
        // Initialize boot counter
        counter.magic = BOOT_COUNTER_MAGIC;
        counter.failCount = 1;
        counter.lastBootTime = 0;
    }

    EEPROM.put(BOOT_COUNTER_ADDR, counter);
    EEPROM.commit();

    Serial.printf("Boot failure count: %d\n", counter.failCount);
}

void bootCounterReset() {
    BootCounter counter;
    counter.magic = BOOT_COUNTER_MAGIC;
    counter.failCount = 0;
    counter.lastBootTime = millis();

    EEPROM.put(BOOT_COUNTER_ADDR, counter);
    EEPROM.commit();

    Serial.println("Boot counter reset - successful boot");
}

bool bootCounterCheckFailsafe() {
    uint8_t failCount = bootCounterGet();

    if (failCount >= BOOT_FAILURE_THRESHOLD) {
        Serial.printf("FAILSAFE: Boot failure threshold reached (%d failures)\n", failCount);
        return true;
    }

    return false;
}

// Power cycle counter functions for user-initiated factory reset
void powerCycleCounterInit() {
    // Increment the power cycle counter
    powerCycleCounterIncrement();
}

uint8_t powerCycleCounterGet() {
    PowerCycleCounter counter;
    uint16_t magic;

    EEPROM.get(POWER_CYCLE_COUNTER_ADDR, magic);

    if (magic == POWER_CYCLE_COUNTER_MAGIC) {
        EEPROM.get(POWER_CYCLE_COUNTER_ADDR, counter);
        return counter.cycleCount;
    }

    return 0;
}

void powerCycleCounterIncrement() {
    PowerCycleCounter counter;
    uint16_t magic;

    EEPROM.get(POWER_CYCLE_COUNTER_ADDR, magic);

    if (magic == POWER_CYCLE_COUNTER_MAGIC) {
        EEPROM.get(POWER_CYCLE_COUNTER_ADDR, counter);
        counter.cycleCount++;
    } else {
        // Initialize power cycle counter
        counter.magic = POWER_CYCLE_COUNTER_MAGIC;
        counter.cycleCount = 1;
    }

    EEPROM.put(POWER_CYCLE_COUNTER_ADDR, counter);
    EEPROM.commit();

    Serial.printf("Power cycle count: %d/%d\n", counter.cycleCount, POWER_CYCLE_THRESHOLD);
}

void powerCycleCounterReset() {
    PowerCycleCounter counter;
    counter.magic = POWER_CYCLE_COUNTER_MAGIC;
    counter.cycleCount = 0;

    EEPROM.put(POWER_CYCLE_COUNTER_ADDR, counter);
    EEPROM.commit();

    Serial.println("Power cycle counter reset");
}

bool powerCycleCounterCheckReset() {
    uint8_t cycleCount = powerCycleCounterGet();

    if (cycleCount >= POWER_CYCLE_THRESHOLD) {
        Serial.printf("USER RESET: Power cycle threshold reached (%d cycles)\n", cycleCount);
        return true;
    }

    return false;
}
