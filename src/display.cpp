#include "display.h"
#include "config.h"
#include "logger.h"
#include <LittleFS.h>
#include <TJpg_Decoder.h>
#include <ESP8266WiFi.h>
#include <vector>
#include <time.h> // For time and date functions

// Font size definitions for clarity
#define FONT_INFO 1      // Small font for IP addresses, etc.
#define FONT_MESSAGE 2   // Font for messages and labels
#define FONT_DEFAULT 4   // Default font size for various things
#define FONT_TIME 6      // Large font for the main clock time

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
    
        for (size_t i = 0; i < text.length(); i++) {
            char c = text.charAt(i);
            if (c == ' ') {            // Check if adding the word exceeds maxWidth
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

void getFormattedDate(char* buffer, size_t bufferSize) {
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    // DD-MM-YYYY\0
    strftime(buffer, bufferSize, "%d-%m-%Y", &timeinfo);
    buffer[bufferSize - 1] = '\0'; // Ensure null-termination
}

void displayInit() {
    logPrint(F("Display init..."));

    tft.init();
    tft.setRotation(0);
    tft.invertDisplay(true);  // Match ESPHome invert_colors: true

    logPrint(F("Display initialized, testing colors..."));

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
    tft.drawString("INIT", 120, 120, FONT_DEFAULT);

    TJpgDec.setJpgScale(1);
    TJpgDec.setSwapBytes(true);
    TJpgDec.setCallback(tft_output);

    strncpy(displayState.line1, "00:00", sizeof(displayState.line1));
    displayState.line2[0] = '\0'; // Initialize custom message line as empty
    displayState.ipInfo[0] = '\0'; // Initialize IP info as empty
    displayState.showImage = false;
    displayState.imagePath[0] = '\0'; // Initialize image path as empty
    displayState.apMode = false;
    displayState.apSSID[0] = '\0';
    displayState.apPassword[0] = '\0';

    // Backlight on (inverted PWM: low value = bright)
    pinMode(PIN_BACKLIGHT, OUTPUT);
    analogWriteFreq(1000);            // Zet PWM frequency
    analogWriteRange(1023);           // 10bit

    // Test: first full brightness
    logPrint(F("Testing backlight..."));
    analogWrite(PIN_BACKLIGHT, 0);    // Inverted: 0 = full bright
    delay(500);

    logPrint(F("Display init complete"));
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
    logPrint(F("displayUpdate() called"));
    if (displayState.showImage && displayState.imagePath[0] != '\0') {
        logPrint(String(F("Rendering image: ")) + displayState.imagePath);
        displayRenderImage((const char*)displayState.imagePath);
    } else if (displayState.apMode) {
        logPrint(F("Rendering AP mode screen"));
        displayRenderAPMode();
    } else {
        logPrint(F("Rendering clock"));
        displayRenderClock();
    }
    logPrint(F("displayUpdate() done"));
}

void displayRenderClock() {
    logPrint(F("displayRenderClock START (Simplified)"));

    tft.fillScreen(TFT_BLACK); // Re-added for previous behavior

    int currentY = 5; // Start from top with small margin

    // Display IP Info at the top (small font)
    if (displayState.ipInfo[0] != '\0') {
        tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
        tft.setTextDatum(TC_DATUM);
        int ipFont = FONT_INFO; // Small font
        tft.setTextFont(ipFont);
        int ipLineHeight = tft.fontHeight();
        std::vector<String> ipWrappedLines = wrapText(String(displayState.ipInfo), ipFont, tft.width() - 10);

        for (size_t i = 0; i < ipWrappedLines.size(); i++) {
            tft.drawString(ipWrappedLines[i], tft.width() / 2, currentY + (i * ipLineHeight), ipFont);
        }
        currentY += ipWrappedLines.size() * ipLineHeight + 10; // Add spacing after IP
    }

    // Display Time (displayState.line1) - Centered
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextDatum(TC_DATUM);
    int timeFont = FONT_TIME;
    tft.setTextFont(timeFont);
    int timeLineHeight = tft.fontHeight();
    std::vector<String> timeWrappedLines = wrapText(String(displayState.line1), timeFont, tft.width());

    // Calculate remaining space and center time vertically in it
    int remainingHeight = tft.height() - currentY;
    int totalTextHeight = timeWrappedLines.size() * timeLineHeight;

    // Add date height to calculation
    char currentDate[16]; // Max DD-MM-YYYY is 10 chars + null = 11. Use 16 for safety.
    getFormattedDate(currentDate, sizeof(currentDate));
    int dateFont = FONT_DEFAULT;
    tft.setTextFont(dateFont);
    int dateLineHeight = tft.fontHeight();
    std::vector<String> dateWrappedLines = wrapText(String(currentDate), dateFont, tft.width());
    int totalDateHeight = dateWrappedLines.size() * dateLineHeight;

    // Center the time+date block in remaining space
    int timeBlockHeight = totalTextHeight + (timeLineHeight / 2) + totalDateHeight;
    currentY += (remainingHeight - timeBlockHeight) / 2;

    // Draw time
    tft.setTextFont(timeFont);

    for (size_t i = 0; i < timeWrappedLines.size(); i++) {
        tft.drawString(timeWrappedLines[i], tft.width() / 2, currentY + (i * timeLineHeight), timeFont);
    }
    currentY += totalTextHeight; // Move Y past the time block

    // Add some padding between time and date
    currentY += timeLineHeight / 2; // Roughly half a line height padding

    // Display Date (getFormattedDate()) - Centered, below time
    tft.setTextFont(dateFont);
    for (size_t i = 0; i < dateWrappedLines.size(); i++) {
        tft.drawString(dateWrappedLines[i], tft.width() / 2, currentY + (i * dateLineHeight), dateFont);
    }
    currentY += totalDateHeight; // Move Y past the date block

    // Display Custom Message (displayState.line2) - Centered, below date
    if (displayState.line2[0] != '\0') {
        int messageFont = FONT_MESSAGE; // Smaller font for messages
        tft.setTextFont(messageFont);
        int messageLineHeight = tft.fontHeight();
        std::vector<String> messageWrappedLines = wrapText(String(displayState.line2), messageFont, tft.width());
        
        // Add some padding between date and message
        currentY += messageLineHeight / 2;

        for (size_t i = 0; i < messageWrappedLines.size(); i++) {
            tft.drawString(messageWrappedLines[i], tft.width() / 2, currentY + (i * messageLineHeight), messageFont);
        }
    }

    logPrint(F("displayRenderClock DONE (Simplified)"));
}

void displayRenderAPMode() {
    logPrint(F("displayRenderAPMode START"));

    tft.fillScreen(TFT_BLACK); // Re-added for previous behavior

    int currentY = 5; // Start from top with small margin

    // Display IP Info at the top (small font)
    if (displayState.ipInfo[0] != '\0') {
        tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
        tft.setTextDatum(TC_DATUM);
        int ipFont = FONT_INFO; // Small font
        tft.setTextFont(ipFont);
        int ipLineHeight = tft.fontHeight();
        std::vector<String> ipWrappedLines = wrapText(String(displayState.ipInfo), ipFont, tft.width() - 10);

        for (size_t i = 0; i < ipWrappedLines.size(); i++) {
            tft.drawString(ipWrappedLines[i], tft.width() / 2, currentY + (i * ipLineHeight), ipFont);
        }
        currentY += ipWrappedLines.size() * ipLineHeight + 20; // Add spacing after IP
    }

    // Calculate remaining vertical space
    int remainingHeight = tft.height() - currentY;

    // Display "AP Mode" header
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.setTextDatum(TC_DATUM);
    int headerFont = FONT_DEFAULT;
    tft.setTextFont(headerFont);
    int headerHeight = tft.fontHeight();

    // Display SSID label and value
    int labelFont = FONT_MESSAGE;
    int valueFont = FONT_DEFAULT;
    tft.setTextFont(labelFont);
    int labelHeight = tft.fontHeight();
    tft.setTextFont(valueFont);
    int valueHeight = tft.fontHeight();

    // Calculate total content height
    int totalContentHeight = headerHeight + 10 + // "AP Mode" + spacing
                            labelHeight + valueHeight + 10 + // SSID section + spacing
                            labelHeight + valueHeight;        // Password section

    // Center content vertically in remaining space
    int contentStartY = currentY + (remainingHeight - totalContentHeight) / 2;

    // Draw "AP Mode" header
    tft.setTextFont(headerFont);
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawString("AP Mode", tft.width() / 2, contentStartY, headerFont);
    contentStartY += headerHeight + 10;

    // Draw SSID label
    tft.setTextFont(labelFont);
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.drawString("SSID:", tft.width() / 2, contentStartY, labelFont);
    contentStartY += labelHeight;

    // Draw SSID value
    tft.setTextFont(valueFont);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    std::vector<String> ssidWrappedLines = wrapText(String(displayState.apSSID), valueFont, tft.width() - 20); // -20 for margins
    int ssidLineY = contentStartY;
    for (size_t i = 0; i < ssidWrappedLines.size(); i++) {
        tft.drawString(ssidWrappedLines[i], tft.width() / 2, ssidLineY + (i * valueHeight), valueFont);
    }
    contentStartY += ssidWrappedLines.size() * valueHeight + 10;

    // Draw Password label
    tft.setTextFont(labelFont);
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.drawString("Password:", tft.width() / 2, contentStartY, labelFont);
    contentStartY += labelHeight;

    // Draw Password value
    tft.setTextFont(valueFont);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    std::vector<String> passwordWrappedLines = wrapText(String(displayState.apPassword), valueFont, tft.width() - 20); // -20 for margins
    int passwordLineY = contentStartY;
    for (size_t i = 0; i < passwordWrappedLines.size(); i++) {
        tft.drawString(passwordWrappedLines[i], tft.width() / 2, passwordLineY + (i * valueHeight), valueFont);
    }

    logPrint(F("displayRenderAPMode DONE"));
}

void displayRenderImage(const char *path) {
    if (!LittleFS.exists(path)) {
        displayShowMessage(F("Image not found"));
        return;
    }

    File jpgFile = LittleFS.open(path, "r");
    if (!jpgFile) {
        String errorMsg = String(F("Failed to open image file: ")) + path;
        displayShowMessage(errorMsg);
        logPrint(errorMsg);
        return;
    }
    logPrint(String(F("INFO: Image file opened: ")) + path);

    tft.fillScreen(TFT_BLACK);

    JRESULT res = TJpgDec.drawFsJpg(0, 0, jpgFile);
    if (res != JDR_OK) {
        String errorMsg = String(F("JPEG Decode Failed\nCode: ")) + String(res);
        displayShowMessage(errorMsg);
        logPrint(errorMsg);
    }

    jpgFile.close(); // Close the file after decoding attempt
}

void displayShowMessage(const String &msg) {
    tft.fillScreen(TFT_BLACK); // Re-added for previous behavior
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextDatum(MC_DATUM);

    int startY = tft.height() / 2; // Starting Y position, will be adjusted
    int font = FONT_DEFAULT;
    
    // Calculate line height based on the font
    tft.setTextFont(font); // Set font for height calculation
    int lineHeight = tft.fontHeight();

    // Split message by newline characters first
    std::vector<String> linesToProcess;
    int prev = 0;
    for (size_t i = 0; i < msg.length(); i++) {
        if (msg.charAt(i) == '\n') {
            linesToProcess.push_back(msg.substring(prev, i));
            prev = i + 1;
        }
    }
    linesToProcess.push_back(msg.substring(prev)); // Add the last part

    std::vector<String> finalWrappedLines;
    for (size_t i = 0; i < linesToProcess.size(); i++) {
        std::vector<String> wrapped = wrapText(linesToProcess[i], font, tft.width());
        finalWrappedLines.insert(finalWrappedLines.end(), wrapped.begin(), wrapped.end());
    }
    
    // Adjust startY to vertically center the block of text
    startY -= (finalWrappedLines.size() * lineHeight) / 2;

    // Draw each wrapped line
    for (size_t i = 0; i < finalWrappedLines.size(); i++) {
        tft.drawString(finalWrappedLines[i], tft.width() / 2, startY + (i * lineHeight), font);
    }
}

void displayShowAPScreen(const char* ssid, const char* password, const char* ip) {
    logPrint(F("Switching to AP screen"));
    displayState.apMode = true;
    displayState.showImage = false; // Ensure we're not trying to show an image
    strncpy(displayState.apSSID, ssid, sizeof(displayState.apSSID));
    displayState.apSSID[sizeof(displayState.apSSID) - 1] = '\0'; // Ensure null-termination
    strncpy(displayState.apPassword, password, sizeof(displayState.apPassword));
    displayState.apPassword[sizeof(displayState.apPassword) - 1] = '\0'; // Ensure null-termination
    strncpy(displayState.ipInfo, ip, sizeof(displayState.ipInfo));
    displayState.ipInfo[sizeof(displayState.ipInfo) - 1] = '\0'; // Ensure null-termination
    displayUpdate();
}
