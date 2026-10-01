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
    // 1. Top HUD Region (0 to 24px) - Deep Slate
    Display.fillRect(0, 0, SCREEN_WIDTH, HUD_HEIGHT, rgb565(20, 24, 34));
    Display.drawFastHLine(0, HUD_HEIGHT - 1, SCREEN_WIDTH, rgb565(60, 70, 90));

    // 2. Canopy Chamber (24 to 280px) - Atmospheric Gradient with Dynamic Sine Waves
    Display.drawGradientV(0, HUD_HEIGHT, SCREEN_WIDTH, CANOPY_HEIGHT, rgb565(15, 20, 45), rgb565(45, 30, 60));

    // Draw animated harmonic guide waves to visually verify 60 FPS fluidity
    int16_t centerY = HUD_HEIGHT + (CANOPY_HEIGHT / 2);
    for (int16_t x = 0; x < SCREEN_WIDTH - 1; x++) {
        float wave1 = sinf((x * 0.04f) + _animationPhase) * 20.0f;
        float wave2 = sinf((x * 0.08f) - (_animationPhase * 1.5f)) * 10.0f;
        int16_t y1 = centerY + (int16_t)(wave1 + wave2);
        
        Display.drawPixel(x, y1, rgb565(120, 200, 255));
        Display.drawPixel(x, y1 + 1, rgb565(80, 160, 240));
    }

    // 3. Substrate Strata (280 to 320px) - Earthy Subterranean Layer
    int16_t substrateY = HUD_HEIGHT + CANOPY_HEIGHT;
    Display.fillRect(0, substrateY, SCREEN_WIDTH, SUBSTRATE_HEIGHT, rgb565(30, 22, 18));
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
