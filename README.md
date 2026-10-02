# Deskflower 🌸

**Ambient Living Generative Botanical Companion & Smart Desk Clock for ESP32-C6**

[![Platform: ESP32-C6](https://img.shields.io/badge/Platform-ESP32--C6%20(RISC--V)-blue.svg)](https://www.espressif.com/)
[![Display: ST7789 172x320 IPS](https://img.shields.io/badge/Display-172%C3%97320%20IPS%20ST7789-green.svg)](https://www.waveshare.com/)
[![Framework: Arduino-ESP32](https://img.shields.io/badge/Framework-Arduino--ESP32%20Core%203.x-orange.svg)](https://github.com/espressif/arduino-esp32)
[![Wi-Fi: 802.11ax Wi-Fi 6](https://img.shields.io/badge/Wi--Fi-802.11ax%20Wi--Fi%206-purple.svg)]()

Deskflower is an ambient, living digital terrarium and smart desk companion engineered for modern workstations. Running on the ESP32-C6 microcontroller with an integrated 1.47" $172 \times 320$ color IPS display, Deskflower merges procedural generative botanical simulation (L-systems) with real-world atmospheric synchronicity, anti-annoyance asymmetric moisture physics, and real-time clock timekeeping.

---

## ✨ Key Features

1. **Procedural Botanical Generation & L-Systems:**
   - Deterministic fractal branching structures with organic thickness tapering, branch curvature, and natural phototropism.
   - Distinct phenotypes: **Highland Orchid**, **Dwarf Fern**, **Micro-Succulent**, and **Flowering Bonsai**.
   - Coherent multi-frequency harmonic wind-sway physics with integer LUT acceleration (60 FPS rendering).
2. **Blooming Lifecycle & Genetics:**
   - 6-stage flower unfolding lifecycle (Bud $\rightarrow$ Swollen Calyx $\rightarrow$ Opening $\rightarrow$ Full Bloom $\rightarrow$ Nectar Stage $\rightarrow$ Seed Pod).
   - Diurnal petal posture (blooms open during daylight and golden hour, resting at night).
   - **16-Character Seed Code Engine (Crockford Base32):** Deterministically encode and share botanical specimens across devices.
3. **5-Phase Diurnal Atmospheric Engine:**
   - **Dawn (05:30 – 08:00):** Pastel peach to lavender vertical sky gradient.
   - **Daylight (08:00 – 17:30):** Crisp radiant sky blue with rising photosynthetic oxygen bubbles.
   - **Golden Hour (17:30 – 19:30):** Warm honey amber wash with elongated shadows.
   - **Dusk (19:30 – 21:30):** Deep violet to twilight indigo.
   - **Night (21:30 – 05:30):** Obsidian navy backdrop with twinkling starfield and drifting bioluminescent fireflies.
4. **Substrate Strata & Asymmetric Moisture Engine:**
   - Multi-layered soil strata texture with subterranean branching roots.
   - Anti-annoyance moisture model: 40%–60% dynamic equilibrium baseline with 04:00 AM autonomic dew recovery.
   - **Safe Aestivation (Dormancy):** Plant never perishes into an ugly brown state; entering aesthetic dormancy under extended drought and reviving instantly with one watering tap.
5. **Wi-Fi 6, SNTP & Open-Meteo Weather Sync:**
   - **Zero-Friction Captive Portal:** SoftAP `Deskflower-Setup` at `192.168.4.1` with mobile-responsive onboarding.
   - **SNTP Wall Clock:** Precise local real-time clock display with pulsing colon.
   - **Live Weather Sync:** HTTPS weather polling updating HUD micro-glyphs (☀️ Clear, ⛅ Partly Cloudy, ☁️ Overcast, 🌧️ Rain, 🌙 Clear Night).
6. **LittleFS State Persistence & Wear Leveling:**
   - Binary state container with CRC32 checksum and atomic temporary file swapping (`/state.tmp` $\rightarrow$ `/state.bin`).
   - Restores generation index, seed code, growth maturity, and hydration instantly upon boot.
7. **Night Mode Backlight Throttling:**
   - Dynamic PWM brightness fading to a gentle ambient glow between 22:30 and 06:00 for comfortable dark-room aesthetics.

---

## 🎮 Hardware Controls & Interaction

| Action | Control | Result / Visual Feedback |
| :--- | :--- | :--- |
| **Water Plant** | **Single Click** Boot Button | Hydrates soil ($+22\%$), triggers raindrop particle cascade, pulses **Cyan LED** |
| **Mutate / Seed Code** | **Double Click** Boot Button | Mutates genome to next generation, prints Crockford Seed Code to Serial, pulses **Amber LED** |
| **Wi-Fi Setup Portal** | **Hold Button (> 2s)** | Starts/stops `Deskflower-Setup` SoftAP captive portal at `192.168.4.1`, pulses **Magenta LED** |

---

## 📐 Pinout & Hardware Architecture (Waveshare ESP32-C6-LCD-1.47)

| Signal | GPIO | Function / Notes |
| :--- | :---: | :--- |
| **LCD_MOSI / SDA** | `GPIO 6` | SPI MOSI Data Out (80 MHz) |
| **LCD_SCLK / SCL** | `GPIO 7` | SPI Hardware Clock |
| **LCD_CS** | `GPIO 14` | ST7789 Chip Select (Active LOW) |
| **LCD_DC** | `GPIO 15` | Data / Command Select |
| **LCD_RST** | `GPIO 21` | Hardware Reset |
| **LCD_BL** | `GPIO 22` | Backlight PWM (5 kHz LEDC) |
| **SD_CS** | `GPIO 4` | **Shared SPI Bus Isolation:** Held `OUTPUT HIGH` on boot |
| **BOOT_BUTTON** | `GPIO 9` | Multi-function input button (Active LOW) |
| **RGB_LED** | `GPIO 8` | Onboard WS2812 Addressable RGB LED |

---

## 🛠️ Building & Flashing

### Requirements
- [arduino-cli](https://arduino.github.io/arduino-cli/) or Arduino IDE 2.x
- `esp32:esp32` core version $\ge 3.0.0$
- `GFX_Library_for_Arduino` library

### Build & Upload Commands (Arduino CLI)

```bash
# 1. Compile firmware with CDC-on-Boot and Huge App partition scheme
arduino-cli compile --fqbn esp32:esp32:esp32c6:CDCOnBoot=cdc,PartitionScheme=huge_app .

# 2. Upload to connected ESP32-C6 board
arduino-cli upload -p /dev/cu.usbmodem101 --fqbn esp32:esp32:esp32c6:CDCOnBoot=cdc,PartitionScheme=huge_app .

# 3. Monitor live serial diagnostics (115200 baud)
arduino-cli monitor -p /dev/cu.usbmodem101 -c baudrate=115200
```

---

## 📦 Project Architecture

```
deskflower/
├── deskflower.ino          # Main setup, loop orchestration, and button event dispatch
├── config.h                # Hardware pinouts, layout geometry, timing, and FPS targets
├── hal_display.h / .cpp    # ST7789 SPI driver (Arduino_GFX), full canvas FB, backlight PWM
├── hal_peripherals.h / .cpp# Single/double/hold button debouncing and WS2812 LED breathing engine
├── hud_manager.h / .cpp    # Top 24px HUD status bar (clock, micro-glyphs, generation index)
├── micro_font.h / .cpp     # 5x7 micro-typography font engine with colon pulse
├── weather_glyphs.h / .cpp # Micro-weather glyph renderer (Sun, Clouds, Rain, Moon)
├── time_atmosphere.h / .cpp# 5-phase diurnal sky palettes, sine transitions, and starfield
├── particle_system.h / .cpp# 48-slot particle pool (oxygen bubbles, fireflies, rain cascade)
├── plant_engine.h / .cpp   # L-System fractal generator, phenotypes, seed codes, sway physics
├── moisture_system.h / .cpp# Asymmetric moisture decay, autonomic dew, dormancy & gauge
├── net_sync.h / .cpp       # Wi-Fi 6, SoftAP Captive Portal, SNTP time, and Open-Meteo sync
├── state_storage.h / .cpp  # LittleFS flash persistence, CRC32 validation, and wear leveling
├── renderer.h / .cpp       # Main composite rendering pipeline (Substrate, Plant, Atmosphere, HUD)
├── MRD.md                  # Market Requirements Document
└── IMPLEMENTATION_PLAN.md  # 10-Slice Implementation Plan & Architecture Specification
```

---

## 📄 License
MIT License. Open-source release ready.
