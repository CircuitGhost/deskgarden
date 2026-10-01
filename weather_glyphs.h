#ifndef WEATHER_GLYPHS_H
#define WEATHER_GLYPHS_H

#include <Arduino.h>

enum WeatherType {
    WEATHER_SUN = 0,
    WEATHER_PARTLY_CLOUDY,
    WEATHER_CLOUDY,
    WEATHER_RAIN,
    WEATHER_MOON
};

// Render 8x8 pixel micro-glyph with multi-tone colors
void drawWeatherGlyph(uint16_t* buffer, int16_t bufWidth, int16_t bufHeight, 
                      int16_t x, int16_t y, WeatherType type);

#endif // WEATHER_GLYPHS_H
