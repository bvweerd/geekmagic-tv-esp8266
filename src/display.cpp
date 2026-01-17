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
TFT_eSprite sprite = TFT_eSprite(&tft);  // Framebuffer for atomic screen updates
DisplayState displayState;
int scrollPos = 240;

bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
    // JPEG decoder outputs 16-bit RGB565, so only use sprite if it's 16-bit
    // For 8-bit sprite or no sprite, draw directly to TFT
    TFT_eSPI* target;
    if (sprite.created() && sprite.getColorDepth() == 16) {
        target = &sprite;
    } else {
        target = &tft;
    }

    if (y >= target->height()) return 0;
    target->pushImage(x, y, w, h, bitmap);
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

// Helper function to get drawing target (sprite if available, else TFT)
TFT_eSPI* getDrawTarget() {
    return sprite.created() ? &sprite : &tft;
}

// Push framebuffer to display in one atomic operation
void pushFramebuffer() {
    if (sprite.created()) {
        // Use batched SPI writes to minimize visible transfer time
        tft.startWrite();
        sprite.pushSprite(0, 0);
        tft.endWrite();
    }
}


void displayInit() {
    logPrint(F("Display init..."));

    tft.init();
    tft.setRotation(0);
    tft.invertDisplay(true);  // Match ESPHome invert_colors: true

    // Initialize framebuffer sprite for atomic screen updates
    // Try 16-bit first (required for JPEG), fall back to 8-bit if memory constrained
    sprite.setColorDepth(16);
    bool spriteCreated = sprite.createSprite(DISPLAY_WIDTH, DISPLAY_HEIGHT);

    if (!spriteCreated) {
        logPrint(F("16-bit sprite failed, trying 8-bit..."));
        sprite.setColorDepth(8);
        spriteCreated = sprite.createSprite(DISPLAY_WIDTH, DISPLAY_HEIGHT);
    }

    if (spriteCreated) {
        sprite.setRotation(0);
        logPrintf("Framebuffer sprite created (%d-bit, %dx%d)", sprite.getColorDepth(), DISPLAY_WIDTH, DISPLAY_HEIGHT);
    } else {
        logPrint(F("WARNING: Framebuffer sprite creation failed, using direct drawing"));
    }

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

    // Track last mode to reset state on mode change (eliminate flickering)
    static uint8_t lastMode = 0; // 0=clock, 1=ap, 2=image
    uint8_t currentMode = (displayState.showImage && displayState.imagePath[0] != '\0') ? 2 : (displayState.apMode ? 1 : 0);

    // On mode change, force full redraw
    if (currentMode != lastMode) {
        TFT_eSPI* drawTarget = getDrawTarget();
        drawTarget->fillScreen(TFT_BLACK);
        pushFramebuffer();
        lastMode = currentMode;
    }

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

    // Track previous state to avoid flickering
    static PreviousDisplayState prevState = {0};

    // For clock updates, always use direct TFT drawing to eliminate sprite push flicker
    // No sprite usage for clock rendering
    int screenWidth = tft.width();
    int screenHeight = tft.height();

    // Check if layout changed (IP appeared/disappeared)
    bool hadIP = prevState.ipInfo[0] != '\0';
    bool hasIP = displayState.ipInfo[0] != '\0';
    bool layoutChanged = (hadIP != hasIP);

    // Check if anything changed - if nothing changed and initialized, skip redraw
    bool timeChanged = strcmp(displayState.line1, prevState.line1) != 0;
    char currentDate[16];
    getFormattedDate(currentDate, sizeof(currentDate));
    bool dateChanged = strcmp(currentDate, prevState.date) != 0;
    bool ipChanged = strcmp(displayState.ipInfo, prevState.ipInfo) != 0;
    bool messageChanged = strcmp(displayState.line2, prevState.line2) != 0;

    bool needsRedraw = !prevState.initialized || layoutChanged || timeChanged || dateChanged || ipChanged || messageChanged;

    if (!needsRedraw) {
        return; // Nothing changed, skip redraw
    }

    // Clear screen on first render or layout change, otherwise keep existing content
    if (!prevState.initialized || layoutChanged) {
        tft.startWrite();
        tft.fillScreen(TFT_BLACK);
        tft.endWrite();
        prevState.initialized = true;
        // Force redraw all on layout change - clear previous state
        prevState.line1[0] = '\0';
        prevState.line2[0] = '\0';
        prevState.date[0] = '\0';
        prevState.ipInfo[0] = '\0'; // Clear IP info to force redraw
        prevState.prevTimeWidth = 0;
        prevState.prevDateWidth = 0;
    }

    int currentY = 5; // Start from top with small margin

    // Display IP Info at the top (small font) - only if changed
    if (displayState.ipInfo[0] != '\0') {
        // Always use direct TFT drawing for IP updates (no sprite)
        tft.setTextColor(TFT_DARKGREY, TFT_BLACK); // Gray text for IP info
        tft.setTextDatum(TC_DATUM);
        int ipFont = FONT_INFO; // Small font
        tft.setTextFont(ipFont);
        int ipLineHeight = tft.fontHeight();
        std::vector<String> ipWrappedLines = wrapText(String(displayState.ipInfo), ipFont, screenWidth - 10);

        if (ipChanged || !prevState.initialized || layoutChanged) {
            // Direct TFT drawing for IP updates - use background color text, minimal clearing
            tft.startWrite();
            // Clear IP area first to ensure clean display (especially on first render)
            static int prevIPLines = 0;
            int maxLines = (ipWrappedLines.size() > prevIPLines) ? ipWrappedLines.size() : prevIPLines;
            if (maxLines > 0) {
                tft.fillRect(0, currentY, screenWidth, maxLines * ipLineHeight + 5, TFT_BLACK);
            }
            prevIPLines = ipWrappedLines.size();

            for (size_t i = 0; i < ipWrappedLines.size(); i++) {
                tft.drawString(ipWrappedLines[i], screenWidth / 2, currentY + (i * ipLineHeight), ipFont);
            }
            tft.endWrite();

            strncpy(prevState.ipInfo, displayState.ipInfo, sizeof(prevState.ipInfo) - 1);
            prevState.ipInfo[sizeof(prevState.ipInfo) - 1] = '\0'; // Ensure null termination
        }
        currentY += ipWrappedLines.size() * ipLineHeight + 10; // Add spacing after IP
    } else if (prevState.ipInfo[0] != '\0') {
        // IP was cleared - erase previous IP area
        tft.startWrite();
        tft.fillRect(0, 5, screenWidth, 30, TFT_BLACK); // Clear reasonable IP area
        tft.endWrite();
        prevState.ipInfo[0] = '\0';
    }

    // Display Time (displayState.line1) - Centered
    tft.setTextDatum(TC_DATUM);
    int timeFont = FONT_TIME;
    tft.setTextFont(timeFont);
    int timeLineHeight = tft.fontHeight();
    std::vector<String> timeWrappedLines = wrapText(String(displayState.line1), timeFont, screenWidth);

    // Calculate remaining space and center time vertically in it
    int remainingHeight = screenHeight - currentY;
    int totalTextHeight = timeWrappedLines.size() * timeLineHeight;

    // Add date height to calculation
    int dateFont = FONT_DEFAULT;
    tft.setTextFont(dateFont);
    int dateLineHeight = tft.fontHeight();
    std::vector<String> dateWrappedLines = wrapText(String(currentDate), dateFont, screenWidth);
    int totalDateHeight = dateWrappedLines.size() * dateLineHeight;

    // Center the time+date block in remaining space
    int timeBlockHeight = totalTextHeight + (timeLineHeight / 2) + totalDateHeight;
    int timeStartY = currentY + (remainingHeight - timeBlockHeight) / 2;
    currentY = timeStartY;

    // Draw time - only if changed
    if (timeChanged || !prevState.initialized || layoutChanged) {
        // Always use direct TFT drawing for clock updates (no sprite)
        tft.setTextFont(timeFont);
        tft.setTextColor(TFT_WHITE, TFT_BLACK); // Text color, background color

        // Calculate exact text width to minimize clearing area
        int maxTextWidth = 0;
        for (size_t i = 0; i < timeWrappedLines.size(); i++) {
            int textWidth = tft.textWidth(timeWrappedLines[i]);
            if (textWidth > maxTextWidth) maxTextWidth = textWidth;
        }

        // Update previous width for next comparison
        int prevTimeWidth = prevState.prevTimeWidth;
        prevState.prevTimeWidth = maxTextWidth;

        // Direct TFT drawing with batched SPI for smooth updates (avoids sprite push flicker)
        // Use text with background color - minimal fillRect to eliminate visible clearing
        tft.startWrite();
        // Draw text with background color - it will overwrite old text
        // If text got shorter, we need to clear the extra area
        if (maxTextWidth < prevTimeWidth) {
            // Only clear the extra width that's no longer needed
            int extraWidth = prevTimeWidth - maxTextWidth;
            int extraX = (screenWidth + maxTextWidth) / 2 + 2; // Right side of new text
            tft.fillRect(extraX, currentY, extraWidth, totalTextHeight, TFT_BLACK);
        }
        for (size_t i = 0; i < timeWrappedLines.size(); i++) {
            tft.drawString(timeWrappedLines[i], screenWidth / 2, currentY + (i * timeLineHeight), timeFont);
        }
        tft.endWrite();

        strncpy(prevState.line1, displayState.line1, sizeof(prevState.line1) - 1);
        prevState.line1[sizeof(prevState.line1) - 1] = '\0'; // Ensure null termination
    }
    currentY += totalTextHeight; // Move Y past the time block

    // Add some padding between time and date
    currentY += timeLineHeight / 2; // Roughly half a line height padding

    // Display Date (getFormattedDate()) - Centered, below time - only if changed
    if (dateChanged || !prevState.initialized || layoutChanged) {
        // Always use direct TFT drawing for date updates (no sprite)
        tft.setTextFont(dateFont);
        tft.setTextColor(TFT_WHITE, TFT_BLACK); // Text with background color

        // Calculate exact text width to minimize clearing area
        int maxTextWidth = 0;
        for (size_t i = 0; i < dateWrappedLines.size(); i++) {
            int textWidth = tft.textWidth(dateWrappedLines[i]);
            if (textWidth > maxTextWidth) maxTextWidth = textWidth;
        }

        // Update previous width for next comparison
        int prevDateWidth = prevState.prevDateWidth;
        prevState.prevDateWidth = maxTextWidth;

        // Direct TFT drawing for date updates - minimal fillRect, use background color text
        tft.startWrite();
        // If text got shorter, clear only the extra area
        if (maxTextWidth < prevDateWidth) {
            int extraWidth = prevDateWidth - maxTextWidth;
            int extraX = (screenWidth + maxTextWidth) / 2 + 2;
            tft.fillRect(extraX, currentY, extraWidth, totalDateHeight, TFT_BLACK);
        }
        for (size_t i = 0; i < dateWrappedLines.size(); i++) {
            tft.drawString(dateWrappedLines[i], screenWidth / 2, currentY + (i * dateLineHeight), dateFont);
        }
        tft.endWrite();

        strncpy(prevState.date, currentDate, sizeof(prevState.date) - 1);
        prevState.date[sizeof(prevState.date) - 1] = '\0'; // Ensure null termination
    }
    currentY += totalDateHeight; // Move Y past the date block

    // Display Custom Message (displayState.line2) - Centered, below date - only if changed
    if (displayState.line2[0] != '\0') {
        int messageFont = FONT_MESSAGE; // Smaller font for messages
        tft.setTextFont(messageFont);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        int messageLineHeight = tft.fontHeight();
        std::vector<String> messageWrappedLines = wrapText(String(displayState.line2), messageFont, screenWidth);

        // Add some padding between date and message
        currentY += messageLineHeight / 2;

        if (messageChanged || !prevState.initialized || layoutChanged) {
            // Direct TFT draw for messages too
            tft.startWrite();
            tft.fillRect(0, currentY, screenWidth, messageWrappedLines.size() * messageLineHeight + 10, TFT_BLACK);
            for (size_t i = 0; i < messageWrappedLines.size(); i++) {
                tft.drawString(messageWrappedLines[i], screenWidth / 2, currentY + (i * messageLineHeight), messageFont);
            }
            tft.endWrite();
            strncpy(prevState.line2, displayState.line2, sizeof(prevState.line2) - 1);
            prevState.line2[sizeof(prevState.line2) - 1] = '\0'; // Ensure null termination
        }
    } else if (prevState.line2[0] != '\0') {
        // Message was cleared - erase previous message
        int messageFont = FONT_MESSAGE;
        tft.setTextFont(messageFont);
        int messageLineHeight = tft.fontHeight();
        currentY += messageLineHeight / 2;
        tft.startWrite();
        tft.fillRect(0, currentY, screenWidth, messageLineHeight * 3, TFT_BLACK);
        tft.endWrite();
        prevState.line2[0] = '\0';
    }

    // No sprite push needed - all drawing is direct to TFT

    logPrint(F("displayRenderClock DONE (Simplified)"));
}

