#include "renderer.h"
#include "particle_system.h"
#include "plant_engine.h"

MainRenderer Renderer;

// 256-entry integer sine LUT (-127 to +127) for instant 1-cycle trig calculations
static const int8_t SIN_TABLE_256[256] = {
       0,    3,    6,    9,   12,   16,   19,   22,   25,   28,   31,   34,   37,   40,   43,   46,
      49,   51,   54,   57,   60,   63,   65,   68,   71,   73,   76,   78,   81,   83,   85,   88,
      90,   92,   94,   96,   98,  100,  102,  104,  106,  107,  109,  111,  112,  113,  115,  116,
     117,  118,  120,  121,  122,  122,  123,  124,  125,  125,  126,  126,  126,  127,  127,  127,
     127,  127,  127,  127,  126,  126,  126,  125,  125,  124,  123,  122,  122,  121,  120,  118,
     117,  116,  115,  113,  112,  111,  109,  107,  106,  104,  102,  100,   98,   96,   94,   92,
      90,   88,   85,   83,   81,   78,   76,   73,   71,   68,   65,   63,   60,   57,   54,   51,
      49,   46,   43,   40,   37,   34,   31,   28,   25,   22,   19,   16,   12,    9,    6,    3,
       0,   -3,   -6,   -9,  -12,  -16,  -19,  -22,  -25,  -28,  -31,  -34,  -37,  -40,  -43,  -46,
     -49,  -51,  -54,  -57,  -60,  -63,  -65,  -68,  -71,  -73,  -76,  -78,  -81,  -83,  -85,  -88,
     -90,  -92,  -94,  -96,  -98, -100, -102, -104, -106, -107, -109, -111, -112, -113, -115, -116,
    -117, -118, -120, -121, -122, -122, -123, -124, -125, -125, -126, -126, -126, -127, -127, -127,
    -127, -127, -127, -127, -126, -126, -126, -125, -125, -124, -123, -122, -122, -121, -120, -118,
    -117, -116, -115, -113, -112, -111, -109, -107, -106, -104, -102, -100,  -98,  -96,  -94,  -92,
     -90,  -88,  -85,  -83,  -81,  -78,  -76,  -73,  -71,  -68,  -65,  -63,  -60,  -57,  -54,  -51,
     -49,  -46,  -43,  -40,  -37,  -34,  -31,  -28,  -25,  -22,  -19,  -16,  -12,   -9,   -6,   -3,
};

static inline int8_t fastSin256(uint8_t angle) {
    return SIN_TABLE_256[angle];
}

MainRenderer::MainRenderer()
    : _lastFrameTime(0),
      _frameCount(0),
      _lastFpsCalcTime(0),
      _currentFps(0.0f),
      _lastFrameRenderTimeUs(0),
      _animPhase(0),
      _lastSkyTopR(0), _lastSkyTopG(0), _lastSkyTopB(0),
      _lastSkyBottomR(0), _lastSkyBottomG(0), _lastSkyBottomB(0) {
    for (int16_t i = 0; i < CANOPY_HEIGHT; i++) {
        _cachedSkyColors[i] = COLOR_BLACK;
    }
}

void MainRenderer::begin() {
    _lastFrameTime = millis();
    _lastFpsCalcTime = millis();
}

void MainRenderer::updateSkyGradientCache(const AtmospherePalette& pal) {
    if (pal.skyTopR == _lastSkyTopR && pal.skyTopG == _lastSkyTopG && pal.skyTopB == _lastSkyTopB &&
        pal.skyBottomR == _lastSkyBottomR && pal.skyBottomG == _lastSkyBottomG && pal.skyBottomB == _lastSkyBottomB) {
        return; // Palette unchanged
    }

    _lastSkyTopR = pal.skyTopR; _lastSkyTopG = pal.skyTopG; _lastSkyTopB = pal.skyTopB;
    _lastSkyBottomR = pal.skyBottomR; _lastSkyBottomG = pal.skyBottomG; _lastSkyBottomB = pal.skyBottomB;

    // Fixed-point 16.16 gradient interpolation (Zero float overhead)
    int32_t r16 = (int32_t)pal.skyTopR << 16;
    int32_t g16 = (int32_t)pal.skyTopG << 16;
    int32_t b16 = (int32_t)pal.skyTopB << 16;

    int32_t dr16 = (((int32_t)pal.skyBottomR - pal.skyTopR) << 16) / (CANOPY_HEIGHT - 1);
    int32_t dg16 = (((int32_t)pal.skyBottomG - pal.skyTopG) << 16) / (CANOPY_HEIGHT - 1);
    int32_t db16 = (((int32_t)pal.skyBottomB - pal.skyTopB) << 16) / (CANOPY_HEIGHT - 1);

    for (int16_t y = 0; y < CANOPY_HEIGHT; y++) {
        _cachedSkyColors[y] = rgb565((uint8_t)(r16 >> 16), (uint8_t)(g16 >> 16), (uint8_t)(b16 >> 16));
        r16 += dr16;
        g16 += dg16;
        b16 += db16;
    }
}

