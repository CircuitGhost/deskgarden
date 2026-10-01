#ifndef RENDERER_H
#define RENDERER_H

#include <Arduino.h>
#include "hal_display.h"
#include "hal_peripherals.h"
#include "hud_manager.h"
#include "time_atmosphere.h"

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
    uint8_t _animPhase;

    // Fast LUT Cache for Diurnal Sky Gradient (256 lines x 2 bytes = 512 bytes)
    uint16_t _cachedSkyColors[CANOPY_HEIGHT];
    uint8_t _lastSkyTopR, _lastSkyTopG, _lastSkyTopB;
    uint8_t _lastSkyBottomR, _lastSkyBottomG, _lastSkyBottomB;

    void updateSkyGradientCache(const AtmospherePalette& pal);
};

extern MainRenderer Renderer;

#endif // RENDERER_H
