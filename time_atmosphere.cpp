#include "time_atmosphere.h"
#include "hal_display.h"

TimeAtmosphereManager Atmosphere;

TimeAtmosphereManager::TimeAtmosphereManager()
    : _currentPhase(PHASE_DAYLIGHT) {
    _currentPalette = getPhaseBasePalette(PHASE_DAYLIGHT);
}

void TimeAtmosphereManager::begin() {
    _currentPalette = getPhaseBasePalette(PHASE_DAYLIGHT);
}

AtmospherePalette TimeAtmosphereManager::getPhaseBasePalette(TimePhase phase) {
    AtmospherePalette pal;
    switch (phase) {
        case PHASE_DAWN: // 05:30 - 08:00 (Pastel Peach to Lavender)
            pal.skyTopR = 52;   pal.skyTopG = 38;   pal.skyTopB = 84;
            pal.skyBottomR = 248; pal.skyBottomG = 172; pal.skyBottomB = 138;
            pal.ledR = 42;      pal.ledG = 22;      pal.ledB = 16;
            pal.starOpacity = 0.2f;
            pal.celestialPos = 0.15f;
            break;

        case PHASE_DAYLIGHT: // 08:00 - 17:30 (Crisp Azure to Pale Cyan)
            pal.skyTopR = 48;   pal.skyTopG = 118;  pal.skyTopB = 205;
            pal.skyBottomR = 175; pal.skyBottomG = 220; pal.skyBottomB = 248;
            pal.ledR = 45;      pal.ledG = 45;      pal.ledB = 38;
            pal.starOpacity = 0.0f;
            pal.celestialPos = 0.50f;
            break;

        case PHASE_GOLDEN_HOUR: // 17:30 - 19:30 (Coral Crimson to Honey Amber)
            pal.skyTopR = 175;  pal.skyTopG = 72;   pal.skyTopB = 92;
            pal.skyBottomR = 255; pal.skyBottomG = 180; pal.skyBottomB = 65;
            pal.ledR = 55;      pal.ledG = 30;      pal.ledB = 6;
            pal.starOpacity = 0.05f;
            pal.celestialPos = 0.85f;
            break;

        case PHASE_DUSK: // 19:30 - 21:30 (Indigo to Magenta Twilight)
            pal.skyTopR = 32;   pal.skyTopG = 24;   pal.skyTopB = 78;
            pal.skyBottomR = 135; pal.skyBottomG = 55;  pal.skyBottomB = 105;
            pal.ledR = 26;      pal.ledG = 10;      pal.ledB = 36;
            pal.starOpacity = 0.45f;
            pal.celestialPos = 0.95f;
            break;

        case PHASE_NIGHT: // 21:30 - 05:30 (Midnight Obsidian to Deep Navy)
        default:
            pal.skyTopR = 6;    pal.skyTopG = 8;    pal.skyTopB = 22;
            pal.skyBottomR = 18;  pal.skyBottomG = 30;  pal.skyBottomB = 52;
            pal.ledR = 4;       pal.ledG = 16;      pal.ledB = 18;
            pal.starOpacity = 1.0f;
            pal.celestialPos = 0.0f;
            break;
    }
    return pal;
}

AtmospherePalette TimeAtmosphereManager::interpolatePalettes(const AtmospherePalette& p1, 
                                                            const AtmospherePalette& p2, 
                                                            float t) {
    // Cosine ease-in-out curve for natural, organic transitions
    float easedT = 0.5f * (1.0f - cosf(t * PI));

    AtmospherePalette out;
    out.skyTopR = (uint8_t)(p1.skyTopR + (p2.skyTopR - p1.skyTopR) * easedT);
    out.skyTopG = (uint8_t)(p1.skyTopG + (p2.skyTopG - p1.skyTopG) * easedT);
    out.skyTopB = (uint8_t)(p1.skyTopB + (p2.skyTopB - p1.skyTopB) * easedT);

    out.skyBottomR = (uint8_t)(p1.skyBottomR + (p2.skyBottomR - p1.skyBottomR) * easedT);
    out.skyBottomG = (uint8_t)(p1.skyBottomG + (p2.skyBottomG - p1.skyBottomG) * easedT);
    out.skyBottomB = (uint8_t)(p1.skyBottomB + (p2.skyBottomB - p1.skyBottomB) * easedT);

    out.ledR = (uint8_t)(p1.ledR + (p2.ledR - p1.ledR) * easedT);
    out.ledG = (uint8_t)(p1.ledG + (p2.ledG - p1.ledG) * easedT);
    out.ledB = (uint8_t)(p1.ledB + (p2.ledB - p1.ledB) * easedT);

    out.starOpacity = p1.starOpacity + (p2.starOpacity - p1.starOpacity) * easedT;
    out.celestialPos = p1.celestialPos + (p2.celestialPos - p1.celestialPos) * easedT;
    return out;
}

