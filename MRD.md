# Market Requirements Document (MRD)
# Project: Deskflower

**Document Version:** 1.0  
**Status:** Approved / Active  
**Target Hardware:** ESP32-C6 (172×320 IPS Display, Onboard RGB LED, Wi-Fi 6)  
**Category:** Ambient Smart Desk Companion / Living Generative Art  

---

## 1. Executive Summary & Product Vision

**Deskflower** is an ambient, living digital terrarium and smart desk companion engineered for modern workstations. Running entirely on a standalone microcontroller (ESP32-C6) with an integrated color IPS display, Deskflower merges procedural generative botanical simulation with real-world atmospheric synchronicity and aesthetic timekeeping.

Unlike traditional virtual pets that demand continuous maintenance and punish neglect with death spirals, Deskflower is built around a **"Zero Annoyance"** philosophy. Real-world weather and diurnal cycles act as its autonomic life-support baseline, ensuring the flora thrives autonomously while rewarding occasional user interaction with tactile, gratifying micro-animations. With dynamic, time-aware atmospheric visual styling ("vibey aesthetics") and an integrated minimalist clock, Deskflower elevates any workspace into a serene, ever-evolving focal point.

---

## 2. Target Audience & Personas

* **The Desk Aesthetic Enthusiast:** Developers, designers, content creators, and remote workers who curate their workspace ambience and seek calming, high-taste digital desktop artifacts.
* **The Low-Stress Digital Pet Fan:** Casual gamers and collectors who cherish the nostalgia and charm of virtual pets/terrariums (e.g., Tamagotchi, Pocket Frogs) but reject demanding upkeep mechanics or nagging notifications.
* **Embedded & Microcontroller Hobbyists:** Makers and firmware engineers interested in procedural generation (L-systems), cellular automata, RISC-V optimization, and compact ESP32-C6 graphics programming.

---

## 3. Product Positioning & Guiding Principles

1. **Zero Annoyance (No Death Spirals):** Neglect leads to dormancy, never death. Plants do not wither into unsightly brown pixels, nor do they trigger intrusive alerts when left unwatered.
2. **Self-Regulating Baseline:** Real-world weather APIs and automated diurnal dew cycles maintain baseline hydration. Manual watering is an accelerator/booster, never a survival requirement.
3. **Vibey, Time-Adaptive Visual Atmosphere:** Visual palettes, backdrop gradients, lighting effects, and particle systems continuously morph throughout the day (dawn, daytime, golden hour, dusk, midnight) to mirror natural ambient room vibes.
4. **Subtle Timekeeping:** Ambient time display is seamlessly woven into the visual hierarchy, offering utility as an aesthetic desktop desk clock without overpowering the botanical view.
5. **Zero Additional Hardware:** Fully self-contained on the bare development board—leveraging the built-in screen, single Boot button, addressable RGB LED, and Wi-Fi 6 radio.

---

## 4. Feature Requirements

### 4.1. Core Moisture & "Anti-Annoyance" Watering Engine

To eliminate maintenance anxiety, Deskflower uses an asymmetric moisture decay model:

| Parameter | Specification | Purpose |
| :--- | :--- | :--- |
| **Baseline Hydration** | 40% – 60% dynamic equilibrium | Natural soil moisture never drops below 35% under standard weather sync, even when untouched for weeks. |
| **Manual Watering Action** | Single click of onboard Boot button | Adds +20% moisture; triggers an instant raindrop particle cascade down the glass display. |
| **Diminishing Returns** | 15-minute cooldown timer | Subsequent clicks trigger visual misting particles without over-saturating or drowning root systems. |
| **Dormancy vs. Death** | Low Moisture State (< 20%) | If Wi-Fi fails and manual watering ceases for extended periods, plants enter **Aestivation** (petals fold, leaves adopt antique pastel tones). A single water tap revives them instantly. |
| **Autonomic Dew Cycle** | Nightly refresh (04:00 local time) | Automatically restores baseline soil moisture to emulate morning dew and condensation. |

---

### 4.2. Procedural Botanical Architecture & Genetics

* **L-System Fractal Stems:** Procedurally generated growth nodes guarantee no two plants share identical branching structures.
* **Continuous Flowering Cycle:** Mature stems generate bud nodes that blossom into diverse botanical variations (mosses, dwarf ferns, highland orchids, micro-succulents).
* **Genetic Cross-Pollination:** Floating pollen motes drift across the display. Contact with mature flowers creates seed pods deposited into the substrate, driving mutations in leaf morphology, bloom color, and growth speed across generations.
* **State Persistence:** Plant structure, age, mutation lineage, and historical statistics persist across reboots via internal flash memory (`LittleFS` / `SPIFFS`).

---

### 4.3. Real-World Environmental Drivers & Dynamic Time Engine

Deskflower synchronizes with OpenWeatherMap / Open-Meteo APIs every 30 minutes over local Wi-Fi 6 and tracks real-time clock (SNTP) updates:

