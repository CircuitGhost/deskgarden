#include "hal_display.h"

// ST7789 Command Set
#define ST7789_NOP        0x00
#define ST7789_SWRESET    0x01
#define ST7789_SLPIN      0x10
#define ST7789_SLPOUT     0x11
#define ST7789_NORON      0x13
#define ST7789_INVOFF     0x20
#define ST7789_INVON      0x21
#define ST7789_DISPOFF    0x28
#define ST7789_DISPON     0x29
#define ST7789_CASET      0x2A
#define ST7789_RASET      0x2B
#define ST7789_RAMWR      0x2C
#define ST7789_COLMOD     0x3A
#define ST7789_MADCTL     0x36

#define MADCTL_MY  0x80
#define MADCTL_MX  0x40
#define MADCTL_MV  0x20
#define MADCTL_ML  0x10
#define MADCTL_RGB 0x00
#define MADCTL_BGR 0x08

DisplayHAL Display;

DisplayHAL::DisplayHAL() 
    : _brightness(DEFAULT_BRIGHTNESS), 
      _colOffset(34),      // Standard 172x320 offset in 240x320 matrix ((240-172)/2 = 34)
      _rowOffset(0), 
      _lastBlitTimeUs(0),
      _frameBuffer(nullptr),
      _dmaActive(false) {}

void DisplayHAL::writeCommand(uint8_t cmd) {
    digitalWrite(PIN_LCD_DC, LOW);
    digitalWrite(PIN_LCD_CS, LOW);
    SPI.transfer(cmd);
    digitalWrite(PIN_LCD_CS, HIGH);
}

void DisplayHAL::writeData(uint8_t data) {
    digitalWrite(PIN_LCD_DC, HIGH);
    digitalWrite(PIN_LCD_CS, LOW);
    SPI.transfer(data);
    digitalWrite(PIN_LCD_CS, HIGH);
}

void DisplayHAL::writeData16(uint16_t data) {
    digitalWrite(PIN_LCD_DC, HIGH);
    digitalWrite(PIN_LCD_CS, LOW);
    SPI.transfer16(data);
    digitalWrite(PIN_LCD_CS, HIGH);
}

void DisplayHAL::writeDataChunk(const uint16_t* data, uint32_t len) {
    digitalWrite(PIN_LCD_DC, HIGH);
    digitalWrite(PIN_LCD_CS, LOW);
    SPI.writeBytes((const uint8_t*)data, len * 2);
    digitalWrite(PIN_LCD_CS, HIGH);
}

void DisplayHAL::setAddrWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    uint16_t xa = x0 + _colOffset;
    uint16_t xb = x1 + _colOffset;
    uint16_t ya = y0 + _rowOffset;
    uint16_t yb = y1 + _rowOffset;

    writeCommand(ST7789_CASET);
    digitalWrite(PIN_LCD_DC, HIGH);
    digitalWrite(PIN_LCD_CS, LOW);
    SPI.transfer16(xa);
    SPI.transfer16(xb);
    digitalWrite(PIN_LCD_CS, HIGH);

    writeCommand(ST7789_RASET);
    digitalWrite(PIN_LCD_DC, HIGH);
    digitalWrite(PIN_LCD_CS, LOW);
    SPI.transfer16(ya);
    SPI.transfer16(yb);
    digitalWrite(PIN_LCD_CS, HIGH);

    writeCommand(ST7789_RAMWR);
}

void DisplayHAL::initST7789() {
    // Hardware Reset Pulse
    if (PIN_LCD_RST >= 0) {
        pinMode(PIN_LCD_RST, OUTPUT);
        digitalWrite(PIN_LCD_RST, HIGH);
        delay(20);
        digitalWrite(PIN_LCD_RST, LOW);
        delay(50);
        digitalWrite(PIN_LCD_RST, HIGH);
        delay(120);
    }

    writeCommand(ST7789_SWRESET);
    delay(150);

    writeCommand(ST7789_SLPOUT);
    delay(120);

    writeCommand(ST7789_COLMOD);
    writeData(0x55); // 16-bit 65k RGB565 color format

    writeCommand(ST7789_MADCTL);
    writeData(MADCTL_RGB); // Default orientation

    writeCommand(ST7789_INVON); // ST7789 IPS panels typically require display inversion
    delay(10);

    writeCommand(ST7789_NORON);
    delay(10);

    writeCommand(ST7789_DISPON);
    delay(100);
}

