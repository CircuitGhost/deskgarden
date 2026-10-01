#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ==========================================
// Display Configuration (ST7789 172x320 IPS)
// ==========================================
#define SCREEN_WIDTH         172
#define SCREEN_HEIGHT        320
#define SCREEN_ROTATION      0     // 0 = Portrait (172x320), 2 = Inverted Portrait

// Standard Hardware Pin Mapping (Defaults match ESP32-C6 1.47" ST7789 boards)
#ifndef PIN_LCD_MOSI
#define PIN_LCD_MOSI         6
#endif

#ifndef PIN_LCD_SCLK
#define PIN_LCD_SCLK         7
#endif

#ifndef PIN_LCD_CS
#define PIN_LCD_CS           14
#endif

#ifndef PIN_LCD_DC
#define PIN_LCD_DC           15
#endif

#ifndef PIN_LCD_RST
#define PIN_LCD_RST          21
#endif

#ifndef PIN_LCD_BL
#define PIN_LCD_BL           22    // Backlight PWM pin
#endif

#ifndef PIN_SD_CS
#define PIN_SD_CS            4     // MicroSD Card CS (Shared SPI; MUST be driven HIGH)
#endif

// SPI Clock Frequency (ST7789 supports up to 40MHz–80MHz)
#define LCD_SPI_FREQ         40000000

// ==========================================
// Peripherals Configuration
// ==========================================
#define PIN_BOOT_BUTTON      9     // Onboard Boot button (Active LOW)
#define PIN_RGB_LED          8     // Onboard WS2812 addressable RGB LED
#define NUM_RGB_LEDS         1

// Backlight PWM parameters
#define LCD_BL_PWM_CHANNEL   0
#define LCD_BL_PWM_FREQ      5000
#define LCD_BL_PWM_RES       8     // 8-bit resolution (0-255)
#define DEFAULT_BRIGHTNESS   220   // 0 - 255

// ==========================================
// Performance & Rendering Pipeline
// ==========================================
#define TARGET_FPS           60
#define FRAME_TIME_MS        (1000 / TARGET_FPS)

// Layout Dimensions
#define HUD_HEIGHT           24
#define CANOPY_HEIGHT        256
#define SUBSTRATE_HEIGHT     40

// Debug & Diagnostics
#define ENABLE_FPS_COUNTER   1
#define ENABLE_SERIAL_LOG    1

#endif // CONFIG_H
