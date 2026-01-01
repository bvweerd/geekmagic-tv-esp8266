#include "display.h"
#include "config.h"
#include "logger.h"
#include <LittleFS.h>
#include <TJpg_Decoder.h>
#include <ESP8266WiFi.h>
#include <vector>
#include <time.h> // For time and date functions

TFT_eSPI tft = TFT_eSPI();
DisplayState displayState;
int scrollPos = 240;

bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
    if (y >= tft.height()) return 0;
    tft.pushImage(x, y, w, h, bitmap);
    return 1;
}

// Helper function to wrap text
std::vector<String> wrapText(String text, int font, int maxWidth) {
    std::vector<String> lines;
    if (text.isEmpty()) {
        lines.push_back("");
        return lines;
    }

    // Set the font for text width calculations
    tft.setTextFont(font);

    String currentLine = "";
    String word = "";
    // Temporarily increase buffer size to handle longer lines during word accumulation
    currentLine.reserve(text.length() + 10); 
    word.reserve(text.length() + 10);

    for (int i = 0; i < text.length(); i++) {
        char c = text.charAt(i);
        if (c == ' ') {
            // Check if adding the word exceeds maxWidth
            if (tft.textWidth(currentLine + word) > maxWidth) { // Use textWidth with current font set
                // If current line is not empty, add it and start a new line with the word
                if (!currentLine.isEmpty()) {
                    lines.push_back(currentLine);
                    currentLine = word; // Start new line with the current word
                } else {
                    // Word itself is longer than maxWidth, force break within word if needed
                    // For simplicity, for now just add the too-long word on its own line
                    lines.push_back(word);
                    currentLine = "";
                }
            } else {
                currentLine += word;
            }
            currentLine += " "; // Add space after word
            word = "";
        } else {
            word += c;
        }
    }

    // Add the last word/part of word
    if (!word.isEmpty()) {
        if (tft.textWidth(currentLine + word) > maxWidth) { // Use textWidth with current font set
            if (!currentLine.isEmpty()) {
                lines.push_back(currentLine);
            }
            lines.push_back(word);
        } else {
            currentLine += word;
        }
    }

    // Add the last line if not empty
    if (!currentLine.isEmpty()) {
        lines.push_back(currentLine);
    }

    return lines;
}

String getFormattedDate() {
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    char dateBuffer[11]; // DD-MM-YYYY\0
    strftime(dateBuffer, sizeof(dateBuffer), "%d-%m-%Y", &timeinfo);
    return String(dateBuffer);
}

void displayInit() {
    logPrint("Display init...");

    tft.init();
    tft.setRotation(0);
    tft.invertDisplay(true);  // Match ESPHome invert_colors: true

    logPrint("Display initialized, testing colors...");

    // Test: fill screen with colors
    tft.fillScreen(TFT_RED);
    delay(500);
    tft.fillScreen(TFT_GREEN);
    delay(500);
    tft.fillScreen(TFT_BLUE);
    delay(500);
    tft.fillScreen(TFT_WHITE);
    delay(500);
    tft.fillScreen(TFT_BLACK);

    // Test text
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("INIT", 120, 120, 4);

    TJpgDec.setJpgScale(1);
    TJpgDec.setSwapBytes(true);
    TJpgDec.setCallback(tft_output);

    displayState.line1 = "00:00";
    displayState.line2 = ""; // Initialize custom message line as empty
    displayState.showImage = false;
    displayState.imagePath = "";

    // Backlight on (inverted PWM: low value = bright)
    pinMode(PIN_BACKLIGHT, OUTPUT);
    analogWriteFreq(1000);            // Zet PWM frequency
    analogWriteRange(1023);           // 10bit

    // Test: first full brightness
    logPrint("Testing backlight...");
    analogWrite(PIN_BACKLIGHT, 0);    // Inverted: 0 = full bright
    delay(500);

    logPrint("Display init complete");
}

void displaySetBrightness(int brightness) {
    brightness = constrain(brightness, 0, 100);

    // ESP8266 PWM range is 0-1023
    // Hardware is inverted: LOW = bright, HIGH = off
    int pwmValue;

    if (brightness == 0) {
        pwmValue = 1023;  // Off
    } else if (brightness == 100) {
        pwmValue = 0;     // Full bright
    } else {
        // Map 1-99 to 1023-0 (inverted)
        pwmValue = map(brightness, 0, 100, 1023, 0);
    }

    analogWrite(PIN_BACKLIGHT, pwmValue);
    logPrintf("Brightness: %d%%, PWM: %d", brightness, pwmValue);
}

