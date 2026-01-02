#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include <LittleFS.h>
#include <WiFiUdp.h> // Required for NTPClient
#include <NTPClient.h> // For NTP client library
#include "config.h"
#include "display.h"
#include "webserver.h"
#include "settings.h"
#include "logger.h"

// NTP server (for NTPClient)
#define NTP_SERVER "pool.ntp.org"

// Define NTP Client
Settings appSettings;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, NTP_SERVER, appSettings.gmtOffset);
WiFiManager wifiManager;
unsigned long lastDisplayUpdate = 0;
unsigned long lastWiFiCheck = 0;
unsigned long lastWiFiReconnectAttempt = 0;
bool wifiFailsafeMode = false;  // True when in AP-only mode after connection failures

bool tryConnectWiFi(int maxAttempts) {
    Serial.printf("Attempting WiFi connection (max %d attempts)...\n", maxAttempts);

    for (int attempt = 1; attempt <= maxAttempts; attempt++) {
        Serial.printf("WiFi attempt %d/%d\n", attempt, maxAttempts);
        displayShowMessage("WiFi...\nAttempt " + String(attempt) + "/" + String(maxAttempts));

        WiFi.mode(WIFI_STA);
        WiFi.begin();

        unsigned long startAttempt = millis();
        while (WiFi.status() != WL_CONNECTED &&
               millis() - startAttempt < WIFI_CONNECTION_TIMEOUT) {
            delay(100);
            yield();
        }

        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("WiFi connected!");
            Serial.printf("IP: %s\n", WiFi.localIP().toString().c_str());
            displayShowMessage("WiFi OK\n" + WiFi.localIP().toString());
            delay(2000);
            return true;
        }

        // Exponential backoff between retries (except on last attempt)
        if (attempt < maxAttempts) {
            int delayMs = WIFI_RETRY_DELAY_MS * (1 << (attempt - 1)); // 2s, 4s, 8s, 16s...
            delayMs = min(delayMs, 30000); // Cap at 30 seconds
            Serial.printf("Retry in %d ms...\n", delayMs);
            delay(delayMs);
        }
    }

    return false;
}

void setupWiFi() {
    displayShowMessage("WiFi Setup...");

    // First try to connect to saved WiFi credentials with retry
    if (tryConnectWiFi(WIFI_RETRY_ATTEMPTS)) {
        wifiFailsafeMode = false;
        return;
    }

    // If connection failed, start AP mode with WiFiManager
    Serial.println("WiFi connection failed - starting AP mode");
    displayShowMessage("WiFi Failed!\nStarting AP...");
    delay(2000);

    wifiManager.setConfigPortalTimeout(WIFI_TIMEOUT);

    if (!wifiManager.autoConnect(WIFI_AP_NAME, WIFI_AP_PASSWORD)) {
        // WiFiManager timeout - enter failsafe AP-only mode
        Serial.println("WiFiManager timeout - entering failsafe AP mode");
        displayShowMessage("Failsafe Mode\nAP Active");

        WiFi.mode(WIFI_AP);
        WiFi.softAP(WIFI_AP_NAME, WIFI_AP_PASSWORD);

        wifiFailsafeMode = true;

        Serial.printf("Failsafe AP started. IP: %s\n", WiFi.softAPIP().toString().c_str());
        displayShowMessage("AP Mode\n" + WiFi.softAPIP().toString());
        delay(3000);
    } else {
        wifiFailsafeMode = false;
        displayShowMessage("WiFi OK\n" + WiFi.localIP().toString());
        delay(2000);
    }
}

void monitorWiFi() {
    // In failsafe mode, periodically try to reconnect to WiFi
    if (wifiFailsafeMode) {
        if (millis() - lastWiFiReconnectAttempt > WIFI_RECONNECT_INTERVAL) {
            Serial.println("Failsafe mode: attempting WiFi reconnection...");
            lastWiFiReconnectAttempt = millis();

            if (tryConnectWiFi(2)) {  // Quick 2 attempts
                wifiFailsafeMode = false;
                Serial.println("Reconnected! Exiting failsafe mode");
                // Restart to reinitialize services properly
                delay(1000);
                ESP.restart();
            }
        }
        return;
    }

    // Normal mode: check WiFi connection periodically
    if (millis() - lastWiFiCheck > WIFI_MONITOR_INTERVAL) {
        lastWiFiCheck = millis();

        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("WiFi connection lost - attempting reconnection");

            if (!tryConnectWiFi(3)) {  // Try 3 quick attempts
                Serial.println("Reconnection failed - entering failsafe mode");
                displayShowMessage("WiFi Lost!\nAP Mode");

                WiFi.mode(WIFI_AP);
                WiFi.softAP(WIFI_AP_NAME, WIFI_AP_PASSWORD);
                wifiFailsafeMode = true;

                Serial.printf("Failsafe AP started. IP: %s\n", WiFi.softAPIP().toString().c_str());
            }
        }
    }
}

void setupMDNS() {
    if (!MDNS.begin(MDNS_HOSTNAME)) {
        Serial.println("mDNS failed");
        return;
    }

    MDNS.addService("http", "tcp", WEB_SERVER_PORT);
    MDNS.addServiceTxt("http", "tcp", "model", "SmartClock");
    MDNS.addServiceTxt("http", "tcp", "vendor", "Custom");
    MDNS.addServiceTxt("http", "tcp", "api", "geekmagic");

    Serial.println("mDNS started: " MDNS_HOSTNAME ".local");
}

