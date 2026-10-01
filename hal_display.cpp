#include "hal_display.h"

// Override Arduino loopTask stack size to 32KB to eliminate stack overflow crashes
size_t getArduinoLoopTaskStackSize(void) {
    return 32768;
}

DisplayHAL Display;

DisplayHAL::DisplayHAL() 
    : _brightness(DEFAULT_BRIGHTNESS), 
      _lastBlitTimeUs(0),
      _bandBuffer(nullptr),
      _bus(nullptr),
      _gfx(nullptr) {}

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

    if (!_gfx->begin(LCD_SPI_FREQ)) {
        return false;
    }

    _gfx->fillScreen(COLOR_BLACK);

    // 5. Allocate 11 KB Band Buffer (172 * 32 * 2 bytes = 11,008 bytes)
    size_t bandSize = SCREEN_WIDTH * BAND_HEIGHT * sizeof(uint16_t);
    _bandBuffer = (uint16_t*)heap_caps_malloc(bandSize, MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
    if (!_bandBuffer) {
        _bandBuffer = (uint16_t*)malloc(bandSize);
    }

    if (_bandBuffer) {
        clearBand(COLOR_BLACK);
        for (uint8_t b = 0; b < NUM_BANDS; b++) {
            pushBand(b);
        }
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

void DisplayHAL::clearBand(uint16_t color) {
    if (!_bandBuffer) return;
    uint32_t totalPixels = SCREEN_WIDTH * BAND_HEIGHT;
    for (uint32_t i = 0; i < totalPixels; i++) {
        _bandBuffer[i] = color;
    }
}

void DisplayHAL::drawPixelLocal(int16_t x, int16_t localY, uint16_t color) {
    if (x < 0 || x >= SCREEN_WIDTH || localY < 0 || localY >= BAND_HEIGHT || !_bandBuffer) return;
    _bandBuffer[localY * SCREEN_WIDTH + x] = color;
}

void DisplayHAL::fillRectLocal(int16_t x, int16_t localY, int16_t w, int16_t h, uint16_t color) {
    if (x >= SCREEN_WIDTH || localY >= BAND_HEIGHT || !_bandBuffer) return;
    if (x < 0) { w += x; x = 0; }
    if (localY < 0) { h += localY; localY = 0; }
    if (x + w > SCREEN_WIDTH) { w = SCREEN_WIDTH - x; }
    if (localY + h > BAND_HEIGHT) { h = BAND_HEIGHT - localY; }
    if (w <= 0 || h <= 0) return;

    for (int16_t row = 0; row < h; row++) {
        uint16_t* ptr = &_bandBuffer[(localY + row) * SCREEN_WIDTH + x];
        for (int16_t col = 0; col < w; col++) {
            *ptr++ = color;
        }
    }
}

void DisplayHAL::drawFastHLineLocal(int16_t x, int16_t localY, int16_t w, uint16_t color) {
    if (localY < 0 || localY >= BAND_HEIGHT || !_bandBuffer) return;
    if (x < 0) { w += x; x = 0; }
    if (x + w > SCREEN_WIDTH) { w = SCREEN_WIDTH - x; }
    if (w <= 0) return;

    uint16_t* ptr = &_bandBuffer[localY * SCREEN_WIDTH + x];
    for (int16_t i = 0; i < w; i++) {
        *ptr++ = color;
    }
}

void DisplayHAL::drawFastVLineLocal(int16_t x, int16_t localY, int16_t h, uint16_t color) {
    if (x < 0 || x >= SCREEN_WIDTH || !_bandBuffer) return;
    if (localY < 0) { h += localY; localY = 0; }
    if (localY + h > BAND_HEIGHT) { h = BAND_HEIGHT - localY; }
    if (h <= 0) return;

    uint16_t* ptr = &_bandBuffer[localY * SCREEN_WIDTH + x];
    for (int16_t i = 0; i < h; i++) {
        *ptr = color;
        ptr += SCREEN_WIDTH;
    }
}

void DisplayHAL::pushBand(uint8_t bandIndex) {
    if (!_bandBuffer || !_gfx || bandIndex >= NUM_BANDS) return;
    uint32_t startUs = micros();

    int16_t y = bandIndex * BAND_HEIGHT;
    _gfx->draw16bitRGBBitmap(0, y, _bandBuffer, SCREEN_WIDTH, BAND_HEIGHT);

    _lastBlitTimeUs = micros() - startUs;
}
