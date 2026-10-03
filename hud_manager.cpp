#include "hud_manager.h"
#include "hal_display.h"

HudManager HUD;

HudManager::HudManager()
    : _hour(10),
      _minute(42),
      _second(0),
      _subSecondMs(0),
      _is24Hour(false),
      _isPM(false),
      _weatherType(WEATHER_SUN),
      _temperatureF(72),
      _humidityPct(64),
      _precipIntensity(0),
      _generationIndex(1),
      _colonPulsePhase(0.0f) {}

void HudManager::begin() {
    _colonPulsePhase = 0.0f;
}

void HudManager::tickInternalClock(uint32_t deltaMs) {
    _subSecondMs += deltaMs;
    while (_subSecondMs >= 1000) {
        _subSecondMs -= 1000;
        _second++;
        if (_second >= 60) {
            _second = 0;
            _minute++;
            if (_minute >= 60) {
                _minute = 0;
                _hour = (_hour + 1) % 24;
            }
        }
    }
}

void HudManager::update(uint32_t deltaMs) {
    tickInternalClock(deltaMs);

    // Colon pulse frequency: 1 full cycle per second
    _colonPulsePhase += (deltaMs * 0.006283f); // 2 * PI / 1000ms
    if (_colonPulsePhase > 2.0f * PI) {
        _colonPulsePhase -= (2.0f * PI);
    }
}

void HudManager::setTime(uint8_t hour, uint8_t min, uint8_t sec, bool is24h) {
    _hour = hour % 24;
    _minute = min % 60;
    _second = sec % 60;
    _is24Hour = is24h;
    _isPM = (_hour >= 12);
}

void HudManager::setWeather(WeatherType type, int16_t tempF, uint8_t humidityPct, uint8_t precipIntensity) {
    _weatherType = type;
    _temperatureF = tempF;
    _humidityPct = humidityPct;
    _precipIntensity = precipIntensity;
}

void HudManager::setGeneration(uint8_t genIndex) {
    _generationIndex = genIndex;
}

void HudManager::render(uint16_t* buffer, int16_t bufWidth, int16_t bufHeight) {
    if (!buffer) return;

    // 1. HUD Background Bar (Dark slate gradient)
    for (int16_t y = 0; y < HUD_HEIGHT; y++) {
        uint8_t r = 16 + (y * 6 / HUD_HEIGHT);
        uint8_t g = 20 + (y * 8 / HUD_HEIGHT);
        uint8_t b = 28 + (y * 10 / HUD_HEIGHT);
        uint16_t barColor = rgb565(r, g, b);

        uint16_t* rowPtr = &buffer[y * bufWidth];
        for (int16_t x = 0; x < bufWidth; x++) {
            rowPtr[x] = barColor;
        }
    }

    // Bottom border separator line
    uint16_t sepColor = rgb565(55, 65, 82);
    for (int16_t x = 0; x < bufWidth; x++) {
        buffer[(HUD_HEIGHT - 1) * bufWidth + x] = sepColor;
    }

    // 2. Format & Render Time (Left section: x = 4, y = 8)
    char timeStr[16];
    uint8_t dispHour = _hour;
    const char* suffix = "";

    if (!_is24Hour) {
        bool pm = (_hour >= 12);
        suffix = pm ? "P" : "A";
        dispHour = _hour % 12;
        if (dispHour == 0) dispHour = 12;
        snprintf(timeStr, sizeof(timeStr), "%2d:%02d%s", dispHour, _minute, suffix);
    } else {
        snprintf(timeStr, sizeof(timeStr), "%02d:%02d", dispHour, _minute);
    }

    // Draw Time with pulsing colon
    uint16_t timeColor = rgb565(220, 230, 245);
    int16_t curX = 4;
    int16_t textY = 8;

    for (int i = 0; timeStr[i] != '\0'; i++) {
        char c = timeStr[i];
        if (c == ':') {
            // Pulse colon brightness
            float brightness = 0.4f + 0.6f * (0.5f + 0.5f * sinf(_colonPulsePhase));
            uint8_t cr = (uint8_t)(220 * brightness);
            uint8_t cg = (uint8_t)(230 * brightness);
            uint8_t cb = (uint8_t)(245 * brightness);
            drawChar5x7(buffer, bufWidth, bufHeight, curX, textY, ':', rgb565(cr, cg, cb));
        } else {
            drawChar5x7(buffer, bufWidth, bufHeight, curX, textY, c, timeColor);
        }
        curX += 6;
    }

    // Subtle divider at x = 54
    for (int16_t dy = 6; dy <= 16; dy++) {
        buffer[dy * bufWidth + 54] = rgb565(40, 48, 62);
    }

    // 3. Render Weather Glyph & Metrics (Center section: x = 60 to 120)
    drawWeatherGlyph(buffer, bufWidth, bufHeight, 60, 8, _weatherType);

    char weatherStr[16];
    snprintf(weatherStr, sizeof(weatherStr), "%d^ %d%%", _temperatureF, _humidityPct);
    drawString5x7(buffer, bufWidth, bufHeight, 71, 8, weatherStr, rgb565(180, 205, 230));

    // Subtle divider at x = 126
    for (int16_t dy = 6; dy <= 16; dy++) {
        buffer[dy * bufWidth + 126] = rgb565(40, 48, 62);
    }

    // 4. Render Generation Index (Right section: x = 132)
    char genStr[12];
    snprintf(genStr, sizeof(genStr), "G%02d", _generationIndex);
    drawString5x7(buffer, bufWidth, bufHeight, 132, 8, genStr, rgb565(120, 220, 180));
}
