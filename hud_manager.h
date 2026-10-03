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
    void setCalendar(uint16_t year, uint8_t month, uint8_t day);
    void setWeather(WeatherType type, int16_t tempF, uint8_t humidityPct, uint8_t precipIntensity = 0);
    void setGeneration(uint8_t genIndex);
    void setUse24Hour(bool is24h) { _is24Hour = is24h; }

    // Getters
    uint8_t getHour() const { return _hour; }
    uint8_t getMinute() const { return _minute; }
    uint8_t getSecond() const { return _second; }
    uint16_t getYear() const { return _year; }
    uint8_t getMonth() const { return _month; }   // 1–12, or 0 before SNTP validates the date
    uint8_t getDay() const { return _day; }
    bool hasCalendar() const { return _month >= 1 && _month <= 12; }
    WeatherType getWeatherType() const { return _weatherType; }
    uint8_t getHumidity() const { return _humidityPct; }
    uint8_t getPrecipIntensity() const { return _precipIntensity; }
    uint8_t getGeneration() const { return _generationIndex; }
    bool getUse24Hour() const { return _is24Hour; }

private:
    // Time state
    uint8_t _hour;
    uint8_t _minute;
    uint8_t _second;
    uint16_t _year;
    uint8_t _month;
    uint8_t _day;
    uint32_t _subSecondMs;
    bool _is24Hour;
    bool _isPM;

    // Weather & Plant metrics
    WeatherType _weatherType;
    int16_t _temperatureF;
    uint8_t _humidityPct;
    uint8_t _precipIntensity;
    uint8_t _generationIndex;

    // Colon pulse animation
    float _colonPulsePhase;

    void tickInternalClock(uint32_t deltaMs);
    void advanceCalendarDay();
};

extern HudManager HUD;

#endif // HUD_MANAGER_H
