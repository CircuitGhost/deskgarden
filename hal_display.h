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

class DisplayHAL {
public:
    DisplayHAL();
    bool begin();

    void setBrightness(uint8_t level);
    uint8_t getBrightness() const { return _brightness; }

    // Direct Full Framebuffer (172 x 320 x 2 = 110 KB in SRAM)
    uint16_t* getFramebuffer() { return _framebuffer; }
    void clear(uint16_t color = COLOR_BLACK);
    void flush();

    // Fast Drawing Primitives
    inline void drawPixel(int16_t x, int16_t y, uint16_t color) {
        if (x >= 0 && x < SCREEN_WIDTH && y >= 0 && y < SCREEN_HEIGHT && _framebuffer) {
            _framebuffer[y * SCREEN_WIDTH + x] = color;
        }
    }

    void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color);
    void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color);
    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);

    uint32_t getLastBlitTimeUs() const { return _lastBlitTimeUs; }
    Arduino_GFX* getGFX() { return _gfx; }
    Arduino_Canvas* getCanvas() { return _canvas; }

private:
    uint8_t _brightness;
    uint32_t _lastBlitTimeUs;
    uint16_t* _framebuffer;
    Arduino_DataBus* _bus;
    Arduino_GFX* _gfx;
    Arduino_Canvas* _canvas;
};

extern DisplayHAL Display;

#endif // HAL_DISPLAY_H
