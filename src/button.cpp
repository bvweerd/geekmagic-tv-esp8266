#include "button.h"
#include "config.h"
#include "logger.h"

// Button state tracking
static bool lastButtonState = HIGH;
static bool currentButtonState = HIGH;
static unsigned long buttonPressStartTime = 0;
static unsigned long lastDebounceTime = 0;
static bool buttonPressed = false;

void buttonInit() {
    pinMode(PIN_BUTTON, INPUT_PULLUP);
    lastButtonState = digitalRead(PIN_BUTTON);
    currentButtonState = lastButtonState;
    logPrintf("Button initialized on GPIO%d (INPUT_PULLUP)", PIN_BUTTON);
}

ButtonPress buttonUpdate() {
    // Read current button state (LOW when pressed with pullup)
    bool reading = digitalRead(PIN_BUTTON);

    // Check if button state changed (for debouncing)
    if (reading != lastButtonState) {
        lastDebounceTime = millis();
    }

    // Debounce: only accept state change after debounce time
    if ((millis() - lastDebounceTime) > BUTTON_DEBOUNCE_MS) {
        // If the button state has changed after debounce
        if (reading != currentButtonState) {
            currentButtonState = reading;

            // Button was just pressed (HIGH -> LOW with pullup)
            if (currentButtonState == LOW && !buttonPressed) {
                buttonPressStartTime = millis();
                buttonPressed = true;
                logPrint("Button pressed");
            }
            // Button was just released (LOW -> HIGH with pullup)
            else if (currentButtonState == HIGH && buttonPressed) {
                unsigned long pressDuration = millis() - buttonPressStartTime;
                buttonPressed = false;

                logPrintf("Button released after %lu ms", pressDuration);

                // Determine press type
                if (pressDuration >= BUTTON_LONG_PRESS_MIN_MS) {
                    logPrint("Long press detected");
                    return BUTTON_LONG;
                } else if (pressDuration <= BUTTON_SHORT_PRESS_MAX_MS) {
                    logPrint("Short press detected");
                    return BUTTON_SHORT;
                }
                // Between short and long threshold - ignore
            }
        }
    }

    lastButtonState = reading;
    return BUTTON_NONE;
}
