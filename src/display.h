#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>
#include <TFT_eSPI.h>

// Define buffer sizes for DisplayState char arrays
#define DISPLAY_LINE_BUFFER_SIZE 64
#define DISPLAY_IP_BUFFER_SIZE 24
#define DISPLAY_PATH_BUFFER_SIZE 64
#define DISPLAY_SSID_BUFFER_SIZE 36
#define DISPLAY_PASS_BUFFER_SIZE 16

struct DisplayState {
    char line1[16];       // Used for NTP time
    char line2[DISPLAY_LINE_BUFFER_SIZE];       // Used for custom messages
    char ipInfo[DISPLAY_IP_BUFFER_SIZE];      // IP address or network info to show at top
    bool showImage;     // True if an image is currently displayed
    char imagePath[DISPLAY_PATH_BUFFER_SIZE];   // Path to the image file
    bool apMode;        // True when showing AP mode credentials screen
    char apSSID[DISPLAY_SSID_BUFFER_SIZE];      // AP mode SSID to display
    char apPassword[DISPLAY_PASS_BUFFER_SIZE];  // AP mode password to display
};

// Previous state for selective redraw (eliminate flickering)
struct PreviousDisplayState {
    char line1[16];
    char line2[DISPLAY_LINE_BUFFER_SIZE];
    char ipInfo[DISPLAY_IP_BUFFER_SIZE];
    char date[16];
    int prevTimeWidth;  // Track previous time text width for proper clearing
    int prevDateWidth; // Track previous date text width for proper clearing
    bool initialized;
};

void displayInit();
void displaySetBrightness(int brightness);
void displayUpdate();
void displayRenderClock();
void displayRenderAPMode();
void displayRenderImage(const char *path);
void displayShowMessage(const String &msg);
void displayShowAPScreen(const char* ssid, const char* password, const char* ip);
void displayBlankScreen();
void displayCycleNextPage();
void displayToggleBacklight();

extern DisplayState displayState;
extern TFT_eSPI tft;
extern TFT_eSprite sprite;  // Framebuffer for atomic screen updates
extern int scrollPos;

#endif