void setupOTA() {
    ArduinoOTA.setHostname(OTA_HOSTNAME);
    ArduinoOTA.setPassword(OTA_PASSWORD);

    ArduinoOTA.onStart([]() {
        String type = (ArduinoOTA.getCommand() == U_FLASH) ? "firmware" : "filesystem";
        Serial.println("OTA Start: " + type);
        displayShowMessage("OTA Update...");
    });

    ArduinoOTA.onEnd([]() {
        Serial.println("OTA Complete");
        displayShowMessage("Success!");
    });

    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        int percent = (progress * 100) / total;
        Serial.printf("Progress: %u%%\n", percent);

        static int lastPercent = -1;
        if (percent != lastPercent) {
            tft.fillRect(20, 130, 200, 20, TFT_BLACK);
            tft.drawRect(20, 130, 200, 20, TFT_WHITE);
            tft.fillRect(22, 132, (percent * 196) / 100, 16, TFT_BLUE);
            lastPercent = percent;
        }
    });

    ArduinoOTA.onError([](ota_error_t error) {
        Serial.printf("OTA Error[%u]: ", error);
        displayShowMessage("OTA Failed!");
    });

    ArduinoOTA.begin();
    Serial.println("OTA ready");
}

void clearImageDirectory() {
    logPrint("Clearing image directory: " + String(IMAGE_DIR));
    Dir dir = LittleFS.openDir(IMAGE_DIR);
    int filesDeleted = 0;
    while (dir.next()) {
        String filepath = dir.fileName();
        if (LittleFS.remove(filepath)) {
            logPrintf("Deleted: %s", filepath.c_str());
            filesDeleted++;
        } else {
            logPrintf("Failed to delete: %s", filepath.c_str());
        }
    }
    logPrintf("Cleared %d files from image directory.", filesDeleted);
}

void setupFilesystem() {
    //LittleFS.format();
    if (!LittleFS.begin()) {
        Serial.println("LittleFS mount failed");
        displayShowMessage("FS Failed!");
        delay(3000);
        return;
    }

    if (!LittleFS.exists(IMAGE_DIR)) {
        LittleFS.mkdir(IMAGE_DIR);
    }

    Serial.println("LittleFS ready");

    clearImageDirectory(); // Call the new function here
}

void setup() {
    Serial.begin(115200);
    delay(100);

    loggerInit();
    logPrint("\n\n========================================");
    logPrint("SmartClock Starting...");
    logPrintf("Firmware Version: %d", FIRMWARE_VERSION);

    // Initialize EEPROM and boot counter
    settingsInit();
    bootCounterInit();  // Increment boot failure counter

    // Check for boot failure threshold
    if (bootCounterCheckFailsafe()) {
        Serial.println("========================================");
        Serial.println("CRITICAL: Boot failure threshold reached!");
        Serial.println("Performing emergency EEPROM reset...");
        Serial.println("========================================");

        // Emergency reset
        settingsReset(appSettings);
        settingsSave(appSettings);
        bootCounterReset();

        Serial.println("EEPROM reset complete. System will restart in 5 seconds...");
        delay(5000);
        ESP.restart();
    }

    // Load and validate settings
    settingsLoad(appSettings);

    displayInit();
    displaySetBrightness(100);  // Full brightness for testing

    displayShowMessage("SmartClock\nInitializing...");
    delay(2000);

    setupFilesystem();
    setupWiFi();

    // NTP initialization using NTPClient library
    if (!wifiFailsafeMode) {
        timeClient.begin();
        logPrint("NTP: Initializing time synchronization using NTPClient...");
    } else {
        logPrint("NTP: Skipped (failsafe mode)");
    }

    // Only setup mDNS and OTA if not in failsafe mode
    if (!wifiFailsafeMode) {
        setupMDNS();
        setupOTA();
    } else {
        logPrint("mDNS/OTA: Skipped (failsafe mode)");
    }

    currentBrightness = 100;  // Keep full brightness
    currentTheme = appSettings.theme;
    currentImage = String(appSettings.lastImage);

    // Always default to clock display on boot as images are cleared
    displayState.showImage = false;

    webserverInit();

    displayShowMessage("Ready!");
    delay(2000);

    logPrint("Calling displayUpdate()...");
    displayUpdate();
    logPrint("Display updated");

    // Boot successful - reset failure counter
    bootCounterReset();
    logPrint("Boot completed successfully");

    if (wifiFailsafeMode) {
        logPrintf("Running in FAILSAFE mode. AP IP: %s", WiFi.softAPIP().toString().c_str());
    } else {
        logPrintf("Setup complete. IP: %s", WiFi.localIP().toString().c_str());
    }
}

void loop() {
    // Monitor WiFi connection and handle failsafe mode
    monitorWiFi();

    // Only handle OTA and mDNS if not in failsafe mode
    if (!wifiFailsafeMode) {
        ArduinoOTA.handle();
        MDNS.update();
        timeClient.update(); // Keep NTP client updated

        // Only update time if not showing an image
        if (!displayState.showImage) {
            displayState.line1 = timeClient.getFormattedTime();
        }
    } else {
        // In failsafe mode, show a status message
        if (!displayState.showImage) {
            displayState.line1 = "AP Mode";
            displayState.line2 = WiFi.softAPIP().toString();
        }
    }

    webserverHandle();

    if (millis() - lastDisplayUpdate > DISPLAY_UPDATE_INTERVAL) {
        if (!displayState.showImage) {
            displayUpdate();
        }
        lastDisplayUpdate = millis();
    }

    yield();
}
