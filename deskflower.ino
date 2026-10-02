#include <Arduino.h>
#include "config.h"
#include "hal_display.h"
#include "hal_peripherals.h"
#include "hud_manager.h"
#include "time_atmosphere.h"
#include "particle_system.h"
#include "plant_engine.h"
#include "moisture_system.h"
#include "net_sync.h"
#include "state_storage.h"
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

    // 2. Initialize Display Pipeline (Arduino_GFX ST7789 + 110KB Canvas Framebuffer)
    bool displayOk = Display.begin();
    #if ENABLE_SERIAL_LOG
    if (displayOk) {
        Serial.println("[HAL] ST7789 172x320 Display initialized successfully.");
    } else {
        Serial.println("[HAL] ERROR: Display initialization failed!");
    }
    Serial.printf("[SYSTEM] Free heap after display init: %u bytes\n", ESP.getFreeHeap());
    #endif

    // 3. Initialize HUD, Atmosphere, Particles, Plant & Moisture System
    HUD.begin();
    Atmosphere.begin();
    Particles.begin();
    Plant.begin();
    Moisture.begin();

    // 4. Initialize LittleFS Persistent State Storage (Restores saved specimen, generation, and moisture)
    Storage.begin();

    HUD.setTime(demoHours[demoTimeStep], 42, 0, false);
    HUD.setWeather(demoWeathers[demoTimeStep], 72, 64);
    Atmosphere.update(HUD.getHour(), HUD.getMinute(), HUD.getSecond());

    // 5. Initialize Network Manager (Wi-Fi 6, FreeRTOS background sync, Captive Portal)
    NetSync.begin();
    #if ENABLE_SERIAL_LOG
    Serial.printf("[NET] NetSync initialized. Status: %s\n", NetSync.getStateString());
    #endif

    // 6. Initialize Main Renderer
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

    // 2. Handle Boot Button Long-Press (Hold > 2s to toggle Captive Portal Onboarding)
    if (Peripherals.wasButtonHeld()) {
        if (!NetSync.isPortalActive()) {
            NetSync.startCaptivePortal();
            Peripherals.pulseLed({255, 0, 255}, 1500);
            #if ENABLE_SERIAL_LOG
            Serial.println("[NET] Button Held: Launched Captive Setup Portal (Deskflower-Setup @ 192.168.4.1)");
            #endif
        } else {
            NetSync.stopCaptivePortal();
            Peripherals.pulseLed({255, 120, 0}, 800);
            #if ENABLE_SERIAL_LOG
            Serial.println("[NET] Button Held: Stopped Captive Setup Portal.");
            #endif
        }
    }

    // 3. Handle Boot Button Click Interaction (Watering & Diurnal Demo cycle when offline)
    if (Peripherals.wasButtonClicked()) {
        if (!NetSync.isTimeSynced()) {
            demoTimeStep = (demoTimeStep + 1) % 5;
            if (demoTimeStep == 0) {
                // New generation cycle: mutate botanical genome & persist
                uint8_t genCounter = HUD.getGeneration() + 1;
                HUD.setGeneration(genCounter);
                Plant.generateFromSeed(millis() ^ 0x5A5A, genCounter % 4);
                Storage.markDirty();

                #if ENABLE_SERIAL_LOG
                char code[20];
                Plant.getSeedCode(code);
                Serial.printf("[GENOME] Generation %u: %s | Seed Code: %s\n", 
                              genCounter, Plant.getPhenotypeName(), code);
                #endif
            }

            HUD.setTime(demoHours[demoTimeStep], 30, 0, false);
            HUD.setWeather(demoWeathers[demoTimeStep], 68 + (demoTimeStep * 2), 48 + (demoTimeStep * 9));
            Atmosphere.update(HUD.getHour(), HUD.getMinute(), HUD.getSecond());
        }

        // Hydrate plant, record lifetime watering & trigger watering particle cascade
        Moisture.water(22.0f);
        Storage.incrementWaterings();
        Particles.triggerWateringCascade(24);

        #if ENABLE_SERIAL_LOG
        Serial.printf("[EVENT] Boot Click -> Phase: %s (%02d:%02d) | Moisture: %.0f%% | Lifetime Waters: %u | Particles: %u\n", 
                      Atmosphere.getPhaseName(), HUD.getHour(), HUD.getMinute(), Moisture.getMoisture(), 
                      Storage.getTotalWaterings(), Particles.getActiveCount());
        #endif

        Peripherals.pulseLed({0, 240, 255}, 600);
    }

    // 4. Update HUD, Atmosphere, Particles, Plant, Moisture, Storage & Network (~60 Hz)
    uint32_t deltaHud = currentMillis - lastHudUpdateTime;
    if (deltaHud >= 16) {
        NetSync.update(deltaHud);
        Storage.update(deltaHud);
        HUD.update(deltaHud);
        Atmosphere.update(HUD.getHour(), HUD.getMinute(), HUD.getSecond());
        Particles.update(deltaHud, Atmosphere.getCurrentPhase(), HUD.getWeatherType());
        Plant.updateLifecycle(deltaHud, Atmosphere.getCurrentPhase());
        Plant.updatePhysics(deltaHud);
        Moisture.update(deltaHud, HUD.getHour(), HUD.getMinute());
        lastHudUpdateTime = currentMillis;
    }

    // 5. 60 FPS Render Loop
    if (currentMillis - lastLoopTime >= FRAME_TIME_MS) {
        lastLoopTime = currentMillis;
        Renderer.renderFrame();
    }

    // 6. Periodic Diagnostics Telemetry (Non-blocking USB CDC)
    #if ENABLE_SERIAL_LOG
    if (Serial && (currentMillis - lastLogTime >= 4000)) {
        lastLogTime = currentMillis;
        if (Serial.availableForWrite() >= 64) {
            const char* mStateStr = Moisture.isAestivating() ? "DORMANT" : (Moisture.getMoisture() < 40.0f ? "THIRSTY" : "LUSH");
            Serial.printf("[DIAG] FPS: %.1f | Render: %u us | Blit: %u us | Net: %s | Moisture: %.0f%% (%s) | Waters: %u | Life: %u m | Free: %u B\n",
                          Renderer.getMeasuredFPS(),
                          Renderer.getFrameRenderTimeUs(),
                          Display.getLastBlitTimeUs(),
                          NetSync.getStateString(),
                          Moisture.getMoisture(),
                          mStateStr,
                          Storage.getTotalWaterings(),
                          Storage.getLifetimeMinutes(),
                          esp_get_free_heap_size());
        }
    }
    #endif

    delay(1);
}
