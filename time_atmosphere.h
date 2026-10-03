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
