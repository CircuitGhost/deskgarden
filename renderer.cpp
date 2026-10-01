#include "renderer.h"
#include "particle_system.h"

MainRenderer Renderer;

MainRenderer::MainRenderer()
    : _lastFrameTime(0),
      _frameCount(0),
      _lastFpsCalcTime(0),
      _currentFps(0.0f),
      _lastFrameRenderTimeUs(0),
      _animationPhase(0.0f) {}

void MainRenderer::begin() {
    _lastFrameTime = millis();
    _lastFpsCalcTime = millis();
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

    // 1. Precalculate harmonic motion wave once per frame (172 points instead of 1,720)
    int16_t waveY[SCREEN_WIDTH];
    int16_t centerY = HUD_HEIGHT + (CANOPY_HEIGHT / 2) + 20;
    for (int16_t x = 0; x < SCREEN_WIDTH; x++) {
        float wave1 = sinf((x * 0.04f) + _animationPhase) * 16.0f;
        float wave2 = sinf((x * 0.08f) - (_animationPhase * 1.5f)) * 8.0f;
        waveY[x] = centerY + (int16_t)(wave1 + wave2);
    }

    // 2. 10-Band Render Loop (Only 11 KB RAM utilized!)
    for (uint8_t b = 0; b < NUM_BANDS; b++) {
        int16_t bandGlobalY0 = b * BAND_HEIGHT;

        // Render each row in this band
        for (int16_t localY = 0; localY < BAND_HEIGHT; localY++) {
            int16_t globalY = bandGlobalY0 + localY;

            if (globalY < HUD_HEIGHT) {
                // 1. Top HUD Row
                uint8_t r = 16 + (globalY * 6 / HUD_HEIGHT);
                uint8_t g = 20 + (globalY * 8 / HUD_HEIGHT);
                uint8_t b_col = 28 + (globalY * 10 / HUD_HEIGHT);
                uint16_t rowCol = (globalY == HUD_HEIGHT - 1) ? rgb565(55, 65, 82) : rgb565(r, g, b_col);
                Display.drawFastHLineLocal(0, localY, SCREEN_WIDTH, rowCol);
            } 
            else if (globalY < (HUD_HEIGHT + CANOPY_HEIGHT)) {
                // 2. Diurnal Sky Gradient Row
                int16_t skyY = globalY - HUD_HEIGHT;
                float factor = (float)skyY / (float)(CANOPY_HEIGHT - 1);
                uint8_t r = (uint8_t)(pal.skyTopR + factor * (pal.skyBottomR - pal.skyTopR));
                uint8_t g = (uint8_t)(pal.skyTopG + factor * (pal.skyBottomG - pal.skyTopG));
                uint8_t b_col = (uint8_t)(pal.skyTopB + factor * (pal.skyBottomB - pal.skyTopB));
                Display.drawFastHLineLocal(0, localY, SCREEN_WIDTH, rgb565(r, g, b_col));
            } 
            else {
                // 3. Substrate & Soil Row
                int16_t subY = globalY - (HUD_HEIGHT + CANOPY_HEIGHT);
                uint16_t rowCol = (subY == 0) ? rgb565(85, 140, 70) : rgb565(28, 20, 16);
                Display.drawFastHLineLocal(0, localY, SCREEN_WIDTH, rowCol);
            }
        }

        // Draw HUD Elements if in Band 0
        if (b == 0) {
            HUD.render(Display.getBandBuffer(), SCREEN_WIDTH, BAND_HEIGHT);
        }

        // Draw Celestial Sun Disc / Night Stars
        if (pal.starOpacity > 0.05f) {
            static const uint8_t starCoords[16][2] = {
                {20, 35}, {45, 60}, {85, 40}, {130, 50}, {155, 75},
                {30, 110}, {70, 95}, {115, 120}, {145, 140}, {15, 160},
                {60, 180}, {100, 165}, {140, 200}, {35, 220}, {80, 240}, {125, 230}
            };
            for (int i = 0; i < 16; i++) {
                int16_t sx = starCoords[i][0];
                int16_t sy = starCoords[i][1];
                if (sy >= bandGlobalY0 && sy < (bandGlobalY0 + BAND_HEIGHT)) {
                    float twinkle = 0.5f + 0.5f * sinf(_animationPhase * 2.5f + (i * 1.3f));
                    uint8_t starVal = (uint8_t)(255 * pal.starOpacity * twinkle);
                    if (starVal > 20) {
                        Display.drawPixelLocal(sx, sy - bandGlobalY0, rgb565(starVal, starVal, (uint8_t)(starVal * 0.9f)));
                    }
                }
            }
        }

        // Draw Harmonic Fluid Motion Wave
        for (int16_t x = 0; x < SCREEN_WIDTH; x++) {
            int16_t wy = waveY[x];
            if (wy >= bandGlobalY0 && wy < (bandGlobalY0 + BAND_HEIGHT)) {
                Display.drawPixelLocal(x, wy - bandGlobalY0, rgb565(180, 235, 255));
            }
        }

        // Draw Ambient Particles in this band
        Particles.renderBand(Display.getBandBuffer(), bandGlobalY0, BAND_HEIGHT, SCREEN_WIDTH);

        // Blit band to LCD
        Display.pushBand(b);
    }

    _lastFrameRenderTimeUs = micros() - startUs;
    _animationPhase += 0.05f;
    if (_animationPhase > 2 * PI) {
        _animationPhase -= 2 * PI;
    }
}