void displayRenderAPMode() {
    logPrint(F("displayRenderAPMode START"));

    // Get drawing target (sprite if available, else TFT)
    TFT_eSPI* drawTarget = getDrawTarget();
    int screenWidth = drawTarget->width();
    int screenHeight = drawTarget->height();

    // Only clear screen on mode change
    static bool apModeRendered = false;
    if (!apModeRendered) {
        drawTarget->fillScreen(TFT_BLACK);
        apModeRendered = true;
    }

    int currentY = 5; // Start from top with small margin

    // Display IP Info at the top (small font)
    if (displayState.ipInfo[0] != '\0') {
        drawTarget->setTextColor(TFT_DARKGREY, TFT_BLACK);
        drawTarget->setTextDatum(TC_DATUM);
        int ipFont = FONT_INFO; // Small font
        drawTarget->setTextFont(ipFont);
        int ipLineHeight = drawTarget->fontHeight();
        std::vector<String> ipWrappedLines = wrapText(String(displayState.ipInfo), ipFont, screenWidth - 10);

        for (size_t i = 0; i < ipWrappedLines.size(); i++) {
            drawTarget->drawString(ipWrappedLines[i], screenWidth / 2, currentY + (i * ipLineHeight), ipFont);
        }
        currentY += ipWrappedLines.size() * ipLineHeight + 20; // Add spacing after IP
    }

    // Calculate remaining vertical space
    int remainingHeight = screenHeight - currentY;

    // Display "AP Mode" header
    drawTarget->setTextColor(TFT_CYAN, TFT_BLACK);
    drawTarget->setTextDatum(TC_DATUM);
    int headerFont = FONT_DEFAULT;
    drawTarget->setTextFont(headerFont);
    int headerHeight = drawTarget->fontHeight();

    // Display SSID label and value
    int labelFont = FONT_MESSAGE;
    int valueFont = FONT_DEFAULT;
    drawTarget->setTextFont(labelFont);
    int labelHeight = drawTarget->fontHeight();
    drawTarget->setTextFont(valueFont);
    int valueHeight = drawTarget->fontHeight();

    // Calculate total content height
    int totalContentHeight = headerHeight + 10 + // "AP Mode" + spacing
                            labelHeight + valueHeight + 10 + // SSID section + spacing
                            labelHeight + valueHeight;        // Password section

    // Center content vertically in remaining space
    int contentStartY = currentY + (remainingHeight - totalContentHeight) / 2;

    // Draw "AP Mode" header
    drawTarget->setTextFont(headerFont);
    drawTarget->setTextColor(TFT_CYAN, TFT_BLACK);
    drawTarget->drawString("AP Mode", screenWidth / 2, contentStartY, headerFont);
    contentStartY += headerHeight + 10;

    // Draw SSID label
    drawTarget->setTextFont(labelFont);
    drawTarget->setTextColor(TFT_DARKGREY, TFT_BLACK);
    drawTarget->drawString("SSID:", screenWidth / 2, contentStartY, labelFont);
    contentStartY += labelHeight;

    // Draw SSID value
    drawTarget->setTextFont(valueFont);
    drawTarget->setTextColor(TFT_WHITE, TFT_BLACK);
    std::vector<String> ssidWrappedLines = wrapText(String(displayState.apSSID), valueFont, screenWidth - 20); // -20 for margins
    int ssidLineY = contentStartY;
    for (size_t i = 0; i < ssidWrappedLines.size(); i++) {
        drawTarget->drawString(ssidWrappedLines[i], screenWidth / 2, ssidLineY + (i * valueHeight), valueFont);
    }
    contentStartY += ssidWrappedLines.size() * valueHeight + 10;

    // Draw Password label
    drawTarget->setTextFont(labelFont);
    drawTarget->setTextColor(TFT_DARKGREY, TFT_BLACK);
    drawTarget->drawString("Password:", screenWidth / 2, contentStartY, labelFont);
    contentStartY += labelHeight;

    // Draw Password value
    drawTarget->setTextFont(valueFont);
    drawTarget->setTextColor(TFT_WHITE, TFT_BLACK);
    std::vector<String> passwordWrappedLines = wrapText(String(displayState.apPassword), valueFont, screenWidth - 20); // -20 for margins
    int passwordLineY = contentStartY;
    for (size_t i = 0; i < passwordWrappedLines.size(); i++) {
        drawTarget->drawString(passwordWrappedLines[i], screenWidth / 2, passwordLineY + (i * valueHeight), valueFont);
    }

    // Push entire framebuffer to display in one atomic operation
    pushFramebuffer();

    logPrint(F("displayRenderAPMode DONE"));
}

