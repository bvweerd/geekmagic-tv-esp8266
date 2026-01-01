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

void setupWiFi() {
    displayShowMessage("WiFi Setup...");

    wifiManager.setConfigPortalTimeout(WIFI_TIMEOUT);

    if (!wifiManager.autoConnect(WIFI_AP_NAME, WIFI_AP_PASSWORD)) {
        displayShowMessage("WiFi Failed!");
        delay(3000);
        ESP.restart();
    }

    displayShowMessage("WiFi OK\n" + WiFi.localIP().toString());
    delay(2000);
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
    logPrint("\n\nSmartClock Starting...");

    settingsInit();
    settingsLoad(appSettings);

    displayInit();
    displaySetBrightness(100);  // Full brightness for testing

    displayShowMessage("SmartClock\nInitializing...");
    delay(2000);

    setupFilesystem();
    setupWiFi();

    // NTP initialization using NTPClient library
    timeClient.begin();
    // Set time offsets are now part of the global NTPClient constructor
    // timeClient.setTimeOffset(appSettings.gmtOffset);
    // timeClient.setDaylightOffset(appSettings.daylightOffset);
    logPrint("NTP: Initializing time synchronization using NTPClient...");

    setupMDNS();
    setupOTA();

    currentBrightness = 100;  // Keep full brightness
    currentTheme = appSettings.theme;
    currentImage = String(appSettings.lastImage);

    // Always default to clock display on boot as images are cleared
    // if (appSettings.lastImage[0] != '\0') {
    //     displayState.imagePath = String(appSettings.lastImage);
    //     displayState.showImage = true;
    // }
    displayState.showImage = false;

    webserverInit();

    displayShowMessage("Ready!");
    delay(2000);

    logPrint("Calling displayUpdate()...");
    displayUpdate();
    logPrint("Display updated");

    logPrintf("Setup complete. IP: %s", WiFi.localIP().toString().c_str());
}

void loop() {
    ArduinoOTA.handle();
    MDNS.update();
    webserverHandle();

    timeClient.update(); // Keep NTP client updated

    // Only update time if not showing an image
    if (!displayState.showImage) {
        displayState.line1 = timeClient.getFormattedTime();
    }
    

    if (millis() - lastDisplayUpdate > DISPLAY_UPDATE_INTERVAL) {
        if (!displayState.showImage) {
            displayUpdate();
        }
        lastDisplayUpdate = millis();
    }

    yield();
}
