#ifndef MICRO_FONT_H
#define MICRO_FONT_H

#include <Arduino.h>

// Crisp 5x7 bitmap font (ASCII 32 ' ' through 90 'Z', plus degree symbol and custom glyphs)
// Each character is 5 columns wide, 7 rows high (packed as 5 bytes, 1 byte per column)

extern const uint8_t micro_font_5x7[][5];

// Render a single character onto a 16-bit RGB565 buffer
void drawChar5x7(uint16_t* buffer, int16_t bufWidth, int16_t bufHeight, 
                 int16_t x, int16_t y, char c, uint16_t color);

// Render a string onto a 16-bit RGB565 buffer
int16_t drawString5x7(uint16_t* buffer, int16_t bufWidth, int16_t bufHeight, 
                      int16_t x, int16_t y, const char* str, uint16_t color, int16_t letterSpacing = 1);

// Measure string width in pixels
int16_t getStringWidth5x7(const char* str, int16_t letterSpacing = 1);

#endif // MICRO_FONT_H
