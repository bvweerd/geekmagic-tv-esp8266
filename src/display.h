#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>
#include <TFT_eSPI.h>

struct DisplayState {
    String line1;       // Used for NTP time
    String line2;       // Used for custom messages
    String ipInfo;      // IP address or network info to show at top
    bool showImage;     // True if an image is currently displayed
    String imagePath;   // Path to the image file
};

void displayInit();
void displaySetBrightness(int brightness);
void displayUpdate();
void displayRenderClock();
void displayRenderImage(const String &path);
void displayShowMessage(const String &msg);

extern DisplayState displayState;
extern TFT_eSPI tft;
extern int scrollPos;

#endif
