#ifndef HUD_MANAGER_H
#define HUD_MANAGER_H

#include <Arduino.h>
#include "micro_font.h"
#include "weather_glyphs.h"

class HudManager {
public:
    HudManager();
    void begin();
    void update(uint32_t deltaMs);
    void render(uint16_t* buffer, int16_t bufWidth, int16_t bufHeight);

    // Setters
    void setTime(uint8_t hour, uint8_t min, uint8_t sec, bool is24h = false);
    void setWeather(WeatherType type, int16_t tempF, uint8_t humidityPct);
    void setGeneration(uint8_t genIndex);
    void setUse24Hour(bool is24h) { _is24Hour = is24h; }

    // Getters
    uint8_t getHour() const { return _hour; }
    uint8_t getMinute() const { return _minute; }
    uint8_t getSecond() const { return _second; }
    WeatherType getWeatherType() const { return _weatherType; }
    uint8_t getGeneration() const { return _generationIndex; }
    bool getUse24Hour() const { return _is24Hour; }

private:
    // Time state
    uint8_t _hour;
    uint8_t _minute;
    uint8_t _second;
    uint32_t _subSecondMs;
    bool _is24Hour;
    bool _isPM;

    // Weather & Plant metrics
    WeatherType _weatherType;
    int16_t _temperatureF;
    uint8_t _humidityPct;
    uint8_t _generationIndex;

    // Colon pulse animation
    float _colonPulsePhase;

    void tickInternalClock(uint32_t deltaMs);
};

extern HudManager HUD;

#endif // HUD_MANAGER_H
