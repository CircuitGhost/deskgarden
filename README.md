# Deskflower 🌸

An ambient, living generative botanical terrarium and smart desk clock for the **ESP32-C6** with a **172×320 IPS display**.

---

## Slice 1: Hardware Bringup & Display Pipeline

This milestone establishes the low-level hardware abstraction layer, double-buffered graphics rendering engine, and input/LED peripherals.

### Hardware Specifications
* **SoC:** ESP32-C6 (Single-core RISC-V @ 160MHz)
* **Display:** 1.47" / 1.9" ST7789 IPS LCD (172×320 resolution)
* **SPI Clock:** 40 MHz with DMA-backed transfer
* **Color Format:** 16-bit RGB565
* **Target Frame Rate:** 60 FPS
* **Peripherals:** 1× Boot Button (GPIO 9), 1× WS2812 Addressable RGB LED (GPIO 8)

---

### Project File Structure

```
deskflower/
├── deskflower.ino          # Main sketch entry point and 60 FPS event loop
├── config.h                # Pin mappings, resolution, timing constants, debug flags
├── hal_display.h           # Display HAL declarations and RGB565 primitives
├── hal_display.cpp         # ST7789 SPI driver, double-buffering, and DMA blitting
├── hal_peripherals.h       # Peripherals HAL declarations (Button & WS2812)
├── hal_peripherals.cpp     # Debounce logic and breathing pulse LED controller
├── renderer.h              # Renderer pipeline and FPS benchmarking
├── renderer.cpp            # Test patterns, region division, harmonic wave testing
├── MRD.md                  # Market Requirements Document
├── IMPLEMENTATION_PLAN.md  # 10-Slice Implementation Plan
└── README.md
```

---

### Default Pinout (ESP32-C6 ST7789 172×320)

| Signal | ESP32-C6 GPIO | Description |
| :--- | :---: | :--- |
| **LCD_MOSI / SDA** | `GPIO 6` | SPI Data Out |
| **LCD_SCLK / SCL** | `GPIO 7` | SPI Clock |
| **LCD_CS** | `GPIO 14` | Chip Select (Active LOW) |
| **LCD_DC** | `GPIO 15` | Data / Command Select |
| **LCD_RST** | `GPIO 21` | Reset |
| **LCD_BL** | `GPIO 22` | Backlight PWM (LEDC) |
| **BOOT_BUTTON** | `GPIO 9` | User Input (Active LOW with internal pull-up) |
| **RGB_LED** | `GPIO 8` | Onboard WS2812 Data |

*Pin assignments can be adjusted in [`config.h`](file:///Users/brianfette/Desktop/deskflower/config.h).*

---

### Verifying Slice 1

1. **Display & Viewport Alignment:**
   * Top 24px HUD status region (Deep slate background with separator line).
   * Middle 256px Canopy Chamber with vertical sky gradient and real-time animated harmonic sine waves.
   * Bottom 40px Substrate layer with earth tones and green soil boundary line.
   * 172×320 outer border verifying exact clipping without edge overflow.
2. **Interaction & Diagnostics:**
   * Top-right green heartbeat indicator pulses with sine-eased timing.
   * Clicking the onboard **Boot Button** triggers an immediate soft cyan pulse on the WS2812 RGB LED.
   * Serial Monitor at `115200 baud` outputs real-time FPS benchmarking and heap metrics.