void MainRenderer::renderFrame() {
    uint32_t startUs = micros();
    uint32_t now = millis();

    _frameCount++;
    if (now - _lastFpsCalcTime >= 1000) {
        _currentFps = (_frameCount * 1000.0f) / (float)(now - _lastFpsCalcTime);
        _frameCount = 0;
        _lastFpsCalcTime = now;
    }

    AtmospherePalette pal = Atmosphere.getCurrentPalette();
    updateSkyGradientCache(pal);

    uint16_t* fb = Display.getFramebuffer();
    if (!fb) return;

    // 1. Render Top HUD Bar (y = 0 .. HUD_HEIGHT - 1)
    HUD.render(fb, SCREEN_WIDTH, SCREEN_HEIGHT);

    // 2. Render Diurnal Sky Canopy (y = HUD_HEIGHT .. HUD_HEIGHT + CANOPY_HEIGHT - 1)
    for (int16_t skyY = 0; skyY < CANOPY_HEIGHT; skyY++) {
        Display.drawFastHLine(0, HUD_HEIGHT + skyY, SCREEN_WIDTH, _cachedSkyColors[skyY]);
    }

    // 3. Render Substrate & Soil (y = HUD_HEIGHT + CANOPY_HEIGHT .. SCREEN_HEIGHT - 1)
    Display.drawFastHLine(0, HUD_HEIGHT + CANOPY_HEIGHT, SCREEN_WIDTH, rgb565(85, 140, 70)); // Grass line
    Display.fillRect(0, HUD_HEIGHT + CANOPY_HEIGHT + 1, SCREEN_WIDTH, SUBSTRATE_HEIGHT - 1, rgb565(28, 20, 16)); // Soil

    // 4. Render Night Stars / Celestial Twinkle (Integer lookup)
    if (pal.starOpacity > 0.05f) {
        static const uint8_t starCoords[16][2] = {
            {20, 35}, {45, 60}, {85, 40}, {130, 50}, {155, 75},
            {30, 110}, {70, 95}, {115, 120}, {145, 140}, {15, 160},
            {60, 180}, {100, 165}, {140, 200}, {35, 220}, {80, 240}, {125, 230}
        };
        for (int i = 0; i < 16; i++) {
            int16_t sx = starCoords[i][0];
            int16_t sy = starCoords[i][1];
            int8_t s = fastSin256((uint8_t)((_animPhase * 3) + (i * 16)));
            uint8_t twinkle255 = 128 + (s >> 1); // 64 to 191
            uint8_t starVal = (uint8_t)((255 * pal.starOpacity * twinkle255) / 255);
            if (starVal > 20) {
                Display.drawPixel(sx, sy, rgb565(starVal, starVal, (uint8_t)(starVal * 0.9f)));
            }
        }
    }

    // 5. Render Harmonic Fluid Motion Wave (Pure Integer LUT lookup)
    int16_t centerY = HUD_HEIGHT + (CANOPY_HEIGHT / 2) + 20;
    for (int16_t x = 0; x < SCREEN_WIDTH; x++) {
        int16_t wave1 = (fastSin256((uint8_t)((x * 2) + _animPhase)) * 14) >> 7;
        int16_t wave2 = (fastSin256((uint8_t)((x * 4) - (_animPhase * 2))) * 6) >> 7;
        int16_t wy = centerY + wave1 + wave2;
        Display.drawPixel(x, wy, rgb565(180, 235, 255));
    }

    // 6. Render Procedural Botanical Plant (Stems, Branches, Leaves)
    Plant.render(fb, SCREEN_WIDTH, SCREEN_HEIGHT);

    // 7. Render Ambient Particles
    Particles.render(fb, SCREEN_WIDTH, SCREEN_HEIGHT);

    _lastFrameRenderTimeUs = micros() - startUs;

    // 8. Flush Full Frame to Display via SPI
    Display.flush();

    _animPhase += 3;
}
