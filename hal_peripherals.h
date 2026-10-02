#ifndef HAL_PERIPHERALS_H
#define HAL_PERIPHERALS_H

#include <Arduino.h>
#include "config.h"

// Simple RGB struct
struct RGBColor {
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

class PeripheralsHAL {
public:
    PeripheralsHAL();
    void begin();
    void update();

    // Button Handling
    bool isButtonPressed() const { return _isPressed; }
    bool wasButtonClicked();       // Returns true once on single click
    bool wasButtonDoubleClicked(); // Returns true once on double click
    bool wasButtonHeld();          // Returns true once when button held > 2 seconds
    uint32_t getPressDurationMs() const { return _pressDurationMs; }

    // RGB LED Handling
    void setLedColor(uint8_t r, uint8_t g, uint8_t b);
    void setLedColor(const RGBColor& color);
    void pulseLed(const RGBColor& color, uint32_t durationMs);

private:
    // Button state
    bool _lastRawState;
    bool _isPressed;
    bool _clickPending;
    bool _singleClickTriggered;
    bool _doubleClickTriggered;
    bool _heldTriggered;
    bool _heldConsumed;
    uint32_t _lastDebounceTime;
    uint32_t _pressStartTime;
    uint32_t _releaseTime;
    uint32_t _pressDurationMs;

    // LED State
    RGBColor _currentLedColor;
    RGBColor _targetLedColor;
    bool _isPulsing;
    uint32_t _pulseStartTime;
    uint32_t _pulseDurationMs;

    void updateLedDriver();
};

extern PeripheralsHAL Peripherals;

#endif // HAL_PERIPHERALS_H
