#ifndef CONFIG_H
#define CONFIG_H

// Pin definitions
#define PIN_BACKLIGHT 5

// Display settings
#define DISPLAY_WIDTH 240
#define DISPLAY_HEIGHT 240

// WiFi settings
#define WIFI_AP_NAME "SmartClock-Setup"
#define WIFI_AP_PASSWORD "smartclock123"
#define WIFI_TIMEOUT 180

// mDNS settings
#define MDNS_HOSTNAME "smartclock"

// OTA settings
#define OTA_HOSTNAME "smartclock"
#define OTA_PASSWORD "admin"

// Web server
#define WEB_SERVER_PORT 80

// Update intervals
#define SCROLL_INTERVAL 30
#define DISPLAY_UPDATE_INTERVAL 5000

// Filesystem
#define IMAGE_DIR "/image/"

// Preferences namespace
#define PREF_NAMESPACE "smartclock"

#endif
