#include <Arduino.h>
#include "config.h"
#include "hal_display.h"
#include "hal_peripherals.h"
#include "hud_manager.h"
#include "time_atmosphere.h"
#include "renderer.h"

uint32_t lastLoopTime = 0;
uint32_t lastLogTime = 0;
uint32_t lastHudUpdateTime = 0;
uint8_t demoTimeStep = 0;

// Pre-set demo hours to showcase all 5 diurnal phases on button clicks
static const uint8_t demoHours[5] = {6, 12, 18, 20, 23}; // Dawn, Day, Golden Hour, Dusk, Night
static const WeatherType demoWeathers[5] = {WEATHER_SUN, WEATHER_PARTLY_CLOUDY, WEATHER_SUN, WEATHER_RAIN, WEATHER_MOON};

void setup() {
    #if ENABLE_SERIAL_LOG
    Serial.begin(115200);
    delay(200);
    Serial.println("\n==========================================");
    Serial.println("🌸 Deskflower - Slice 3: Diurnal Atmosphere");
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

    // 3. Initialize HUD, Time & Atmosphere
    HUD.begin();
    Atmosphere.begin();

    HUD.setTime(demoHours[demoTimeStep], 42, 0, false);
    HUD.setWeather(demoWeathers[demoTimeStep], 72, 64);
    HUD.setGeneration(1);

    Atmosphere.update(HUD.getHour(), HUD.getMinute(), HUD.getSecond());

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

    // 2. Handle Boot Button Click Interaction (Cycles Diurnal Phases: Dawn -> Day -> Golden -> Dusk -> Night)
    if (Peripherals.wasButtonClicked()) {
        demoTimeStep = (demoTimeStep + 1) % 5;
        HUD.setTime(demoHours[demoTimeStep], 30, 0, false);
        HUD.setWeather(demoWeathers[demoTimeStep], 68 + (demoTimeStep * 2), 48 + (demoTimeStep * 9));
        Atmosphere.update(HUD.getHour(), HUD.getMinute(), HUD.getSecond());

        #if ENABLE_SERIAL_LOG
        Serial.printf("[EVENT] Boot Click -> Switched to Phase: %s (Time: %02d:30)\n", 
                      Atmosphere.getPhaseName(), HUD.getHour());
        #endif

        // Trigger interactive water / confirmation cyan pulse on LED
        Peripherals.pulseLed({0, 240, 255}, 600);
    }

    // 3. Update HUD Time & Pulsing Colon Animation
    uint32_t deltaHud = currentMillis - lastHudUpdateTime;
    if (deltaHud >= 16) { // ~60 Hz tick
        HUD.update(deltaHud);
        Atmosphere.update(HUD.getHour(), HUD.getMinute(), HUD.getSecond());
        lastHudUpdateTime = currentMillis;

        // Keep ambient LED in sync with active atmospheric color temperature
        if (!Peripherals.isButtonPressed()) {
            RGBColor amb = Atmosphere.getAmbientLedColor();
            // Peripherals.setLedColor(amb);
        }
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
