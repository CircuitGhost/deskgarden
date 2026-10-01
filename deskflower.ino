#include <Arduino.h>
#include "config.h"
#include "hal_display.h"
#include "hal_peripherals.h"
#include "hud_manager.h"
#include "renderer.h"

uint32_t lastLoopTime = 0;
uint32_t lastLogTime = 0;
uint32_t lastHudUpdateTime = 0;
uint8_t demoWeatherCycle = 0;

void setup() {
    #if ENABLE_SERIAL_LOG
    Serial.begin(115200);
    delay(200);
    Serial.println("\n==========================================");
    Serial.println("🌸 Deskflower - Slice 2: HUD & Clock");
    Serial.println("Target: ESP32-C6 (172x320 ST7789 IPS)");
    Serial.println("==========================================");
    #endif

    // 1. Initialize Peripherals (Boot Button & WS2812 RGB LED)
    Peripherals.begin();

    // 2. Initialize Display Pipeline
    bool displayOk = Display.begin();
    if (!displayOk) {
        #if ENABLE_SERIAL_LOG
        Serial.println("[HAL] ERROR: Display framebuffer allocation failed!");
        #endif
    } else {
        #if ENABLE_SERIAL_LOG
        Serial.println("[HAL] ST7789 172x320 Display initialized with Double Buffering");
        #endif
    }

    // 3. Initialize HUD & Clock
    HUD.begin();
    HUD.setTime(10, 42, 0, false); // Initial 10:42 AM
    HUD.setWeather(WEATHER_SUN, 72, 64);
    HUD.setGeneration(1);

    // 4. Initialize Main Renderer
    Renderer.begin();

    // Initial greeting pulse on RGB LED
    Peripherals.pulseLed({80, 220, 255}, 1000); // Soft cyan pulse
    lastLoopTime = millis();
    lastHudUpdateTime = millis();
}

void loop() {
    uint32_t currentMillis = millis();

    // 1. Poll Hardware Inputs
    Peripherals.update();

    // 2. Handle Boot Button Click Interaction (Cycle weather glyph demo + pulse)
    if (Peripherals.wasButtonClicked()) {
        demoWeatherCycle = (demoWeatherCycle + 1) % 5;
        WeatherType newWeather = static_cast<WeatherType>(demoWeatherCycle);
        HUD.setWeather(newWeather, 68 + (demoWeatherCycle * 3), 50 + (demoWeatherCycle * 8));

        #if ENABLE_SERIAL_LOG
        Serial.printf("[EVENT] Boot Button Clicked -> Weather Cycle: %d\n", demoWeatherCycle);
        #endif
        // Visual cyan confirmation pulse on onboard RGB LED
        Peripherals.pulseLed({0, 240, 255}, 800);
    }

    // 3. Update HUD Time & Pulsing Colon Animation
    uint32_t deltaHud = currentMillis - lastHudUpdateTime;
    if (deltaHud >= 16) { // ~60 Hz tick for smooth colon pulsing
        HUD.update(deltaHud);
        lastHudUpdateTime = currentMillis;
    }

    // 4. 60 FPS Render Loop
    if (currentMillis - lastLoopTime >= FRAME_TIME_MS) {
        lastLoopTime = currentMillis;
        Renderer.renderFrame();
    }

    // 4. Periodic Diagnostics Logging (every 3 seconds)
    #if ENABLE_SERIAL_LOG
    if (currentMillis - lastLogTime >= 3000) {
        lastLogTime = currentMillis;
        Serial.printf("[DIAG] FPS: %.1f | Frame Render: %u us | Blit Time: %u us | Free Heap: %u bytes\n",
                      Renderer.getMeasuredFPS(),
                      Renderer.getFrameRenderTimeUs(),
                      Display.getLastBlitTimeUs(),
                      esp_get_free_heap_size());
    }
    #endif

    // Yield to avoid watchdog trigger
    yield();
}
