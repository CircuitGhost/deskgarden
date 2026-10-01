#include <Arduino.h>
#include "config.h"
#include "hal_display.h"
#include "hal_peripherals.h"
#include "hud_manager.h"
#include "time_atmosphere.h"
#include "particle_system.h"
#include "renderer.h"

// Explicit prototypes to prevent Arduino preprocessor insertion bugs
void setup();
void loop();

uint8_t demoTimeStep = 0;
static const uint8_t demoHours[5] = {6, 12, 18, 20, 23};
static const WeatherType demoWeathers[5] = {WEATHER_SUN, WEATHER_PARTLY_CLOUDY, WEATHER_SUN, WEATHER_RAIN, WEATHER_MOON};

uint32_t lastLoopTime = 0;
uint32_t lastHudUpdateTime = 0;
uint32_t lastLogTime = 0;

void setup() {
    #if ENABLE_SERIAL_LOG
    Serial.begin(115200);
    delay(500);
    Serial.println("\n==========================================");
    Serial.println("🌸 Deskflower - ESP32-C6 Living Companion");
    Serial.println("==========================================");
    Serial.printf("[SYSTEM] Free heap on boot: %u bytes\n", ESP.getFreeHeap());
    #endif

    // 1. Initialize Peripherals (Boot Button & WS2812 RGB LED)
    Peripherals.begin();
    #if ENABLE_SERIAL_LOG
    Serial.println("[HAL] Peripherals initialized.");
    #endif

    // 2. Initialize Display Pipeline (Arduino_GFX ST7789 + 11KB Band Buffer)
    bool displayOk = Display.begin();
    #if ENABLE_SERIAL_LOG
    if (displayOk) {
        Serial.println("[HAL] ST7789 172x320 Display initialized successfully.");
    } else {
        Serial.println("[HAL] ERROR: Display initialization failed!");
    }
    Serial.printf("[SYSTEM] Free heap after display init: %u bytes\n", ESP.getFreeHeap());
    #endif

    // 3. Initialize HUD, Time, Atmosphere & Particles
    HUD.begin();
    Atmosphere.begin();
    Particles.begin();

    HUD.setTime(demoHours[demoTimeStep], 42, 0, false);
    HUD.setWeather(demoWeathers[demoTimeStep], 72, 64);
    HUD.setGeneration(1);
    Atmosphere.update(HUD.getHour(), HUD.getMinute(), HUD.getSecond());

    // 4. Initialize Main Renderer
    Renderer.begin();

    lastLoopTime = millis();
    lastHudUpdateTime = millis();
    lastLogTime = millis();
    #if ENABLE_SERIAL_LOG
    Serial.println("[SYSTEM] Main engine running.");
    #endif
}

void loop() {
    uint32_t currentMillis = millis();

    // 1. Poll Hardware Inputs
    Peripherals.update();

    // 2. Handle Boot Button Click Interaction (Cycles Diurnal Phases + triggers Raindrop Cascade)
    if (Peripherals.wasButtonClicked()) {
        demoTimeStep = (demoTimeStep + 1) % 5;
        HUD.setTime(demoHours[demoTimeStep], 30, 0, false);
        HUD.setWeather(demoWeathers[demoTimeStep], 68 + (demoTimeStep * 2), 48 + (demoTimeStep * 9));
        Atmosphere.update(HUD.getHour(), HUD.getMinute(), HUD.getSecond());

        // Trigger watering particle cascade
        Particles.triggerWateringCascade(24);

        #if ENABLE_SERIAL_LOG
        Serial.printf("[EVENT] Boot Click -> Phase: %s (%02d:30) | Weather: %d | Active Particles: %u\n", 
                      Atmosphere.getPhaseName(), HUD.getHour(), demoWeathers[demoTimeStep], Particles.getActiveCount());
        #endif

        Peripherals.pulseLed({0, 240, 255}, 600);
    }

    // 3. Update HUD Time & Pulsing Colon Animation (~60 Hz)
    uint32_t deltaHud = currentMillis - lastHudUpdateTime;
    if (deltaHud >= 16) {
        HUD.update(deltaHud);
        Atmosphere.update(HUD.getHour(), HUD.getMinute(), HUD.getSecond());
        Particles.update(deltaHud, Atmosphere.getCurrentPhase(), HUD.getWeatherType());
        lastHudUpdateTime = currentMillis;
    }

    // 4. 60 FPS Render Loop
    if (currentMillis - lastLoopTime >= FRAME_TIME_MS) {
        lastLoopTime = currentMillis;
        Renderer.renderFrame();
    }

    // 5. Periodic Diagnostics Telemetry (Non-blocking USB CDC)
    #if ENABLE_SERIAL_LOG
    if (Serial && (currentMillis - lastLogTime >= 4000)) {
        lastLogTime = currentMillis;
        if (Serial.availableForWrite() >= 64) {
            Serial.printf("[DIAG] FPS: %.1f | Render: %u us | Blit: %u us | Particles: %u | Free Heap: %u bytes\n",
                          Renderer.getMeasuredFPS(),
                          Renderer.getFrameRenderTimeUs(),
                          Display.getLastBlitTimeUs(),
                          Particles.getActiveCount(),
                          esp_get_free_heap_size());
        }
    }
    #endif

    delay(1);
}
