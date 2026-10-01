#include "hal_display.h"

// Override Arduino loopTask stack size to 32KB to eliminate stack overflow crashes
size_t getArduinoLoopTaskStackSize(void) {
    return 32768;
}

DisplayHAL Display;

DisplayHAL::DisplayHAL() 
    : _brightness(DEFAULT_BRIGHTNESS), 
      _lastBlitTimeUs(0),
      _framebuffer(nullptr),
      _bus(nullptr),
      _gfx(nullptr),
      _canvas(nullptr) {}

bool DisplayHAL::begin() {
    // 1. Isolate SD Card on shared SPI bus
    pinMode(PIN_SD_CS, OUTPUT);
    digitalWrite(PIN_SD_CS, HIGH);

    // 2. Backlight Control
    if (PIN_LCD_BL >= 0) {
        pinMode(PIN_LCD_BL, OUTPUT);
        digitalWrite(PIN_LCD_BL, HIGH);
    }

    // 3. Hardware SPI DataBus (DC, CS, SCK, MOSI, MISO)
    _bus = new Arduino_HWSPI(
        PIN_LCD_DC, PIN_LCD_CS, PIN_LCD_SCLK, PIN_LCD_MOSI, GFX_NOT_DEFINED
    );

    // 4. Waveshare ST7789 172x320 panel with 34px col offset & IPS inversion (matches skydesk)
    _gfx = new Arduino_ST7789(
        _bus, PIN_LCD_RST, 0 /* rotation */, true /* IPS */,
        SCREEN_WIDTH, SCREEN_HEIGHT,
        34 /* col_offset1 */, 0 /* row_offset1 */, 34 /* col_offset2 */, 0 /* row_offset2 */
    );

    if (!_gfx->begin(80000000)) {
        return false;
    }

    _gfx->fillScreen(COLOR_BLACK);

    // 5. Allocate 110 KB Full Framebuffer Canvas (172 x 320 x 2 bytes)
    _canvas = new Arduino_Canvas(
        SCREEN_WIDTH, SCREEN_HEIGHT, _gfx, 0, 0, 0
    );

    if (_canvas->begin(GFX_SKIP_OUTPUT_BEGIN)) {
        _framebuffer = _canvas->getFramebuffer();
    } else {
        // Fallback: allocate raw framebuffer in SRAM
        size_t fbSize = SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(uint16_t);
        _framebuffer = (uint16_t*)heap_caps_malloc(fbSize, MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
        if (!_framebuffer) {
            _framebuffer = (uint16_t*)malloc(fbSize);
        }
    }

    if (_framebuffer) {
        clear(COLOR_BLACK);
        flush();
        return true;
    }
    return false;
}

void DisplayHAL::setBrightness(uint8_t level) {
    _brightness = level;
    if (PIN_LCD_BL >= 0) {
        digitalWrite(PIN_LCD_BL, (_brightness > 0) ? HIGH : LOW);
    }
}

void DisplayHAL::clear(uint16_t color) {
    if (!_framebuffer) return;
    uint32_t totalPixels = SCREEN_WIDTH * SCREEN_HEIGHT;
    uint32_t color32 = ((uint32_t)color << 16) | color;
    uint32_t* ptr32 = (uint32_t*)_framebuffer;
    uint32_t count32 = totalPixels >> 1;
    while (count32--) {
        *ptr32++ = color32;
    }
}

void DisplayHAL::drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) {
    if (y < 0 || y >= SCREEN_HEIGHT || !_framebuffer) return;
    if (x < 0) { w += x; x = 0; }
    if (x + w > SCREEN_WIDTH) { w = SCREEN_WIDTH - x; }
    if (w <= 0) return;

    uint16_t* ptr = &_framebuffer[y * SCREEN_WIDTH + x];
    uint32_t color32 = ((uint32_t)color << 16) | color;

    if (((uintptr_t)ptr & 2) && w > 0) {
        *ptr++ = color;
        w--;
    }

    uint32_t* ptr32 = (uint32_t*)ptr;
    int16_t count32 = w >> 1;
    while (count32--) {
        *ptr32++ = color32;
    }

    if (w & 1) {
        ptr = (uint16_t*)ptr32;
        *ptr = color;
    }
}

void DisplayHAL::drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) {
    if (x < 0 || x >= SCREEN_WIDTH || !_framebuffer) return;
    if (y < 0) { h += y; y = 0; }
    if (y + h > SCREEN_HEIGHT) { h = SCREEN_HEIGHT - y; }
    if (h <= 0) return;

    uint16_t* ptr = &_framebuffer[y * SCREEN_WIDTH + x];
    for (int16_t i = 0; i < h; i++) {
        *ptr = color;
        ptr += SCREEN_WIDTH;
    }
}

void DisplayHAL::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    if (x >= SCREEN_WIDTH || y >= SCREEN_HEIGHT || !_framebuffer) return;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > SCREEN_WIDTH) { w = SCREEN_WIDTH - x; }
    if (y + h > SCREEN_HEIGHT) { h = SCREEN_HEIGHT - y; }
    if (w <= 0 || h <= 0) return;

    for (int16_t row = 0; row < h; row++) {
        drawFastHLine(x, y + row, w, color);
    }
}

void DisplayHAL::flush() {
    if (!_framebuffer || !_gfx) return;
    uint32_t startUs = micros();

    if (_canvas) {
        _canvas->flush();
    } else {
        _gfx->draw16bitRGBBitmap(0, 0, _framebuffer, SCREEN_WIDTH, SCREEN_HEIGHT);
    }

    _lastBlitTimeUs = micros() - startUs;
}