void TimeAtmosphereManager::update(uint8_t hour, uint8_t minute, uint8_t second) {
    uint32_t currentMinute = (hour * 60) + minute;

    // Phase Time Bounds (in minutes from midnight 00:00)
    // Dawn: 330 to 480 (05:30 to 08:00) = 150 mins
    // Daylight: 480 to 1050 (08:00 to 17:30) = 570 mins
    // Golden Hour: 1050 to 1170 (17:30 to 19:30) = 120 mins
    // Dusk: 1170 to 1290 (19:30 to 21:30) = 120 mins
    // Night: 1290 to 330 (21:30 to 05:30 next day) = 480 mins

    if (currentMinute >= 330 && currentMinute < 480) {
        _currentPhase = PHASE_DAWN;
        float progress = (float)(currentMinute - 330) / 150.0f;
        _currentPalette = interpolatePalettes(getPhaseBasePalette(PHASE_DAWN), 
                                              getPhaseBasePalette(PHASE_DAYLIGHT), 
                                              progress);
    } else if (currentMinute >= 480 && currentMinute < 1050) {
        _currentPhase = PHASE_DAYLIGHT;
        float progress = (float)(currentMinute - 480) / 570.0f;
        _currentPalette = interpolatePalettes(getPhaseBasePalette(PHASE_DAYLIGHT), 
                                              getPhaseBasePalette(PHASE_GOLDEN_HOUR), 
                                              progress);
    } else if (currentMinute >= 1050 && currentMinute < 1170) {
        _currentPhase = PHASE_GOLDEN_HOUR;
        float progress = (float)(currentMinute - 1050) / 120.0f;
        _currentPalette = interpolatePalettes(getPhaseBasePalette(PHASE_GOLDEN_HOUR), 
                                              getPhaseBasePalette(PHASE_DUSK), 
                                              progress);
    } else if (currentMinute >= 1170 && currentMinute < 1290) {
        _currentPhase = PHASE_DUSK;
        float progress = (float)(currentMinute - 1170) / 120.0f;
        _currentPalette = interpolatePalettes(getPhaseBasePalette(PHASE_DUSK), 
                                              getPhaseBasePalette(PHASE_NIGHT), 
                                              progress);
    } else {
        _currentPhase = PHASE_NIGHT;
        float progress = 0.0f;
        if (currentMinute >= 1290) {
            progress = (float)(currentMinute - 1290) / 480.0f;
        } else {
            progress = (float)(currentMinute + (1440 - 1290)) / 480.0f;
        }
        _currentPalette = interpolatePalettes(getPhaseBasePalette(PHASE_NIGHT), 
                                              getPhaseBasePalette(PHASE_DAWN), 
                                              progress);
    }
}

const char* TimeAtmosphereManager::getPhaseName() const {
    switch (_currentPhase) {
        case PHASE_DAWN: return "DAWN";
        case PHASE_DAYLIGHT: return "DAY";
        case PHASE_GOLDEN_HOUR: return "GOLDEN";
        case PHASE_DUSK: return "DUSK";
        case PHASE_NIGHT: return "NIGHT";
        default: return "UNKNOWN";
    }
}

void TimeAtmosphereManager::renderSky(uint16_t* buffer, int16_t bufWidth, int16_t bufHeight, 
                                     int16_t startY, int16_t height) {
    if (!buffer || height <= 0) return;

    uint16_t colorTop = rgb565(_currentPalette.skyTopR, _currentPalette.skyTopG, _currentPalette.skyTopB);
    uint16_t colorBottom = rgb565(_currentPalette.skyBottomR, _currentPalette.skyBottomG, _currentPalette.skyBottomB);

    uint8_t r1 = _currentPalette.skyTopR;
    uint8_t g1 = _currentPalette.skyTopG;
    uint8_t b1 = _currentPalette.skyTopB;

    uint8_t r2 = _currentPalette.skyBottomR;
    uint8_t g2 = _currentPalette.skyBottomG;
    uint8_t b2 = _currentPalette.skyBottomB;

    for (int16_t row = 0; row < height; row++) {
        int16_t py = startY + row;
        if (py >= bufHeight) break;

        float factor = (float)row / (float)(height - 1);
        uint8_t r = (uint8_t)(r1 + factor * (r2 - r1));
        uint8_t g = (uint8_t)(g1 + factor * (g2 - g1));
        uint8_t b = (uint8_t)(b1 + factor * (b2 - b1));
        uint16_t rowColor = rgb565(r, g, b);

        uint16_t* rowPtr = &buffer[py * bufWidth];
        for (int16_t x = 0; x < bufWidth; x++) {
            rowPtr[x] = rowColor;
        }
    }
}
