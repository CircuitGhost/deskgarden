#ifndef RENDERER_H
#define RENDERER_H

#include <Arduino.h>
#include "hal_display.h"
#include "hal_peripherals.h"
#include "hud_manager.h"

class MainRenderer {
public:
    MainRenderer();
    void begin();
    void renderFrame();

    // Diagnostics
    float getMeasuredFPS() const { return _currentFps; }
    uint32_t getFrameRenderTimeUs() const { return _lastFrameRenderTimeUs; }

private:
    uint32_t _lastFrameTime;
    uint32_t _frameCount;
    uint32_t _lastFpsCalcTime;
    float _currentFps;
    uint32_t _lastFrameRenderTimeUs;
    float _animationPhase;

    void drawSlice1TestPattern();
    void drawDiagnosticsOverlay();
};

extern MainRenderer Renderer;

#endif // RENDERER_H
