#include "renderer.h"

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

    // 1. Frame-Rate Calculation
    _frameCount++;
    if (now - _lastFpsCalcTime >= 1000) {
        _currentFps = (_frameCount * 1000.0f) / (float)(now - _lastFpsCalcTime);
        _frameCount = 0;
        _lastFpsCalcTime = now;
    }

    // 2. Clear Screen & Draw Slice 1 Pipeline Visual Test
    drawSlice1TestPattern();

    #if ENABLE_FPS_COUNTER
    drawDiagnosticsOverlay();
    #endif

    // 3. Blit to Screen (DMA / SPI)
    Display.pushFrame();

    _lastFrameRenderTimeUs = micros() - startUs;
    _animationPhase += 0.05f;
    if (_animationPhase > 2 * PI) {
        _animationPhase -= 2 * PI;
    }
}

void MainRenderer::drawSlice1TestPattern() {
    // 1. Top HUD Region (0 to 24px) - Rendered via HUD Manager
    HUD.render(Display.getBackBuffer(), SCREEN_WIDTH, SCREEN_HEIGHT);

    // 2. Canopy Chamber (24 to 280px) - Dynamic Diurnal Atmospheric Sky
    Atmosphere.renderSky(Display.getBackBuffer(), SCREEN_WIDTH, SCREEN_HEIGHT, HUD_HEIGHT, CANOPY_HEIGHT);

    AtmospherePalette pal = Atmosphere.getCurrentPalette();

    // Render Night Stars if starOpacity > 0
    if (pal.starOpacity > 0.05f) {
        static const uint8_t starCoords[16][2] = {
            {20, 35}, {45, 60}, {85, 40}, {130, 50}, {155, 75},
            {30, 110}, {70, 95}, {115, 120}, {145, 140}, {15, 160},
            {60, 180}, {100, 165}, {140, 200}, {35, 220}, {80, 240}, {125, 230}
        };

        for (int i = 0; i < 16; i++) {
            float twinkle = 0.5f + 0.5f * sinf(_animationPhase * 2.5f + (i * 1.3f));
            uint8_t starVal = (uint8_t)(255 * pal.starOpacity * twinkle);
            if (starVal > 20) {
                uint16_t starCol = rgb565(starVal, starVal, (uint8_t)(starVal * 0.9f));
                Display.drawPixel(starCoords[i][0], starCoords[i][1], starCol);
            }
        }
    }

    // Render Celestial Sun / Moon glow in upper sky
    int16_t sunX = (int16_t)(30 + pal.celestialPos * (SCREEN_WIDTH - 60));
    int16_t sunY = HUD_HEIGHT + 35 - (int16_t)(sinf(pal.celestialPos * PI) * 15.0f);
    
    if (Atmosphere.getCurrentPhase() == PHASE_GOLDEN_HOUR || Atmosphere.getCurrentPhase() == PHASE_DAYLIGHT || Atmosphere.getCurrentPhase() == PHASE_DAWN) {
        // Soft glowing sun core
        Display.fillRect(sunX - 2, sunY - 2, 5, 5, rgb565(255, 245, 180));
        Display.drawPixel(sunX - 3, sunY, rgb565(255, 200, 100));
        Display.drawPixel(sunX + 3, sunY, rgb565(255, 200, 100));
        Display.drawPixel(sunX, sunY - 3, rgb565(255, 200, 100));
        Display.drawPixel(sunX, sunY + 3, rgb565(255, 200, 100));
    }

    // Draw animated harmonic guide waves to visually verify 60 FPS fluidity
    int16_t centerY = HUD_HEIGHT + (CANOPY_HEIGHT / 2) + 20;
    for (int16_t x = 0; x < SCREEN_WIDTH - 1; x++) {
        float wave1 = sinf((x * 0.04f) + _animationPhase) * 16.0f;
        float wave2 = sinf((x * 0.08f) - (_animationPhase * 1.5f)) * 8.0f;
        int16_t y1 = centerY + (int16_t)(wave1 + wave2);
        
        Display.drawPixel(x, y1, rgb565(180, 235, 255));
        Display.drawPixel(x, y1 + 1, rgb565(100, 180, 230));
    }

    // 3. Substrate Strata (280 to 320px) - Earthy Subterranean Layer
    int16_t substrateY = HUD_HEIGHT + CANOPY_HEIGHT;
    Display.fillRect(0, substrateY, SCREEN_WIDTH, SUBSTRATE_HEIGHT, rgb565(28, 20, 16));
    Display.drawFastHLine(0, substrateY, SCREEN_WIDTH, rgb565(85, 140, 70)); // Soil boundary line

    // Screen edge alignment borders (verify 172x320 clipping)
    Display.drawFastVLine(0, 0, SCREEN_HEIGHT, rgb565(40, 50, 65));
    Display.drawFastVLine(SCREEN_WIDTH - 1, 0, SCREEN_HEIGHT, rgb565(40, 50, 65));
}

void MainRenderer::drawDiagnosticsOverlay() {
    // Mini visual heartbeat indicator in top-right HUD corner (blinks smoothly)
    int16_t heartX = SCREEN_WIDTH - 12;
    int16_t heartY = 6;
    uint8_t pulseVal = (uint8_t)(128 + 127 * sinf(_animationPhase * 2.0f));
    uint16_t pulseColor = rgb565(pulseVal, 220, 120);

    Display.fillRect(heartX, heartY, 6, 6, pulseColor);
}