int getWiFiSignalPercent() {
    long rssi = WiFi.RSSI();
    if (rssi <= -100) return 0;
    if (rssi >= -50) return 100;
    return 2 * (rssi + 100);
}

void displayUpdate() {
    logPrint("displayUpdate() called");
    if (displayState.showImage && !displayState.imagePath.isEmpty()) {
        logPrint("Rendering image: " + displayState.imagePath);
        displayRenderImage(displayState.imagePath);
    } else {
        logPrint("Rendering clock");
        displayRenderClock();
    }
    logPrint("displayUpdate() done");
}

void displayRenderClock() {
    logPrint("displayRenderClock START (Simplified)");

    tft.fillScreen(TFT_BLACK);

    // Display Time (displayState.line1) - Centered
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextDatum(TC_DATUM);
    int timeFont = 6;
    tft.setTextFont(timeFont);
    int timeLineHeight = tft.fontHeight();
    std::vector<String> timeWrappedLines = wrapText(displayState.line1, timeFont, tft.width());
    
    // Initial Y for time block, adjusted for multiple lines and vertical centering
    int totalTextHeight = timeWrappedLines.size() * timeLineHeight;
    int currentY = (tft.height() / 2) - (totalTextHeight / 2);

    for (int i = 0; i < timeWrappedLines.size(); i++) {
        tft.drawString(timeWrappedLines[i], tft.width() / 2, currentY + (i * timeLineHeight), timeFont);
    }
    currentY += totalTextHeight; // Move Y past the time block

    // Display Date (getFormattedDate()) - Centered, below time
    String currentDate = getFormattedDate();
    int dateFont = 4;
    tft.setTextFont(dateFont);
    int dateLineHeight = tft.fontHeight();
    std::vector<String> dateWrappedLines = wrapText(currentDate, dateFont, tft.width());

    // Add some padding between time and date
    currentY += dateLineHeight / 2; // Roughly half a line height padding

    for (int i = 0; i < dateWrappedLines.size(); i++) {
        tft.drawString(dateWrappedLines[i], tft.width() / 2, currentY + (i * dateLineHeight), dateFont);
    }
    currentY += dateWrappedLines.size() * dateLineHeight; // Move Y past the date block

    // Display Custom Message (displayState.line2) - Centered, below date
    if (!displayState.line2.isEmpty()) {
        int messageFont = 2; // Smaller font for messages
        tft.setTextFont(messageFont);
        int messageLineHeight = tft.fontHeight();
        std::vector<String> messageWrappedLines = wrapText(displayState.line2, messageFont, tft.width());
        
        // Add some padding between date and message
        currentY += messageLineHeight / 2;

        for (int i = 0; i < messageWrappedLines.size(); i++) {
            tft.drawString(messageWrappedLines[i], tft.width() / 2, currentY + (i * messageLineHeight), messageFont);
        }
    }

    logPrint("displayRenderClock DONE (Simplified)");
}

void displayRenderImage(const String &path) {
    if (!LittleFS.exists(path)) {
        displayShowMessage("Image not found");
        return;
    }

    File jpgFile = LittleFS.open(path, "r");
    if (!jpgFile) {
        String errorMsg = "Failed to open image file: " + path;
        displayShowMessage(errorMsg);
        logPrint(errorMsg);
        return;
    }
    logPrint("INFO: Image file opened: " + path);

    tft.fillScreen(TFT_BLACK);

    JRESULT res = TJpgDec.drawFsJpg(0, 0, jpgFile);
    if (res != JDR_OK) {
        String errorMsg = "JPEG Decode Failed\nCode: " + String(res);
        displayShowMessage(errorMsg);
        logPrint(errorMsg);
    }

    jpgFile.close(); // Close the file after decoding attempt
}

void displayShowMessage(const String &msg) {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextDatum(MC_DATUM);

    int startY = tft.height() / 2; // Starting Y position, will be adjusted
    int font = 4;
    
    // Calculate line height based on the font
    tft.setTextFont(font); // Set font for height calculation
    int lineHeight = tft.fontHeight();

    std::vector<String> wrappedLines = wrapText(msg, font, tft.width());

    // Adjust startY to vertically center the block of text
    startY -= (wrappedLines.size() * lineHeight) / 2;

    // Draw each wrapped line
    for (int i = 0; i < wrappedLines.size(); i++) {
        tft.drawString(wrappedLines[i], tft.width() / 2, startY + (i * lineHeight), font);
    }
}