void displayBlankScreen() {
    TFT_eSPI* drawTarget = getDrawTarget();
    drawTarget->fillScreen(TFT_BLACK);
    pushFramebuffer();
    logPrint(F("Display blanked to black."));
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

    // Direct image swap without clearing screen for smooth transitions
    // The new JPEG will overwrite the previous image directly
    // If using 16-bit sprite, JPEG draws to sprite, then we push it all at once
    // If using 8-bit sprite or no sprite, JPEG draws directly to TFT
    bool useSprite = sprite.created() && sprite.getColorDepth() == 16;

    if (useSprite) {
        // Draw to sprite first, then push all at once
        JRESULT res = TJpgDec.drawFsJpg(0, 0, jpgFile);
        if (res != JDR_OK) {
            String errorMsg = String(F("JPEG Decode Failed\nCode: ")) + String(res);
            displayShowMessage(errorMsg);
            logPrint(errorMsg);
        } else {
            // Push entire framebuffer to display in one atomic operation
            pushFramebuffer();
        }
    } else {
        // Draw directly to TFT with batched SPI
        tft.startWrite();
        JRESULT res = TJpgDec.drawFsJpg(0, 0, jpgFile);
        tft.endWrite();
        if (res != JDR_OK) {
            String errorMsg = String(F("JPEG Decode Failed\nCode: ")) + String(res);
            displayShowMessage(errorMsg);
            logPrint(errorMsg);
        }
    }

    jpgFile.close(); // Close the file after decoding attempt
}