#### Weather Synchronicity
* **Rain / High Humidity (> 70%):** Generates soft glass-condensation mist particles; automatically satisfies soil hydration.
* **Overcast / Low Light:** Slows vegetative growth rate; shifts foliage tones to rich emerald and forest greens.
* **Clear Sun:** Boosts photosynthetic growth velocity; triggers animated micro oxygen bubbles rising from leaf surfaces.

#### Diurnal Atmosphere & "Vibey" Time-of-Day Shifting
The visual backdrop, color grading, and particle physics transition across distinct time phases:

* **Dawn (05:30 – 08:00):** Gentle pastel peach-to-lavender horizon gradient, rising morning mist, awakening leaf poses.
* **Daylight (08:00 – 17:30):** Crisp, high-contrast natural skylight with floating oxygen particles and diurnal blooms fully unfurled.
* **Golden Hour (17:30 – 19:30):** Warm honey and amber chromatic wash across leaves and flowers; elongated soft shadows.
* **Dusk / Twilight (19:30 – 21:30):** Indigo-to-violet sky gradient, diurnal petals tuck in; evening spores begin to illuminate.
* **Night / Midnight (21:30 – 05:30):** Deep obsidian/navy palette with drifting bioluminescent fireflies and star motes; nocturnal ferns gently unfurl.

---

### 4.4. Hardware Output, Time Display & Visual Layout (172×320 IPS)

```
+------------------------------------+
| 10:42 AM | ☀️ 72°F 64% | GEN 04   |  <- Top HUD (24px)
+------------------------------------+
|                                    |
|       [ Atmospheric Sky & ]        |
|       [ Floating Spores   ]        |
|                                    |
|          _--_     _--_             |
|         ( *  )---(  * )            |  <- Canopy & Flora Chamber (256px)
|          \  /     \  /             |     (L-System procedural stems,
|           \/       \/              |      blooms, dynamic sky gradient)
|            \       /               |
|             \     /                |
|              \   /                 |
|               | |                  |
+------------------------------------+
|  ===~~==~~~~=~=~=~=~=~=~ [💧 62%]  |  <- Substrate & Root Strata (40px)
|   \ \ /  / \  \ / / \  /           |
+------------------------------------+
```

1. **Top HUD Bar (Top 24px):**
   * **Ambient Clock:** Clean, minimalist digital time readout (12h/24h selectable via config) with smooth colon pulse.
   * **Micro Weather Metric & Symbol:** Dynamic 8×8 / 10×10 pixel micro-glyph reflecting real-time conditions (☀️ Clear Sun, ⛅ Partly Cloudy, ☁️ Overcast, 🌧️ Rain/Mist, 🌙 Clear Night) alongside ambient temperature and local humidity percentage.
   * **Lineage Indicator:** Current botanical generation index (e.g., `GEN 04`).
2. **Canopy & Flora Chamber (Middle 256px):**
   * High-contrast vertical botanical biome rendering swaying stems, opening blossoms, fluttering pollen, and ambient atmospheric particle physics matched to the time-of-day gradient.
3. **Substrate & Root Strata (Bottom 40px):**
   * Cross-section cutaway displaying dynamic root growth and an unobtrusive moisture meter seamlessly integrated into the soil boundary line.
4. **Onboard Addressable RGB LED (Ambient Cast):**
   * **Daytime:** Soft warm 5000K daylight fill at low lumen.
   * **Golden Hour:** Gentle amber/sunset illumination.
   * **Night:** Dim bioluminescent emerald/violet glow.
   * **Watering Event:** Subtle cyan breathing pulse confirming touch input without jarring flashes.

---

## 5. Technical Specifications

| Component | Target Allocation & Specification |
| :--- | :--- |
| **SoC / Architecture** | ESP32-C6 (Single-core 32-bit RISC-V @ 160MHz) |
| **Display & Controller** | 172×320 IPS LCD (ST7789 controller via high-speed SPI) |
| **Graphics Framework** | `LovyanGFX` or `TFT_eSPI` with double-buffered rendering |
| **Frame Rate** | 30–60 FPS smooth rendering for particle physics and swaying math |
| **SRAM Budget** | < 120 KB allocated for double frame buffers, genetics tree, and particle arrays |
| **Non-Volatile Storage** | `LittleFS` (< 64 KB for state logs, genetic mutations, and Wi-Fi preferences) |
| **Network & Time Sync** | Wi-Fi 6 (802.11ax) / WPA3; non-blocking async HTTPS polling + SNTP timekeeping |
| **Power Consumption** | Optimized for continuous 24/7 USB-C desk power with dynamic display brightness throttling |

---

## 6. Success Metrics & Viral Potential

* **Desk Appeal & Peripheral Delight:** Subtle, organic motion designed to catch the corner of the eye without breaking deep work focus.
* **Zero-Friction Onboarding:** Firmware setup requires only connecting to a captive portal Wi-Fi manager (`WiFiManager` / ESP-IDF Captive Portal); immediate self-configuration with no companion app required.
* **Community Shareability:** Exportable 16-character alphanumeric **"Seed Codes"** representing unique plant genetics and color traits, enabling users to share, trade, and clone rare specimens across developer communities and social platforms.
