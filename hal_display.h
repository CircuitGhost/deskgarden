#ifndef HAL_DISPLAY_H
#define HAL_DISPLAY_H

#include <Arduino.h>
#include <SPI.h>
#include "config.h"

// 16-bit RGB565 Color Definitions
#define COLOR_BLACK       0x0000
#define COLOR_WHITE       0xFFFF
#define COLOR_NAVY        0x000F
#define COLOR_DARKGREEN   0x03E0
#define COLOR_DARKCYAN    0x03EF
#define COLOR_MAROON      0x7800
#define COLOR_PURPLE      0x780F
#define COLOR_OLIVE       0x7BE0
#define COLOR_LIGHTGREY   0xC618
#define COLOR_DARKGREY    0x7BEF
#define COLOR_BLUE        0x001F
#define COLOR_GREEN       0x07E0
#define COLOR_CYAN        0x07FF
#define COLOR_RED         0xF800
#define COLOR_MAGENTA     0xF81F
#define COLOR_YELLOW      0xFFE0
#define COLOR_ORANGE      0xFD20
#define COLOR_AMBER       0xFDC0
#define COLOR_PEACH       0xFDB8
#define COLOR_LAVENDER    0xC57D
#define COLOR_PASTEL_TEAL 0x7E37

// Convert 8-bit RGB to 16-bit RGB565
inline uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

class DisplayHAL {
public:
    DisplayHAL();
    bool begin();
    
    // Backlight Control
    void setBrightness(uint8_t level);
    uint8_t getBrightness() const { return _brightness; }

    // Direct Display Control
    void setAddrWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
    void writeCommand(uint8_t cmd);
    void writeData(uint8_t data);
    void writeData16(uint16_t data);
    void writeDataChunk(const uint16_t* data, uint32_t len);

    // Framebuffer / Canvas Management
    uint16_t* getBackBuffer() { return _frameBuffer; }
    void clear(uint16_t color = COLOR_BLACK);
    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
    void drawPixel(int16_t x, int16_t y, uint16_t color);
    void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color);
    void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color);
    void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color);
    void drawGradientV(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t colorTop, uint16_t colorBottom);
    
    // Blit current backbuffer to LCD via DMA / SPI
    void pushFrame();

    // Diagnostics
    uint32_t getLastBlitTimeUs() const { return _lastBlitTimeUs; }

private:
    uint8_t _brightness;
    uint8_t _colOffset;
    uint8_t _rowOffset;
    uint32_t _lastBlitTimeUs;
    uint16_t* _frameBuffer;
    bool _dmaActive;

    void initST7789();
};

extern DisplayHAL Display;

#endif // HAL_DISPLAY_H
