#ifndef SECRETS_EXAMPLE_H
#define SECRETS_EXAMPLE_H

// =============================================================================
// Deskflower User Configuration & Secrets Template
// Copy this file to "secrets.h" and fill in your network and location details.
// Note: "secrets.h" is git-ignored to prevent sharing private credentials.
// =============================================================================

// Wi-Fi 6 / 802.11 b/g/n/ax Credentials
#define SECRET_WIFI_SSID        "YOUR_WIFI_SSID"
#define SECRET_WIFI_PASSWORD    "YOUR_WIFI_PASSWORD"

// Geographic Location (Used for Open-Meteo / OpenWeatherMap real-time sync)
// Example: New York City (40.7128, -74.0060), London (51.5074, -0.1278), Tokyo (35.6762, 139.6503)
#define SECRET_LATITUDE         "40.7128"
#define SECRET_LONGITUDE        "-74.0060"

// Timezone Configuration (POSIX format or UTC offset in seconds)
// Standard US Eastern Time: "EST5EDT,M3.2.0,M11.1.0"
// Standard US Pacific Time: "PST8PDT,M3.2.0,M11.1.0"
// Standard UTC/GMT:         "GMT0"
// Standard Central European:"CET-1CEST,M3.5.0,M10.5.0/3"
#define SECRET_TIMEZONE_TZ      "EST5EDT,M3.2.0,M11.1.0"

// Optional OpenWeatherMap API Key (Leave empty if using free Open-Meteo API)
#define SECRET_OWM_API_KEY      ""

#endif // SECRETS_EXAMPLE_H
