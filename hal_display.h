#ifndef HAL_DISPLAY_H
#define HAL_DISPLAY_H

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include "config.h"

// 16-bit RGB565 Color Definitions (Standard format for Arduino_GFX)
inline uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

#define COLOR_BLACK       0x0000
#define COLOR_WHITE       0xFFFF
#define COLOR_NAVY        rgb565(0, 0, 128)
#define COLOR_DARKGREEN   rgb565(0, 100, 0)
#define COLOR_DARKCYAN    rgb565(0, 139, 139)
#define COLOR_LIGHTGREY   rgb565(180, 180, 180)
#define COLOR_DARKGREY    rgb565(60, 60, 60)
#define COLOR_BLUE        rgb565(0, 0, 255)
#define COLOR_GREEN       rgb565(0, 255, 0)
#define COLOR_CYAN        rgb565(0, 255, 255)
#define COLOR_RED         rgb565(255, 0, 0)
#define COLOR_MAGENTA     rgb565(255, 0, 255)
#define COLOR_YELLOW      rgb565(255, 255, 0)
#define COLOR_AMBER       rgb565(255, 180, 0)
#define COLOR_PEACH       rgb565(255, 180, 140)
#define COLOR_LAVENDER    rgb565(200, 160, 255)

// Band buffer height (172 * 32 * 2 = 11,008 bytes -> lightweight and rock-solid)
#define BAND_HEIGHT       32
#define NUM_BANDS         (SCREEN_HEIGHT / BAND_HEIGHT) // 320 / 32 = 10 bands

class DisplayHAL {
public:
    DisplayHAL();
    bool begin();

    void setBrightness(uint8_t level);
    uint8_t getBrightness() const { return _brightness; }

    // Band Buffer (172 x 32)
    uint16_t* getBandBuffer() { return _bandBuffer; }
    void clearBand(uint16_t color = COLOR_BLACK);
    void pushBand(uint8_t bandIndex);

    // Primitives on Band Buffer (local y = 0 to BAND_HEIGHT - 1)
    void drawPixelLocal(int16_t x, int16_t localY, uint16_t color);
    void fillRectLocal(int16_t x, int16_t localY, int16_t w, int16_t h, uint16_t color);
    void drawFastHLineLocal(int16_t x, int16_t localY, int16_t w, uint16_t color);
    void drawFastVLineLocal(int16_t x, int16_t localY, int16_t h, uint16_t color);

    uint32_t getLastBlitTimeUs() const { return _lastBlitTimeUs; }
    Arduino_GFX* getGFX() { return _gfx; }

private:
    uint8_t _brightness;
    uint32_t _lastBlitTimeUs;
    uint16_t* _bandBuffer;
    Arduino_DataBus* _bus;
    Arduino_GFX* _gfx;
};

extern DisplayHAL Display;

#endif // HAL_DISPLAY_H
