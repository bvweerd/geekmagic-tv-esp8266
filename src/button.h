#ifndef BUTTON_H
#define BUTTON_H

#include <Arduino.h>

// Button press types
enum ButtonPress {
    BUTTON_NONE = 0,
    BUTTON_SHORT = 1,
    BUTTON_LONG = 2
};

// Initialize button GPIO and state
void buttonInit();

// Update button state (call in main loop)
// Returns BUTTON_NONE, BUTTON_SHORT, or BUTTON_LONG
ButtonPress buttonUpdate();

#endif