bool DisplayHAL::begin() {
    pinMode(PIN_LCD_DC, OUTPUT);
    pinMode(PIN_LCD_CS, OUTPUT);
    digitalWrite(PIN_LCD_CS, HIGH);
    digitalWrite(PIN_LCD_DC, HIGH);

    // Initialize SPI
    SPI.begin(PIN_LCD_SCLK, -1, PIN_LCD_MOSI, PIN_LCD_CS);
    SPI.setFrequency(LCD_SPI_FREQ);
    SPI.setDataMode(SPI_MODE0);

    // Allocate Double Buffer (172 * 320 * 2 bytes = 110,080 bytes in internal/PSRAM heap)
    size_t bufferSize = SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(uint16_t);
    _frameBuffer = (uint16_t*)heap_caps_malloc(bufferSize, MALLOC_CAP_DMA | MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
    if (!_frameBuffer) {
        _frameBuffer = (uint16_t*)malloc(bufferSize);
    }

    initST7789();

    // Configure Backlight PWM
    #if defined(ESP32)
    ledcAttach(PIN_LCD_BL, LCD_BL_PWM_FREQ, LCD_BL_PWM_RES);
    setBrightness(_brightness);
    #else
    pinMode(PIN_LCD_BL, OUTPUT);
    digitalWrite(PIN_LCD_BL, HIGH);
    #endif

    clear(COLOR_BLACK);
    pushFrame();
    return (_frameBuffer != nullptr);
}

void DisplayHAL::setBrightness(uint8_t level) {
    _brightness = level;
    #if defined(ESP32)
    ledcWrite(PIN_LCD_BL, _brightness);
    #else
    analogWrite(PIN_LCD_BL, _brightness);
    #endif
}

void DisplayHAL::clear(uint16_t color) {
    if (!_frameBuffer) return;
    uint32_t totalPixels = SCREEN_WIDTH * SCREEN_HEIGHT;
    for (uint32_t i = 0; i < totalPixels; i++) {
        _frameBuffer[i] = color;
    }
}

void DisplayHAL::drawPixel(int16_t x, int16_t y, uint16_t color) {
    if (x < 0 || x >= SCREEN_WIDTH || y < 0 || y >= SCREEN_HEIGHT || !_frameBuffer) return;
    _frameBuffer[y * SCREEN_WIDTH + x] = color;
}

void DisplayHAL::drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) {
    if (y < 0 || y >= SCREEN_HEIGHT || !_frameBuffer) return;
    if (x < 0) { w += x; x = 0; }
    if (x + w > SCREEN_WIDTH) { w = SCREEN_WIDTH - x; }
    if (w <= 0) return;

    uint16_t* ptr = &_frameBuffer[y * SCREEN_WIDTH + x];
    for (int16_t i = 0; i < w; i++) {
        *ptr++ = color;
    }
}

void DisplayHAL::drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) {
    if (x < 0 || x >= SCREEN_WIDTH || !_frameBuffer) return;
    if (y < 0) { h += y; y = 0; }
    if (y + h > SCREEN_HEIGHT) { h = SCREEN_HEIGHT - y; }
    if (h <= 0) return;

    uint16_t* ptr = &_frameBuffer[y * SCREEN_WIDTH + x];
    for (int16_t i = 0; i < h; i++) {
        *ptr = color;
        ptr += SCREEN_WIDTH;
    }
}

void DisplayHAL::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    if (x >= SCREEN_WIDTH || y >= SCREEN_HEIGHT || !_frameBuffer) return;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > SCREEN_WIDTH) { w = SCREEN_WIDTH - x; }
    if (y + h > SCREEN_HEIGHT) { h = SCREEN_HEIGHT - y; }
    if (w <= 0 || h <= 0) return;

    for (int16_t row = 0; row < h; row++) {
        uint16_t* ptr = &_frameBuffer[(y + row) * SCREEN_WIDTH + x];
        for (int16_t col = 0; col < w; col++) {
            *ptr++ = color;
        }
    }
}

void DisplayHAL::drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) {
    int16_t steep = abs(y1 - y0) > abs(x1 - x0);
    if (steep) {
        int16_t t;
        t = x0; x0 = y0; y0 = t;
        t = x1; x1 = y1; y1 = t;
    }
    if (x0 > x1) {
        int16_t t;
        t = x0; x0 = x1; x1 = t;
        t = y0; y0 = y1; y1 = t;
    }

    int16_t dx = x1 - x0;
    int16_t dy = abs(y1 - y0);
    int16_t err = dx / 2;
    int16_t ystep = (y0 < y1) ? 1 : -1;

    for (; x0 <= x1; x0++) {
        if (steep) {
            drawPixel(y0, x0, color);
        } else {
            drawPixel(x0, y0, color);
        }
        err -= dy;
        if (err < 0) {
            y0 += ystep;
            err += dx;
        }
    }
}

void DisplayHAL::drawGradientV(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t colorTop, uint16_t colorBottom) {
    if (h <= 0 || w <= 0) return;

    uint8_t r1 = (colorTop >> 11) & 0x1F;
    uint8_t g1 = (colorTop >> 5) & 0x3F;
    uint8_t b1 = colorTop & 0x1F;

    uint8_t r2 = (colorBottom >> 11) & 0x1F;
    uint8_t g2 = (colorBottom >> 5) & 0x3F;
    uint8_t b2 = colorBottom & 0x1F;

    for (int16_t row = 0; row < h; row++) {
        float factor = (float)row / (float)(h - 1);
        uint8_t r = (uint8_t)(r1 + factor * (r2 - r1));
        uint8_t g = (uint8_t)(g1 + factor * (g2 - g1));
        uint8_t b = (uint8_t)(b1 + factor * (b2 - b1));
        uint16_t blended = (r << 11) | (g << 5) | b;

        drawFastHLine(x, y + row, w, blended);
    }
}

void DisplayHAL::pushFrame() {
    if (!_frameBuffer) return;
    uint32_t startUs = micros();

    setAddrWindow(0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1);
    digitalWrite(PIN_LCD_DC, HIGH);
    digitalWrite(PIN_LCD_CS, LOW);

    // Byte-swap for SPI transfer if required by big-endian / little-endian alignment
    // ST7789 expects MSB first
    uint32_t totalPixels = SCREEN_WIDTH * SCREEN_HEIGHT;
    uint8_t* bytePtr = (uint8_t*)_frameBuffer;

    // Fast SPI bulk transfer
    SPI.writeBytes(bytePtr, totalPixels * 2);

    digitalWrite(PIN_LCD_CS, HIGH);
    _lastBlitTimeUs = micros() - startUs;
}
