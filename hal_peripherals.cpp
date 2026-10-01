#include "hal_peripherals.h"

#define DEBOUNCE_DELAY_MS  35

PeripheralsHAL Peripherals;

PeripheralsHAL::PeripheralsHAL()
    : _lastRawState(HIGH),
      _isPressed(false),
      _clickConsumed(true),
      _lastDebounceTime(0),
      _pressStartTime(0),
      _pressDurationMs(0),
      _currentLedColor{0, 0, 0},
      _targetLedColor{0, 0, 0},
      _isPulsing(false),
      _pulseStartTime(0),
      _pulseDurationMs(0) {}

void PeripheralsHAL::begin() {
    pinMode(PIN_BOOT_BUTTON, INPUT_PULLUP);
    pinMode(PIN_RGB_LED, OUTPUT);
    digitalWrite(PIN_RGB_LED, LOW);
}

void PeripheralsHAL::update() {
    uint32_t now = millis();

    // 1. Button Debounce & Event Detection
    bool rawState = (digitalRead(PIN_BOOT_BUTTON) == LOW); // Active LOW

    if (rawState != _lastRawState) {
        _lastDebounceTime = now;
        _lastRawState = rawState;
    }

    if ((now - _lastDebounceTime) > DEBOUNCE_DELAY_MS) {
        if (rawState != _isPressed) {
            _isPressed = rawState;
            if (_isPressed) {
                _pressStartTime = now;
                _clickConsumed = false;
            } else {
                _pressDurationMs = now - _pressStartTime;
            }
        }
    }

    // 2. LED Animation / Pulse Update
    if (_isPulsing) {
        uint32_t elapsed = now - _pulseStartTime;
        if (elapsed >= _pulseDurationMs) {
            _isPulsing = false;
            updateLedDriver();
        } else {
            // Sine ease-in-out breathing pulse
            float progress = (float)elapsed / (float)_pulseDurationMs;
            float brightnessFactor = sinf(progress * PI); // 0.0 -> 1.0 -> 0.0
            
            uint8_t r = (uint8_t)(_targetLedColor.r * brightnessFactor);
            uint8_t g = (uint8_t)(_targetLedColor.g * brightnessFactor);
            uint8_t b = (uint8_t)(_targetLedColor.b * brightnessFactor);

            #if defined(ESP32)
            neopixelWrite(PIN_RGB_LED, r, g, b);
            #endif
        }
    }
}

bool PeripheralsHAL::wasButtonClicked() {
    if (!_clickConsumed && !_isPressed) {
        _clickConsumed = true;
        return true;
    }
    return false;
}

void PeripheralsHAL::setLedColor(uint8_t r, uint8_t g, uint8_t b) {
    _currentLedColor = {r, g, b};
    _isPulsing = false;
    updateLedDriver();
}

void PeripheralsHAL::setLedColor(const RGBColor& color) {
    setLedColor(color.r, color.g, color.b);
}

void PeripheralsHAL::pulseLed(const RGBColor& color, uint32_t durationMs) {
    _targetLedColor = color;
    _pulseDurationMs = durationMs;
    _pulseStartTime = millis();
    _isPulsing = true;
}

void PeripheralsHAL::updateLedDriver() {
    #if defined(ESP32)
    neopixelWrite(PIN_RGB_LED, _currentLedColor.r, _currentLedColor.g, _currentLedColor.b);
    #endif
}