void displayShowMessage(const String &msg) {
    TFT_eSPI* drawTarget = getDrawTarget();
    int screenWidth = drawTarget->width();
    int screenHeight = drawTarget->height();

    drawTarget->setTextColor(TFT_WHITE, TFT_BLACK);
    drawTarget->setTextDatum(MC_DATUM);

    int startY = screenHeight / 2; // Starting Y position, will be adjusted
    int font = FONT_DEFAULT;

    // Calculate line height based on the font
    drawTarget->setTextFont(font); // Set font for height calculation
    int lineHeight = drawTarget->fontHeight();

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
        std::vector<String> wrapped = wrapText(linesToProcess[i], font, screenWidth);
        finalWrappedLines.insert(finalWrappedLines.end(), wrapped.begin(), wrapped.end());
    }

    // Adjust startY to vertically center the block of text
    startY -= (finalWrappedLines.size() * lineHeight) / 2;

    // Draw everything to framebuffer first
    drawTarget->fillScreen(TFT_BLACK); // Clear screen
    // Draw each wrapped line
    for (size_t i = 0; i < finalWrappedLines.size(); i++) {
        drawTarget->drawString(finalWrappedLines[i], screenWidth / 2, startY + (i * lineHeight), font);
    }

    // Push entire framebuffer to display in one atomic operation
    pushFramebuffer();
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

