#include <Arduino.h>
#include "config.h"
#include "hal_display.h"
#include "hal_peripherals.h"
#include "renderer.h"

uint32_t lastLoopTime = 0;
uint32_t lastLogTime = 0;

void setup() {
    #if ENABLE_SERIAL_LOG
    Serial.begin(115200);
    delay(200);
    Serial.println("\n==========================================");
    Serial.println("🌸 Deskflower - Slice 1: Hardware Bringup");
    Serial.println("Target: ESP32-C6 (172x320 ST7789 IPS)");
    Serial.println("==========================================");
    #endif

    // 1. Initialize Peripherals (Boot Button & WS2812 RGB LED)
    Peripherals.begin();
    #if ENABLE_SERIAL_LOG
    Serial.println("[HAL] Peripherals initialized (Button: GPIO9, RGB: GPIO8)");
    #endif

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

    // 3. Initialize Main Renderer
    Renderer.begin();

    // Initial greeting pulse on RGB LED
    Peripherals.pulseLed({80, 220, 255}, 1000); // Soft cyan pulse
    lastLoopTime = millis();
}

void loop() {
    uint32_t currentMillis = millis();

    // 1. Poll Hardware Inputs
    Peripherals.update();

    // 2. Handle Boot Button Click Interaction
    if (Peripherals.wasButtonClicked()) {
        #if ENABLE_SERIAL_LOG
        Serial.println("[EVENT] Boot Button Clicked -> Triggering Water Pulse");
        #endif
        // Visual cyan confirmation pulse on onboard RGB LED
        Peripherals.pulseLed({0, 240, 255}, 800);
    }

    // 3. 60 FPS Render Loop
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
