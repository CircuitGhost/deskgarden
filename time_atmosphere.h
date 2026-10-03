#ifndef TIME_ATMOSPHERE_H
#define TIME_ATMOSPHERE_H

#include <Arduino.h>
#include "config.h"
#include "hal_peripherals.h"

enum TimePhase {
    PHASE_DAWN = 0,        // 05:30 - 08:00 (Pastel peach to lavender)
    PHASE_DAYLIGHT,        // 08:00 - 17:30 (Crisp sky blue to pale azure)
    PHASE_GOLDEN_HOUR,     // 17:30 - 19:30 (Warm honey amber & sunset glow)
    PHASE_DUSK,            // 19:30 - 21:30 (Twilight indigo & deep violet)
    PHASE_NIGHT            // 21:30 - 05:30 (Obsidian navy & bioluminescent ambience)
};

// Midnight phosphorescence is narrower than PHASE_NIGHT: 23:00 inclusive through 05:00 exclusive.
inline bool isPhosphorescentHour(uint8_t hour, uint8_t minute) {
    uint16_t mins = (uint16_t)hour * 60u + minute;
    return mins >= (23u * 60u) || mins < (5u * 60u);
}

// Northern-hemisphere meteorological seasons. Month is 1–12 from the SNTP local calendar.
enum Season : uint8_t {
    SEASON_SPRING = 0,   // March–May
    SEASON_SUMMER,       // June–August
    SEASON_AUTUMN,       // September–November
    SEASON_WINTER        // December–February
};

// False when month is 0 (the wall clock has not been validated yet).
inline bool seasonFromMonth(uint8_t month, Season& season) {
    if (month >= 3 && month <= 5) {
        season = SEASON_SPRING;
        return true;
    }
    if (month >= 6 && month <= 8) {
        season = SEASON_SUMMER;
        return true;
    }
    if (month >= 9 && month <= 11) {
        season = SEASON_AUTUMN;
        return true;
    }
    if (month == 12 || month == 1 || month == 2) {
        season = SEASON_WINTER;
        return true;
    }
    return false;
}

// 0 on the first day of the season, 255 on the last. Unknown months stay at 0.
inline uint8_t seasonProgress(uint8_t month, uint8_t day) {
    uint8_t index = 0;
    if (month == 12) index = 0;
    else if (month == 1 || month == 2) index = month;
    else if (month >= 3 && month <= 5) index = (uint8_t)(month - 3);
    else if (month >= 6 && month <= 8) index = (uint8_t)(month - 6);
    else if (month >= 9 && month <= 11) index = (uint8_t)(month - 9);
    else return 0;

    if (day < 1) day = 1;
    if (day > 31) day = 31;
    uint16_t span = (uint16_t)index * 85u + ((uint16_t)(day - 1) * 85u) / 30u;
    if (span > 255u) span = 255u;
    return (uint8_t)span;
}

struct AtmospherePalette {
    uint8_t skyTopR, skyTopG, skyTopB;
    uint8_t skyBottomR, skyBottomG, skyBottomB;
    uint8_t ledR, ledG, ledB;
    float starOpacity;      // 0.0 (day) to 1.0 (deep night)
    float celestialPos;     // 0.0 (sunrise) -> 0.5 (zenith) -> 1.0 (sunset)
};

class TimeAtmosphereManager {
public:
    TimeAtmosphereManager();
    void begin();
    
    // Update atmosphere based on 24h time
    void update(uint8_t hour, uint8_t minute, uint8_t second);

    // Fast rendering of the time-shifted sky gradient to buffer
    void renderSky(uint16_t* buffer, int16_t bufWidth, int16_t bufHeight, 
                   int16_t startY, int16_t height);

    // Getters
    TimePhase getCurrentPhase() const { return _currentPhase; }
    const char* getPhaseName() const;
    AtmospherePalette getCurrentPalette() const { return _currentPalette; }
    RGBColor getAmbientLedColor() const { 
        return {_currentPalette.ledR, _currentPalette.ledG, _currentPalette.ledB}; 
    }

private:
    TimePhase _currentPhase;
    AtmospherePalette _currentPalette;
    
    AtmospherePalette interpolatePalettes(const AtmospherePalette& p1, 
                                          const AtmospherePalette& p2, 
                                          float t);
    AtmospherePalette getPhaseBasePalette(TimePhase phase);
};

extern TimeAtmosphereManager Atmosphere;

#endif // TIME_ATMOSPHERE_H
