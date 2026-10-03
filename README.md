# Deskflower 🌸

A digital botanical terrarium and ambient desk clock running on the **Waveshare ESP32-C6-LCD-1.47** (172×320 IPS display).

Deskflower simulates living procedural flora using L-systems that sway, bloom, cross-pollinate, and react to your local weather and time of day. It is designed to sit on your desk with zero maintenance—plants regulate their own baseline moisture and never wither away.

---

## ⚡ Quick Install (Browser Flasher)

If you use **Google Chrome** or **Microsoft Edge**, you can flash Deskflower directly from your browser without installing Arduino IDE or any command-line tools:

👉 **[Open the Web Flasher](https://circuitghost.github.io/deskgarden/dist/)** *(or open `dist/index.html` locally)*

1. Plug your ESP32-C6 into your computer with a USB-C cable.
2. Click **Install Deskflower** and select the serial port from the popup.
3. Once the progress bar reaches 100%, the screen will immediately boot into Deskflower.

---

## 🎮 Controls

| Action | Gesture | What it does |
| :--- | :--- | :--- |
| **Water Soil** | **Single click** | Adds +22% moisture, triggers raindrop particle physics, pulses cyan LED |
| **Mutate Specimen** | **Double click** | Advances to next generation, exports 16-char Seed Code to serial, pulses amber LED |
| **Wi-Fi Setup Portal** | **Hold (> 2 sec)** | Starts captive AP (`Deskflower-Setup` at `192.168.4.1`), pulses magenta LED |

---

## ✨ Features

* **Procedural Plant Generator:** Fractal branching with natural upward phototropism, tapered stems, and four phenotypes: Highland Orchid, Dwarf Fern, Micro-Succulent, and Flowering Bonsai.
* **Diurnal Atmosphere:** Sky gradients and lighting transition smoothly across 5 phases: Dawn (peach/lavender), Daylight (azure), Golden Hour (amber), Dusk (violet), and Night (starfield + bioluminescent fireflies).
* **Live Weather & Clock Sync:** Syncs with Open-Meteo and SNTP over Wi-Fi 6 to mirror real-time conditions (Sun, Clouds, Rain, Mist, Moon) and local temperature/humidity on the top HUD.
* **Local Web Dashboard:** Visit `http://deskflower.local` on your phone or laptop while connected to your home Wi-Fi to check hydration telemetry, rename your terrarium, and copy/import Seed Codes.
* **ESP-NOW Peer Mesh:** If multiple Deskflower units are nearby, they discover each other automatically and share drifting pollen spores across screens to breed hybrid varieties.
* **Four-Season Shifts & Wildlife:** Calendar-aware seasonal palettes (autumn foliage, winter frost, spring cherry blossoms) and occasional visits from pixel ladybugs, pygmy moths, and loam snails.
* **Safe Aestivation & Persistence:** If left unwatered, plants gracefully enter a safe dormancy mode rather than displaying an ugly brown state. Full genome, age, and water stats persist across power loss via LittleFS.
* **Night Mode Backlight:** Automatically dims display PWM between 22:30 and 06:00 for comfortable dark-room aesthetics.

---

## 📐 Hardware & Pinout

**Target Board:** Waveshare ESP32-C6-LCD-1.47 (ST7789 172×320 IPS, WS2812 RGB LED, Boot button)

| Signal | GPIO | Notes |
| :--- | :---: | :--- |
| **LCD MOSI** | `GPIO 6` | SPI Data (80 MHz) |
| **LCD SCLK** | `GPIO 7` | SPI Clock |
| **LCD CS** | `GPIO 14` | ST7789 Chip Select |
| **LCD DC** | `GPIO 15` | Data / Command |
| **LCD RST** | `GPIO 21` | Hardware Reset |
| **LCD Backlight** | `GPIO 22` | Backlight PWM (5 kHz LEDC) |
| **SD CS** | `GPIO 4` | Shared SPI isolation (held HIGH) |
| **Boot Button** | `GPIO 9` | Multi-gesture input (Active LOW) |
| **RGB LED** | `GPIO 8` | Onboard WS2812 LED |

---

## 🛠️ Building with Arduino CLI

If you prefer building from source:

### Prerequisites
* [arduino-cli](https://arduino.github.io/arduino-cli/)
* ESP32 Arduino Core 3.x (`esp32:esp32`)
* `GFX_Library_for_Arduino`

### Compile & Flash

```bash
# 1. Compile with CDC-on-Boot and Huge App partition table
arduino-cli compile --fqbn esp32:esp32:esp32c6:CDCOnBoot=cdc,PartitionScheme=huge_app .

# 2. Upload to your connected board
arduino-cli upload -p /dev/cu.usbmodem101 --fqbn esp32:esp32:esp32c6:CDCOnBoot=cdc,PartitionScheme=huge_app .

# 3. View serial telemetry (115200 baud)
arduino-cli monitor -p /dev/cu.usbmodem101 -c baudrate=115200
```

---

## 📁 Codebase Layout

```
deskflower/
├── deskflower.ino       # Main loop, timing orchestration, button dispatch
├── config.h             # Pin definitions, geometry, and rendering constants
├── hal_display.*        # ST7789 SPI driver, full canvas framebuffer, backlight PWM
├── hal_peripherals.*    # Single/double/hold button debouncing, WS2812 LED breathing
├── hud_manager.*        # Top 24px HUD (clock, micro-weather glyphs, generation index)
├── micro_font.*         # 5x7 micro-typography font engine with pulsing colon
├── weather_glyphs.*     # 8x8 micro-weather icon bitmaps
├── time_atmosphere.*    # 5-phase diurnal sky palettes and celestial twinkle
├── particle_system.*    # Particle physics pool (oxygen bubbles, rain, mist, fauna)
├── plant_engine.*       # L-System generator, phenotypes, seed codes, wind sway
├── moisture_system.*    # Asymmetric decay, autonomic dew recovery, soil gauge
├── net_sync.*           # Wi-Fi 6, SoftAP captive portal, SNTP time, Open-Meteo
├── mesh_sync.*          # ESP-NOW peer discovery and cross-screen pollen sharing
├── web_dashboard.*      # Embedded HTTP server for http://deskflower.local
├── state_storage.*      # LittleFS flash state persistence with CRC32 wear leveling
├── renderer.*           # Framebuffer composition pipeline (Substrate + Plant + Sky + HUD)
└── dist/                # Pre-compiled binaries and 1-click ESP Web Tools flasher
```

---

## 📄 License

MIT License. Open-source release ready.
