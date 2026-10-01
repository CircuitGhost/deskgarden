#include "weather_glyphs.h"
#include "hal_display.h"

// 8x8 Weather Bitmaps (Row-major, 8 bytes per icon)
static const uint8_t glyph_sun[8] = {
    0b00100100,
    0b00011000,
    0b10111101,
    0b01111110,
    0b01111110,
    0b10111101,
    0b00011000,
    0b00100100
};

static const uint8_t glyph_partly_cloudy[8] = {
    0b00110000,
    0b01111000,
    0b00110110,
    0b00011111,
    0b00111111,
    0b01111111,
    0b01111111,
    0b00000000
};

static const uint8_t glyph_cloudy[8] = {
    0b00000000,
    0b00110110,
    0b01111111,
    0b11111111,
    0b11111111,
    0b01111110,
    0b00000000,
    0b00000000
};

static const uint8_t glyph_rain[8] = {
    0b00110110,
    0b01111111,
    0b11111111,
    0b01111110,
    0b00000000,
    0b01001001,
    0b00100100,
    0b01001001
};

static const uint8_t glyph_moon[8] = {
    0b00011100,
    0b00111000,
    0b01110000,
    0b01110000,
    0b01110000,
    0b00111000,
    0b00011100,
    0b00000000
};

void drawWeatherGlyph(uint16_t* buffer, int16_t bufWidth, int16_t bufHeight, 
                      int16_t x, int16_t y, WeatherType type) {
    if (!buffer) return;

    const uint8_t* bitmap = nullptr;
    uint16_t primaryColor = COLOR_YELLOW;
    uint16_t secondaryColor = COLOR_CYAN;

    switch (type) {
        case WEATHER_SUN:
            bitmap = glyph_sun;
            primaryColor = COLOR_AMBER;
            break;
        case WEATHER_PARTLY_CLOUDY:
            bitmap = glyph_partly_cloudy;
            primaryColor = COLOR_PEACH;
            secondaryColor = COLOR_LIGHTGREY;
            break;
        case WEATHER_CLOUDY:
            bitmap = glyph_cloudy;
            primaryColor = COLOR_LIGHTGREY;
            break;
        case WEATHER_RAIN:
            bitmap = glyph_rain;
            primaryColor = COLOR_LIGHTGREY;
            secondaryColor = COLOR_CYAN;
            break;
        case WEATHER_MOON:
            bitmap = glyph_moon;
            primaryColor = COLOR_LAVENDER;
            break;
    }

    if (!bitmap) return;

    for (int row = 0; row < 8; row++) {
        int16_t py = y + row;
        if (py < 0 || py >= bufHeight) continue;

        uint8_t rowBits = bitmap[row];
        for (int col = 0; col < 8; col++) {
            int16_t px = x + col;
            if (px < 0 || px >= bufWidth) continue;

            if (rowBits & (1 << (7 - col))) {
                uint16_t colVal = primaryColor;
                // For rain drops (bottom 3 rows of rain glyph), color with cyan
                if (type == WEATHER_RAIN && row >= 5) {
                    colVal = secondaryColor;
                }
                buffer[py * bufWidth + px] = colVal;
            }
        }
    }
}
