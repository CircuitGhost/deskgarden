#include "moisture_system.h"
#include "micro_font.h"

MoistureSystem Moisture;

MoistureSystem::MoistureSystem()
    : _moisturePct(55.0f),
      _state(MOISTURE_BALANCED),
      _aestivationRatio(0.0f),
      _lastWaterTime(0),
      _consecutiveWaters(0),
      _lastHour(255),
      _dewRecoveredToday(false) {}

void MoistureSystem::begin() {
    _moisturePct = 55.0f;
    _state = MOISTURE_BALANCED;
    _aestivationRatio = 0.0f;
    _lastWaterTime = millis();
    _consecutiveWaters = 0;
    _lastHour = 255;
    _dewRecoveredToday = false;
}

void MoistureSystem::water(float boost) {
    uint32_t now = millis();
    uint32_t timeSinceLastWater = now - _lastWaterTime;

    // Diminishing returns check (15 minutes = 900,000 ms)
    if (timeSinceLastWater < 900000 && _consecutiveWaters > 0) {
        _consecutiveWaters++;
        boost = boost / (1.0f + 0.5f * _consecutiveWaters);
    } else {
        _consecutiveWaters = 1;
    }

    _lastWaterTime = now;
    _moisturePct += boost;
    if (_moisturePct > 100.0f) _moisturePct = 100.0f;
}

void MoistureSystem::update(uint32_t deltaMs, uint8_t hour, uint8_t minute) {
    // 1. Slow organic transpiration decay (Gentle drift toward equilibrium)
    // ~1% decay per 120 seconds of simulation
    float decayPerMs = 0.000008f;
    if (_moisturePct > 20.0f) {
        _moisturePct -= (decayPerMs * deltaMs);
    } else {
        // Floor at 12% in aestivation (never decays to 0% or broken state)
        if (_moisturePct > 12.0f) {
            _moisturePct -= (decayPerMs * 0.2f * deltaMs);
        }
    }

    // 2. 04:00 AM Autonomic Dew Cycle Recovery
    if (hour != _lastHour) {
        _lastHour = hour;
        if (hour == 0) {
            _dewRecoveredToday = false; // Reset daily dew flag at midnight
        }
    }

    if (hour == 4 && minute <= 5 && !_dewRecoveredToday) {
        if (_moisturePct < 52.0f) {
            _moisturePct = 52.0f + (rand() % 8); // Autonomic morning dew recharge
        }
        _dewRecoveredToday = true;
    }

    // 3. State classification & Aestivation Transition
    if (_moisturePct >= 60.0f) {
        _state = MOISTURE_THRIVING;
    } else if (_moisturePct >= 40.0f) {
        _state = MOISTURE_BALANCED;
    } else if (_moisturePct >= 20.0f) {
        _state = MOISTURE_THIRSTY;
    } else {
        _state = MOISTURE_AESTIVATING;
    }

    // Smooth visual transition into/out of dormancy
    float targetAestivation = (_state == MOISTURE_AESTIVATING) ? 1.0f : 0.0f;
    float diff = targetAestivation - _aestivationRatio;
    _aestivationRatio += diff * (deltaMs * 0.002f);
}

void MoistureSystem::renderGauge(uint16_t* buffer, int16_t screenWidth, int16_t screenHeight) {
    if (!buffer) return;

    // Minimalist 40px moisture indicator at bottom of substrate (x = 66 to 106, y = 308 to 314)
    int16_t barX = 66;
    int16_t barY = 308;
    int16_t barW = 40;
    int16_t barH = 5;

    // Dark inset container background
    uint16_t bgCol = rgb565(18, 12, 10);
    uint16_t borderCol = rgb565(55, 42, 32);

    for (int16_t y = 0; y < barH; y++) {
        for (int16_t x = 0; x < barW; x++) {
            uint16_t col = (y == 0 || y == barH - 1 || x == 0 || x == barW - 1) ? borderCol : bgCol;
            buffer[(barY + y) * screenWidth + (barX + x)] = col;
        }
    }

    // Filled hydration level
    int16_t fillW = (int16_t)((barW - 2) * (_moisturePct / 100.0f));
    if (fillW > barW - 2) fillW = barW - 2;

    uint16_t fillCol;
    if (_state == MOISTURE_AESTIVATING) {
        fillCol = rgb565(180, 130, 70); // Muted amber in dormancy
    } else if (_state == MOISTURE_THIRSTY) {
        fillCol = rgb565(120, 175, 190); // Pale cyan
    } else {
        fillCol = rgb565(50, 190, 230);  // Vibrant hydrating aqua
    }

    for (int16_t y = 1; y < barH - 1; y++) {
        for (int16_t x = 1; x <= fillW; x++) {
            buffer[(barY + y) * screenWidth + (barX + x)] = fillCol;
        }
    }
}
