#ifndef MOISTURE_SYSTEM_H
#define MOISTURE_SYSTEM_H

#include <Arduino.h>
#include "config.h"
#include "hal_display.h"

enum MoistureState : uint8_t {
    MOISTURE_THRIVING = 0,      // 60% - 100%: Lush, radiant, peak growth
    MOISTURE_BALANCED,          // 40% - 60%: Dynamic equilibrium baseline
    MOISTURE_THIRSTY,           // 20% - 40%: Slight droop, ready for water
    MOISTURE_AESTIVATING        // < 20%: Elegant safe dormancy (pastel antique tint, petals fold)
};

class MoistureSystem {
public:
    MoistureSystem();
    void begin();

    // Simulation tick
    void update(uint32_t deltaMs, uint8_t hour, uint8_t minute);

    // User watering interaction
    void water(float boost = 22.0f);

    // State queries
    float getMoisture() const { return _moisturePct; }
    MoistureState getState() const { return _state; }
    bool isAestivating() const { return _state == MOISTURE_AESTIVATING; }
    float getAestivationRatio() const { return _aestivationRatio; } // 0.0 = full vitality, 1.0 = dormant

    // Embedded Substrate Moisture Indicator Renderer
    void renderGauge(uint16_t* buffer, int16_t screenWidth = SCREEN_WIDTH, int16_t screenHeight = SCREEN_HEIGHT);

private:
    float _moisturePct;         // 0.0 to 100.0%
    MoistureState _state;
    float _aestivationRatio;    // Smooth visual transition into/out of dormancy
    uint32_t _lastWaterTime;    // Timestamp of last watering event (millis)
    uint32_t _consecutiveWaters; // Diminishing returns counter
    uint8_t _lastHour;          // For 04:00 AM autonomic dew recovery check
    bool _dewRecoveredToday;
};

extern MoistureSystem Moisture;

#endif // MOISTURE_SYSTEM_H