void displayCycleNextPage() {
    // Don't cycle pages if in AP mode
    if (displayState.apMode) {
        logPrint(F("Page cycling disabled in AP mode"));
        return;
    }

    // Cycle: Clock -> Image (if available) -> Clock
    if (!displayState.showImage) {
        // Currently showing clock, try to switch to image if available
        if (displayState.imagePath[0] != '\0' && LittleFS.exists(displayState.imagePath)) {
            logPrint(F("Cycling to image page"));
            displayState.showImage = true;
        } else {
            logPrint(F("No image available, staying on clock page"));
        }
    } else {
        // Currently showing image, switch back to clock
        logPrint(F("Cycling to clock page"));
        displayState.showImage = false;
    }

    // Immediately update the display
    displayUpdate();
}

// Track backlight state for toggle functionality
static int savedBrightness = 100;
static bool backlightOn = true;

void displayToggleBacklight() {
    if (backlightOn) {
        // Turn off backlight
        logPrint(F("Backlight OFF"));
        savedBrightness = 100; // Assume current brightness is 100 (could be read from settings)
        displaySetBrightness(0);
        backlightOn = false;
    } else {
        // Turn on backlight
        logPrintf("Backlight ON (brightness: %d)", savedBrightness);
        displaySetBrightness(savedBrightness);
        backlightOn = true;
    }
}
